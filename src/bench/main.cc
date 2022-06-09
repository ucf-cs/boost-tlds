//------------------------------------------------------------------------------
//
//     Testing different priority queues
//
//------------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <array>
#include <set>
#include <string>
#include <thread>
#include <mutex>
#include <boost/random.hpp>
#include <sched.h>
#include "common/timehelper.h"
#include "common/threadbarrier.h"
#include "bench/setadaptor.h"
#include "TxnFuncTester.h"

static int NUM_LISTS = 2;

/**
 * The main method that does work on the thread; Randomly performs \a transSize operations.
 *
 * @param threadId  the id of this thread.
 * @param testSize  the number of tests this worker will perform.
 * @param transSize the number of transactions per test.
 * @param keyRange  the range of values to use when assigning keys to a transaction.
 * @param insertion the threshold used to determine if an operation should be an INSERT. Should be less than \a keyRange.
 * @param deletion  the threshold used to determine if an operation should be a DELETE. Should be less than \a keyRange.
 * @param barrier   the reference to the ThreadBarrier
 * @param set       the reference to the SetAdapter
 */
template<typename T>
void WorkThread(int threadId, uint32_t testSize, uint32_t tranSize, uint32_t keyRange, uint32_t insertion, uint32_t deletion, ThreadBarrier& barrier,  T& set)
{
    //set affinity for each thread
    cpu_set_t cpu = {{0}};
    CPU_SET(threadId, &cpu);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpu);

    double startTime = Time::GetWallTime();

    boost::mt19937 randomGenKey;
    boost::mt19937 randomGenOp;
    boost::mt19937 randomGenVal;
    randomGenKey.seed(startTime + threadId);
    randomGenOp.seed(startTime + threadId + 1000);
    randomGenVal.seed(startTime + threadId + 99999);
    boost::uniform_int<uint64_t> randomDistKey(1, keyRange);
    boost::uniform_int<uint64_t> randomDistOp(1, 100);
    boost::uniform_int<uint64_t> randomDistVal(1, 10);

    set.Init();

    barrier.Wait();

    SetOpArray ops(tranSize);

    for(unsigned int i = 0; i < testSize; ++i)
    {
        for(uint32_t t = 0; t < tranSize; ++t)
        {
            uint32_t op_dist = randomDistOp(randomGenOp);
            ops[t].type     = op_dist <= insertion ? INSERT : op_dist <= insertion + deletion ? DELETE : FIND;
            ops[t].key      = randomDistKey(randomGenKey);
            ops[t].value    = randomDistVal(randomGenVal);
            //ops[t].container     = &(set.m_list);
        }

        set.ExecuteOps(ops);
    }

    set.Uninit();
}

/**
 * Populates the list with values before calling the worker thread.
 *
 * @param numThread the number of threads to create
 * @param testSize  the number of tests this worker will perform.
 * @param transSize the number of transactions per test.
 * @param keyRange  the range of values to use when assigning keys to a transaction.
 * @param insertion the threshold used to determine if an operation should be an INSERT. Should be less than \a keyRange.
 * @param deletion  the threshold used to determine if an operation should be a DELETE. Should be less than \a keyRange.
 * @param set       the reference to the SetAdapter
 */
template<typename T>
void Tester(uint32_t numThread, uint32_t testSize, uint32_t tranSize, uint64_t keyRange, uint32_t insertion, uint32_t deletion,  SetAdaptor<T>& set)
{
    std::vector<std::thread> thread(numThread);
    ThreadBarrier barrier(numThread + 1);

    double startTime = Time::GetWallTime();
    boost::mt19937 randomGen;
    boost::mt19937 randomGenVal;
    randomGen.seed(startTime - 10);
    randomGenVal.seed(startTime + 10);
    boost::uniform_int<uint32_t> randomDist(1, keyRange);
    boost::uniform_int<uint32_t> randomDistVal(1, 10);

    set.Init();

    SetOpArray ops(1);

    for(unsigned int i = 0; i < keyRange; ++i)
    {
        ops[0].type     = INSERT;
        ops[0].key      = randomDist(randomGen);
        ops[0].value    = randomDistVal(randomGenVal);
        set.ExecuteOps(ops);
    }

    //Create joinable threads
    for (unsigned i = 0; i < numThread; i++) 
    {
        thread[i] = std::thread(WorkThread<SetAdaptor<T> >, i + 1, testSize, tranSize, keyRange, insertion, deletion, std::ref(barrier), std::ref(set));
    }

    barrier.Wait();

    {
        ScopedTimer timer(true);

        //Wait for the threads to finish
        for (unsigned i = 0; i < thread.size(); i++) 
        {
            thread[i].join();
        }
    }

    set.Uninit();
}

