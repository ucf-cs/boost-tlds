/**
 * transskip.h
 * Header file for TransSkip
 */
#ifndef __TRANSSKIP_H__
#define __TRANSSKIP_H__


#include <cstdint>
#include "common/assert.h"
#include "common/allocator.h"
#include "translink/dataObjects.h"
#include "translink/transaction.h"
#include "translink/TxnList.h"
#include "common/fraser/ptst.h"

/* REMOVE
typedef unsigned long setkey_t;
typedef void         *setval_t;
*/


#ifdef __SET_IMPLEMENTATION__

/*************************************
 * INTERNAL DEFINITIONS
 */

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


/*
 * SUPPORT FOR WEAK ORDERING OF MEMORY ACCESSES
 */

#ifdef WEAK_MEM_ORDER

/* Read field @_f into variable @_x. */
#define READ_FIELD(_x,_f)                                       \
do {                                                            \
    (_x) = (_f);                                                \
    if ( (_x) == INVALID_FIELD ) { RMB(); (_x) = (_f); }        \
    assert((_x) != INVALID_FIELD);                              \
} while ( 0 )

#else

/* Read field @_f into variable @_x. */
#define READ_FIELD(_x,_f) ((_x) = (_f))

#endif

 #endif /* __SET_IMPLEMENTATION__ */
//#define NUM_LEVELS 20


/*************************************
 * PUBLIC DEFINITIONS
 */

/*************************************
 * Transaction Definitions
 */

class TransSkip : public TransactionalContainer
{
private:

    /** The allocator for the Node Descriptor */
    Allocator<NodeDesc>* m_nodeDescAllocator;

    void printMetrics();

    int gc_id[NUM_LEVELS];

    bool IsSameOperation(NodeDesc* nodeDesc1, NodeDesc* nodeDesc2);

    bool FinishPendingTxn(NodeDesc* nodeDesc, Desc* desc);

    bool DoesNodeExist(Node* node, setkey_t key);

    bool IsNodeActive(NodeDesc* nodeDesc);

    bool DoesKeyExist(NodeDesc* nodeDesc);

    void LocatePred(Node*& pred, Node*& curr, setkey_t key);

    void Print();

    Node* m_tail;
    Node m_head;

    Node* weak_search_predecessors(setkey_t k, Node* *pa, Node* *na);

    void mark_deleted(Node* x, int level);

    Node* strong_search_predecessors(setkey_t k, Node* *pa, Node* *na);

    bool help_ops(Desc* desc, uint8_t opid);

    void free_node(ptst_t *ptst, Node* n);

    bool IsKeyExist(NodeDesc* nodeDesc);

    Node* alloc_node(ptst_t *ptst);

    int get_level(ptst_t *ptst);

    int check_for_full_delete(Node* x);

    void do_full_delete(ptst_t *ptst, Node* x, int level);

    void init_transskip_subsystem(void);

    void transskip_free();

public:

    /**
     * Allocates a new pointer to a TransSkip
     * @param transaction   the transaction handler
     * @return              a pointer to a new instance of TransSkip
     */
    static TransSkip* Allocate(TransactionHandler* transaction);

    ~TransSkip();

    ReturnCode Insert(uint64_t key, Desc* desc, uint8_t opid, Node*& inserted, Node*& pred);
    ReturnCode Delete(uint64_t key, Desc* desc, uint8_t opid, Node*& deleted, Node*& pred);
    ReturnCode Find(uint64_t key, Desc* desc, uint8_t opid, int &value);
    void Init();
    
    void destroy_transskip_subsystem();

};


#endif /* __TRANSSKIP_H__ */

