#include <cxxtest/TestSuite.h>
#include "translink/list/translist.h"
#include "translink/dataObjects.h"
#include <thread>
#include <boost/random.hpp>
#include "common/timehelper.h"
#include "common/threadbarrier.h"

class TransListSuite : public CxxTest::TestSuite
{
private:
    static TransactionHandler* transaction;
    static TransList* list;

    static std::vector<std::map<std::string, int>> inputMap;
    static std::vector<TransactionalFunction> function;

    uint32_t setType = 0;
    uint32_t numThread = 16;
    uint32_t testSize = 1000;
    uint32_t tranSize = 4;
    uint64_t keyRange = 100;
    uint32_t insertion = 33;
    uint32_t deletion = 33;

public:
    void setUp()
    {

        transaction = new TransactionHandler(testSize, numThread+ 1, tranSize, testSize, 1);
        list = new TransList(transaction);

        std::vector<std::thread> thread(numThread);
        ThreadBarrier barrier(numThread + 1);

        double startTime = Time::GetWallTime();
        boost::mt19937 randomGen;
        boost::mt19937 randomGenVal;
        randomGen.seed(startTime - 10);
        randomGenVal.seed(startTime + 10);
        boost::uniform_int<uint32_t> randomDist(1, keyRange);
        boost::uniform_int<uint32_t> randomDistVal(1, 10);

        // Initialize transaction
        transaction->containers.push_back(list);
        transaction->Init();

        // populate list
        SetOpArray ops(1);
        for(unsigned int i = 0; i < keyRange; ++i)
        {
            ops[0].type     = INSERT;
            ops[0].key      = randomDist(randomGen);
            ops[0].value    = randomDistVal(randomGenVal);
            ops[0].container = list;
            transaction->ExecuteOps(ops);
        }

        transaction->ResetMetrics();

        std::map<std::string, int> DTT_on_lists_inputMap;
        DTT_on_lists_inputMap["key"] = randomDist(randomGen);
        DTT_on_lists_inputMap["value"] = randomDistVal(randomGenVal);
        inputMap.push_back(DTT_on_lists_inputMap);
        function.push_back(DTT_function);
    }

    void tearDown()
    {
        delete(list);
        delete(transaction);
    }

    static ReturnCode DTT_function(Desc* desc, std::map<std::string, int>& inputMap)
    {
        int foundValue = 0;

        ReturnCode retCode = OK;

        // Find the key
        retCode = transaction->CallOps(desc, list, FIND, inputMap["key"], 0, foundValue);

        // Insert the value we want
        if (retCode == OK) {
            // if (foundValue != inputMap["value"]) {
                retCode = transaction->CallOps(desc, list, DELETE, inputMap["key"], 0, foundValue);
                
                if (retCode == OK) {
                    retCode = transaction->CallOps(desc, list, INSERT, inputMap["key"], inputMap["value"], foundValue);
                }
                if (retCode == SKIP) return retCode;
            // }
        } else if (retCode == FAIL) {
            retCode = transaction->CallOps(desc, list, INSERT, inputMap["key"], inputMap["value"], foundValue);
        }
        
        if (retCode == SKIP) return retCode;
        
        // Check the final value
        retCode = transaction->CallOps(desc, list, FIND, inputMap["key"], 0, foundValue);
        if(retCode == OK) {
            inputMap["foundValue"] = foundValue;
        }

        return retCode;
    }

    static void WorkerThread(int threadId, ThreadBarrier& barrier, int index, int testSize, int keyRange, std::map<std::string, int>** outputMap)
    {
        //set affinity for each thread
        cpu_set_t cpu = {{0}};
        CPU_SET(threadId, &cpu);
        sched_setaffinity(0, sizeof(cpu_set_t), &cpu);

        double startTime = Time::GetWallTime();
        boost::mt19937 randomGen;
        boost::mt19937 randomGenVal;
        randomGen.seed(startTime - 10);
        randomGenVal.seed(startTime + 10);
        boost::uniform_int<uint32_t> randomDist(1, keyRange);
        boost::uniform_int<uint32_t> randomDistVal(1, 10);

        transaction->Init();

        barrier.Wait();

        bool retCode;
        // Pass in function pointer
        for (int i=0; i<testSize; i++) {
            retCode = transaction->ExecuteOps(function[index], &(inputMap[index]), outputMap);

            TS_ASSERT_EQUALS(retCode, true);
            if (retCode) {
                TS_ASSERT_EQUALS((**outputMap)["foundValue"], (**outputMap)["value"]);
                //printf("foundValue: %u == value %u\n", (**outputMap)["foundValue"], (**outputMap)["value"]);
            }
        }
    }

    void test_DynamicTransactions()
    {
        std::vector<std::thread> thread(numThread);
        ThreadBarrier barrier(numThread + 1);
        std::map<std::string, int>* outputMap = NULL;

        //Create joinable threads
        for (unsigned i = 0; i < numThread; i++) 
        {
            thread[i] = std::thread(WorkerThread, i + 1, std::ref(barrier), 0, testSize, keyRange, &outputMap);
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

        // Validate outputMap
        TS_ASSERT_EQUALS((*outputMap)["foundValue"], (*outputMap)["value"]);
    }

};

TransactionHandler* TransListSuite::transaction;
TransList* TransListSuite::list;

std::vector<std::map<std::string, int>> TransListSuite::inputMap;
std::vector<TransactionalFunction> TransListSuite::function;