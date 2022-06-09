/**
 * Transaction List abstract class
 *
 * This class provides an abstact interface for abstract data types wanting to
 * support transactions
 */
#ifndef TXNLIST_H
#define TXNLIST_H
#include "translink/dataObjects.h"

class TransactionalContainer
{
public:
    /**
     * The reference to the transaction object
     */
    TransactionHandler* transaction;

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
            Node*& inserted, Node*& pred) = 0;

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
            Node*& deleted, Node*& pred) = 0;

    /**
     * Finds a node in the list
     *
     * @param key       The key of the operation this insert is apart of
     * @param desc      The Descriptor object describing this operation
     * @param opid      The id (index) of the operation in the descriptor
     * @param value     A reference to the value to set if we find the key
     * @return          The ReturnCode of the final status of the operation
     */
    virtual ReturnCode Find(setkey_t key, Desc* desc, uint8_t opid, int &value) = 0;

    /**
     * Initializes any thread specific subsystems such as memory allocators
     * or garbage collectors
     */
    virtual void Init() = 0;

    virtual ~TransactionalContainer() {};

};

#endif /* end of include guard: TXNLIST_H */
