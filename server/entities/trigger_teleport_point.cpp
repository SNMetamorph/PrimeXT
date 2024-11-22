#include "trigger_teleport_point.h"
#include "player.h"
#include "monsters.h"

LINK_ENTITY_TO_CLASS( trigger_teleport_point, CTriggerTeleportPoint );

BEGIN_DATADESC( CTriggerTeleportPoint )
	DEFINE_KEYFIELD( m_entity, FIELD_STRING, "entityname" ),
END_DATADESC();

void CTriggerTeleportPoint::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "entityname"))
	{
		m_entity = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CTriggerTeleportPoint::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator))
		return;

	if (!pActivator || !pActivator->IsPlayer())
		pActivator = CBaseEntity::Instance(INDEXENT(1));

	CBasePlayer* pPlayer = (CBasePlayer*)pActivator;
	CBaseEntity* pEntity = NULL;

	if (FStrEq(STRING(m_entity), "player") || !m_entity)
		pEntity = pPlayer;
	else
		pEntity = UTIL_FindEntityByTargetname(pEntity, STRING(m_entity));

	if (!pEntity)
	{
		ALERT(at_error, "trigger_teleport_point \"%s\" can't find specified entity!\n", STRING(pev->targetname));
		return;
	}

	// this entity acts as a teleport target
	Vector targetOrg = GetAbsOrigin();
	Vector targetAng = GetAbsAngles();

	Vector entityVelocity = g_vecZero;
	if (FBitSet(pev->spawnflags, PT_KEEP_VELOCITY)) {
		entityVelocity = pEntity->GetAbsVelocity();
	}

	Vector entityAngles = targetAng;
	if (FBitSet(pev->spawnflags, PT_KEEP_ANGLES)) {
		entityAngles = pEntity->GetAbsAngles();
	}

	// offset to correctly teleport the player...
	if (pEntity->IsPlayer()) {
		targetOrg.z += 36;
	}

	pEntity->Teleport(&targetOrg, &entityAngles, &entityVelocity);

	UTIL_FireTargets(pev->target, pActivator, pCaller, USE_TOGGLE, 0);

	if (FBitSet(pev->spawnflags, PT_REMOVEONFIRE)) {
		UTIL_Remove(this);
	}
}
