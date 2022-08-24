#ifndef NX_INTERFACE_H
#define NX_INTERFACE_H
/*----------------------------------------------------------------------------*\
|
|					Public Interface to NVIDIA PhysX Technology
|
|							     www.nvidia.com
|
\*----------------------------------------------------------------------------*/

enum NxInterfaceType
{
	NX_INTERFACE_STATS,
	NX_INTERFACE_LAST
};


class NxInterface
{
public:
  virtual int getVersionNumber(void) const = 0;
  virtual NxInterfaceType getInterfaceType(void) const = 0;

protected:
  virtual ~NxInterface(){};
};

#endif
//NVIDIACOPYRIGHTBEGIN
///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010 NVIDIA Corporation
// All rights reserved. www.nvidia.com
///////////////////////////////////////////////////////////////////////////
//NVIDIACOPYRIGHTEND

