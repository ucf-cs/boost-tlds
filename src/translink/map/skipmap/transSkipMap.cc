#include "translink/map/skipmap/transSkipMap.h"

TransSkipMap::~TransSkipMap()
{
    transskip_free();
}

ReturnCode TransSkipMap::Insert(setkey_t key, Desc* desc, uint8_t opid,
            Node*& inserted, Node*& pred)
{
    inserted = NULL;
    bool ret = false;
    NodeDesc* nodeDesc = NULL;

    ptst_t    *ptst;
    Node* preds[NUM_LEVELS], *succs[NUM_LEVELS];
    Node* succ, *new_node = NULL, *new_next, *old_next;

    int i, level;

    key = CALLER_TO_INTERNAL_KEY(key);

    ptst = fr_critical_enter();

    succ = weak_search_predecessors(key, preds, succs);

 retry:

    if ( succ->key == key )
    {
        NodeDesc* oldCurrDesc = succ->nodeDesc;

        if(IS_MARKED(oldCurrDesc))
        {
            READ_FIELD(level, succ->level);
            mark_deleted(succ, level & LEVEL_MASK);
            succ = strong_search_predecessors(key, preds, succs);
            goto retry;
        }

        if(!FinishPendingTxn(oldCurrDesc, desc))
        {
            if ( new_node != NULL ) free_node(ptst, new_node);
            ret = false;
            goto out;
        }

        if(nodeDesc == NULL)
        {
            nodeDesc = this->m_nodeDescAllocator->Alloc();
            nodeDesc->desc = desc;
            nodeDesc->opid = opid;
            // save a reference to the old Descriptor
            if (oldCurrDesc->desc->status == ABORTED) {
                nodeDesc->previousNodeDesc = oldCurrDesc->previousNodeDesc;
            } else {
                nodeDesc->previousNodeDesc = oldCurrDesc;
            }
        }

        if(IsSameOperation(oldCurrDesc, nodeDesc))
        {
            ret = true;
            goto out;
        }

        if(!IsKeyExist(oldCurrDesc))
        {
            NodeDesc* currDesc = succ->nodeDesc;

            if(desc->status != ACTIVE)
            {
                if ( new_node != NULL ) free_node(ptst, new_node);
                ret = false;
                goto out;
            }

            //if(currDesc == oldCurrDesc)
            {
                //Update desc
                currDesc = __sync_val_compare_and_swap(&succ->nodeDesc, oldCurrDesc, nodeDesc);

                if(currDesc == oldCurrDesc)
                {
                    if ( new_node != NULL ) free_node(ptst, new_node);
                    inserted = succ;
                    ret = true;
                    goto out;
                }
            }

            goto retry;
        }
        else
        {
            if ( new_node != NULL ) free_node(ptst, new_node);
            ret = false;
            goto out;
        }
    }

#ifdef WEAK_MEM_ORDER
    /* Free node from previous attempt, if this is a retry. */
    if ( new_node != NULL ) 
    { 
        free_node(ptst, new_node);
        new_node = NULL;
    }
#endif

    /* Not in the list, so initialise a new node for insertion. */
    if ( new_node == NULL )
    {
        new_node    = alloc_node(ptst);
        new_node->key = key;
        //new_node->v = (void*)0xf0f0f0f0;

        if(nodeDesc == NULL)
        {
            nodeDesc = this->m_nodeDescAllocator->Alloc();
            nodeDesc->desc = desc;
            nodeDesc->opid = opid;
            // previousDesc will be NULL beacause this is a new node
            nodeDesc->previousNodeDesc = NULL;
        }

        new_node->nodeDesc = nodeDesc;
    }
    level = new_node->level;

    /* If successors don't change, this saves us some CAS operations. */
    for ( i = 0; i < level; i++ )
    {
        new_node->next[i] = succs[i];
    }

    /* We've committed when we've inserted at level 1. */
    WMB_NEAR_CAS(); /* make sure node fully initialised before inserting */

    if(desc->status != ACTIVE)
    {
        ret = false;
        goto out;
    }

    /* Swap the old node with new_node */
    old_next = CASPO(&preds[0]->next[0], succ, new_node);
    if ( old_next != succ )
    {
        succ = strong_search_predecessors(key, preds, succs);
        goto retry;
    }

    /* Insert at each of the other levels in turn. */
    i = 1;
    while ( i < level )
    {
        pred = preds[i];
        succ = succs[i];

        /* Someone *can* delete @new under our feet! */
        new_next = new_node->next[i];
        if ( is_marked_ref(new_next) ) goto success;

        /* Ensure forward pointer of new node is up to date. */
        if ( new_next != succ )
        {
            old_next = CASPO(&new_node->next[i], new_next, succ);
            if ( is_marked_ref(old_next) ) goto success;
            assert(old_next == new_next);
        }

        /* Ensure we have unique key values at every level. */
        if ( succ->key == key ) goto new_world_view;
        assert((pred->key < key) && (succ->key > key));

        /* Replumb predecessor's forward pointer. */
        old_next = CASPO(&pred->next[i], succ, new_node);
        if ( old_next != succ )
        {
        new_world_view:
            RMB(); /* get up-to-date view of the world. */
            (void)strong_search_predecessors(key, preds, succs);
            continue;
        }

        /* Succeeded at this level. */
        i++;
    }

 success:
    /* Ensure node is visible at all levels before punting deletion. */
    WEAK_DEP_ORDER_WMB();
    if ( check_for_full_delete(new_node) ) 
    {
        MB(); /* make sure we see all marks in @new. */
        do_full_delete(ptst, new_node, level - 1);
    }

    inserted = new_node;
    ret = true;

 out:
    fr_critical_exit(ptst);
    inserted = NULL;
    pred = NULL;
    return ret ? OK : FAIL;
}

