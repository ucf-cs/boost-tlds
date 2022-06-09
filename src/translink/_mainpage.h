/**
 * \mainpage Guide to the documentation
 * 
 * \section intro_sec Preface
 * 
 * This documentation is presented in doxygen format to view the important components of LFDTT.
 * This instruction guide to using LFDTT as an external library that is able to be included from 
 * the C++ boost library is currently not supported. This guide allows parts of the code to be 
 * explained separately, simply through objects and functions by building the program.
 * 
 * As it is currently not supported as a library, there are necessary steps before using the LFDTT
 * code along with testing tools that are also provided. This guide will provide the instructions 
 * to utilize the LFDTT codebase and tools that it provides. The documentation will also cover the 
 * basic implementation of the code so that  a user will be able to properly use and implement their 
 * code along with the codebase.
 * 
 * Simply begin viewing the documentation through the navigation bar. By clicking on one of the
 * classes, it will present the name of the class/struct along with the file with which it belongs 
 * to. They will list the functions and attributes/variables that belong to that reference. By scrolling
 * down, it will explain what the parameters and return values are for each function. Some classes
 * will contain member variables which will also contain a brief description of what they represent.
 * 
 * 
 * 
 * 
 * 
 * \section installation Installation Guide
 * 
 * The instructions for installing dependices were used in the Ubuntu distribution 18.04.1 for Linux
 * at the time of this documentation (April 2019). It may be different for some operating systems. 
 * 
 * \subsection step1 Install Dependencies
 *  
 * sudo apt-get install libboost-all-dev libgoogle-perftools-dev libtool m4 automake cmake libtbb-dev libgsl0-dev <br>
 * OR <br>
 * sudo apt-get install libboost-all-dev <br>
 * sudo apt-get install libgoogle-perftools-dev<br>
 * sudo apt-get install libtool m4 automake<br>
 * sudo apt-get install cmake <br>
 * sudo apt-get install libtbb-dev <br>
 * sudo apt-get install libgs10-dev<br>
 * 
 * \subsection step2 Install GCC
 * 
 * sudo apt-get update && \ <br>
 * sudo apt-get install build-essential software-properties-common -y && \ <br>
 * sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y && \ <br>
 * sudo apt-get update && \ <br>
 * sudo apt-get install gcc-snapshot -y && \ <br>
 * sudo apt-get update && \ <br>
 * sudo apt-get install gcc-7 g++-7 -y && \ <br>
 * sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 --slave /usr/bin/g++ g++ /usr/bin/g++-7 && \ <br>
 * sudo apt-get install gcc-4.8 g++-4.8 -y && \ <br>
 * sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.8 60 --slave /usr/bin/g++ g++ /usr/bin/g++-4.8; <br>
 * sudo update-alternatives --config gcc <br>
 * gcc -v <br>
 * 
 * \subsection step3 Install PERF (Possibly needed, please check to see if installed already)
 * 
 * sudo apt-get install libgoogle-perftools-dev <br>
 * sudo apt install linux-tools-common gawk <br>
 * sudo apt-get install linux-tools-generic <br>
 * sudo apt-get install linux-tools-4.13.0-45-generic <br>
 * sudo apt-get install linux-cloud-tools-4.13.0-45-generic <br>
 * 
 * \subsection step4 Get from Github
 * 
 * mkdir temp <br>
 * cd temp <br>
 * git clone https://github.com/ucfcs/Fall2018-Group42-boost-tlds.git <br>
 * (change "temp" folder name to "trans-dev") <br>
 * cd trans-dev <br>
 * bash bootstrap.sh <br>
 * cd ../trans-compile <br>
 * ../trans-dev/configure <br>
 * 
 * \subsection step5 MAKE and TEST
 * 
 * cd trans-compile <br>
 * make -j8 <br>
 * cd trans-dev/script <br>
 * python pqtest.py ../../trans-compile/src/trans <br>
 * 
 * \subsection step6 DEBUG
 * 
 * cd trans-compile <br>
 * make -j8 CXXFLAGS='-O0 -g' <br>
 * cd src <br>
 * 
 * gdb --args ./trans <data-structure> <nthreads> <iterations> <txn-size> <key-range> <percent-insert> <percent-delete> <br>
 * OR <br>
 * libtool --mode=execute gdb trans <br>
 * OR <br>
 * gdb ./trans <br>
 * 
 * 
 * 
 * \section usage_sec How to use LFDTT
 * 
 * The current LFDTT codebase is applicable toward the implementation of list and map datastructures.
 * Three operations supported are the Find, Insert, and Delete operations. There is a main file in 
 * the bench directory provided to showcase how the codebase can be implemented to act on those operations.
 * 
 * To begin, create a new TransactionHandler object by passing in the number of tests to run,
 * the number of threads to use, the number of transactions per test, and the number of containers per test
 * into the constructor.<br>
 * 
 * Next, create a new TransactionalContainer object and set the mentioned TransactionalHandler class to the 
 * transaction inside the container. This is done simply to reference all the method calls in reference to the list.
 * 
 * \subsection function_pointer Creating the Function Pointers
 * 
 * There are prerequisites to using the current implementation of creating function pointers.The function 
 * pointers have to be wrapped in a class that has to be created in order to contain information regarding 
 * 1) the datastructure list(s) itself noted as TransactionContainer, and 2) the information regarding 
 * transactions in order to call transaction based methods noted as _txn. Both of these local variables are
 * set outside of the class to reference them after the transactional execution. Shown below is a quick 
 * implementation of how to create function pointers:
 * 
 * \code
 * #include "translink/transaction.h"
 * 
 * class TxnFuncTester
 * {
 * public:
 *      inline static std::vector<TransactionalContainer*> _list;
 *      
 *      inline static TransactionHandler* _txn;
 * 
 *      static ReturnCode MyTxnFunction(Desc* desc, std::map<std::string, int>& inputMap){
 * #ifdef DEBUG
 *          printf("Input map 'x': %d\n", inputMap["x"]);
 * #endif
 *          int foundValue = 0;
 * 
 *          ReturnCode retCode = TxnFuncTester::_txn->CallOps(desc, TxnFuncTester::_list[0], INSERT, 1, 1, foundValue);
 * #ifdef DEBUG     
 *          if (retCode == OK)
 *              printf("INSERT SUCCESSFUL\n");
 *          else
 *              printf("FAILED OR SKIPPED\n");
 * #endif
 *          inputMap["value"] = 100;
 * 
 *          return OK;
 *      }
 * }
 * \endcode
 * 
 * \subsection operation_Calls Calling Operations
 * 
 * Calling operations are only done within the function pointers. Shown in the \ref function_pointer section
 * will provide a a quick implmentation on operation calls. The last argument of CallOps() foundValue
 * is used only in the FIND operation but will need to be included when using the method. See more details
 * regarding Find() in \ref using_Find.
 * 
 * \subsection operation_Execution Executing Operations as Transactions
 * 
 * In order to execute operations, we need to pass in a function pointer that contains the operations to
 * perform on the datastructure list, an input map, and an a reference to the output map.
 * \code{.cpp}
 * ExecuteOps(const TransactionalFunction txnFunction, std::map<std::string, int>* inputMap, std::map<std::string, int>** outputMap)
 * \endcode
 * Once Executeops() is called, it will handle storing the data into the descriptor object which will
 * then call HelpOps() proceeding to call the function pointer inside HelpOps().
 * 
 * \subsection using_Find Find Operation
 * 
 * In order to call Find(), simply pass it in as FIND into the third parameter of CallOps().
 * 
 * - It returns OK if the key is found and stores the value at the node into the value variable.<br>
 * - It returns SKIP if another thread may be executing on the node.<br>
 * - It returns FAIL if the key cannot be found.<br>
 * 
 * The definition of Find() can be found here TransactionalContainer.
 * 
 * \subsection using_Insert Insert Operation
 * 
 * In order to call Insert(), simply pass it in as INSERT into the third parameter of CallOps().
 * 
 * - It returns OK if the key is successfully inserted along with the value.<br>
 * - It returns SKIP if another thread may be executing on the node.<br>
 * - It returns FAIL if the key exists and cannot be inserted.<br>
 * 
 * The definition of Insert can be found here TransactionalContainer.
 * 
 * \subsection using_Delete Delete Operation
 * 
 * In order to call Delete(), simply pass it in as DELETE into the third parameter of CallOps().
 * 
 * - It returns OK if the key is successfully deleted.<br>
 * - It returns SKIP if another thread may be executing on the node.<br>
 * - It returns FAIL if the key cannot be found and deleted.<br>
 * 
 * The definition of Delete() can be found here TransactionalContainer.
 * 
 * This concludes the guide.
 */