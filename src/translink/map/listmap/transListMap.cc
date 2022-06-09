/**
 * Trans List Map
 *
 * A map implemented with a list using transactions
 */
#include "translink/map/listmap/transListMap.h"

TransListMap::TransListMap(TransactionHandler* transaction)
    : m_tail(new Node(0xffffffff, NULL, NULL))
    , m_head(new Node(0, m_tail, NULL))
    , m_nodeAllocator(transaction->cap * transaction->threadCount *  Node::SizeOf() * transaction->transSize, transaction->threadCount, Node::SizeOf())
    , m_nodeDescAllocator(transaction->cap * transaction->threadCount *  NodeDesc::SizeOf() * transaction->transSize, transaction->threadCount, NodeDesc::SizeOf())
{
    this->transaction = transaction;
    this->transaction->ResetMetrics();
}

TransListMap::~TransListMap()
{
    printMetrics();
}

ReturnCode TransListMap::Insert(setkey_t key, Desc* desc, uint8_t opid,
            Node*& inserted, Node*& pred)
{
    // create the nodeDesc which refers to the current operation in the descriptor
    inserted = NULL;
    NodeDesc* nodeDesc = new(m_nodeDescAllocator.Alloc()) NodeDesc(desc, NULL, opid);
    Node* new_node = NULL;
    Node* curr = m_head;

    while(true)
    {
        LocatePred(pred, curr, key);

        if(!DoesNodeExist(curr, key))
        {

            if(desc->status != ACTIVE)
            {
                return FAIL;
            }

            if(new_node == NULL)
            {
                if(nodeDesc == NULL)
                {
                    // I don't think this code gets called anymore
                    // now that we allocate the nodeDesc at the
                    // beginning of the method
                    nodeDesc = new(this->m_nodeDescAllocator.Alloc()) NodeDesc(desc, NULL, opid);
                    // nodeDesc->desc = desc;
                    // nodeDesc->opid = opid;
                    // // save a reference to the old Descriptor
                    // nodeDesc->previousNodeDesc = NULL;
                }
                new_node = new(m_nodeAllocator.Alloc()) Node(key, NULL, nodeDesc);
            }
            new_node->next[0] = curr;

            Node* pred_next = __sync_val_compare_and_swap(&pred->next[0], curr, new_node);

            // if pred->next still points to curr
            if(pred_next == curr)
            {
                ASSERT_CODE
                    (
                     __sync_fetch_and_add(&g_count_ins, 1);
                     __sync_fetch_and_add(&g_count_ins_new, 1);
                    );

                inserted = new_node;
                return OK;
            }

            // Restart
            //
            // pred->next does not point to curr anymore.
            // If pred->next is marked for deletion,
            // relocate curr from head, otherwise relocate
            // from pred.
            curr = IS_MARKED(pred_next) ? m_head : pred;
        }
        else
        {
            NodeDesc* oldCurrDesc = curr->nodeDesc;

            if(IS_MARKED(oldCurrDesc))
            {
                if(!IS_MARKED(curr->next[0]))
                {
                    (__sync_fetch_and_or(&curr->next[0], 0x1));
                }
                curr = m_head;
                continue;
            }

            FinishPendingTxn(oldCurrDesc, desc);

            if(nodeDesc == NULL)
            {
                // save a reference to the old Descriptor
                nodeDesc = new(this->m_nodeDescAllocator.Alloc()) NodeDesc(desc,
                    (oldCurrDesc->desc->status == ABORTED) ? oldCurrDesc->previousNodeDesc : oldCurrDesc
                    , opid);
                // nodeDesc->desc = desc;
                // nodeDesc->opid = opid;
                // save a reference to the old Descriptor
                // if (oldCurrDesc->desc->status == ABORTED) {
                //     nodeDesc->previousNodeDesc = oldCurrDesc->previousNodeDesc;
                // } else {
                //     nodeDesc->previousNodeDesc = oldCurrDesc;
                // }
            }

            if(IsSameOperation(oldCurrDesc, nodeDesc))
            {
                return SKIP;
            }

            if(!DoesKeyExist(oldCurrDesc))
            {
                NodeDesc* currDesc = curr->nodeDesc;

                if(desc->status != ACTIVE)
                {
                    return FAIL;
                }

                // Update desc
                currDesc = __sync_val_compare_and_swap(&curr->nodeDesc, oldCurrDesc, nodeDesc);

                if(currDesc == oldCurrDesc)
                {
                    ASSERT_CODE
                        (
                         __sync_fetch_and_add(&g_count_ins, 1);
                        );

                    inserted = curr;
                    return OK; 
                }
            }
            else
            {
                return FAIL;
            }
        }
    }
}

