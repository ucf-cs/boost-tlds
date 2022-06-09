/**
 * Trans List
 *
 * A linked list using transactions
 */
#ifndef TRANSLIST_H
#define TRANSLIST_H

#include <cstdint>
#include <vector>
#include "common/assert.h"
#include "common/NodeAllocator.h"
#include "common/NodeDescAllocator.h"
#include "translink/dataObjects.h"
#include "translink/transaction.h"
#include "translink/TxnList.h"

/**
 * A linked list using transactions
 */
class TransList : public TransactionalContainer
{
public:

    /**
     * Constructor
     *
     * @param transaction a reference to the Transaction
     */
    TransList(TransactionHandler* transaction);

    /**
     * Deconstructor
     */
    ~TransList();

    /**
     * Inserts a node into the list
     *
     * @param key       The key of the operation this insert is apart of
     * @param desc      The Descriptor object describing this operation
     * @param opid      The id (index) of the operation in the descriptor
     * @param inserted  The node being inserted
     * @param pred      The predecessor node
     * @return          The ReturnCode of the final status of the operation
     */
    virtual ReturnCode Insert(setkey_t key, Desc* desc, uint8_t opid, Node*& inserted, Node*& pred);

    /**
     * Deletes a node from the list
     *
     * @param key       The key of the operation this insert is apart of
     * @param desc      The Descriptor object describing this operation
     * @param opid      The id (index) of the operation in the descriptor
     * @param deleted   The node being deleted
     * @param pred      The predecessor node
     * @return          The ReturnCode of the final status of the operation
     */
    virtual ReturnCode Delete(setkey_t key, Desc* desc, uint8_t opid, Node*& deleted, Node*& pred);

    /**
     * Finds a node in the list
     *
     * @param key       The key of the operation this insert is apart of
     * @param desc      The Descriptor object describing this operation
     * @param opid      The id (index) of the operation in the descriptor
     * @return          The ReturnCode of the final status of the operation
     */
    virtual ReturnCode Find(setkey_t key, Desc* desc, uint8_t opid, int &value);

    virtual void Init();

    char const* printMe();

    // TODO: Make these private and wrap in an Init() function
    /** The allocator for the Node */
    NodeAllocator<Node> m_nodeAllocator;

    // /** The allocator for the Descriptor */
    // Allocator<Desc> m_descAllocator;

    /** The allocator for the Node Descriptor */
    NodeDescAllocator<NodeDesc> m_nodeDescAllocator;

    /**
     * Prints metrics of all the transactions performed on the list
     */
    void printMetrics();

private:

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
    void FinishPendingTxn(NodeDesc* nodeDesc, Desc* desc);

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
    Node* m_head;

    ASSERT_CODE
    (
        /** The total number of operations */
        uint32_t g_count = 0;

        /** The number of INSERT operations */
        uint32_t g_count_ins = 0;

        /** The number of new INSERT operations */
        uint32_t g_count_ins_new = 0;

        /** The number of DELETE operations */
        uint32_t g_count_del = 0;

        /** The number of new DELETE operations */
        uint32_t g_count_del_new = 0;

        /** The number of FIND operations */
        uint32_t g_count_fnd = 0;
    )

    /** The total number of committed transactions */
    uint32_t g_count_commit = 0;

    /** The total number of aborts */
uint32_t g_count_abort = 0;

    /** The number of fake aborts */
    uint32_t g_count_fake_abort = 0;
};

#endif /* end of include guard: TRANSLIST_H */