ReturnCode TransSkipMap::Delete(setkey_t key, Desc* desc, uint8_t opid, Node*& deleted, Node*& pred)
{
    deleted = NULL;
    bool ret = false;
    NodeDesc* nodeDesc = NULL;

    ptst_t    *ptst;
    Node      *succ;

    key = CALLER_TO_INTERNAL_KEY(key);

    ptst = fr_critical_enter();

    succ = weak_search_predecessors(key, NULL, NULL);

 retry:

    if ( succ->key == key )
    {
        NodeDesc* oldCurrDesc = succ->nodeDesc;

        if(IS_MARKED(oldCurrDesc))
        {
            ret = false;
            goto out;
            //READ_FIELD(level, succ->level);
            //mark_deleted(succ, level & LEVEL_MASK);
            //succ = strong_search_predecessors(key, preds, succs);
            //goto retry;
        }

        if(!FinishPendingTxn(oldCurrDesc, desc))
        {
            ret = false;
            goto out;
        }

        if(nodeDesc == NULL)
        {
            nodeDesc = this->m_nodeDescAllocator->Alloc();
            nodeDesc->desc = desc;
            nodeDesc->opid = opid;
            // save a reference to the old Descriptor
            if (oldCurrDesc->desc->status == ABORTED) {
                nodeDesc->previousNodeDesc = oldCurrDesc->previousNodeDesc;
            } else {
                nodeDesc->previousNodeDesc = oldCurrDesc;
            }
        }

        if(IsSameOperation(oldCurrDesc, nodeDesc))
        {
            ret = true;
            goto out;
        }

        if(IsKeyExist(oldCurrDesc))
        {
            NodeDesc* currDesc = succ->nodeDesc;

            if(desc->status != ACTIVE)
            {
                ret = false;
                goto out;
            }

            //if(currDesc == oldCurrDesc)
            {
                //Update desc 
                currDesc = __sync_val_compare_and_swap(&succ->nodeDesc, oldCurrDesc, nodeDesc);

                if(currDesc == oldCurrDesc)
                {
                    deleted = succ;
                    ret = true;
                    goto out;
                }
            }

            goto retry;
        }
        else
        {
            ret = false;
            goto out;
        }
    }
    else
    {
        ret = false;
        goto out;
    }

out:
    fr_critical_exit(ptst);
    deleted = NULL;
    return ret ? OK : FAIL;
}

