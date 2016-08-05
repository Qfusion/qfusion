#ifndef AI_STATIC_VECTOR_H
#define AI_STATIC_VECTOR_H

#include <initializer_list>
#include <utility>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef _MSC_VER
#pragma warning( disable : 4324 )       // structure was padded due to alignment specifier
#endif

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

    static constexpr size_type RealChunkSize()
    {
        return (sizeof(T) % alignof(T)) ? sizeof(T) + alignof(T) - (sizeof(T) % alignof(T)) : sizeof(T);
    }

    alignas(16) char staticBuffer[RealChunkSize() * N];

#ifndef _MSC_VER
    inline static void fail_with(const char *format, ...) __attribute__((format(printf, 1, 2))) __attribute((noreturn))
#else
    inline static void fail_with(const char *format, ...)
#endif
    {
        va_list va;
        va_start(va, format);
        vprintf(format, va);
        // Ensure that all buffered output will be shown
        fflush(stdout);
        va_end(va);
        abort();
    }

    inline ptrdiff_t idx(const_iterator ptr) { return ptr - basePointer; }
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
        if (index >= count)
            fail_with("Index %d is out of range (count = %d)\n", index, count);
#endif
        return basePointer[index];
    }
    inline const T &operator[](size_type index) const
    {
#ifdef _DEBUG
        if (index >= count)
            fail_with("Index %d is out of range (count = %d)\n", index, count);
#endif
        return basePointer[index];
    }

    inline size_type size() const { return count; }
    inline static constexpr size_type capacity() { return N; }
    inline bool empty() const { return count == 0; }

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
            fail_with("push_back(): count == N (%d)", N);
