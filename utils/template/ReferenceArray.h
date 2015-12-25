// Copyright (C) 2000  Sean Cavanaugh
// This file is licensed under the terms of the Lesser GNU Public License
// (see LPGL.txt, or http://www.gnu.org/copyleft/lesser.txt)

#if !defined(AFX_ReferenceArray_H__BAEBCE9D_CD68_40AF_8A54_B23A0D14E807__INCLUDED_)
#define AFX_ReferenceArray_H__BAEBCE9D_CD68_40AF_8A54_B23A0D14E807__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif
#include "assert.h"
#include "limits.h"

#include "ReferenceCounter.h"

/*!
  \author  Sean Cavanaugh
  \email   sean@dimensionalrift.com
  \cvsauthor $Author: sean $
  \date    $Date: 2000/09/11 20:28:24 $
  \version $Revision: 1.1 $ 
  \brief   ReferenceArray is exactly the same as ReferencePtr, except it expects
           the objects pointing to it to be allocated with new[] instead (and thusly
           the object is deleted with delete[] when the reference count hits zero)
           Additionally it provides a [] operator which returns a refernece to the
           item in the list.  No bounds checking is performed.

           Arrays of basic data types (char, int, float, etc) should use ReferencePtr,
           as delete[] is not required for them.  ReferencePtr can also handle void types
           as a template parameter.
*/
template<class DATA_T>
class ReferenceArrayBlock
{
public:
	DATA_T*			pData;			    // User defined data block
	mutable ReferenceCounter	ReferenceCount;
};


template<class DATA_T>
class ReferenceArray  
{
public:
	// Construction
	ReferenceArray();
	ReferenceArray(DATA_T* other);
	ReferenceArray(const ReferenceArray<DATA_T>& other);
	virtual ~ReferenceArray();

	// Assignment
	ReferenceArray<DATA_T>& operator=(const ReferenceArray<DATA_T>& other);       
	ReferenceArray<DATA_T>& operator=(DATA_T* other);

	// Dereferencing
	operator DATA_T*() const {return m_pData->pData;}
	DATA_T& operator[](unsigned int offset) const {return m_pData->pData[offset];}
	DATA_T& operator[](int offset) const {return m_pData->pData[offset];}

protected:
	// Internal methods
	void Alloc();				// Allocate the m_pData
	void Release();				// Releases a reference count (possibly freeing memory)

protected:
	// Member data
	ReferenceArrayBlock<DATA_T>* m_pData;
};


template<class DATA_T>
ReferenceArray<DATA_T>::ReferenceArray()
{
	Alloc();
}

template<class DATA_T>
ReferenceArray<DATA_T>::ReferenceArray(DATA_T* other)
{
	Alloc();
	m_pData->pData = other;
	m_pData->ReferenceCount = 1;
}

template<class DATA_T>
ReferenceArray<DATA_T>::ReferenceArray(const ReferenceArray<DATA_T>& other)
{
	m_pData = other.m_pData;
	m_pData->ReferenceCount++;
}

template<class DATA_T>
ReferenceArray<DATA_T>::~ReferenceArray()
{
	Release();
}

template<class DATA_T>
ReferenceArray<DATA_T>& ReferenceArray<DATA_T>::operator=(const ReferenceArray<DATA_T>& other)
{
	if (m_pData != other.m_pData)
	{
		Release();
		m_pData = other.m_pData;
		m_pData->ReferenceCount++;
	}
	return *this;
}

template<class DATA_T>
ReferenceArray<DATA_T>& ReferenceArray<DATA_T>::operator=(DATA_T* other)
{
	if (m_pData->ReferenceCount.dec() <= 0)
	{
		delete[] m_pData->pData;
		m_pData->pData = other;
		m_pData->ReferenceCount = 1;
	}
	else
	{
		Alloc();
		m_pData->pData = other;
	}
	return *this;
}

template<class DATA_T>
void ReferenceArray<DATA_T>::Alloc()
{
	m_pData = new ReferenceArrayBlock<DATA_T>;
	m_pData->ReferenceCount = 1;
	m_pData->pData = NULL;
}

template<class DATA_T>
void ReferenceArray<DATA_T>::Release()
{
	assert(m_pData != NULL);
	if (m_pData->ReferenceCount.dec() <= 0)
	{
		delete[] m_pData->pData;
		m_pData->pData = NULL;
		delete m_pData;
		m_pData = NULL;
	}
}

#endif // !defined(AFX_ReferenceArray_H__BAEBCE9D_CD68_40AF_8A54_B23A0D14E807__INCLUDED_)

