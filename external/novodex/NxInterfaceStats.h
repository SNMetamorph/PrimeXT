#ifndef NX_INTERFACE_STATS_H
#define NX_INTERFACE_STATS_H
/*----------------------------------------------------------------------------*\
|
|					Public Interface to NVIDIA PhysX Technology
|
|							     www.nvidia.com
|
\*----------------------------------------------------------------------------*/

#include "NxInterface.h"

#define NX_INTERFACE_STATS_VERSION 100

class NxInterfaceStats : public NxInterface
{
public:
  virtual int             getVersionNumber(void) const { return NX_INTERFACE_STATS_VERSION; };
  virtual NxInterfaceType getInterfaceType(void) const { return NX_INTERFACE_STATS; };
	virtual bool	          getHeapSize(int &used,int &unused) = 0;

};

#endif
//NVIDIACOPYRIGHTBEGIN
///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010 NVIDIA Corporation
// All rights reserved. www.nvidia.com
///////////////////////////////////////////////////////////////////////////
//NVIDIACOPYRIGHTEND

