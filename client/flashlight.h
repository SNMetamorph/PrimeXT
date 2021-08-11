#pragma once
#include "ref_params.h"

class CPlayerFlashlight
{
public:
    static CPlayerFlashlight &Instance();
    void TurnOn();
    void TurnOff();
    void Update(ref_params_t *ref);

private:
    bool m_Active = false;
};

extern CPlayerFlashlight &g_PlayerFlashlight;
