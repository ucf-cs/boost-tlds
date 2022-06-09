/**
 * Set Adaptor
 *
 * A harness for running tests with Transactions.
 */

#ifndef SETADAPTOR_H
#define SETADAPTOR_H

#include "translink/list/translist.h"
#include "translink/skiplist/transskip.h"
#include "rstm/list/rstmlist.hpp"
#include "translink/map/skipmap/transSkipMap.h"
#include "translink/map/listmap/transListMap.h"
#include "boosting/list/boostinglist.h"
#include "boosting/skiplist/boostingskip.h"
#include "common/allocator.h"
#include "translink/dataObjects.h"
#include "ostm/skiplist/stmskip.h"

/**
 * Template class for the Set Adaptor
 */
template<typename T>
class SetAdaptor
{
};

/**
 * Set Adaptor for TransList
 */
template<>
class SetAdaptor<TransactionalContainer>
{
public:
    /** The transaction to execute operations with*/
    TransactionHandler* transaction;

    /**
     * Constructor
     *
     * @param transaction a reference to the Transaction
     */
    SetAdaptor(TransactionHandler* transaction)
        : transaction(transaction)
    { }

    /**
     * Initialization
     */
    void Init()
    {
        transaction->Init();

        transaction->ResetMetrics();
    }

    /**
     * Uninitialization
     */
    void Uninit(){}

    /**
     * Executes the operations in \a ops on the list
     *
     * @param ops   the operations to perform on the list
     */
    bool ExecuteOps(const SetOpArray& ops)
    {

        
        return transaction->ExecuteOps(ops);
    }

    /**
     * Executes the operation defined by \a txnFunction
     *
     * @param txnFunction   the operation to perform on the list
     */
    bool ExecuteOps(const TransactionalFunction txnFunction, std::map<std::string, int>* inputMap, std::map<std::string, int>** outputMap)
    {
        return transaction->ExecuteOps(txnFunction, inputMap, outputMap);
    }
};

/**
 * Set Adaptor for TransList
 */
template<>
class SetAdaptor<TransList>
{
public:
    /** The transaction to execute operations with*/
    TransactionHandler* transaction;

    /** The list to execute operations on */
    TransList m_list;

    /**
     * Constructor
     *
     * @param transaction a reference to the Transaction
     */
    SetAdaptor(TransactionHandler* transaction)
        : m_list(transaction)
        , transaction(transaction)
    { }

    /**
     * Initailization
     */
    void Init()
    {
        m_list.transaction->Init();
        m_list.transaction->ResetMetrics();
    }

    /**
     * Uninitialization
     */
    void Uninit(){}

    /**
     * Executes the operations in \a ops on the list
     *
     * @param ops   the operations to perform on the list
     */
    bool ExecuteOps(const SetOpArray& ops)
    {
        return m_list.transaction->ExecuteOps(ops);
    }

    /**
     * Executes the operation defined by \a txnFunction
     *
     * @param txnFunction   the operation to perform on the list
     */
    bool ExecuteOps(const TransactionalFunction txnFunction, std::map<std::string, int>* inputMap, std::map<std::string, int>** outputMap)
    {
        return m_list.transaction->ExecuteOps(txnFunction, inputMap, outputMap);
    }
};

/**
 * Set Adaptor for TransSkip
 */
template<>
class SetAdaptor<TransSkip>
{
public:
    /** The transaction to execute operations with*/
    TransactionHandler* transaction;

    /** The list to execute operations on */
    TransSkip m_list;

    /**
     * Constructor
     *
     * @param cap           the number of tests to run
     * @param threadCount   the number of threads to use
     * @param transSize     the number of transactions per test
     */
    SetAdaptor(TransactionHandler* transaction)
        : transaction(transaction)
    {
        m_list = *(TransSkip::Allocate(transaction));
    }

    /**
     * Initialization
     */
    void Init()
    {
        m_list.transaction->Init();
        m_list.transaction->ResetMetrics();
    }

    /**
     * Uninitialization
     */
    void Uninit()
    {
        m_list.destroy_transskip_subsystem(); 
    }

    /**
     * Executes the operations in \a ops on the list
     *
     * @param ops   the operations to perform on the list
     */
    bool ExecuteOps(const SetOpArray& ops)
    {
        return m_list.transaction->ExecuteOps(ops);
    }

    /**
     * Executes the operation defined by \a txnFunction
     *
     * @param txnFunction   the operation to perform on the list
     */
    bool ExecuteOps(const TransactionalFunction txnFunction, std::map<std::string, int>* inputMap, std::map<std::string, int>** outputMap)
    {
        return m_list.transaction->ExecuteOps(txnFunction, inputMap, outputMap);
    }
};

/**
 * Set Adaptor for RSTMList
 */
template<>
class SetAdaptor<RSTMList>
{
public:
    /** The list to execute operations on */
    RSTMList m_list;

    /**
     * Constructor
     */
    SetAdaptor()
    {
        TM_SYS_INIT();
    }

