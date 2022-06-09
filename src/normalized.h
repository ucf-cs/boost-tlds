// these two define statements
// are used for struct DataNode
#define KEY uint32_t
#define VALUE uint32_t


// found in mapadaptor.h
// includes an enumeration for UPDATE, which
// lists should never access. Only maps call
// that member
enum OpType
{
    FIND = 0,
    INSERT,
    DELETE, 
    UPDATE
};

struct Operation // found in boostingskip.h
{
    Operation() : type(0), key(0){}
    Operation(uint8_t _type, uint32_t _key) : type(_type), key(_key){}

    uint8_t type;
    uint32_t key;
};

// found in boostingmap.h
// has more members in the struct than the 
// struct above
struct Operation
{
    Operation() : type(0), key(0), val(0), expected(0), threadId(0){}
    Operation(uint8_t _type, uint32_t _key, uint32_t _threadId) : type(_type), key(_key), val(0), expected(0), threadId(_threadId){}
    Operation(uint8_t _type, uint32_t _key, uint32_t _val, uint32_t _threadId) : type(_type), key(_key), val(_val), expected(0), threadId(_threadId){}
    Operation(uint8_t _type, uint32_t _key, uint32_t _val, uint32_t _expected, uint32_t _threadId) : type(_type), key(_key), val(_val), expected(_expected), threadId(_threadId){}

    uint8_t type;
    uint32_t key;
    uint32_t val;
    uint32_t expected;
    uint32_t threadId; // note could be smaller type
};

enum OpStatus
{
    ACTIVE = 0,
    COMMITTED,
    ABORTED,
};

enum ReturnCode // found in boostinglist.h
{
    OK = 0,
    LOCK_FAIL,
    OP_FAIL
};

// found in translist.h
// difference what gets enumerated to 1 and 2
// can we normalize this to a single enumeration? 
enum ReturnCode
{
    OK = 0,
    SKIP,
    FAIL
};

struct Desc
{
    static size_t SizeOf(uint8_t size)
    {
        return sizeof(uint8_t) + sizeof(uint8_t) + sizeof(Operator) * size;
    }

    // Status of the transaction: values in [0, size] means live txn, values -1 means aborted, value -2 means committed.
    volatile uint8_t status;
    uint8_t size;
    Operator ops[];
};

// found in translist.h
struct NodeDesc
{
    NodeDesc(Desc* _desc, uint8_t _opid)
        : desc(_desc), opid(_opid){}

    Desc* desc;
    uint8_t opid;
};

struct Node // found in lockfreelist.h
{
    uint32_t key;
    Node* next;
};

// found in rstmlist.hpp
// only difference to what's above is that there
// are constuctors in this struct for the members
struct Node
{
    int m_val;
    Node* m_next;

    // ctors
    Node(int val = -1) : m_val(val), m_next() { }

    Node(int val, Node* next) : m_val(val), m_next(next) { }
};

// found in translist.h
// difference is a node descriptor
struct Node
{
    Node(): key(0), next(NULL), nodeDesc(NULL){}
    Node(uint32_t _key, Node* _next, NodeDesc* _nodeDesc)
        : key(_key), next(_next), nodeDesc(_nodeDesc){}

    uint32_t key;
    Node* next;
    
    NodeDesc* nodeDesc;
};

// found in transskip.h
// difference is there is a key and a value, 
// not just an associated key
struct node_t
{
    int        level;
#define LEVEL_MASK     0x0ff
#define READY_FOR_FREE 0x100
    setkey_t  k;
    setval_t  v;

    NodeDesc* nodeDesc;

    node_t* next[1];
};


// found in translist.h
struct HelpStack
{
    void Init()
    {
        index = 0;
    }

    void Push(Desc* desc)
    {
        ASSERT(index < 255, "index out of range");

        helps[index++] = desc;
    }

    void Pop()
    {
        ASSERT(index > 0, "nothing to pop");

        index--;
    }

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

    Desc* helps[256];
    uint8_t index;
};

// found in transskip.h
struct trans_skip
{
    Allocator<Desc>* descAllocator;
    Allocator<NodeDesc>* nodeDescAllocator;

    node_t* tail;
    node_t head;
};

// found in nbmap.h
typedef struct 
{
    union
    {
        HASH hash;
        void* next;
    };
    #ifdef USE_KEY
    KEY key;/*Remove after Debugging */
    #endif
    
    VALUE value;
} DataNode;