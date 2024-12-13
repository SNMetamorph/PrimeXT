#include "weapon_logic_funcs_impl.h"
#include "gamerules.h"

CWeaponLogicFuncsImpl::CWeaponLogicFuncsImpl(CBasePlayerWeapon *weaponEntity) :
	m_pWeapon(weaponEntity)
{
}

const char *CWeaponLogicFuncsImpl::GetWeaponClassname()
{
	return STRING(m_pWeapon->pev->classname);
}

int CWeaponLogicFuncsImpl::GetWeaponBodygroup()
{
	return m_pWeapon->pev->body;
}

bool CWeaponLogicFuncsImpl::GetNextBestWeapon()
{
	return g_pGameRules->GetNextBestWeapon( m_pWeapon->m_pPlayer, m_pWeapon );
}
