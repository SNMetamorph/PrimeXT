#include "extdll.h"
#include "util.h"
#include "cbase.h"

//=======================================================================================================================
// diffusion - teleports entity regardless of its position (no teleport brush needed) - this entity also acts as a target
//=======================================================================================================================

#define PT_KEEP_VELOCITY	BIT(0) // keep the velocity of the entity
#define PT_KEEP_ANGLES		BIT(1) // don't snap to new angles
#define PT_REMOVEONFIRE		BIT(2)

// UNDONE: redirect player's velocity to entity's view origin?

class CTriggerTeleportPoint : public CBaseDelay
{
	DECLARE_CLASS(CTriggerTeleportPoint, CBaseDelay);
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void KeyValue(KeyValueData* pkvd);
	int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

	string_t m_entity;
};