ReturnCode TransSkipMap::Find(setkey_t key, Desc* desc, uint8_t opid, int &value)
{
    NodeDesc* nodeDesc = NULL;

    bool ret;
    ptst_t *ptst;
    Node   *x;

    key = CALLER_TO_INTERNAL_KEY(key);

    ptst = fr_critical_enter();

    x = weak_search_predecessors(key, NULL, NULL);

retry:
    if ( x->key == key )
    {
        NodeDesc* oldCurrDesc = x->nodeDesc;

        if(IS_MARKED(oldCurrDesc))
        {
            ret = false;
            goto out;
            //READ_FIELD(level, x->level);
            //mark_deleted(x, level & LEVEL_MASK);
            //x = strong_search_predecessors(key, NULL, NULL);
            //goto retry;
        }

        if(!FinishPendingTxn(oldCurrDesc, desc))
        {
            ret = false;
            goto out;
        }

        if(nodeDesc == NULL)
        {
            nodeDesc = this->m_nodeDescAllocator->Alloc();
            nodeDesc->desc = desc;
            nodeDesc->opid = opid;
            // save a reference to the old Descriptor
            if (oldCurrDesc->desc->status == ABORTED) {
                nodeDesc->previousNodeDesc = oldCurrDesc->previousNodeDesc;
            } else {
                nodeDesc->previousNodeDesc = oldCurrDesc;
            }
        }

        if(IsSameOperation(oldCurrDesc, nodeDesc))
        {
            ret = true;
            goto out;
        }

        if(IsKeyExist(oldCurrDesc))
        {
            NodeDesc* currDesc = x->nodeDesc;

            if(desc->status != ACTIVE)
            {
                ret = false;
                goto out;
            }

            //if(currDesc == oldCurrDesc)
            {
                //Update desc
                currDesc = __sync_val_compare_and_swap(&x->nodeDesc, oldCurrDesc, nodeDesc);

                if(currDesc == oldCurrDesc)
                {
                    ret = true;
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
                    goto out;
                }
            }

            goto retry;
        }
        else
        {
            ret = false;
            goto out;
        }
    }
    else
    {
        ret = false;
        goto out;
    }

out:
    fr_critical_exit(ptst);
    return ret ? OK : FAIL;
}

/* This function does not remove marked nodes. Use it optimistically. */
Node* TransSkipMap::weak_search_predecessors(setkey_t k, Node* *pa, Node* *na)
{
    Node* x, *x_next;
    setkey_t  x_next_k;
    int        i;

    x = &m_head;
    for ( i = NUM_LEVELS - 1; i >= 0; i-- )
    {
        for ( ; ; )
        {
            READ_FIELD(x_next, x->next[i]);
            x_next = (Node*)get_unmarked_ref(x_next);

            READ_FIELD(x_next_k, x_next->key);
            if ( x_next_k >= k ) break;

            x = x_next;
        }

        if ( pa ) pa[i] = x;
        if ( na ) na[i] = x_next;
    }

    return(x_next);
}

Node* TransSkipMap::strong_search_predecessors(setkey_t k, Node* *pa, Node* *na)
{
    Node* x, *x_next, *old_x_next, *y, *y_next;
    setkey_t y_k;
    int        i;

 retry:
    RMB();

    x = &m_head;
    for ( i = NUM_LEVELS - 1; i >= 0; i-- )
    {
        /* We start our search at previous level's unmarked predecessor. */
        READ_FIELD(x_next, x->next[i]);
        /* If this pointer's marked, so is @pa[i+1]. May as well retry. */
        if ( is_marked_ref(x_next) ) goto retry;

        for ( y = x_next; ; y = y_next )
        {
            /* Shift over a sequence of marked nodes. */
            for ( ; ; )
            {
                READ_FIELD(y_next, y->next[i]);
                if ( !is_marked_ref(y_next) ) break;
                y = (Node*)get_unmarked_ref(y_next);
            }

            READ_FIELD(y_k, y->key);
            if ( y_k >= k ) break;

            /* Update estimate of predecessor at this level. */
            x      = y;
            x_next = y_next;
        }

        /* Swing forward pointer over any marked nodes. */
        if ( x_next != y )
        {
            old_x_next = CASPO(&x->next[i], x_next, y);
            if ( old_x_next != x_next ) goto retry;
        }

        if ( pa ) pa[i] = x;
        if ( na ) na[i] = y;
    }

    return(y);
}