/**
 * The main method that does work on the thread; Randomly performs \a transSize operations.
 *
 * @param threadId  the id of this thread.
 * @param testSize  the number of tests this worker will perform.
 * @param transSize the number of transactions per test.
 * @param keyRange  the range of values to use when assigning keys to a transaction.
 * @param insertion the threshold used to determine if an operation should be an INSERT. Should be less than \a keyRange.
 * @param deletion  the threshold used to determine if an operation should be a DELETE. Should be less than \a keyRange.
 * @param barrier   the reference to the ThreadBarrier
 * @param set       the reference to the SetAdapter
 */
template<typename T>
void TxnWorkThread(int threadId, uint32_t testSize, uint32_t tranSize, uint32_t keyRange, uint32_t insertion, uint32_t deletion, ThreadBarrier& barrier,  T& set, std::vector<TransactionalContainer*> list)
{
    TxnFuncTester* theClass = new TxnFuncTester();
    TxnFuncTester::_txn = set.transaction;
    TxnFuncTester::_list = list;//&(set.m_list);

    //set affinity for each thread
    cpu_set_t cpu = {{0}};
    CPU_SET(threadId, &cpu);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpu);

    double startTime = Time::GetWallTime();

    boost::mt19937 randomGenKey;
    boost::mt19937 randomGenOp;
    boost::mt19937 randomGenVal;
    boost::mt19937 randomGenList;
    randomGenKey.seed(startTime + threadId);
    randomGenOp.seed(startTime + threadId + 1000);
    randomGenVal.seed(startTime + threadId + 99999);
    randomGenList.seed(startTime + threadId + 99999);
    boost::uniform_int<uint64_t> randomDistKey(1, keyRange);
    boost::uniform_int<uint64_t> randomDistOp(1, 100);
    boost::uniform_int<uint64_t> randomDistVal(1, 10);
    boost::uniform_int<uint64_t> randomDistList(0, set.transaction->containerSize-1);

    set.Init();

    barrier.Wait();

    SetOpArray ops(tranSize);

    for(unsigned int i = 0; i < testSize; ++i)
    {
        for(uint32_t t = 0; t < tranSize; ++t)
        {
            uint32_t op_dist = randomDistOp(randomGenOp);
            ops[t].type     = op_dist <= insertion ? INSERT : op_dist <= insertion + deletion ? DELETE : FIND;
            ops[t].key      = randomDistKey(randomGenKey);
            ops[t].value    = randomDistVal(randomGenVal);
            ops[t].container = list[randomDistList(randomGenList)];
            int temp = 0;
        }
        set.ExecuteOps(ops);
    }

    set.Uninit();
}

/**
 * The main method that does work on the thread; Executes one of TxnFuncTesters methods.
 *
 * @param threadId  the id of this thread.
 * @param testSize  the number of tests this worker will perform.
 * @param transSize the number of transactions per test.
 * @param keyRange  the range of values to use when assigning keys to a transaction.
 * @param insertion the threshold used to determine if an operation should be an INSERT. Should be less than \a keyRange.
 * @param deletion  the threshold used to determine if an operation should be a DELETE. Should be less than \a keyRange.
 * @param barrier   the reference to the ThreadBarrier
 * @param set       the reference to the SetAdapter
 */
template<typename T>
void DTTWorkThread(int threadId, uint32_t testSize, uint32_t tranSize, uint32_t keyRange, uint32_t insertion, uint32_t deletion,
    ThreadBarrier& barrier,  T& set, std::vector<TransactionalContainer*> list, int dttFunction)
{
    TxnFuncTester* theClass = new TxnFuncTester();
    TxnFuncTester::_txn = set.transaction;
    TxnFuncTester::_list = list;//&(set.m_list);

    //set affinity for each thread
    cpu_set_t cpu = {{0}};
    CPU_SET(threadId, &cpu);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpu);

    double startTime = Time::GetWallTime();

    boost::mt19937 randomGenKey;
    boost::mt19937 randomGenOp;
    boost::mt19937 randomGenVal;
    boost::mt19937 randomGenList;
    randomGenKey.seed(startTime + threadId);
    randomGenOp.seed(startTime + threadId + 1000);
    randomGenVal.seed(startTime + threadId + 99999);
    randomGenList.seed(startTime + threadId + 99999);
    boost::uniform_int<uint64_t> randomDistKey(1, keyRange);
    boost::uniform_int<uint64_t> randomDistOp(1, 100);
    boost::uniform_int<uint64_t> randomDistVal(1, 10);
    boost::uniform_int<uint64_t> randomDistList(0, set.transaction->containerSize-1);

    set.Init();

    barrier.Wait();

    // Pass in function pointer
    std::map<std::string, int>* outputMap = NULL;
    int index = dttFunction - 1;
    for (int i=0; i<testSize; i++) {
    set.ExecuteOps(theClass->function[index], &(theClass->inputMap[index]), &outputMap);
    }

    set.Uninit();
}