    /**
     * Deconstructor
     */
    ~SetAdaptor()
    {
        TM_SYS_SHUTDOWN();

        printf("Total commit %u, abort (total/fake) %u/%u\n", g_count_commit, g_count_abort, g_count_stm_abort - g_count_abort);
    }

    /**
     * Initialization
     */
    void Init()
    {
        TM_THREAD_INIT();
        g_count_commit = 0;
        g_count_abort = 0;
        g_count_stm_abort = 0;
    }

    /**
     * Uninitialization
     */
    void Uninit()
    {
        TM_THREAD_SHUTDOWN();
        TM_GET_THREAD();

        __sync_fetch_and_add(&g_count_stm_abort, tx->num_aborts);
    }

    /**
     * Executes the operations in \a ops on the list
     *
     * @param ops   the operations to perform on the list
     */
    bool ExecuteOps(const SetOpArray& ops) __attribute__ ((optimize (0)))
    {
        bool ret = true;

        TM_BEGIN(atomic)
        {
            if(ret == true)
            {
                for(uint32_t i = 0; i < ops.size(); ++i)
                {
                    uint32_t val = ops[i].key;
                    if(ops[i].type == FIND)
                    {
                        ret = m_list.lookup(val TM_PARAM);
                    }
                    else if(ops[i].type == INSERT)
                    {
                        ret = m_list.insert(val TM_PARAM);
                    }
                    else
                    {
                        ret = m_list.remove(val TM_PARAM);
                    }

                    if(ret == false)
                    {
                        //stm::restart();
                        tx->tmabort(tx);
                        break;
                    }
                }
            }
        } 
        TM_END;

        if(ret)
        {
            __sync_fetch_and_add(&g_count_commit, 1);
        }
        else
        {
            __sync_fetch_and_add(&g_count_abort, 1);
        }

        return ret;
    }

    /**
     * Executes the operation defined by \a txnFunction
     *
     * @param txnFunction   the operation to perform on the list
     */
    bool ExecuteOps(const TransactionalFunction txnFunction, std::map<std::string, int>* inputMap, std::map<std::string, int>** outputMap)
    {
        return false;
    }

private:

    /** the number of commits */
    uint32_t g_count_commit = 0;

    /** the number of commited aborts */
    uint32_t g_count_abort = 0;

    /** the total number of commits */
    uint32_t g_count_stm_abort = 0;
};

/**
 * Set Adaptor for stm_skip
 */
template<>
class SetAdaptor<stm_skip>
{
public:
    /** The list to execute operations on */
    stm_skip* m_list;

    /**
     * Constructor
     */
    SetAdaptor()
    {
        init_stmskip_subsystem();
        m_list = stmskip_alloc();
    }

    /**
     * Deconstructor
     */
    ~SetAdaptor()
    {
        destory_stmskip_subsystem();
    }

    /**
     * Initialization
     */
    void Init()
    {
        stmskip_reset_metrics();
    }

    /**
     * Uninitialization
     */
    void Uninit()
    { }

    /**
     * Executes the operations in \a ops on the list
     *
     * @param ops   the operations to perform on the list
     */
    bool ExecuteOps(const SetOpArray& ops)
    {
        bool ret = stmskip_execute_ops(m_list, (set_op*)ops.data(), ops.size());

        return ret;
    }

    /**
     * Executes the operation defined by \a txnFunction
     *
     * @param txnFunction   the operation to perform on the list
     */
    bool ExecuteOps(const TransactionalFunction txnFunction, std::map<std::string, int>* inputMap, std::map<std::string, int>** outputMap)
    {
        return false;
    }
};

/**
 * Set Adaptor for Boosting List
 */
template<>
class SetAdaptor<BoostingList>
{
public:
   /** The list to execute operations on */
    BoostingList m_list;

    /**
     * Constructor
     */
    SetAdaptor()
    {
    }

    /**
     * Deconstructor
     */
    ~SetAdaptor()
    {
    }

    /**
     * Initialization
     */
    void Init()
    {
        m_list.Init();
        m_list.ResetMetrics();
    }

    /**
     * Uninitialization
     */
    void Uninit()
    {
        m_list.Uninit();
    }

    /**
     * Executes the operations in \a ops on the list
     *
     * @param ops   the operations to perform on the list
     */
    bool ExecuteOps(const SetOpArray& ops)
    {
        BoostingList::ReturnCode ret = BoostingList::OP_FAIL;

        for(uint32_t i = 0; i < ops.size(); ++i)
        {
            uint32_t key = ops[i].key;

            if(ops[i].type == FIND)
            {
                ret = m_list.Find(key);
            }
            else if(ops[i].type == INSERT)
            {
                ret = m_list.Insert(key);
            }
            else
            {
                ret = m_list.Delete(key);
            }

            if(ret != BoostingList::OK)
            {
                m_list.OnAbort(ret);
                break;
            }
        }

        if(ret == BoostingList::OK)
        {
            m_list.OnCommit();
        }

        return ret;
    }

