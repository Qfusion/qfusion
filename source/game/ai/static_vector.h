#ifndef AI_STATIC_VECTOR_H
#define AI_STATIC_VECTOR_H

#include <utility>
#include <stdio.h>
#include <stdlib.h>

template <typename T, unsigned int N>
class alignas(16) StaticVector
{
public:
    typedef unsigned int size_type;
    typedef T *iterator;
    typedef const T *const_iterator;
    typedef T &reference;
    typedef const T &const_reference;

private:
    // Disable moving and, implicitly, copying
    StaticVector(StaticVector<T, N> &&that) = delete;

    T *basePointer;
    size_type count;

    static constexpr size_type realChunkSize()
    {
        return (sizeof(T) % alignof(T)) ? sizeof(T) + alignof(T) - (sizeof(T) % alignof(T)) : sizeof(T);
    }

    alignas(16) char staticBuffer[realChunkSize() * N];
public:
    inline StaticVector(): basePointer((T*)staticBuffer), count(0) {}

    inline ~StaticVector() { clear(); }

    inline void clear()
    {
        for (size_type i = 0; i < count; ++i)
        {
            (basePointer + i)->~T();
        }
        count = 0;
    }

    inline T &operator[](size_type index)
    {
#ifdef _DEBUG
        if (index > count)
        {
            printf("Index %d is out of range (count = %d)\n", index, count);
            abort();
        }
#endif
        return basePointer[index];
    }
    inline const T &operator[](size_type index) const
    {
#ifdef _DEBUG
        if (index > count)
        {
            printf("Index %d is out of range (count = %d)\n", index, count);
            abort();
        }
#endif
        return basePointer[index];
    }

    inline size_type size() { return count; }
    inline size_type capacity() { return N; }
    inline bool empty() { return count == 0; }

    inline iterator begin() { return basePointer; }
    inline const_iterator begin() const { return basePointer; }
    inline iterator end() { return basePointer + count; }
    inline const_iterator end() const { return basePointer + count; }

    inline const_iterator cbegin() const { return basePointer; }
    inline const_iterator cend() const { return basePointer + count; }

    inline reference front() { return operator[](0); };
    inline const_reference front() const { return operator[](0); }
    inline reference back() { return operator[](count - 1); }
    inline const_reference back() const { return operator[](count - 1); }

    inline void push_back(const T &elem)
    {
#ifdef _DEBUG
        if (count == N)
        {
            printf("push_back(): count == N (%d)\n", N);
            abort();
        }
#endif
        new (basePointer + count)T(elem);
        count++;
    }

    inline void emplace_back(T && elem)
    {
#ifdef _DEBUG
        if (count == N)
        {
            printf("emplace_back(): count == N (%d)\n", N);
            abort();
        }
#endif
        new (basePointer + count)T(std::forward<T>(elem));
        count++;
    }

    inline void pop_back()
    {
#ifdef _DEBUG
        if (count == 0)
        {
            printf("pop_back(): count == 0");
            abort();
        }
#endif
        count--;
        (basePointer + count)->~T();
    }
};

#endif
