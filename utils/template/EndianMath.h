// Copyright (C) 2000  Sean Cavanaugh
// This file is licensed under the terms of the Lesser GNU Public License
// (see LPGL.txt, or http://www.gnu.org/copyleft/lesser.txt)

#ifndef ENDIAN_H__
#define ENDIAN_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "basictypes.h"
#include "BaseMath.h"

class Endian
{
public:
    inline static INT16 FASTCALL Flip(const INT16 x)
    {
        INT16 a = (x >> 8) & 0x000FF;
        INT16 b = (x << 8) & 0x0FF00;
        INT16 rval = (a | b);
        return rval;
    }

    inline static UINT16 FASTCALL Flip(const UINT16 x)
    {
        UINT16 a = (x >> 8) & 0x000FF;
        UINT16 b = (x << 8) & 0x0FF00;
        UINT16 rval = (a | b);
        return rval;
    }

    inline static INT32 FASTCALL Flip(const INT32 x)
    {
        INT32 a = (x >> 24) & 0x0000000FF;
        INT32 b = (x >> 8)  & 0x00000FF00;
        INT32 c = (x << 8)  & 0x000FF0000;
        INT32 d = (x << 24) & 0x0FF000000;
        INT32 rval = (a | b | c | d);
        return rval;
    }

    inline static UINT32 FASTCALL Flip(const UINT32 x)
    {
        INT32 a = (x >> 24) & 0x0000000FF;
        INT32 b = (x >> 8)  & 0x00000FF00;
        INT32 c = (x << 8)  & 0x000FF0000;
        INT32 d = (x << 24) & 0x0FF000000;
        INT32 rval = (a | b | c | d);
        return rval;
    }
#if 0
    inline static INT64 FASTCALL Flip(const INT64 x)
    {
        INT64 a = (x >> 56) & 0x000000000000000FF;
        INT64 b = (x >> 40) & 0x0000000000000FF00;
        INT64 c = (x >> 24) & 0x00000000000FF0000;
        INT64 d = (x >> 8 ) & 0x000000000FF000000;
        INT64 e = (x << 8 ) & 0x0000000FF00000000;
        INT64 f = (x << 24) & 0x00000FF0000000000;
        INT64 g = (x << 40) & 0x000FF000000000000;
        INT64 h = (x << 56) & 0x0FF00000000000000;
        INT64 rval = (a | b | c | d | e | f | g | h);
        return rval;
    }

    inline static UINT64 FASTCALL Flip(const UINT64 x)
    {
        UINT64 a = (x >> 56) & 0x000000000000000FF;
        UINT64 b = (x >> 40) & 0x0000000000000FF00;
        UINT64 c = (x >> 24) & 0x00000000000FF0000;
        UINT64 d = (x >> 8 ) & 0x000000000FF000000;
        UINT64 e = (x << 8 ) & 0x0000000FF00000000;
        UINT64 f = (x << 24) & 0x00000FF0000000000;
        UINT64 g = (x << 40) & 0x000FF000000000000;
        UINT64 h = (x << 56) & 0x0FF00000000000000;
        UINT64 rval = (a | b | c | d | e | f | g | h);
        return rval;
    }
#endif
    inline static float FASTCALL Flip(const float x)
    {
        union floatflipper
        {
            struct _x_t
            {
                BYTE    _v[4];
            } _x;
            float   _f;
        };

        floatflipper tmp;
        tmp._f = x;
        SWAP(tmp._x._v[0], tmp._x._v[3]);
        SWAP(tmp._x._v[1], tmp._x._v[2]);
        return tmp._f;
    }

    inline static double FASTCALL Flip(const double x)
    {
        union floatflipper
        {
            struct _x_t
            {
		BYTE	_v[8];
            } _x;
            double  _d;
        };

        floatflipper tmp;
        tmp._d = x;
        SWAP(tmp._x._v[0], tmp._x._v[7]);
        SWAP(tmp._x._v[1], tmp._x._v[6]);
        SWAP(tmp._x._v[2], tmp._x._v[5]);
        SWAP(tmp._x._v[3], tmp._x._v[4]);
        return tmp._d;
    }

    inline static void FlipArray(unsigned size, INT16* x)
    {
        for (unsigned i=0 ; i<size ; i++, x++)
        {
            *x = Flip(*x);
        }
    }

    inline static void FlipArray(unsigned size, UINT16* x)
    {
        for (unsigned i=0 ; i<size ; i++, x++)
        {
            *x = Flip(*x);
        }
    }

    inline static void FlipArray(unsigned size, INT32* x)
    {
        for (unsigned i=0 ; i<size ; i++, x++)
        {
            *x = Flip(*x);
        }
    }

    inline static void FlipArray(unsigned size, UINT32* x)
    {
        for (unsigned i=0 ; i<size ; i++, x++)
        {
            *x = Flip(*x);
        }
    }
#if 0
    inline static void FlipArray(unsigned size, INT64* x)
    {
        for (unsigned i=0 ; i<size ; i++, x++)
        {
            *x = Flip(*x);
        }
    }
    inline static void FlipArray(unsigned size, UINT64* x)
    {
        for (unsigned i=0 ; i<size ; i++, x++)
        {
            *x = Flip(*x);
        }
    }
#endif

    inline static void FlipArray(unsigned size, float* x)
    {
        for (unsigned i=0 ; i<size ; i++, x++)
        {
            *x = Flip(*x);
        }
    }

    inline static void FlipArray(unsigned size, double* x)
    {
        for (unsigned i=0 ; i<size ; i++, x++)
        {
            *x = Flip(*x);
        }
    }
};

#endif // ENDIAN_H__

