// Copyright (C) 2000  Sean Cavanaugh
// This file is licensed under the terms of the Lesser GNU Public License
// (see LPGL.txt, or http://www.gnu.org/copyleft/lesser.txt)

#ifndef BASEMATH_H__
#define BASEMATH_H__

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#if defined(_WIN32) && !defined(FASTCALL)
#define FASTCALL __fastcall
#endif

//
// MIN/MAX/AVG functions, work best with basic types
//

template < typename T >
inline T FASTCALL MIN(const T a, const T b)
{
    return ((a < b) ? a : b);
}

template < typename T >
inline T FASTCALL MIN(const T a, const T b, const T c)
{
    return (MIN(MIN(a, b), c));
}

template < typename T >
inline T FASTCALL MAX(const T a, const T b)
{
    return ((a > b) ? a : b);
}

template < typename T >
inline T FASTCALL MAX(const T a, const T b, const T c)
{
    return (MAX(MAX(a, b), c));
}

template < typename T >
inline T FASTCALL AVG(const T a, const T b)
{
    return ((a + b) / 2);
}

//
// MIN/MAX/AVG functions, work best with user-defined types
// (hopefully the compiler will choose the right one in most cases
//


template < typename T >
inline T FASTCALL MIN(const T& a, const T& b)
{
    return ((a < b) ? a : b);
}

template < typename T >
inline T FASTCALL MIN(const T& a, const T& b, const T& c)
{
    return (MIN(MIN(a, b), c));
}

template < typename T >
inline T FASTCALL MAX(const T& a, const T& b)
{
    return ((a > b) ? a : b);
}

template < typename T >
inline T FASTCALL MAX(const T& a, const T& b, const T& c)
{
    return (MAX(MAX(a, b), c));
}

template < typename T >
inline T FASTCALL AVG(const T& a, const T& b)
{
    return ((a + b) / 2);
}


//
// Generic Array Operations
//

template < typename T >
inline T FASTCALL MIN(const T* array, const int size)
{
    assert(size);
    T               val = array[0];

    for (int i = 1; i < size; i++)
    {
        if (val > array[i])
        {
            val = array[i];
        }
    }
    return val;
}

template < typename T >
inline T FASTCALL MAX(const T* array, const int size)
{
    assert(size);
    T               val = array[0];

    for (int i = 1; i < size; i++)
    {
        if (val < array[i])
        {
            val = array[i];
        }
    }
    return val;
}

template < typename T >
inline T FASTCALL AVG(const T* array, const int size)
{
    assert(size);
    T               sum = array[0];

    for (int i = 1; i < size; i++)
    {
        sum += array[i];
    }
    return sum / num;
}

template < typename T >
inline T FASTCALL SUM(const T* array, const int size)
{
    assert(size);
    T               sum = array[0];

    for (int i = 1; i < size; i++)
    {
        sum += array[i];
    }
    return sum;
}


// Uses one temp to swap, works best with user-defined types or doubles/long doubles
template < typename T >
inline void FASTCALL SWAP(T& a, T& b)
{
    T               temp = a;

    a = b;
    b = temp;
}


// XOR math to swap (no temps), works with integral types very well
template < typename T >
inline void FASTCALL XOR_SWAP(T& a, T& b)
{
    a ^= b;
    b ^= a;
    a ^= b;
}


// Uses two temps to swap, works very well with built-in types on pipelined CPUs
template < typename T >
inline void FASTCALL PIPE_SWAP(T& a, T& b)
{
    T               tmpA = a;
    T               tmpB = b;

    b = tmpA;
    a = tmpB;
}


#endif // BASEMATH_H__

