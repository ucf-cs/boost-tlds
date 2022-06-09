#include "translink/transaction.h"

__thread TransactionHandler::HelpStack TransactionHandler::helpStack;

__thread OnFinish* onCommit[256];
__thread OnFinish* onAbort[256];

__thread uint32_t onCommitIndex = 0;
__thread uint32_t onAbortIndex = 0;

static int current_tran = 0;

OnFinishAllocator<OnFinish>* TransactionHandler::m_onFinishAllocator;

TransactionHandler::TransactionHandler(uint64_t cap, uint64_t threadCount, uint32_t transSize, uint32_t testSize, uint32_t containerSize)
    : m_descAllocator(cap * threadCount * Desc::SizeOf(transSize) * containerSize, threadCount, Desc::SizeOf(transSize))
    , cap(cap)
    , threadCount(threadCount)
    , transSize(transSize)
    , testSize(testSize)
    , containerSize(containerSize)
{
    m_onFinishAllocator = new OnFinishAllocator<OnFinish>(cap * threadCount * sizeof(OnFinish) * containerSize * testSize, threadCount, sizeof(OnFinish));
}

/** Version of execute ops that takes in a list of operations to perform in a single transaction. (Non-DTT) */
bool TransactionHandler::ExecuteOps(const SetOpArray& ops)
{
    Desc* desc = m_descAllocator.Alloc();
    desc->size = ops.size();
    desc->status = ACTIVE;

    for(uint32_t i = 0; i < ops.size(); ++i)
    {
        desc->ops[i].type = ops[i].type;
        desc->ops[i].key = ops[i].key;
        desc->ops[i].value = ops[i].value;
        desc->ops[i].container = ops[i].container;
        char const* opTypeName = (ops[i].type == 0? "Find" : ops[i].type == 1 ? "Insert" : "Delete");
    }

    helpStack.Init();

    bool ret = HelpOps(desc, 0);

    //bool ret = desc->status != ABORTED;

    ASSERT_CODE
    (
        if(ret)
        {
            for(uint32_t i = 0; i < desc->size; ++i)
            {
                if(desc->ops[i].type == INSERT)
                {
                    __sync_fetch_and_add(&g_count, 1);
                }
                else if(desc->ops[i].type == DELETE)
                {
                    __sync_fetch_and_sub(&g_count, 1);
                }
                else
                {
                    __sync_fetch_and_add(&g_count_fnd, 1);
                }
                  break;
            }
        }
    );

    return ret;
}

/**
 * Version of execute ops that is used in dynamic transactions ehich takes in a function 
 * pointer, an input map, and an output map.
*/
bool TransactionHandler::ExecuteOps(const TransactionalFunction txnFunction, std::map<std::string, int>* inputMap, std::map<std::string, int>** outputMap) {

    helpStack.Init();
    onCommitIndex = 0;
    onAbortIndex = 0;

    Desc* desc = m_descAllocator.Alloc();
    desc->status = ACTIVE;
    desc->function = txnFunction;
    desc->inputMap = inputMap;
    desc->outputMap = outputMap;   
    desc->returnValue[256] = {};

    bool ret = HelpOps(desc);

    ASSERT_CODE
    (
        if(ret)
        {
            for(uint32_t i = 0; i < desc->size; ++i)
            {
                switch (desc->ops[i].dsType) {
                    case TXNLIST:
                        if(desc->ops[i].type == INSERT)
                        {
                            __sync_fetch_and_add(&g_count, 1);
                        }
                        else if(desc->ops[i].type == DELETE)
                        {
                            __sync_fetch_and_sub(&g_count, 1);
                        }
                        else
                        {
                            __sync_fetch_and_add(&g_count_fnd, 1);
                        }
                        break;
                }
            }
        }
    );

    return ret;
}

Desc* TransactionHandler::AllocateDesc(uint8_t size)
{
    Desc* desc = m_descAllocator.Alloc();
    desc->size = size;
    desc->status = ACTIVE;

    return desc;
}

