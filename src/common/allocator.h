#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <cstdint>
#include <malloc.h>
#include <atomic>
#include <common/assert.h>

template<typename DataType>
class Allocator 
{
public:
    Allocator(uint64_t totalBytes, uint64_t threadCount, uint64_t typeSize)
    {
        m_totalBytes = totalBytes;
        m_threadCount = threadCount;
        m_typeSize = typeSize;
        m_ticket = 0;
        m_pool = (char*)memalign(m_typeSize, totalBytes);

        ASSERT(m_pool, "Memory pool initialization failed.");
    }

    ~Allocator()
    {
        free(m_pool);
    }

    //Every thread need to call init once before any allocation
    void Init()
    {
        uint64_t threadId = __sync_fetch_and_add(&m_ticket, 1);
        ASSERT(threadId < m_threadCount, "ThreadId specified should be smaller than thread count.");

        m_base = m_pool + threadId * m_totalBytes / m_threadCount;
#ifdef FINE
        printf("ID: %u Desc     [base: %p = pool(%p) + threadId(%u) * totalBytes(%u) / threadCount(%u)] typeSize: %u [%p]\n", threadId, m_base, m_pool, threadId, m_totalBytes, m_threadCount, m_typeSize, (char*)(m_pool + m_totalBytes));
#endif
        m_freeIndex = 0;
    }

    void Uninit()
    { }

    DataType* Alloc()
    {
        // Watch this to see what is causing seg faults.
        ASSERT(m_freeIndex < m_totalBytes / m_threadCount, "out of capacity.");

        allocs++;

        char* ret = m_base + m_freeIndex;
#ifdef FINE
        printf("Desc     [base: %p | pool: %p] : ", m_base, m_pool);
        printf("    %p <---> %p] | %p (Alloc #%u)\n", ret, (char*)(m_base+m_freeIndex+m_typeSize), (char*)(m_pool+m_totalBytes), allocs);
        if ((m_base+m_freeIndex+m_typeSize) >= (m_pool+m_totalBytes)){
            printf(" ^--- The above allocation will exceed capacity\n");
        }
#endif


        m_freeIndex += m_typeSize;

        return (DataType*)ret;
    }

private:
    char* m_pool;
    uint64_t m_totalBytes;      //number of elements T in the pool
    uint64_t m_threadCount;
    uint64_t m_ticket;
    uint64_t m_typeSize;

    static uint64_t allocs;
    static __thread char* m_base;
    static __thread uint64_t m_freeIndex;
};

template<typename T>
uint64_t Allocator<T>::allocs = 0;

template<typename T>
__thread char* Allocator<T>::m_base;

template<typename T>
__thread uint64_t Allocator<T>::m_freeIndex;

#endif /* end of include guard: ALLOCATOR_H */
