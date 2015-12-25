// Copyright (C) 2000  Sean Cavanaugh
// This file is licensed under the terms of the Lesser GNU Public License
// (see LPGL.txt, or http://www.gnu.org/copyleft/lesser.txt)

#if !defined(AFX_REFERENCEOBJECT_H__BAEBCE9D_CD68_40AF_8A54_B23A0D14E807__INCLUDED_)
#define AFX_REFERENCEOBJECT_H__BAEBCE9D_CD68_40AF_8A54_B23A0D14E807__INCLUDED_

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

template<class HEADER_T, class DATA_T>
class ReferenceObjectBlock
{
public:
	DATA_T*			pData;			// User defined data block
	mutable ReferenceCounter	ReferenceCount;
	unsigned int	AllocLength;	// Current size of array, in T sized units.  if AllocLength = 0, pData is const and needs CopyOnWrite
	HEADER_T		Header;			// User defined header block (must be safe to copy with operator=)
};



/*!
  \author  Sean Cavanaugh
  \email   sean@dimensionalrift.com
  \cvsauthor $Author: sean $
  \date    $Date: 2000/09/11 20:28:24 $
  \version $Revision: 1.1 $ 
  \brief   ReferenceObject is a template class designed to be used a base class for pass-by-reference objects, to that things such as returning objects up the stack efficiently are possible.
  \bug     EVERY non const function needs its very first operation to be CopyForWrite()
  \bug     EVERY derived class should define its operator=() to call the ReferenceObject base class version (notice this operator is protected,)
  \bug     HEADER_T will probably need an operator=() defined for all but the most trivial types
  \bug     Store objects or simple data types, NO POINTERS (use ReferencePtrObject for that) (they will leak like mad)

*/



template<class HEADER_T, class DATA_T>
class ReferenceObject  
{
public:
	ReferenceObject();
	ReferenceObject(unsigned int size);
	ReferenceObject(const ReferenceObject<HEADER_T,DATA_T>& other);
	virtual ~ReferenceObject();

public:
	HEADER_T& getHeader();
	DATA_T* getData();

	ReferenceObject<HEADER_T,DATA_T>& operator=(const ReferenceObject<HEADER_T,DATA_T>& other);

protected:
	void AllocBlock(unsigned int size);	// Allocate Header+Data Block (size in T units)
	void AllocHeader();			// Allocate Header Block
	void AllocData(unsigned int size);	// Alocate Data Block (size in T units)

	virtual void Release();				// Releases a reference count (possibly freeing memory)
	virtual void InitHeader();			// User defined Header Initialization function
	virtual void FreeHeader();			// User defined Header destructor function
	virtual void CopyForWrite();					// Make unique copy for writing, must first instruction in all non const-functions in derived classes
	virtual void CopyForWrite(unsigned int size);	// same as CopyForWrite() except takes a resize parameter

	ReferenceObjectBlock<HEADER_T,DATA_T>* m_pData;
};


template<class HEADER_T, class DATA_T>
ReferenceObject<HEADER_T,DATA_T>::ReferenceObject()
{
	m_pData = NULL;
	AllocHeader();
	InitHeader();
}

template<class HEADER_T, class DATA_T>
ReferenceObject<HEADER_T,DATA_T>::ReferenceObject(unsigned int size)
{
	m_pData = NULL;
	AllocBlock(size);
}

template<class HEADER_T, class DATA_T>
ReferenceObject<HEADER_T,DATA_T>::ReferenceObject(const ReferenceObject<HEADER_T,DATA_T>& other)
{
	m_pData = other.m_pData;
	m_pData->ReferenceCount++;
}

template<class HEADER_T, class DATA_T>
ReferenceObject<HEADER_T,DATA_T>::~ReferenceObject()
{
	Release();
}

template<class HEADER_T, class DATA_T>
ReferenceObject<HEADER_T,DATA_T>& ReferenceObject<HEADER_T,DATA_T>::operator=(const ReferenceObject<HEADER_T,DATA_T>& other)
{
	if (m_pData != other.m_pData)
	{
		Release();
		m_pData = other.m_pData;
		m_pData->ReferenceCount++;
	}
	return *this;
}

template<class HEADER_T, class DATA_T>
void ReferenceObject<HEADER_T,DATA_T>::Release()
{
	assert(m_pData != NULL);
	if (m_pData->ReferenceCount.dec() <= 0)
	{
		FreeHeader();
		delete[] m_pData->pData;
		delete m_pData;
		m_pData = NULL;
	}
}

template<class HEADER_T, class DATA_T>
void ReferenceObject<HEADER_T,DATA_T>::AllocBlock(unsigned int size)
{
	AllocHeader();
	AllocData(size);
	InitHeader();	// Initialize user defined header
}

template<class HEADER_T, class DATA_T>
void ReferenceObject<HEADER_T,DATA_T>::AllocHeader()
{
	m_pData = new ReferenceObjectBlock<HEADER_T,DATA_T>;

	m_pData->ReferenceCount = 1;
	m_pData->pData = NULL;
	m_pData->AllocLength = 0;
}

template<class HEADER_T, class DATA_T>
void ReferenceObject<HEADER_T,DATA_T>::AllocData(unsigned int size)
{
	assert(m_pData != NULL);
	assert((size * sizeof(DATA_T)) < INT_MAX);

	if (size)
	{
		m_pData->pData = new DATA_T[size];
	}
	else
	{
		m_pData->pData = NULL;
	}
	m_pData->AllocLength = size;
}

template<class HEADER_T, class DATA_T>
void ReferenceObject<HEADER_T,DATA_T>::InitHeader()
{
	// NOTE: Derive this function to initialize the Header object(s)
}

template<class HEADER_T, class DATA_T>
void ReferenceObject<HEADER_T,DATA_T>::FreeHeader()
{
	// NOTE: Derive this function to clean up the Header object(s)
}

template<class HEADER_T, class DATA_T>
void ReferenceObject<HEADER_T,DATA_T>::CopyForWrite()
{
	CopyForWrite(m_pData->AllocLength);
}

template<class HEADER_T, class DATA_T>
void ReferenceObject<HEADER_T,DATA_T>::CopyForWrite(unsigned int size)
{
	unsigned int oldsize = m_pData->AllocLength;

	if (size)
	{
		if ((m_pData->ReferenceCount > 1) || (size != oldsize))
		{
			ReferenceObjectBlock<HEADER_T,DATA_T>* pTmp = m_pData;
			AllocBlock(size);
			m_pData->Header = pTmp->Header;
			unsigned int sizetocopy = min(size,oldsize);
			// memcpy(m_pData->pData, pTmp->pData, min(size,oldsize) * sizeof(DATA_T));  // memcpy not safe for objects
			for (unsigned int x=0;x<sizetocopy;x++)
			{
				m_pData->pData[x] = pTmp->pData[x];
			}
			if (pTmp->ReferenceCount.dec() <= 0)
			{
				delete[] pTmp->pData;
				delete pTmp;
			}
		}
	}
	else	// Replace reference to a null object (since size is zero)
	{
		Release();
		AllocHeader();
		InitHeader();
	}
}

template<class HEADER_T, class DATA_T>
HEADER_T& ReferenceObject<HEADER_T,DATA_T>::getHeader()
{
	CopyForWrite();
	return m_pData->Header;
}

template<class HEADER_T, class DATA_T>
DATA_T* ReferenceObject<HEADER_T,DATA_T>::getData()
{
	CopyForWrite();
	return m_pData->pData;
}

#endif // !defined(AFX_REFERENCEOBJECT_H__BAEBCE9D_CD68_40AF_8A54_B23A0D14E807__INCLUDED_)