#endif
        new (basePointer + count)T(elem);
        count++;
    }

    inline void emplace_back(T && elem)
    {
#ifdef _DEBUG
        if (count == N)
            fail_with("emplace_back(): count == N (%d)", N);
#endif
        new (basePointer + count)T(std::forward<T>(elem));
        count++;
    }

    inline void pop_back()
    {
#ifdef _DEBUG
        if (count == 0)
            fail_with("pop_back(): count == 0");
#endif
        count--;
        (basePointer + count)->~T();
    }

    inline iterator emplace(const_iterator position, T &&elem)
    {
#ifdef _DEBUG
        if (position < cbegin() || position > cend())
            fail_with("emplace(): position %ld is outside of valid range [0,%ld]", idx(position), idx(cend()));
#endif
        if (position != cend())
        {
            ptrdiff_t index = position - cbegin();
            basePointer[index].~T();
            new (basePointer + index)T(std::forward<T>(elem));
            return basePointer + index;
        }
        emplace_back(std::move(elem));
        return end() - 1;
    }

    inline iterator erase(const_iterator position)
    {
#ifdef _DEBUG
        if (position < cbegin() || position >= cend())
            fail_with("erase(): `position` %ld is outside of valid range [0,%ld)", idx(position), idx(cend()));
#endif
        // Shift the array left
        for (ptrdiff_t i = position - cbegin(); i < (ptrdiff_t)count - 1; ++i)
            basePointer[i] = basePointer[i + 1];
        // Chop the tail cell
        basePointer[--count].~T();
        return basePointer + (position - cbegin());
    }

    inline iterator erase(const_iterator first, const_iterator last)
    {
#ifdef _DEBUG
        if (first > last)
            fail_with("erase(): `first` %ld > `last` %ld", idx(first), idx(last));
        if (first < cbegin())
            fail_with("erase(): `first` %ld < `(c)begin()` %ld", idx(first), idx(cbegin()));
        if (last > cend())
            fail_with("erase(): `last` %ld > `(c)end()` %ld", idx(last), idx(cend()));
#endif
        ptrdiff_t shift = last - first;
        // Shift the array left
        for (ptrdiff_t i = first - cbegin(); i < (ptrdiff_t)count - shift; ++i)
            basePointer[i] = basePointer[i + shift];
        // Call tail destructors (`shift` times)
        for (const_iterator it = cend() - shift; it != cend(); ++it)
            (*it).~T();
        // Modify count
        count -= shift;
        return basePointer + (first - cbegin());
    }

    inline iterator insert(const_iterator position, const T &val)
    {
#ifdef _DEBUG
        if (count == N)
            fail_with("insert(): capacity has been exceeded");
        if (position < cbegin() || position > cend())
            fail_with("insert(): `position` %ld is outside of valid range [0,%ld]", idx(position), idx(cend()));
#endif
        // Shift the array right
        for (ptrdiff_t i = (ptrdiff_t)count - 1; i >= position - cbegin(); --i)
        {
            T &oldVal = basePointer[i];
            new (basePointer + i + 1)T(oldVal);
            oldVal.~T();
        }
        new (basePointer + (position - cbegin()))T(val);
        count++;
        return basePointer + (position - cbegin());
    }

    inline iterator insert(const_iterator position, size_type n, const T &val)
    {
#ifdef _DEBUG
        if (count + n > N)
            fail_with("insert(): capacity has been exceeded (count=%d,N=%d,n=%d)", count, N, n);
        if (position < cbegin() || position > cend())
            fail_with("insert(): `position` %ld is outside of valid range [%ld,%ld]",
                      idx(position), idx(cbegin()), idx(cend()));
#endif
        // Shift the array right
        for (ptrdiff_t i = (ptrdiff_t)count - 1; i >= position - cbegin(); --i)
        {
            T &oldVal = basePointer[i];
            new (basePointer + i + n)T(oldVal);
            oldVal.~T();
        }
        // Construct new elements before the shifted ones
        for (ptrdiff_t i = position - cbegin(), end = position - cbegin() + n; i < end; ++i)
            new (basePointer + i)T(val);
        count += n;
        return basePointer + (position - cbegin());
    }

    template <typename InputIterator>
    inline iterator insert(const_iterator position, InputIterator first, InputIterator last)
    {
        const_iterator curr_position = position;
        for (;first != last; first++)
            curr_position = insert(curr_position, *first) + 1;
        return basePointer + (position - cbegin());
    }

    inline iterator insert(const_iterator position, T &&val)
    {
#ifdef _DEBUG
        if (count == N)
            fail_with("insert(): capacity has been exceeded");
        if (position < cbegin() || position > cend())
            fail_with("insert(): `position` %ld is outside of valid range [0,%ld]", idx(position), idx(cend()));
#endif
        // Shift the array right
        for (ptrdiff_t i = (ptrdiff_t)count - 1; i >= position - cbegin(); --i)
        {
            T &oldVal = basePointer[i];
            new (basePointer + i + 1)T(oldVal);
            oldVal.~T();
        }
        new (basePointer + (position - cbegin()))T(std::forward<T>(val));
        count++;
        return basePointer + (position - cbegin());
    }

    inline iterator insert(const_iterator position, std::initializer_list<T> vals)
    {
#ifdef _DEBUG
        if (count + vals.size() > N)
            fail_with("insert(): capacity has been exceeded (`size()`=%d,N=%d,`vals.size()`=%lu)", count, N, vals.size());
        if (position < cbegin() || position > cend())
            fail_with("insert(): `position` %ld is outside of valid range [0,%ld]", idx(position), idx(cend()));
#endif
        // Shift the array right
        for (ptrdiff_t i = (ptrdiff_t)count - 1; i >= position - cbegin(); --i)
        {
            T &oldVal = basePointer[i];
            new (basePointer + i + vals.size())T(oldVal);
            oldVal.~T();
        }
        // Construct new elements based on their provided values
        ptrdiff_t i = position - cbegin();
        for (auto &val: vals)
        {
            new (basePointer + i)T(val);
            ++i;
        }
        count += vals.size();
        return basePointer + (position - cbegin());
    }
};

#endif