/**
 * Populates the list with values before calling the worker thread.
 *
 * @param numThread     the number of threads to create
 * @param testSize      the number of tests this worker will perform.
 * @param transSize     the number of transactions per test.
 * @param keyRange      the range of values to use when assigning keys to a transaction.
 * @param insertion     the threshold used to determine if an operation should be an INSERT. Should be less than \a keyRange.
 * @param deletion      the threshold used to determine if an operation should be a DELETE. Should be less than \a keyRange.
 * @param set           the reference to the SetAdapter
 * @param transaction   the reference to the TransactionHandler
 */
template<typename T>
void TxnTester(uint32_t numThread, uint32_t testSize, uint32_t tranSize, uint64_t keyRange, uint32_t insertion, uint32_t deletion,  SetAdaptor<T>& set, TransactionHandler* transaction, int dttFunction = 0)
{
    std::vector<std::thread> thread(numThread);
    ThreadBarrier barrier(numThread + 1);

    double startTime = Time::GetWallTime();
    boost::mt19937 randomGen;
    boost::mt19937 randomGenVal;
    randomGen.seed(startTime - 10);
    randomGenVal.seed(startTime + 10);
    boost::uniform_int<uint32_t> randomDist(1, keyRange);
    boost::uniform_int<uint32_t> randomDistVal(1, 10);

    /** The list to execute operations on */
    std::vector<TransactionalContainer*> list;

    list.push_back(&set.m_list);
    transaction->containers.push_back(&set.m_list);

    // Initialize the set adapter in order to initialize the allocators
    set.Init();

    SetOpArray ops(1);

    for (int j=0;j<transaction->containerSize;j++) {
        for(unsigned int i = 0; i < keyRange; ++i)
        {
            ops[0].type     = INSERT;
            ops[0].key      = randomDist(randomGen);
            ops[0].value    = randomDistVal(randomGenVal);
            ops[0].container = list[j];
            set.ExecuteOps(ops);
        }
    }

    transaction->ResetMetrics();

    //Create joinable threads
    for (unsigned i = 0; i < numThread; i++) 
    {
        if (dttFunction > 0) {
            thread[i] = std::thread(DTTWorkThread<SetAdaptor<T> >, i + 1, testSize, tranSize, keyRange, insertion, deletion, std::ref(barrier), std::ref(set), list, dttFunction);
        } else {
            thread[i] = std::thread(TxnWorkThread<SetAdaptor<T> >, i + 1, testSize, tranSize, keyRange, insertion, deletion, std::ref(barrier), std::ref(set), list);
        }
    }

    barrier.Wait();

    {
        ScopedTimer timer(true);

        //Wait for the threads to finish
        for (unsigned i = 0; i < thread.size(); i++) 
        {
            thread[i].join();
        }
    }

    set.Uninit();
}

/**
 * Populates the list with values before calling the worker thread.
 *
 * @param numThread     the number of threads to create
 * @param testSize      the number of tests this worker will perform.
 * @param transSize     the number of transactions per test.
 * @param keyRange      the range of values to use when assigning keys to a transaction.
 * @param insertion     the threshold used to determine if an operation should be an INSERT. Should be less than \a keyRange.
 * @param deletion      the threshold used to determine if an operation should be a DELETE. Should be less than \a keyRange.
 * @param set           the reference to the SetAdapter
 * @param transaction   the reference to the TransactionHandler
 */
