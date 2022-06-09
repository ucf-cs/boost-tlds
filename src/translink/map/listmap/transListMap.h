/**
 * Trans List Map
 *
 * A map implemented with a list using transactions
 */
#ifndef TRANSLISTMAP_H
#define TRANSLISTMAP_H

#include "translink/transaction.h"
#include <vector>

class TransListMap : public TransactionalContainer
{
private:
    /**
     * The size of the array table used in the hash map
     */
    int SIZE_OF_TABLE = 128;

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

    void printMetrics();

    /** The node pointing to the tail of the list */
    Node* m_tail;

    /** The node pointing to the head of the list */
    Node* m_head;

public:

    // TODO: Make private and wrap in Init() method
    /** The allocator for the Node */
    Allocator<Node> m_nodeAllocator;

    /** The allocator for the Node Descriptor */
    Allocator<NodeDesc> m_nodeDescAllocator;

    /**
     * Constructor
     *
     * @param transaction a reference to the Transaction
     */
    TransListMap(TransactionHandler* transaction);

    /**
     * Deconstructor
     */
    ~TransListMap();

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
    virtual ReturnCode Insert(setkey_t key, Desc* desc, uint8_t opid,
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
    virtual ReturnCode Delete(setkey_t key, Desc* desc, uint8_t opid,
            Node*& deleted, Node*& pred);

    /**
     * Finds a node in the list
     *
     * @param key       The key of the operation this insert is apart of
     * @param desc      The Descriptor object describing this operation
     * @param opid      The id (index) of the operation in the descriptor
     * @return          The ReturnCode of the final status of the operation
     */
    virtual ReturnCode Find(setkey_t key, Desc* desc, uint8_t opid, int& value);

    virtual void Init();

};

#endif /* end of include guard: TRANSLISTMAP_H */