bool TransactionHandler::HelpOps(Desc* desc, uint8_t opid)
{
    if(desc->status != ACTIVE)
    {
        return desc->status != ABORTED;
    }

    //Cyclic dependency check
    if(helpStack.Contain(desc))
    {
        if(__sync_bool_compare_and_swap(&desc->status, ACTIVE, ABORTED))
        {
            __sync_fetch_and_add(&g_count_abort, 1);
            __sync_fetch_and_add(&g_count_fake_abort, 1);
        }

        return desc->status != ABORTED;
    }

    ReturnCode ret = OK;

    std::vector<Node*> delNodes;
    std::vector<Node*> delPredNodes;
    std::vector<Node*> insNodes;
    std::vector<Node*> insPredNodes;

    helpStack.Push(desc);

    while(desc->status == ACTIVE && ret != FAIL && opid < desc->size)
    {
        const Operator& op = desc->ops[opid];

        if(op.type == INSERT)
        {
            Node* inserted;
            Node* pred;
            ret = op.container->Insert(op.key, desc, opid, inserted, pred);

#ifdef DEBUG
            printf("Inserting value %u into node %u into list [%p]... status: %d\n", desc->ops[opid].value, desc->ops[opid].key, desc->ops[opid].container, ret);
#endif

            insNodes.push_back(inserted);
            insPredNodes.push_back(pred);
        }
        else if(op.type == DELETE)
        {
            Node* deleted = NULL;
            Node* pred = NULL;
            ret = op.container->Delete(op.key, desc, opid, deleted, pred);
#ifdef DEBUG
            printf("Deleting value %u from node %u in list [%p]... status: %d\n", desc->ops[opid].value, desc->ops[opid].key, desc->ops[opid].container, ret);
#endif

            // TODO: Chris: This is a bandaid fix, we should check in skiplists if the transaction was
            // successful and then proceed to physically delete those nodes. I think skip list
            // handles delete in check_for_full_delete. That logic should(?) be moved out here.
            // Else, move this rollback logic into the TransList class

            // Only add nodes if the list populated those values
            if (deleted != NULL){
                delNodes.push_back(deleted);
            }
            if (pred != NULL) {
                delPredNodes.push_back(pred);
            }
        }
        else
        {

            int value = 0;
#ifdef DEBUG
            printf("Finding key %u (value %u) in list [%p]\n", desc->ops[opid].key, desc->ops[opid].value, desc->ops[opid].container);
#endif
            ret = op.container->Find(op.key, desc, opid, value);
#ifdef DEBUG
            printf("Found=%d. Key %u contains value %d\n", ret, op.key, value);
#endif
        }

        opid++;
    }

    helpStack.Pop();

    if(ret != FAIL)
    {
        if(__sync_bool_compare_and_swap(&desc->status, ACTIVE, COMMITTED))
        {
            if (delPredNodes.size() > 0) {
                MarkForDeletion(&delNodes, &delPredNodes, desc);
            }
            __sync_fetch_and_add(&g_count_commit, 1);
        }
    }
    else
    {
        if(__sync_bool_compare_and_swap(&desc->status, ACTIVE, ABORTED))
        {
            MarkForDeletion(&insNodes, &insPredNodes, desc);
            __sync_fetch_and_add(&g_count_abort, 1);
        }
    }

    return desc->status != ABORTED;
}

