/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "ehandle.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"

edict_t *EHANDLE::Get(void)
{
	if (m_pent)
	{
#if 0
		// keep client entity always in actual state
		if (ENTINDEX(m_pent) == 1)
			m_serialnumber = m_pent->serialnumber;
#endif
		if (m_pent->serialnumber == m_serialnumber)
			return m_pent;
	}

	return NULL;
}

edict_t *EHANDLE::Set(edict_t *pent)
{
	m_pent = pent;
	if (pent)
		m_serialnumber = m_pent->serialnumber;
	return pent;
}

CBaseEntity *EHANDLE::GetPointer()
{
	return (CBaseEntity *)GET_PRIVATE(Get());
}

EHANDLE::operator CBaseEntity*(void)
{
	return (CBaseEntity *)GET_PRIVATE(Get());
}

CBaseEntity *EHANDLE::operator=(CBaseEntity *pEntity)
{
	if (pEntity)
	{
		m_pent = ENT(pEntity);
		if (m_pent)
			m_serialnumber = m_pent->serialnumber;
	}
	else
	{
		m_pent = NULL;
		m_serialnumber = 0;
	}
	return pEntity;
}

EHANDLE::operator int(void)
{
	return Get() != NULL;
}

CBaseEntity *EHANDLE::operator->(void)
{
	return (CBaseEntity *)GET_PRIVATE(Get());
}