ReturnCode TransListMap::Delete(setkey_t key, Desc* desc, uint8_t opid, Node*& deleted, Node*& pred)
{
    deleted = NULL;
    NodeDesc* nodeDesc = new(m_nodeDescAllocator.Alloc()) NodeDesc(desc, NULL, opid);
    Node* curr = m_head;

    while(true)
    {
        LocatePred(pred, curr, key);

        if(DoesNodeExist(curr, key))
        {
            NodeDesc* oldCurrDesc = curr->nodeDesc;

            if(IS_MARKED(oldCurrDesc))
            {
                return FAIL;
            }

            FinishPendingTxn(oldCurrDesc, desc);

            if(nodeDesc == NULL)
            {
                // save a reference to the old Descriptor
                nodeDesc = new (this->m_nodeDescAllocator.Alloc()) NodeDesc(desc,
                    (oldCurrDesc->desc->status == ABORTED) ? oldCurrDesc->previousNodeDesc : oldCurrDesc
                    , opid);
                // nodeDesc->desc = desc;
                // nodeDesc->opid = opid;
                // // save a reference to the old Descriptor
                // if (oldCurrDesc->desc->status == ABORTED) {
                //     nodeDesc->previousNodeDesc = oldCurrDesc->previousNodeDesc;
                // } else {
                //     nodeDesc->previousNodeDesc = oldCurrDesc;
                // }
            }

            if(IsSameOperation(oldCurrDesc, nodeDesc))
            {
                return SKIP;
            }

            if(DoesKeyExist(oldCurrDesc))
            {
                NodeDesc* currDesc = curr->nodeDesc;

                if(desc->status != ACTIVE)
                {
                    return FAIL;
                }

                //Update desc
                currDesc = __sync_val_compare_and_swap(&curr->nodeDesc, oldCurrDesc, nodeDesc);

                if(currDesc == oldCurrDesc)
                {
                    ASSERT_CODE
                        (
                         __sync_fetch_and_add(&g_count_del, 1);
                        );

                    deleted = curr;
                    return OK; 
                }
            }
            else
            {
                return FAIL;
            }
        }
        else
        {
            return FAIL;
        }
    }
}

ReturnCode TransListMap::Find(setkey_t key, Desc* desc, uint8_t opid, int& value)
{

    NodeDesc* nodeDesc = new(m_nodeDescAllocator.Alloc()) NodeDesc(desc, NULL, opid);
    Node* pred;
    Node* curr = m_head;

    while(true)
    {
        LocatePred(pred, curr, key);

        if(DoesNodeExist(curr, key))
        {
            /* TODO: Refactor to method */
            NodeDesc* oldCurrDesc = curr->nodeDesc;

            if(IS_MARKED(oldCurrDesc))
            {
                if(!IS_MARKED(curr->next[0]))
                {
                    (__sync_fetch_and_or(&curr->next[0], 0x1));
                }
                curr = m_head;
                continue;
            }

            FinishPendingTxn(oldCurrDesc, desc);
            /* end Refactor to method */

            if(nodeDesc == NULL)
            {
                // save a reference to the old Descriptor
                nodeDesc = new (this->m_nodeDescAllocator.Alloc()) NodeDesc(desc,
                (oldCurrDesc->desc->status == ABORTED) ? oldCurrDesc->previousNodeDesc : oldCurrDesc
                , opid);
                // nodeDesc->desc = desc;
                // nodeDesc->opid = opid;
                // // save a reference to the old Descriptor
                // if (oldCurrDesc->desc->status == ABORTED) {
                //     nodeDesc->previousNodeDesc = oldCurrDesc->previousNodeDesc;
                // } else {
                //     nodeDesc->previousNodeDesc = oldCurrDesc;
                // }
            }

            if(IsSameOperation(oldCurrDesc, nodeDesc))
            {
                return SKIP;
            }

            if(DoesKeyExist(oldCurrDesc))
            {
                NodeDesc* currDesc = curr->nodeDesc;

                if(desc->status != ACTIVE)
                {
                    return FAIL;
                }

                //Update desc
                currDesc = __sync_val_compare_and_swap(&curr->nodeDesc, oldCurrDesc, nodeDesc);

                if(currDesc == oldCurrDesc)
                {
                    if (currDesc->desc->status != ABORTED) {
                        value = currDesc->desc->ops[currDesc->opid].value;
                    } else if (currDesc->previousNodeDesc != NULL) {
                        value = currDesc->previousNodeDesc->desc->ops[currDesc->previousNodeDesc->opid].value;
                    }
                    // Update the new node descriptor with the old value if we
                    // were just looking for the node
                    if (nodeDesc->desc->ops[nodeDesc->opid].type == FIND) {
                        nodeDesc->desc->ops[nodeDesc->opid].value = value;
                    }
                    return OK; 
                }
            }
            else
            {
                return FAIL;
            }
        }
        else
        {
            return FAIL;
        }
    }
}

