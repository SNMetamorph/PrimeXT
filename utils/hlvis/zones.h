// Copyright (C) 2000  Sean Cavanaugh
// This file is licensed under the terms of the Lesser GNU Public License
// (see LPGL.txt, or http://www.gnu.org/copyleft/lesser.txt)

#ifndef ZONING_H__
#define ZONING_H__

#if _MSC_VER >= 1000
#pragma once
#endif

#include "basictypes.h"
#include "winding.h"
#include "boundingbox.h"


// Simple class of visibily flags and zone id's.  No concept of location is in this class
class Zones
{
public:
    inline void flag(UINT32 src, UINT32 dst)
    {
        if ((src < m_ZoneCount) && (dst < m_ZoneCount))
        {
            m_ZonePtrs[src][dst] = true;
            m_ZonePtrs[dst][src] = true;
        }
    }
    inline bool check(UINT32 zone1, UINT32 zone2)
    {
        if ((zone1 < m_ZoneCount) && (zone2 < m_ZoneCount))
        {
            return m_ZonePtrs[zone1][zone2];
        }
        return false;
    }
    
    void set(UINT32 zone, const BoundingBox& bounds);
    UINT32 getZoneFromBounds(const BoundingBox& bounds);
    UINT32 getZoneFromWinding(const Winding& winding);

public:
    Zones(UINT32 ZoneCount)
    {
        m_ZoneCount = ZoneCount + 1;    // Zone 0 is used for all points outside all nodes
        m_ZoneVisMatrix = new bool[m_ZoneCount * m_ZoneCount];
        memset(m_ZoneVisMatrix, 0, sizeof(bool) * m_ZoneCount * m_ZoneCount);
        m_ZonePtrs = new bool*[m_ZoneCount];
        m_ZoneBounds = new BoundingBox[m_ZoneCount];

        UINT32 x;
        bool* dstPtr = m_ZoneVisMatrix;
        bool** srcPtr = m_ZonePtrs;
        for (x=0; x<m_ZoneCount; x++, srcPtr++, dstPtr += m_ZoneCount)
        {
            *srcPtr = dstPtr;
        }
    }
    virtual ~Zones()
    {
        delete[] m_ZoneVisMatrix;
        delete[] m_ZonePtrs;
        delete[] m_ZoneBounds;
    }

protected:
    UINT32       m_ZoneCount;
    bool*        m_ZoneVisMatrix;  // Size is (m_ZoneCount * m_ZoneCount) and data is duplicated for efficiency
    bool**       m_ZonePtrs;    // Lookups into m_ZoneMatrix for m_ZonePtrs[x][y] style;
    BoundingBox* m_ZoneBounds;
};

Zones* MakeZones();

#endif