    /**
     * Executes the operation defined by \a txnFunction
     *
     * @param txnFunction   the operation to perform on the list
     */
    bool ExecuteOps(const TransactionalFunction txnFunction, std::map<std::string, int>* inputMap, std::map<std::string, int>** outputMap)
    {
        return false;
    }
};

/**
 * Set Adaptor for Boosting Skip
 */
template<>
class SetAdaptor<BoostingSkip>
{
public:
   /** The list to execute operations on */
    BoostingSkip m_list;

    /**
     * Constructor
     */
    SetAdaptor()
    {
    }

    /**
     * Deconstructor
     */
    ~SetAdaptor() 
    { 
    }

    /**
     * Initialization
     */
    void Init()
    {
        m_list.Init();
        m_list.ResetMetrics();
    }

    /**
     * Uninitialization
     */
    void Uninit()
    {
        m_list.Uninit();
    }

    /**
     * Executes the operations in \a ops on the list
     *
     * @param ops   the operations to perform on the list
     */
    bool ExecuteOps(const SetOpArray& ops)
    {
        BoostingSkip::ReturnCode ret = BoostingSkip::OP_FAIL;

        for(uint32_t i = 0; i < ops.size(); ++i)
        {
            uint32_t key = ops[i].key;

            if(ops[i].type == FIND)
            {
                ret = m_list.Find(key);
            }
            else if(ops[i].type == INSERT)
            {
                ret = m_list.Insert(key);
            }
            else
            {
                ret = m_list.Delete(key);
            }

            if(ret != BoostingSkip::OK)
            {
                m_list.OnAbort(ret);
                break;
            }
        }

        if(ret == BoostingSkip::OK)
        {
            m_list.OnCommit();
        }

        return ret;
    }

    /**
     * Executes the operation defined by \a txnFunction
     *
     * @param txnFunction   the operation to perform on the list
     */
    bool ExecuteOps(const TransactionalFunction txnFunction, std::map<std::string, int>* inputMap, std::map<std::string, int>** outputMap)
    {
        return false;
    }
};

/**
 * Set Adaptor for TransSkipMap
 */
template<>
class SetAdaptor<TransSkipMap>
{
public:
    /** The transaction to execute operations with*/
    TransactionHandler* transaction;

    /** The list to execute operations on */
    TransSkipMap m_list;

    /**
     * Constructor
     *
     * @param cap           the number of tests to run
     * @param threadCount   the number of threads to use
     * @param transSize     the number of transactions per test
     */
    SetAdaptor(TransactionHandler* transaction)
        : transaction(transaction)
    {
        m_list = *(TransSkipMap::Allocate(transaction));
    }

    /**
     * Initialization
     */
    void Init()
    {
        m_list.transaction->Init();
        m_list.transaction->ResetMetrics();
    }

    /**
     * Uninitialization
     */
    void Uninit()
    {
        m_list.destroy_transskip_subsystem();
    }

    /**
     * Executes the operations in \a ops on the list
     *
     * @param ops   the operations to perform on the list
     */
    bool ExecuteOps(const SetOpArray& ops)
    {
        return m_list.transaction->ExecuteOps(ops);
    }

    /**
     * Executes the operation defined by \a txnFunction
     *
     * @param txnFunction   the operation to perform on the list
     */
    bool ExecuteOps(const TransactionalFunction txnFunction, std::map<std::string, int>* inputMap, std::map<std::string, int>** outputMap)
    {
        return m_list.transaction->ExecuteOps(txnFunction, inputMap, outputMap);
    }
};

/**
 * Set Adaptor for TransListMap
 */
template<>
class SetAdaptor<TransListMap>
{
public:
    /** The transaction to execute operations with*/
    TransactionHandler* transaction;

    /** The list to execute operations on */
    TransListMap m_list;

    /**
     * Constructor
     *
     * @param cap           the number of tests to run
     * @param threadCount   the number of threads to use
     * @param transSize     the number of transactions per test
     */
    SetAdaptor(TransactionHandler* transaction)
        : m_list(transaction)
        , transaction(transaction)
    { 
    }

    /**
     * Deconstructor
     */
    ~SetAdaptor()
    {
    }

    /**
     * Initialization
     */
    void Init()
    {
        m_list.transaction->Init();
        m_list.transaction->ResetMetrics();
    }

    /**
     * Uninitialization
     */
    void Uninit() { }

    /**
     * Executes the operations in \a ops on the list
     *
     * @param ops   the operations to perform on the list
     */
    bool ExecuteOps(const SetOpArray& ops)
    {
        return m_list.transaction->ExecuteOps(ops);
    }

    /**
     * Executes the operation defined by \a txnFunction
     *
     * @param txnFunction   the operation to perform on the list
     */
    bool ExecuteOps(const TransactionalFunction txnFunction, std::map<std::string, int>* inputMap, std::map<std::string, int>** outputMap)
    {
        return m_list.transaction->ExecuteOps(txnFunction, inputMap, outputMap);
    }
};



#endif /* end of include guard: SETADAPTOR_H */
