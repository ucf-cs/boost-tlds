/**
 * Contains multiple objects used in transactions
 */
#ifndef DATAOBJECTS_H
#define DATAOBJECTS_H

#include <cstdlib>
#include <cstdint>
#include <vector>
#include <map>
#include <tgmath.h>


/** Fine for 2^NUM_LEVELS nodes. */
#define NUM_LEVELS 20

/** The maximum number of operations supported in a transaciton */
#define MAX_OPS 255

/** Forward declaraction of TransactionalContainer */
class TransactionalContainer;

/** Forward declaraction of Descriptor */
struct Desc;

/** Forward declaration of OnFinish */
struct OnFinish;

/** \typedef Key used in skip lists */
typedef unsigned long setkey_t;

/** \typedef Val used in skip lists */
typedef void         *setval_t;


/**
 * The status of a transactional operation
 */
enum OpStatus
{
    ACTIVE = 0, /**< Active */
    COMMITTED,  /**< Commited */
    ABORTED     /**< Aborted */
};

/**
 * The status of an operation on a list
 */
enum ReturnCode
{
	OK = 0, /**< Okay */
	SKIP,   /**< Skipped */
	FAIL    /**< Failed */
};

/**
 * The type of operation to perform on a list
 */
enum OpType
{
	FIND = 0,   /**< Find */
	INSERT,     /**< Insert */
	DELETE      /**< Delete */
};

/**
 * Contains the operation to perform on a data structure
 */
struct Operator
{
    /** The type of operation to perform */
    uint8_t type;

    /** The key for this operation */
    uint32_t key;

    /** The value associated with the key */
    int value;

    /** A reference to the container */
    TransactionalContainer* container;
};

/** \typedef SetOpArray
 * A list of Operations */
typedef std::vector<Operator> SetOpArray;

/** \typedef TransactionalFunction
 * 
 * A transactional function pointer.
 * 
 * @param   desc The descriptor object
 * @param   & The reference to the input map 
 * 
 */
typedef ReturnCode (*TransactionalFunction)(Desc* desc, std::map<std::string, int>&);

/**
 * The descriptor object containing information about transactions to perform
 */
struct Desc
{
    /**
     * Calcuates the size of Desc
     * Note: because we memalign by typeSize, the size of this object
     * needs to be a power of 2
     *
     * @return      The size of this descriptor
     */
    static size_t SizeOf(uint8_t size)
    {
        // return sizeof(uint8_t) + sizeof(uint8_t) + sizeof(Operator) * size + sizeof(TransactionalFunction)
        //     + sizeof(std::map<std::string, int>*) + sizeof(std::map<std::string, int>**) + sizeof(ReturnCode*) * size;
        return pow(2, 13);
    }

    /** The status of the operations */
    volatile uint8_t status;

    /** The number of operations */
    uint8_t size;

    /** A pointer to the transactional function */
    //ReturnCode (*TransactionalFunction)(Desc* desc, std::map<std::string, int>&); 
    TransactionalFunction function;

    /** The map containing the input values to the function */
    std::map<std::string, int>* inputMap;

    /** A reference to a pointer to the output map of the function. The pointer referenced can be null. */
    std::map<std::string, int>** outputMap;

    /** The operations to perform */
    Operator ops[MAX_OPS];

    /** The list that stores to see if the operation has completed. */
    ReturnCode *returnValue[MAX_OPS];
};

/**
 * A descriptor for a specific operation.
 * Note: because we memalign by typeSize, the size of this object
 * needs to be a power of 2
 */
struct NodeDesc
{
    /**
     * Constructor
     *
     * @param _desc The descriptor object
     * @param _oldNodeDesc The previous nodeDesc that last modified the node. Can be null.
     * @param _opid The id of the operation, in other words the index into the
     * operations of the descriptor
     */
    NodeDesc(Desc* _desc, NodeDesc* _oldNodeDesc, uint8_t _opid)
        : desc(_desc), previousNodeDesc(_oldNodeDesc), opid(_opid){}

    /**
     * Calcuates the size of NodeDesc
     * Note: because we memalign by typeSize, the size of this object
     * needs to be a power of 2
     *
     * @return      The size of this descriptor
     */
    static size_t SizeOf()
    {
        return pow(2, 5);
    }

    /** The descriptor object */
    Desc* desc;

    /** The previous descriptor object the node referenced before
     * the current descriptor replaced it */
    NodeDesc* previousNodeDesc;

    /** The id of the operation */
    uint8_t opid;
};

/**
 * A generic node object for a list.
 * Note: because we memalign by typeSize, the size of this object
 * needs to be a power of 2
 */
struct Node
{
    /** Constructor */
    Node(): key(0), nodeDesc(NULL){}

    /**
     * Constructor
     *
     * @param _key      The key for the node
     * @param _next     A pointer to the next node
     * @param _nodeDesc A reference to the node descriptor
     */
    Node(setkey_t _key, Node* _next, NodeDesc* _nodeDesc)
        : key(_key), nodeDesc(_nodeDesc)
	{
		next[0] = _next;
	}

    /**
     * Calcuates the size of Node
     * Note: because we memalign by typeSize, the size of this object
     * needs to be a power of 2
     *
     * @return      The size of this descriptor
     */
    static size_t SizeOf()
    {
        return pow(2, 8);
    }

    /** The level of the node, used in skip lists */
    int        level;
#define LEVEL_MASK     0x0ff
#define READY_FOR_FREE 0x100

    /** The value of the node (used in skip lists) */
    //setval_t  v;

    /** The key of the node (for transactions) */
    setkey_t key;

    /** A reference to the node descriptor */
    NodeDesc* nodeDesc;

    /** The pointer to the next node */
    Node* next[NUM_LEVELS];
};

/**
 * Contains properties that will be marked for for deletion at the
 * end of a transaction.
 */
struct OnFinish
{
    /** The container which will hold the information of a list. */
    TransactionalContainer *container;

    /** The node to be inserted or deleted */
    Node *node;

    /** The predecessor Node */
    Node *predNode;

    /** The descriptor object */
    Desc *desc;

};

#endif /* end of include guard: DATAOBJECTS_H */