void TransSkipMap::mark_deleted(Node* x, int level)
{
    Node* x_next;

    while ( --level >= 0 )
    {
        x_next = x->next[level];
        while ( !is_marked_ref(x_next) )
        {
            x_next = CASPO(&x->next[level], x_next, get_marked_ref(x_next));
        }
        WEAK_DEP_ORDER_WMB(); /* mark in order */
    }
}

inline bool TransSkipMap::IsSameOperation(NodeDesc* nodeDesc1, NodeDesc* nodeDesc2)
{
    return nodeDesc1->desc == nodeDesc2->desc && nodeDesc1->opid == nodeDesc2->opid;
}

inline bool TransSkipMap::DoesNodeExist(Node* node, setkey_t key)
{
    return node != NULL && node->key == key;
}

inline bool TransSkipMap::FinishPendingTxn(NodeDesc* nodeDesc, Desc* desc)
{
    // The node accessed by the operations in same transaction is always active
    if(nodeDesc->desc == desc)
    {
        return true;
    }

    if (desc->function == NULL) {
        transaction->HelpOps(nodeDesc->desc, nodeDesc->opid + 1);
    } else {
        transaction->HelpOps(nodeDesc->desc);
    }

    return true;
}

void TransSkipMap::free_node(ptst_t *ptst, Node* n)
{
    fr_gc_free(ptst, (void *)n, gc_id[(n->level & LEVEL_MASK) - 1]);
}

Node* TransSkipMap::alloc_node(ptst_t *ptst)
{
    int l;
    Node *n;
    l = get_level(ptst);
    n = (Node*)fr_gc_alloc(ptst, gc_id[l - 1]);
    n->level = l;

    return(n);
}

int TransSkipMap::get_level(ptst_t *ptst)
{
    unsigned long r = rand_next(ptst);
    int l = 1;
    r = (r >> 4) & ((1 << (NUM_LEVELS-1)) - 1);
    while ( (r & 1) ) { l++; r >>= 1; }
    return(l);
}

int TransSkipMap::check_for_full_delete(Node* x)
{
    int level = x->level;
    return ((level & READY_FOR_FREE) ||
            (CASIO(&x->level, level, level | READY_FOR_FREE) != level));
}

void TransSkipMap::do_full_delete(ptst_t *ptst, Node* x, int level)
{
    int k = x->key;
#ifdef WEAK_MEM_ORDER
    Node* preds[NUM_LEVELS];
    int i = level;
 retry:
    (void)strong_search_predecessors(k, preds, NULL);
    /*
     * Above level 1, references to @x can disappear if a node is inserted
     * immediately before and we see an old value for its forward pointer. This
     * is a conservative way of checking for that situation.
     */
    if ( i > 0 ) RMB();
    while ( i > 0 )
    {
        Node *n = get_unmarked_ref(preds[i]->next[i]);
        while ( n->key < k )
        {
            n = get_unmarked_ref(n->next[i]);
            RMB(); /* we don't want refs to @x to "disappear" */
        }
        if ( n == x ) goto retry;
        i--; /* don't need to check this level again, even if we retry. */
    }
#else
    (void)strong_search_predecessors(k, NULL, NULL);
#endif
    free_node(ptst, x);
}

void TransSkipMap::init_transskip_subsystem(void)
{
    int i;

    fr_init_ptst_subsystem();
    fr_init_gc_subsystem();

    for ( i = 0; i < NUM_LEVELS; i++ )
    {
        gc_id[i] = fr_gc_add_allocator(sizeof(Node) + i*sizeof(Node *));
    }
}