/**
 * definition of the helping scheme
 * in dynamic transactions
*/
bool TransactionHandler::HelpOps(Desc* desc)
{
    if(desc->status != ACTIVE)
    {
        return desc->status != ABORTED;
    }

    //Cyclic dependency check
    if(helpStack.Contain(desc))
    {
        if(__sync_bool_compare_and_swap(&desc->status, ACTIVE, ABORTED))
        {
            __sync_fetch_and_add(&g_count_abort, 1);
            __sync_fetch_and_add(&g_count_fake_abort, 1);
        }

        return desc->status != ABORTED;
    }

    ReturnCode ret = OK;

    std::vector<Node*> delNodes;
    std::vector<Node*> delPredNodes;
    std::vector<Node*> insNodes;
    std::vector<Node*> insPredNodes;

    helpStack.Push(desc);

    // populate local map
    std::map<std::string, int> localMap;
    localMap.insert(desc->inputMap->begin(), desc->inputMap->end());

    // Call the function
    ret = (*(desc->function))(desc, localMap);

    helpStack.Pop();

    if(ret != FAIL)
    {
        if(__sync_bool_compare_and_swap(&desc->status, ACTIVE, COMMITTED))
        {
            if (delPredNodes.size() > 0) {
                MarkForDeletion(&delNodes, &delPredNodes, desc);
            }
            __sync_fetch_and_add(&g_count_commit, 1);
        }
    }
    else
    {
        if(__sync_bool_compare_and_swap(&desc->status, ACTIVE, ABORTED))
        {
            MarkForDeletion(&insNodes, &insPredNodes, desc);
            __sync_fetch_and_add(&g_count_abort, 1);
        }
    }

    if (*(desc->outputMap) == NULL) {
        *(desc->outputMap) = new std::map<std::string, int>();
        (*(desc->outputMap))->insert(localMap.begin(), localMap.end());
    }

    if (onCommitIndex > 0) {

        for (u_int32_t i = 0; i < onCommitIndex; i++)
        {
            MarkForDeletionOnFinish(onCommit[i]);
            onCommit[i] = NULL;
        }
    }

    if (onAbortIndex > 0) {
        for (u_int32_t i = 0; i < onAbortIndex; i++)
        {
            MarkForDeletionOnFinish(onAbort[i]);
            onAbort[i] = NULL;
        }
    }

    return desc->status != ABORTED;
}

/*  
*   Definition of calling a single operation within a transaction
*/ 
ReturnCode TransactionHandler::CallOps(Desc* desc, TransactionalContainer* list, OpType opType, 
    setkey_t key, int value, int& foundValue)
{
    if (desc->status == ABORTED)
        return FAIL;

    int opId = helpStack.GetOpId();
    std::thread::id threadID = std::this_thread::get_id();

    if (desc->returnValue[opId] != NULL) {
#ifdef DEBUG
            if (opType == FIND) {
                printf("%p Finding key %u (value %u) in list [%p]\n", threadID, desc->ops[opId].key, key, desc->ops[opId].container);
            } else if (opType == INSERT) {
                printf("%p Inserting value %u into node %u into list [%p]\n", threadID, desc->ops[opId].value, key, desc->ops[opId].container);
            } else {
                printf("%p Deleting node %u (value %u) in list [%p]\n", threadID, key, desc->ops[opId].value, desc->ops[opId].container);
            }
            printf("%p    CallOps helping scheme activated: Op [%d] returned %d value: %u.\n", threadID, opType, *(desc->returnValue[opId]), desc->ops[opId].value);
#endif
        if (opType == FIND && *(desc->returnValue[opId]) == OK) {
            foundValue = desc->ops[opId].value;
            return *(desc->returnValue[opId]);
        }
        helpStack.NewOp();
        return *(desc->returnValue[opId]);
    }

    Operator op;
    op.type = opType;
    op.key = key;
    op.container = list;
    op.value = value;

    /** This operator variable holds the operation being performed.
    *   The desc->ops[] in this implementation is different from the
    *   non-transactional implementation where it carries a LIST of operations
    *   to perform instead.
    */
    desc->ops[opId] = op;

    ReturnCode* ret = new ReturnCode;

    if (opType == FIND)
    {
#ifdef DEBUG
        printf("%p Finding key %u (value %u) in list [%p]\n", threadID, desc->ops[opId].key, key, desc->ops[opId].container);
#endif
        *ret = list->Find(key, desc, opId, foundValue);
#ifdef DEBUG
        printf("%p Found=%d. Key %u contains value %d\n", threadID, *ret, key, foundValue);
#endif
    }
    else if (opType == INSERT)
    {
        Node* insertNode;
        Node* predNode;

        *ret = list->Insert(key, desc, opId, insertNode, predNode);
#ifdef DEBUG
        printf("%p Inserting value %u into node %u into list [%p]... status: %d\n", threadID, desc->ops[opId].value, key, desc->ops[opId].container, *ret);
#endif

        if (*ret == OK) {
            onCommit[onCommitIndex++] = new(TransactionHandler::m_onFinishAllocator->Alloc()) OnFinish{list, insertNode, predNode, desc};
        }
        else if (*ret == ABORTED) {
            onAbort[onAbortIndex++] = new(TransactionHandler::m_onFinishAllocator->Alloc()) OnFinish{list, insertNode, predNode, desc};
        }
    }
    else
    {
        Node* deleteNode;
        Node* predNode;
        
        *ret = list->Delete(key, desc, opId, deleteNode, predNode);
#ifdef DEBUG
        printf("%p Deleting node %u (value %u) in list [%p]... status: %d\n", threadID, key, desc->ops[opId].value, desc->ops[opId].container, *ret);
#endif
        
        if (*ret == OK) {
            onCommit[onCommitIndex++] = new(TransactionHandler::m_onFinishAllocator->Alloc()) OnFinish{list, deleteNode, predNode, desc};
        }
        else if (*ret == ABORTED) {
            onAbort[onAbortIndex++] = new(TransactionHandler::m_onFinishAllocator->Alloc()) OnFinish{list, deleteNode, predNode, desc};
        }
    }

    // if (*ret == OK) {
    if (*ret != SKIP) {
        desc->returnValue[opId] = ret;
        if (*ret == OK && opType == FIND) {
            desc->ops[opId].value = foundValue;
        }
        helpStack.NewOp();
    }
//     } else if (desc->returnValue[opId] != NULL) {
// #ifdef DEBUG
//             printf("%p    CallOps helping scheme activated (transaction failed but found returnValue): Op [%d] returned %d value: %u.\n", threadID, opType, *(desc->returnValue[opId]), desc->ops[opId].value);
// #endif
//         if (opType == FIND && *(desc->returnValue[opId]) == OK) {
//             foundValue = desc->ops[opId].value;
//         }
//         if (*ret != SKIP){
//             helpStack.NewOp();
//         }
//         return *(desc->returnValue[opId]);
//     } else if (*ret == FAIL) {
//         desc->returnValue[opId] = ret;
//         helpStack.NewOp();
//     }

    return *ret;
}

