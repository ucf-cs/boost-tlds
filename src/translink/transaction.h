/**
 * Transaction class
 *
 * Handles the allocation of Descriptor and Node objects and facilitates
 * the execution of transactions.
 */

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <thread>
#include "common/assert.h"
#include "common/allocator.h"
#include "common/OnFinishAllocator.h"
#include "translink/dataObjects.h"
#include "translink/list/translist.h"

#ifndef TRANSACTION_H
#define TRANSACTION_H

/**
 * methodology for logically
 * marking and unmarking a node
*/
#define SET_MARK(_p)    ((Node *)(((uintptr_t)(_p)) | 1))
#define CLR_MARK(_p)    ((Node *)(((uintptr_t)(_p)) & ~1))
#define CLR_MARKD(_p)    ((NodeDesc *)(((uintptr_t)(_p)) & ~1))
#define IS_MARKED(_p)     (((uintptr_t)(_p)) & 1)

/**
 * Handles the allocation of Descriptor and Node objects and facilitates
 * the execution of transactions.
 */
/*template<>*/
class TransactionHandler
{
private:
    /**
     * A stack used to help pending operations
     */
    struct HelpStack
    {
        /**
         * Initialization
         */
        void Init()
        {
            index = 0;
        }

        /**
         * Pushes the descriptor onto the stack
         *
         * @param desc  The descriptor
         */
        void Push(Desc* desc)
        {
            ASSERT(index >= MAX_OPS, "index out of range");

            helps[index] = desc;
            opIds[index] = 0;
            index++;
        }

        /**
         * "Pops" the top of the stack. (Decrements the index)
         */
        void Pop()
        {
            ASSERT(index <= 0, "nothing to pop");

            index--;
        }

        /**
         * Checks if the stack contains the descriptor
         *
         * @param desc  The descriptor
         * @return      True if the stack contains the descriptor
         */
        bool Contain(Desc* desc)
        {
            for(uint8_t i = 0; i < index; i++)
            {
                if(helps[i] == desc)
                {
                    return true;
                }
            }

            return false;
        }

        void NewOp()
        {
            // TODO: check if index is 0
            opIds[index-1]++;
        }

        uint8_t GetOpId()
        {
            // TODO: check if index is 0
            return opIds[index-1];
        }

        /** The help stack */
        Desc* helps[256];

        /** The operation IDs */
        uint8_t opIds[256];

        /** The current index of the help stack */
        uint8_t index;
    };

    /** Helpstack used in to help operations finish */
    static __thread HelpStack helpStack;

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

    /**
     * Logically marks the nodes in the list for deletion
     *
     * @param nodes The list of nodes marked for deletion
     * @param preds The list of predecessor nodes to the nodes marked for deletion
     * @param desc  The descriptor object
     */
    inline void MarkForDeletion(const std::vector<Node*>* nodes, const std::vector<Node*>* preds, Desc* desc);

    /**
     * Logically marks the nodes in a list for deletion
     * 
     * @param onFinish  the onFinish object
     */
    inline void MarkForDeletionOnFinish(const OnFinish *onFinish);

    /** The allocator for OnFinish objects */
    static OnFinishAllocator<OnFinish>* m_onFinishAllocator;

public:

    /**
     * The list of containers that will be operated on during transactions.
     * Used to call the Init() method.
     */
    std::vector<TransactionalContainer*> containers;

    // TODO create accessors for these members

    /** The total number of committed transactions */
    uint32_t g_count_commit = 0;

    /** The total number of aborts */
    uint32_t g_count_abort = 0;

    /** The number of fake aborts */
    uint32_t g_count_fake_abort = 0;

    /** The allocator for the Descriptor */
    Allocator<Desc> m_descAllocator;

    /** The number of tests to run */
    uint64_t cap;

    /** The number of threads to use */
    uint64_t threadCount;

    /** The number of transactions per test */
    uint32_t transSize;

    /** The number of times the test will run */
    uint32_t testSize;

    /** The number of containers per test */
    uint32_t containerSize;

    /**
     * Constructor
     *
     * @param cap           the number of tests to run
     * @param threadCount   the number of threads to use
     * @param transSize     the number of transactions per test
     * @param containerSize the number of containers per test
     */
    TransactionHandler(uint64_t cap, uint64_t threadCount, uint32_t transSize, uint32_t testSize, uint32_t containerSize);

    /**
     * Executes the operations in \a ops on the list
     *
     * @param ops   the operations to perform on the list
     */
    bool ExecuteOps(const SetOpArray& ops);

    /**
     * Executes the operation defined by \a txnFunction
     *
     * @param txnFunction   the operation to perform on the list
     * @param inputMap      the map containing input values
     * @param outputMap     a pointer to a pointer of a map containing input values. The reference to the pointer can be null.
     * @return              True if none of the transaction was not aborted
     */
    bool ExecuteOps(const TransactionalFunction txnFunction, std::map<std::string, int>* inputMap, std::map<std::string, int>** outputMap);

    // TODO: Remove this method, since we'll be using transactional functions
    // instead of a list of operations
    /**
     * Helps finish the operation of any currently pending transaction in
     * the Descriptor
     *
     * @param desc  the Descriptor object
     * @param opid  the id (index) of the operation in the descriptor to help finish
     * @return      If the help operation was successful (no aborts)
     */
    bool HelpOps(Desc* desc, uint8_t opid);

    /**
     * Helps finish the operation of any currently pending transaction in
     * the Descriptor. This method is used for transactional functions.
     *
     * @param desc  the Descriptor object
     * @return      If the help operation was successful (no aborts)
     */
    bool HelpOps(Desc* desc);

    /**
     * 
     * Runs specific operation that a user calls inside the function pointer
     * that gets passed into ExecuteOps.
     * 
     * @param desc          the Descriptor object for the transaction
     * @param list          the container to perform operations on
     * @param opType        the type of operation
     * @param key           the Key value for the node
     * @param value         the value to insert (for INSERT operations)
     * @param foundValue    a reference variable to set to the found value (for FIND operations).
     *                      Is not set if the key is not found.
     * @return              the return code; whether the operation was OK, ABORTED, or SKIPPED
     */
    static ReturnCode CallOps(Desc* desc, TransactionalContainer* list, OpType opType, 
    setkey_t key, int value, int& foundValue);

    /**
     * Allocates memory for the Descriptor object
     *
     * @param size the number of operations in the Descriptor
     * @return the new descriptor
     */
    Desc* AllocateDesc(uint8_t size);

    /**
     * Initializes the transaction handler, specifically all of the memory allocators as well
     * as any specified by the lists that might be used in an operation
     */
    void Init();

    /**
     * Resets the metrics
     */
    void ResetMetrics();

    /**
     * Prints metrics
     */
    void PrintMetrics();
};

#endif /* end of include guard: TRANSACTION_H */