template<typename T>
void MultiTxnTester(uint32_t numThread, uint32_t testSize, uint32_t tranSize, uint64_t keyRange, uint32_t insertion, uint32_t deletion,
    SetAdaptor<T>& set, TransactionHandler* transaction, int dttFunction = 0)
{
    std::vector<std::thread> thread(numThread);
    ThreadBarrier barrier(numThread + 1);

    double startTime = Time::GetWallTime();
    boost::mt19937 randomGen;
    boost::mt19937 randomGenVal;
    randomGen.seed(startTime - 10);
    randomGenVal.seed(startTime + 10);
    boost::uniform_int<uint32_t> randomDist(1, keyRange);
    boost::uniform_int<uint32_t> randomDistVal(1, 10);


    /** The list to execute operations on */
    std::vector<TransactionalContainer*> list;

    TransList transList = TransList(transaction); 
    TransSkip* skipList = TransSkip::Allocate(transaction);
    /* Currently, the garbage collection subsystem for transkip seems to
       depend on there being only one instance of it per thread, which is not
       the case when we have multiple skip maps using the same gc system. I
       think this is because it depends on some thread local static variable */
    // TransSkipMap* transSkipMap = TransSkipMap::Allocate(transaction);
    /* The same is true for Allocator */
    // TransListMap transListMap = TransListMap(transaction);

    list.push_back(&transList);
    transaction->containers.push_back(&transList);
    list.push_back(skipList);
    transaction->containers.push_back(skipList);
    // list.push_back(&transListMap);
    // transaction->containers.push_back(&transListMap);
    // list.push_back(transSkipMap);
    // transaction->containers.push_back(transSkipMap);

    // Initialize the set adapter in order to initialize the allocators
    set.Init();

    SetOpArray ops(1);

    for (int j=0;j<transaction->containerSize;j++) {
        for(unsigned int i = 0; i < keyRange; ++i)
        {
            ops[0].type     = INSERT;
            ops[0].key      = randomDist(randomGen);
            ops[0].value    = randomDistVal(randomGenVal);
            ops[0].container = list[j];
            set.ExecuteOps(ops);
        }
    }

    transaction->ResetMetrics();

    //Create joinable threads
    for (unsigned i = 0; i < numThread; i++) 
    {
        if (dttFunction > 0) {
            thread[i] = std::thread(DTTWorkThread<SetAdaptor<T> >, i + 1, testSize, tranSize, keyRange, insertion, deletion, std::ref(barrier), std::ref(set), list, dttFunction);
        } else {
            thread[i] = std::thread(TxnWorkThread<SetAdaptor<T> >, i + 1, testSize, tranSize, keyRange, insertion, deletion, std::ref(barrier), std::ref(set), list);
        }
    }

    barrier.Wait();

    {
        ScopedTimer timer(true);

        //Wait for the threads to finish
        for (unsigned i = 0; i < thread.size(); i++) 
        {
            thread[i].join();
        }
    }

    // we have to manually delete skiplists 
    delete(skipList);

    set.Uninit();
}

/**
 * The main thread
 */
