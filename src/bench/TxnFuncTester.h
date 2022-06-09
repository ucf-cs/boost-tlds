#include "translink/transaction.h"

/**
 * Tester class that contains the actual list and information about
 * the transaction in which the function pointer MyTxnFunction is called within.
 * CallOps() is performed from a transaction which is initialized outside of the
 * class on the specified list.
*/
class TxnFuncTester
{
public:

    /** The transactional container we are operating on */
    inline static std::vector<TransactionalContainer*> _list;

    /** The transaction handler */
    inline static TransactionHandler* _txn;

    /**
     * The function containing transactional operations
     * \param desc      the descriptor
     * \param inputMap  the map containing input values
     */
    static ReturnCode MyTxnFunction(Desc* desc, std::map<std::string, int>& inputMap){

#ifdef DEBUG
        printf("Input map 'x': %d\n", inputMap["x"]);
#endif

        int foundValue = 0;

        ReturnCode retCode = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[0], INSERT, 1, 1, foundValue);

#ifdef DEBUG
        if (retCode == OK)
            printf("INSERT SUCCESSFUL\n");
        else
            printf("FAILED OR SKIPPED\n");
#endif

        ReturnCode retCode2 = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[0], FIND, 1, 1, foundValue);

#ifdef DEBUG
        if (retCode2 == OK)
            printf("%i\n", foundValue);
        else
            printf("FAILED OR SKIPPED\n");
#endif

        ReturnCode retCode3 = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[0], DELETE, 1, 1, foundValue);

#ifdef DEBUG
        if (retCode3 == OK)
            printf("DELETE SUCCESSFUL\n");
        else
            printf("FAILED OR SKIPPED\n");
