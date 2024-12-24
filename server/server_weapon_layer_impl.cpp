#include "server_weapon_layer_impl.h"
#include "gamerules.h"

CServerWeaponLayerImpl::CServerWeaponLayerImpl(CBasePlayerWeapon *weaponEntity) :
	m_pWeapon(weaponEntity)
{
}

int CServerWeaponLayerImpl::GetWeaponBodygroup()
{
	return m_pWeapon->pev->body;
}

bool CServerWeaponLayerImpl::GetNextBestWeapon()
{
	return g_pGameRules->GetNextBestWeapon( m_pWeapon->m_pPlayer, m_pWeapon );
}

int CServerWeaponLayerImpl::GetPlayerAmmo(int ammoType)
{
	return m_pWeapon->m_pPlayer->m_rgAmmo[ammoType];
}

void CServerWeaponLayerImpl::SetPlayerAmmo(int ammoType, int count)
{
	m_pWeapon->m_pPlayer->m_rgAmmo[ammoType] = count;
}

void CServerWeaponLayerImpl::SetPlayerWeaponAnim(int anim)
{
	m_pWeapon->m_pPlayer->pev->weaponanim = anim;
}

void CServerWeaponLayerImpl::SetPlayerViewmodel(int model)
{
	m_pWeapon->m_pPlayer->pev->viewmodel = model;
}

bool CServerWeaponLayerImpl::CheckPlayerButtonFlag(int buttonMask)
{
	return FBitSet(m_pWeapon->m_pPlayer->pev->button, buttonMask);
}

void CServerWeaponLayerImpl::ClearPlayerButtonFlag(int buttonMask)
{
	ClearBits(m_pWeapon->m_pPlayer->pev->button, buttonMask);
}

float CServerWeaponLayerImpl::GetPlayerNextAttackTime()
{
	return m_pWeapon->m_pPlayer->m_flNextAttack; 
}

void CServerWeaponLayerImpl::SetPlayerNextAttackTime(float value)
{
	m_pWeapon->m_pPlayer->m_flNextAttack = GetWeaponTimeBase() + value;
}

float CServerWeaponLayerImpl::GetWeaponTimeBase()
{
	return 0.0f; // zero because we're using predicting
}