int main(int argc, const char *argv[])
{
    uint32_t setType = 9;
    uint32_t numThread = 1;
    uint32_t testSize = 100;
    uint32_t tranSize = 1;
    uint64_t keyRange = 100;
    uint32_t insertion = 33;
    uint32_t deletion = 33;

    /**
     * If command line arguments are passed in, 
     * reassign the values defined above to
     * what is passed in
    */
    if(argc > 1) setType = atoi(argv[1]);
    if(argc > 2) numThread = atoi(argv[2]);
    if(argc > 3) testSize = atoi(argv[3]);
    if(argc > 4) tranSize = atoi(argv[4]);
    if(argc > 5) keyRange = atoi(argv[5]);
    if(argc > 6) insertion = atoi(argv[6]);
    if(argc > 7) deletion = atoi(argv[7]);

    assert(setType < 15);
    assert(keyRange < 0xffffffff);

    const char* setName[] = 
    {   "TransList",
        "RSTMList",
        "BoostingList",
        "TransSkip",
        "BoostingSkip",
        "OSTMSkip",
        "TransSkipMap",
        "TransListMap",
        "Multiple Containers",
        "TransList with DTT",
        "TransSkip with DTT",
        "TransListMap with DTT",
        "TransSkipMap with DTT",
        "Multiple Containers with DTT",
        "TransListMap Double Access with DTT"
    };

    printf("Start testing %s with %d threads %d iterations %d operations %lu unique keys %d%% insert %d%% delete.\n", setName[setType], numThread, testSize, tranSize, keyRange, insertion, (insertion + deletion) >= 100 ? 100 - insertion : deletion);

    /**
     * complete transaction with specified type
     * of data structure
    */
    switch(setType)
    {
    case 0:
        {
            TransactionHandler* transaction = new TransactionHandler(testSize, numThread+ 1, tranSize, testSize, 1);
            SetAdaptor<TransList> set(transaction);
            TxnTester(numThread, testSize, tranSize, keyRange, insertion, deletion, set, transaction);
            delete(transaction);
        }
        break;
    case 1:
        {
            SetAdaptor<RSTMList> set;
            Tester(numThread, testSize, tranSize, keyRange, insertion, deletion, set);
        }
        break;
    case 2:
        {
            SetAdaptor<BoostingList> set;
            Tester(numThread, testSize, tranSize, keyRange, insertion, deletion, set);
        }
        break;
    case 3:
        {
            TransactionHandler* transaction = new TransactionHandler(testSize, numThread+ 1, tranSize, testSize, 1);
            SetAdaptor<TransSkip> set(transaction);
            TxnTester(numThread, testSize, tranSize, keyRange, insertion, deletion, set, transaction);
            delete(transaction);
        }
        break;
    case 4:
        {
            SetAdaptor<BoostingSkip> set;
            Tester(numThread, testSize, tranSize, keyRange, insertion, deletion, set);
        }
        break;
    case 5:
        {
            SetAdaptor<stm_skip> set;
            Tester(numThread, testSize, tranSize, keyRange, insertion, deletion, set);
        }
        break;
    case 6:
        {
            TransactionHandler* transaction = new TransactionHandler(testSize, numThread+ 1, tranSize, testSize, 1);
            SetAdaptor<TransSkipMap> set(transaction);
            TxnTester(numThread, testSize, tranSize, keyRange, insertion, deletion, set, transaction);
            delete(transaction);
        }
        break;
    case 7:
        {
            TransactionHandler* transaction = new TransactionHandler(testSize, numThread+ 1, tranSize, testSize, 1);
            SetAdaptor<TransListMap> set(transaction);
            TxnTester(numThread, testSize, tranSize, keyRange, insertion, deletion, set, transaction);
            delete(transaction);
        }
        break;
    case 8:
        {
            TransactionHandler* transaction = new TransactionHandler(testSize, numThread+ 1, tranSize, testSize, NUM_LISTS);
            SetAdaptor<TransactionalContainer> set(transaction);
            MultiTxnTester(numThread, testSize, tranSize, keyRange, insertion, deletion, set, transaction);
            delete(transaction);
        }
        break;
    case 9:
        {
            TransactionHandler* transaction = new TransactionHandler(testSize, numThread+ 1, tranSize, testSize, 1);
            SetAdaptor<TransList> set(transaction);
            TxnTester(numThread, testSize, tranSize, keyRange, insertion, deletion, set, transaction, 2);
            delete(transaction);
        }
        break;
    case 10:
        {
            TransactionHandler* transaction = new TransactionHandler(testSize, numThread+ 1, tranSize, testSize, 1);
            SetAdaptor<TransSkip> set(transaction);
            TxnTester(numThread, testSize, tranSize, keyRange, insertion, deletion, set, transaction, 2);
            delete(transaction);
        }
        break;
    case 11:
        {
            TransactionHandler* transaction = new TransactionHandler(testSize, numThread+ 1, tranSize, testSize, 1);
            SetAdaptor<TransListMap> set(transaction);
            TxnTester(numThread, testSize, tranSize, keyRange, insertion, deletion, set, transaction, 2);
            delete(transaction);
        }
        break;
    case 12:
        {
            TransactionHandler* transaction = new TransactionHandler(testSize, numThread+ 1, tranSize, testSize, 1);
            SetAdaptor<TransSkipMap> set(transaction);
            TxnTester(numThread, testSize, tranSize, keyRange, insertion, deletion, set, transaction, 2);
            delete(transaction);
        }
        break;
    case 13:
        {
            TransactionHandler* transaction = new TransactionHandler(testSize, numThread+ 1, tranSize, testSize, 1);
            SetAdaptor<TransactionalContainer> set(transaction);
            MultiTxnTester(numThread, testSize, tranSize, keyRange, insertion, deletion, set, transaction, 2);
            delete(transaction);
        }
        break;
    case 14:
        {
            TransactionHandler* transaction = new TransactionHandler(testSize, numThread+ 1, tranSize, testSize, 1);
            SetAdaptor<TransListMap> set(transaction);
            TxnTester(numThread, testSize, tranSize, keyRange, insertion, deletion, set, transaction, 5);
            delete(transaction);
        }
        break;
    default:
        break;
    }

    return 0;
}
