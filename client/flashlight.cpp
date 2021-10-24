#include "flashlight.h"
#include "gl_local.h"
#include "pmtrace.h"
#include "enginecallback.h"
#include "stringlib.h"
#include "gl_cvars.h"

CPlayerFlashlight &g_PlayerFlashlight = CPlayerFlashlight::Instance();

CPlayerFlashlight &CPlayerFlashlight::Instance()
{
	static CPlayerFlashlight instance;
	return instance;
}

void CPlayerFlashlight::TurnOn()
{
    m_Active = true;
}

void CPlayerFlashlight::TurnOff()
{
    m_Active = false;
}

void CPlayerFlashlight::Update(ref_params_t *pparams)
{
	if (!m_Active)
		return;

	static float add = 0.0f;
	float addideal = 0.0f;
	pmtrace_t ptr;

	Vector origin = pparams->vieworg + Vector(0.0f, 0.0f, 6.0f) + (pparams->right * 5.0f) + (pparams->forward * 2.0f);
	Vector vecEnd = origin + (pparams->forward * 700.0f);

	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(origin, vecEnd, PM_NORMAL, -1, &ptr);
	const char *texName = gEngfuncs.pEventAPI->EV_TraceTexture(ptr.ent, origin, vecEnd);

	if (ptr.fraction < 1.0f)
		addideal = (1.0f - ptr.fraction) * 30.0f;
	float speed = (add - addideal) * 10.0f;
	if (speed < 0) speed *= -1.0f;

	if (add < addideal)
	{
		add += pparams->frametime * speed;
		if (add > addideal) add = addideal;
	}
	else if (add > addideal)
	{
		add -= pparams->frametime * speed;
		if (add < addideal) add = addideal;
	}

	CDynLight *flashlight = CL_AllocDlight(FLASHLIGHT_KEY);

	R_SetupLightParams(flashlight, origin, pparams->viewangles, 700.0f, 35.0f + add, LIGHT_SPOT);
	R_SetupLightTexture(flashlight, tr.flashlightTexture);

	flashlight->color = Vector(100.4f, 100.4f, 100.4f); // make model dymanic lighting happy
	flashlight->die = pparams->time + 0.05f;

	if (texName && (!Q_strnicmp(texName, "reflect", 7) || !Q_strnicmp(texName, "mirror", 6)) && r_allow_mirrors->value)
	{
		CDynLight *flashlight2 = CL_AllocDlight(FLASHLIGHT_KEY + 1);
		float d = 2.0f * DotProduct(pparams->forward, ptr.plane.normal);
		Vector angles, dir = (pparams->forward - ptr.plane.normal) * d;
		VectorAngles(dir, angles);

		R_SetupLightParams(flashlight2, ptr.endpos, angles, 700.0f, 35.0f + add, LIGHT_SPOT);
		R_SetupLightTexture(flashlight2, tr.flashlightTexture);
		flashlight2->color = Vector(1.4f, 1.4f, 1.4f); // make model dymanic lighting happy
		flashlight2->die = pparams->time + 0.01f;
	}
}