#endif

        inputMap["value"] = 100;
        
        return OK;
    }

    /**
     * The function containing transactional operations
     * \param desc      the descriptor
     * \param inputMap  the map containing input values
     */
    static ReturnCode DTT_on_lists(Desc* desc, std::map<std::string, int>& inputMap){
        
        int foundValue = 0;

        ReturnCode retCode = OK;

        for (int i=0;i<TxnFuncTester::_list.size(); i++) {

            retCode = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[i], FIND, inputMap["key"], 0, foundValue);

            // if found, delete it
            if (retCode == OK) {
                inputMap["found_" + i] = 1;
                inputMap["foundValue_" + i] = foundValue;
                retCode = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[i], DELETE, inputMap["key"], 0, foundValue);
            } else if (retCode == FAIL) { // otherwise insert it
                inputMap["found_" + i] = 0;
                retCode = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[i], INSERT, inputMap["key"], inputMap["value"], foundValue);
            }

            if (retCode == FAIL) return retCode;

        }
        
        return retCode;
    }

    /**
     * The function to test double access issue
     * \param desc      the descriptor
     * \param inputMap  the map containing input values
     */
    static ReturnCode MapDoubleAccess(Desc* desc, std::map<std::string, int>& inputMap){

        ReturnCode retCode = OK;

        int size = inputMap["size"];

        for (int i=0;i<size; i++) {
        
            std::stringstream ss;
            ss << "key" << i;
            std::string index = ss.str();
            int foundValue = 0;

            retCode = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[0], FIND, inputMap[index], 0, foundValue);

            // if found, find it again and make sure the value is the same
            if (retCode == OK) {
                inputMap["found_" + i] = 1;
                inputMap["foundValue_" + i] = foundValue;

                // if found, proceed to second find operation
                if (retCode == OK) {
                    int secondFind = 0;
                    retCode = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[0], FIND, inputMap[index], 0, secondFind);

                    // check that the values are the same
                    if(retCode == OK) {
                        inputMap["secondFound_" + i] = 1;
                        inputMap["secondFoundValue_" + i] = secondFind;

                        if (foundValue != secondFind){
                            #ifdef DEBUG
                            printf("Double access test case failed!\n");
                            #endif
                            return FAIL;
                        }

                    } else if (retCode == FAIL) return retCode;

                } else if (retCode == FAIL) return retCode;
            } else if (retCode == FAIL) { // otherwise insert it
                inputMap["found_" + i] = 0;
                retCode = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[0], INSERT, inputMap[index], inputMap["value"], foundValue);
                // if successful, perform first find again
                if (retCode == OK) {
                    retCode = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[0], FIND, inputMap[index], 0, foundValue);

                    // if successful, perform second find
                    if(retCode == OK) {
                        inputMap["found_" + i] = 1;
                        inputMap["foundValue_" + i] = foundValue;

                        int secondFind = 0;
                        retCode = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[0], FIND, inputMap[index], 0, secondFind);

                        // check that the values are the same
                        if (retCode == OK) {
                            inputMap["secondFound_" + i] = 1;
                            inputMap["secondFoundValue_" + i] = secondFind;

                            if (foundValue != secondFind){
                                #ifdef DEBUG
                                printf("Double access test case failed!\n");
                                #endif
                                return FAIL;
                            }
                        } else if (retCode == FAIL) return retCode;

                    } else if (retCode == FAIL) return retCode;

                } else if (retCode == FAIL) return retCode;
            }

            if (retCode == FAIL) return retCode;

        }
        
        return retCode;
    }

    static ReturnCode populateList(Desc* desc, std::map<std::string, int>& inputMap){

        int foundValue = 0;
        ReturnCode retCode = OK;

        for (int i=0;i<TxnFuncTester::_list.size(); i++) {
            std::map<std::string, int>::iterator it = inputMap.begin();
            while(it != inputMap.end())
            {
                setkey_t key = (setkey_t)std::stoi(it->first);
                retCode = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[i], INSERT, it->second, 0, foundValue);
                if(retCode == OK)
                {
                    printf("Inserted variable %s with value %d.\n", it->first, it->second);
                }
            }
        }
        return retCode;
    }

    static ReturnCode addDoubledVal(Desc* desc, std::map<std::string, int>& inputMap){

        int foundValue = 0;
        ReturnCode retCode = OK;

        for (int i=0;i<TxnFuncTester::_list.size(); i++) {
            std::map<std::string, int>::iterator it = inputMap.begin();
            while(it != inputMap.end())
            {
                retCode = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[i], FIND, it->second, 0, foundValue);
                if (retCode == OK)
                {
                   int doubledVal = it->second*2;
                   retCode = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[i], INSERT, doubledVal, 0, foundValue); 
                   std::string val = std::to_string(doubledVal);
                   inputMap[val] = doubledVal;
                }
            }
        }
        return retCode;
    }


    static std::vector<std::map<std::string, int>> inputMap;

    static std::vector<TransactionalFunction> function;

    TxnFuncTester(){
        Init();
    }

    static void Init() {
        inputMap.clear();
        function.clear();

        // 1: MyTxnFunction
        std::map<std::string, int> MyTxnFunction_inputMap;
        MyTxnFunction_inputMap["x"] = 1;
        MyTxnFunction_inputMap["y"] = 2;
        MyTxnFunction_inputMap["z"] = 3;
        inputMap.push_back(MyTxnFunction_inputMap);
        function.push_back(MyTxnFunction);

        // 2: DTT_on_lists
        std::map<std::string, int> DTT_on_lists_inputMap;
        DTT_on_lists_inputMap["key"] = 10;
        DTT_on_lists_inputMap["value"] = 2;
        inputMap.push_back(DTT_on_lists_inputMap);
        function.push_back(DTT_on_lists);

        // 3: populateList
        std::map<std::string, int> populateList_inputMap;
        populateList_inputMap["1"] = 2;
        populateList_inputMap["2"] = 4;
        populateList_inputMap["3"] = 6;
        populateList_inputMap["4"] = 8;
        populateList_inputMap["5"] = 10;
        populateList_inputMap["6"] = 12;
        inputMap.push_back(populateList_inputMap);
        function.push_back(populateList);

        // 4: addDoubledVal
        std::map<std::string, int> addDoubledVal_inputMap;
        addDoubledVal_inputMap["1"] = 3;
        addDoubledVal_inputMap["2"] = 6;
        addDoubledVal_inputMap["3"] = 9;
        addDoubledVal_inputMap["4"] = 12;
        addDoubledVal_inputMap["5"] = 15;
        addDoubledVal_inputMap["6"] = 18;
        inputMap.push_back(addDoubledVal_inputMap);
        function.push_back(addDoubledVal);

        // 5: MapDoubleAccess
        std::map<std::string, int> mapDoubleAccess_inputMap;
        mapDoubleAccess_inputMap["size"] = 3;
        mapDoubleAccess_inputMap["value"] = 12345;
        mapDoubleAccess_inputMap["key0"] = 1;
        mapDoubleAccess_inputMap["key1"] = 2;
        mapDoubleAccess_inputMap["key2"] = 3;
        mapDoubleAccess_inputMap["key3"] = 4;
        mapDoubleAccess_inputMap["key4"] = 5;
        mapDoubleAccess_inputMap["key5"] = 6;
        mapDoubleAccess_inputMap["key6"] = 7;
        mapDoubleAccess_inputMap["key7"] = 8;
        mapDoubleAccess_inputMap["key8"] = 9;
        mapDoubleAccess_inputMap["key9"] = 10;
        inputMap.push_back(mapDoubleAccess_inputMap);
        function.push_back(MapDoubleAccess);
    }

};

std::vector<std::map<std::string, int>> TxnFuncTester::inputMap;

std::vector<TransactionalFunction> TxnFuncTester::function;