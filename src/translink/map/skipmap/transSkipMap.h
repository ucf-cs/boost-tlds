/**
 * Trans Skip Map
 *
 * A hash map implemented with a skip list using transactions
 */
#ifndef TRANSSKIPMAP_H
#define TRANSSKIPMAP_H

#include "translink/transaction.h"
#include <assert.h>
#include <vector>

extern "C"
{
#include "common/fraser/portable_defns.h"
#include "common/fraser/ptst.h"
}

/* Fine for 2^NUM_LEVELS nodes. */
//#define NUM_LEVELS 20

/* Internal key values with special meanings. */
#define INVALID_FIELD   (0)    /* Uninitialised field value.     */
#define SENTINEL_KEYMIN ( 1UL) /* Key value of first dummy node. */
#define SENTINEL_KEYMAX (~0UL) /* Key value of last dummy node.  */

/*
 * Used internally be set access functions, so that callers can use
 * key values 0 and 1, without knowing these have special meanings.
 */
#define CALLER_TO_INTERNAL_KEY(_k) ((_k) + 2)
#define INTERNAL_TO_CALLER_KEY(_k) ((_k) - 2)

class TransSkipMap : public TransactionalContainer
{
private:

    int gc_id[NUM_LEVELS];

    /** The allocator for the Node Descriptor */
    Allocator<NodeDesc>* m_nodeDescAllocator;

    /********************************************************
     * TxnList methods are private so we can wrap them in Map
     * implementation methods
     */

    /**
     * Determines if the operation in the node descriptors are the same
     *
     * @param nodeDesc1 The first node descriptor object to compare
     * @param nodeDesc2 The second node descriptor object to compare
     * @return          True if the two node descriptors are the same
     */
    bool IsSameOperation(NodeDesc* nodeDesc1, NodeDesc* nodeDesc2);

    /**
     * Finishes the pending transaction
     *
     * @param nodeDesc  The node descriptor
     * @param desc      The descriptor
     */
    bool FinishPendingTxn(NodeDesc* nodeDesc, Desc* desc);

    /**
     * Determines if the node exists
     *
     * @param node  The node to check for
     * @param key   The key of the node
     * @return      True if the node exists
     */
    bool DoesNodeExist(Node* node, setkey_t key);

    /**
     * Determines if the node is active
     *
     * @param nodeDesc  The node descriptor
     * @return          True if the node is active
     */
    bool IsNodeActive(NodeDesc* nodeDesc);

    /**
     * Determines if the node exists
     *
     * @param nodeDesc  The node descriptor to check for
     * @return          True if the key exists
     */
    bool DoesKeyExist(NodeDesc* nodeDesc);

    /**
     * Locates the predecessor of the node with \a key
     *
     * @param pred  The predecessor node to the key
     * @param curr  The node to the key
     */
    void LocatePred(Node*& pred, Node*& curr, setkey_t key);

    /**
     * Prints the status of each node. Used for debugging.
     */
    void Print();

    /** The node pointing to the tail of the list */
    Node* m_tail;

    /** The node pointing to the head of the list */
    Node m_head;

    Node* weak_search_predecessors(setkey_t k, Node* *pa, Node* *na);

    /*
     * Mark @x deleted at every level in its list from @level down to level 1.
     * When all forward pointers are marked, node is effectively deleted.
     * Future searches will properly remove node by swinging predecessors'
     * forward pointers.
     */
    void mark_deleted(Node* x, int level);

    /*
     * Search for first non-deleted node, N, with key >= @k at each level in @l.
     * RETURN VALUES:
     *  Array @pa: @pa[i] is non-deleted predecessor of N at level i
     *  Array @na: @na[i] is N itself, which should be pointed at by @pa[i]
     *  MAIN RETURN VALUE: same as @na[0].
     */
    Node* strong_search_predecessors(setkey_t k, Node* *pa, Node* *na);

    bool help_ops(Desc* desc, uint8_t opid);

    void free_node(ptst_t *ptst, Node* n);

    bool IsKeyExist(NodeDesc* nodeDesc);

    /*
     * Allocate a new node, and initialise its @level field.
     * NB. Initialisation will eventually be pushed into garbage collector,
     * because of dependent read reordering.
     */
    Node *alloc_node(ptst_t *ptst);

    /*
     * Random level generator. Drop-off rate is 0.5 per level.
     * Returns value 1 <= level <= NUM_LEVELS.
     */
    int get_level(ptst_t *ptst);

    int check_for_full_delete(Node* x);

    void do_full_delete(ptst_t *ptst, Node* x, int level);

    void init_transskip_subsystem(void);

    void transskip_free();

public:
    /**
     * The reference to the transaction object
     */
    TransactionHandler* transaction;

    /**
     * Allocates a new pointer to a TransSkipMap
     * @param transaction   the transaction handler
     * @return              a pointer to a new instance of TransSkipMap
     */
    static TransSkipMap* Allocate(TransactionHandler* transaction);

    /**
     * Deconstructor
     */
    ~TransSkipMap();

    /**
     * Inserts a node into the list
     *
     * @param key       The key of the operation this insert is apart of
     * @param desc      The Descriptor object describing this operation
     * @param opid      The id (index) of the operation in the descriptor
     * @param inserted  The node that was inserted
     * @param pred      The predecessor node
     * @return          The ReturnCode of the final status of the operation
     */
    ReturnCode Insert(setkey_t key, Desc* desc, uint8_t opid,
            Node*& inserted, Node*& pred);

    /**
     * Deletes a node from the list
     *
     * @param key       The key of the operation this insert is apart of
     * @param desc      The Descriptor object describing this operation
     * @param opid      The id (index) of the operation in the descriptor
     * @param deleted   The node that was deleted
     * @param pred      The predecessor node
     * @return          The ReturnCode of the final status of the operation
     */
    ReturnCode Delete(setkey_t key, Desc* desc, uint8_t opid,
            Node*& deleted, Node*& pred);

    /**
     * Finds a node in the list
     *
     * @param key       The key of the operation this insert is apart of
     * @param desc      The Descriptor object describing this operation
     * @param opid      The id (index) of the operation in the descriptor
     * @return          The ReturnCode of the final status of the operation
     */
    ReturnCode Find(setkey_t key, Desc* desc, uint8_t opid, int &value);

    void destroy_transskip_subsystem(void);

    void Init();

    /**
     * Prints metrics of all the transactions performed on the list
     */
    void printMetrics();

};

#endif /* end of include guard: TRANSSKIPMAP_H */
