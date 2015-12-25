// Copyright (C) 2000  Sean Cavanaugh
// This file is licensed under the terms of the Lesser GNU Public License
// (see LPGL.txt, or http://www.gnu.org/copyleft/lesser.txt)

#if !defined(AFX_ReferencePtr_H__BAEBCE9D_CD68_40AF_8A54_B23A0D14E807__INCLUDED_)
#define AFX_ReferencePtr_H__BAEBCE9D_CD68_40AF_8A54_B23A0D14E807__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif
#include "assert.h"
#include "limits.h"

#ifdef SYSTEM_POSIX
#ifdef HAVE_STDDEF_H
// For NULL
#include <stddef.h>
#endif
#endif

#include "ReferenceCounter.h"

/*!
  \author  Sean Cavanaugh
  \email   sean@dimensionalrift.com
  \cvsauthor $Author: sean $
  \date    $Date: 2000/09/11 20:28:24 $
  \version $Revision: 1.1 $ 
  \brief   ReferencePtr is a simplified ReferencePtr, primiarily for use with reference counted pointers.
	 Its purpose is solely for simplified garbage collection, and not any kind of advanced
	 copy on write functionality.  Passing a normal pointer to this class effectively starts
	 its reference count at one and should not be manually deleted. (But it may be referenced
	 as long as you know the object still exists in some scope somewhere)
*/
template<class DATA_T>
class ReferencePtrBlock
{
public:
	DATA_T*			pData;			    // User defined data block
	mutable ReferenceCounter ReferenceCount;
};


template<class DATA_T>
class ReferencePtr  
{
public:
	// Construction
	ReferencePtr();
	ReferencePtr(DATA_T* other);
	ReferencePtr(const ReferencePtr<DATA_T>& other);
	virtual ~ReferencePtr();

	// Assignment
	ReferencePtr<DATA_T>& operator=(const ReferencePtr<DATA_T>& other);
	ReferencePtr<DATA_T>& operator=(DATA_T* other);

	// Dereferencing
	operator DATA_T*() const {return m_pData->pData;}
	DATA_T* operator->() const {return m_pData->pData;}

protected:
	// Internal methods
	void Alloc();				// Allocate the m_pData
	void Release();				// Releases a reference count (possibly freeing memory)

protected:
	// Member data
	ReferencePtrBlock<DATA_T>* m_pData;
};


template<class DATA_T>
ReferencePtr<DATA_T>::ReferencePtr()
{
	Alloc();
}

template<class DATA_T>
ReferencePtr<DATA_T>::ReferencePtr(DATA_T* other)
{
	Alloc();
	m_pData->pData = other;
	m_pData->ReferenceCount = 1;
}

template<class DATA_T>
ReferencePtr<DATA_T>::ReferencePtr(const ReferencePtr<DATA_T>& other)
{
	m_pData = other.m_pData;
	m_pData->ReferenceCount++;
}

template<class DATA_T>
ReferencePtr<DATA_T>::~ReferencePtr()
{
	Release();
}

template<class DATA_T>
ReferencePtr<DATA_T>& ReferencePtr<DATA_T>::operator=(const ReferencePtr<DATA_T>& other)
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
ReferencePtr<DATA_T>& ReferencePtr<DATA_T>::operator=(DATA_T* other)
{
	if (m_pData->ReferenceCount.dec() <= 0)
	{
		delete m_pData->pData;
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
void ReferencePtr<DATA_T>::Alloc()
{
	m_pData = new ReferencePtrBlock<DATA_T>;
	m_pData->ReferenceCount = 1;
	m_pData->pData = NULL;
}

template<class DATA_T>
void ReferencePtr<DATA_T>::Release()
{
	assert(m_pData != NULL);
	if (m_pData->ReferenceCount.dec() <= 0)
	{
		delete m_pData->pData;
		m_pData->pData = NULL;
		delete m_pData;
		m_pData = NULL;
	}
}

#endif // !defined(AFX_ReferencePtr_H__BAEBCE9D_CD68_40AF_8A54_B23A0D14E807__INCLUDED_)