inline bool TransListMap::IsSameOperation(NodeDesc* nodeDesc1, NodeDesc* nodeDesc2)
{
    return nodeDesc1->desc == nodeDesc2->desc && nodeDesc1->opid == nodeDesc2->opid;
}

inline bool TransListMap::DoesNodeExist(Node* node, setkey_t key)
{
    return node != NULL && node->key == key;
}

inline void TransListMap::FinishPendingTxn(NodeDesc* nodeDesc, Desc* desc)
{
    // The node accessed by the operations in same transaction is always active
    if(nodeDesc->desc == desc)
    {
        return;
    }

    if (desc->function == NULL) {
        transaction->HelpOps(nodeDesc->desc, nodeDesc->opid + 1);
    } else {
        transaction->HelpOps(nodeDesc->desc);
    }
}

inline bool TransListMap::IsNodeActive(NodeDesc* nodeDesc)
{
    return nodeDesc->desc->status == COMMITTED;
}

inline bool TransListMap::DoesKeyExist(NodeDesc* nodeDesc)
{
    bool isNodeActive = IsNodeActive(nodeDesc);
    uint8_t opType = nodeDesc->desc->ops[nodeDesc->opid].type;

    return  (opType == FIND) || (isNodeActive && opType == INSERT) || (!isNodeActive && opType == DELETE);
}

inline void TransListMap::LocatePred(Node*& pred, Node*& curr, setkey_t key)
{
    Node* pred_next;

    while(curr->key < key)
    {
        pred = curr;
        pred_next = CLR_MARK(pred->next[0]);
        curr = pred_next;

        if (curr->nodeDesc != NULL && curr->nodeDesc->desc->status == ABORTED) {
            int temp = 0;

        }

        while(IS_MARKED(curr->next[0]))
        {
            curr = CLR_MARK(curr->next[0]);
        }

        if(curr != pred_next)
        {
            //Failed to remove deleted nodes, start over from pred
            if(!__sync_bool_compare_and_swap(&pred->next[0], pred_next, curr))
            {
                curr = m_head;
            }
        }
    }

    ASSERT(pred, "pred must be valid");
}

void TransListMap::Init()
{
    m_nodeAllocator.Init();
    m_nodeDescAllocator.Init();
}

inline void TransListMap::Print()
{
    Node* curr = m_head->next[0];

    while(curr != m_tail)
    {
        printf("Node [%p] Key [%lu] Status [%s]\n", curr, curr->key, DoesKeyExist(CLR_MARKD(curr->nodeDesc))? "Exist":"Inexist");
        curr = CLR_MARK(curr->next[0]);
    }
}

void TransListMap::printMetrics()
{
    printf("Total commit %u, abort (total/fake) %u/%u\n", transaction->g_count_commit, transaction->g_count_abort, transaction->g_count_fake_abort);
}