void TransSkipMap::destroy_transskip_subsystem(void)
{
    fr_destroy_gc_subsystem();
}

inline bool TransSkipMap::IsKeyExist(NodeDesc* nodeDesc)
{
    bool isNodeActive = IsNodeActive(nodeDesc);
    uint8_t opType = nodeDesc->desc->ops[nodeDesc->opid].type;

    return (opType == FIND) || (isNodeActive && opType == INSERT) || (!isNodeActive && opType == DELETE);
}

inline bool TransSkipMap::IsNodeActive(NodeDesc* nodeDesc)
{
    return nodeDesc->desc->status == COMMITTED;
}

inline bool TransSkipMap::DoesKeyExist(NodeDesc* nodeDesc)
{
    bool isNodeActive = IsNodeActive(nodeDesc);
    uint8_t opType = nodeDesc->desc->ops[nodeDesc->opid].type;

    return  (opType == FIND) || (isNodeActive && opType == INSERT) || (!isNodeActive && opType == DELETE);
}

inline void TransSkipMap::LocatePred(Node*& pred, Node*& curr, setkey_t key)
{
    Node* pred_next;

    while(curr->key < key)
    {
        pred = curr;
        pred_next = CLR_MARK(pred->next[0]);
        curr = pred_next;

        while(IS_MARKED(curr->next[0]))
        {
            curr = CLR_MARK(curr->next[0]);
        }

        if(curr != pred_next)
        {
            //Failed to remove deleted nodes, start over from pred
            if(!__sync_bool_compare_and_swap(&pred->next[0], pred_next, curr))
            {
                curr = &m_head;
            }
        }
    }

    ASSERT(pred, "pred must be valid");
}

inline void TransSkipMap::Print()
{
    Node* curr = m_head.next[0];

    while(curr != m_tail)
    {
        printf("Node [%p] Key [%lu] Status [%s]\n", curr, curr->key, DoesKeyExist(CLR_MARKD(curr->nodeDesc))? "Exist":"Inexist");
        curr = CLR_MARK(curr->next[0]);
    }
}

TransSkipMap* TransSkipMap::Allocate(TransactionHandler* transaction)
{
    TransSkipMap* transSkipMap;
    Node *n;
    int i;

    n = (Node*)malloc(sizeof(*n) + (NUM_LEVELS-1)*sizeof(Node *));
    memset(n, 0, sizeof(*n) + (NUM_LEVELS-1)*sizeof(Node *));
    n->key = SENTINEL_KEYMAX;

    /*
     * Set the forward pointers of final node to other than NULL,
     * otherwise READ_FIELD() will continually execute costly barriers.
     * Note use of 0xfe -- that doesn't look like a marked value!
     */
    memset(n->next, 0xfe, NUM_LEVELS*sizeof(Node *));

    transSkipMap = (TransSkipMap*)malloc(sizeof(*transSkipMap) + (NUM_LEVELS-1)*sizeof(Node *));
    transSkipMap->m_head.key = SENTINEL_KEYMIN;
    transSkipMap->m_head.level = NUM_LEVELS;
    for ( i = 0; i < NUM_LEVELS; i++ )
    {
        transSkipMap->m_head.next[i] = n;
    }

    transSkipMap->m_tail = n;

    transSkipMap->m_nodeDescAllocator = new Allocator<NodeDesc>(transaction->cap * transaction->threadCount *  NodeDesc::SizeOf() * transaction->transSize, transaction->threadCount, NodeDesc::SizeOf());

    transSkipMap->transaction = transaction;

    new (transSkipMap) TransSkipMap;

    return transSkipMap;
}

void TransSkipMap::Init()
{
    m_nodeDescAllocator->Init();

    init_transskip_subsystem();
}

void TransSkipMap::transskip_free()
{
    printMetrics();

    //transskip_print(l);
}

void TransSkipMap::printMetrics()
{
    printf("Total commit %u, abort (total/fake) %u/%u\n", transaction->g_count_commit, transaction->g_count_abort, transaction->g_count_fake_abort);
}