/**
 * logically delete nodes from data structure
*/
inline void TransactionHandler::MarkForDeletion(const std::vector<Node*>* nodes, const std::vector<Node*>* preds, Desc* desc)
{
    if (nodes == NULL) {
        return;
    }

    // Mark nodes for logical deletion
    for(uint32_t i = 0; i < nodes->size(); ++i)
    {
        Node* n = (*nodes)[i];
        if(n != NULL)
        {
            NodeDesc* nodeDesc = n->nodeDesc;

            if(nodeDesc->desc == desc)
            {
                if(__sync_bool_compare_and_swap(&n->nodeDesc, nodeDesc, SET_MARK(nodeDesc)))
                {
                    Node* pred = (*preds)[i];
                    Node* succ = CLR_MARK(__sync_fetch_and_or(&n->next[0], 0x1));
                    __sync_bool_compare_and_swap(&pred->next[0], n, succ);
                }
            }
        }
    }
}

void TransactionHandler::Init()
{
    m_descAllocator.Init();
    m_onFinishAllocator->Init();

    for (int i=0; i<containers.size(); i++) {
        containers[i]->Init();
    }
}

/**
 * logically delete nodes from data structure
*/
inline void TransactionHandler::MarkForDeletionOnFinish(const OnFinish* onFinish)
{
    if (onFinish == NULL || onFinish->node == NULL) {
        return;
    }

    // Mark nodes for logical deletion
    Node* n = onFinish->node;
    if(n != NULL)
    {
        NodeDesc* nodeDesc = n->nodeDesc;

        if(nodeDesc->desc == onFinish->desc)
        {
            if(__sync_bool_compare_and_swap(&n->nodeDesc, nodeDesc, SET_MARK(nodeDesc)))
            {
                Node* pred = onFinish->predNode;
                Node* succ = CLR_MARK(__sync_fetch_and_or(&n->next[0], 0x1));
                __sync_bool_compare_and_swap(&pred->next[0], n, succ);
            }
        }
    }
}

void TransactionHandler::ResetMetrics()
{
    g_count_commit = 0;
    g_count_abort = 0;
    g_count_fake_abort = 0;
}

void TransactionHandler::PrintMetrics()
{
    ASSERT_CODE
    (
        printf("Total node count %u, Inserts (total/new) %u/%u, Deletes (total/new) %u/%u, Finds %u\n", g_count, g_count_ins, g_count_ins_new, g_count_del , g_count_del_new, g_count_fnd);
    );
}
