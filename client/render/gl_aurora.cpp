// 02/08/02 November235: Particle System
#include "hud.h"
#include "utils.h"
#include "const.h"
#include <stringlib.h>
#include "entity_state.h"
#include "event_api.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "com_model.h"
#include "pmtrace.h" // for contents and traceline
#include "pm_defs.h"
#include "gl_local.h"
#include "gl_studio.h"
#include "gl_aurora.h"
#include "gl_cvars.h"

CParticleSystemManager	g_pParticleSystems; // buz - static single object

void UTIL_CreateAurora( cl_entity_t *ent, const char *file, int attachment, float lifetime )
{
	if( !g_fRenderInitialized )
		return;

	int iCompare;

	// verify file exists
	// g-cont. idea! use COMPARE_FILE_TIME instead of LOAD_FILE
	if( !g_fRenderInitialized || COMPARE_FILE_TIME( file, file, &iCompare ))
	{
		CParticleSystem *pSystem = new CParticleSystem( ent, file, attachment, lifetime );
		g_pParticleSystems.AddSystem( pSystem );
	}
	else
	{
		ALERT( at_error, "CreateAurora: couldn't load %s\n", file );
	}
}

void UTIL_RemoveAurora( cl_entity_t *ent )
{
	if( !g_fRenderInitialized )
		return;

	CParticleSystem *pSystem = g_pParticleSystems.FindSystem( NULL, ent );

	// find all the partsystems that attached with this entity
	while( pSystem != NULL )
	{
		g_pParticleSystems.MarkSystemForDeletion( pSystem );
		pSystem = g_pParticleSystems.FindSystem( pSystem, ent );
	}
}

CParticleSystemManager :: CParticleSystemManager( void )
{
	m_pFirstSystem = NULL;
}

CParticleSystemManager :: ~CParticleSystemManager( void )
{
	ClearSystems();
}

void CParticleSystemManager :: AddSystem( CParticleSystem *pNewSystem )
{
	pNewSystem->m_pNextSystem = m_pFirstSystem;
	m_pFirstSystem = pNewSystem;
}

CParticleSystem *CParticleSystemManager :: FindSystem( CParticleSystem *pFirstSystem, cl_entity_t *pEntity )
{
	CParticleSystem *pSys;

	if( pFirstSystem != NULL )
		pSys = pFirstSystem->m_pNextSystem;
	else pSys = m_pFirstSystem;
	
	while( pSys != NULL )
	{
		if( pEntity == pSys->m_pEntity )
			return pSys;
		pSys = pSys->m_pNextSystem;
	}

	return NULL;
}

void CParticleSystemManager :: MarkSystemForDeletion( CParticleSystem *pSys )
{
	// parent entity is removed from server.
	pSys->MarkForDeletion();
	pSys->m_pEntity = NULL;
}

// blended particles don't use the z-buffer, so we need to sort them before drawing.
// for efficiency, only the systems are sorted - individual particles just get drawn in order of creation.
// (this should actually make things look better - no ugly popping when one particle passes through another.)
void CParticleSystemManager :: SortSystems( void )
{
	CParticleSystem *pSystem, *pLast;
	CParticleSystem *pBeforeCompare, *pCompare;
	
	if( !m_pFirstSystem ) return;

	// calculate how far away each system is from the viewer
	for( pSystem = m_pFirstSystem; pSystem; pSystem = pSystem->m_pNextSystem )
		pSystem->CalculateDistance();

	// do an insertion sort on the systems
	pLast = m_pFirstSystem;
	pSystem = pLast->m_pNextSystem;

	while( pSystem )
	{
		if( pLast->m_fViewerDist < pSystem->m_fViewerDist )
		{
			// pSystem is in the wrong place! First, let's unlink it from the list
			pLast->m_pNextSystem = pSystem->m_pNextSystem;

			// then find somewhere to insert it
			if( m_pFirstSystem == pLast || m_pFirstSystem->m_fViewerDist < pSystem->m_fViewerDist )
			{
				// pSystem comes before the first system, insert it there
				pSystem->m_pNextSystem = m_pFirstSystem;
				m_pFirstSystem = pSystem;
			}
			else
			{
				// insert pSystem somewhere within the sorted part of the list
				pBeforeCompare = m_pFirstSystem;
				pCompare = pBeforeCompare->m_pNextSystem;

				while( pCompare != pLast )
				{
					if( pCompare->m_fViewerDist < pSystem->m_fViewerDist )
					{
						// pSystem comes before pCompare. We've found where it belongs.
						break;
					}

					pBeforeCompare = pCompare;
					pCompare = pBeforeCompare->m_pNextSystem;
				}

				// we've found where pSystem belongs. Insert it between pBeforeCompare and pCompare.
				pBeforeCompare->m_pNextSystem = pSystem;
				pSystem->m_pNextSystem = pCompare;
			}
		}
		else
		{
			// pSystem is in the right place, move on
			pLast = pSystem;
		}

		pSystem = pLast->m_pNextSystem;
	}
}

void CParticleSystemManager :: UpdateSystems( void )
{
	AURSTATE		state;

	if( FBitSet( RI->params, ( RP_ENVVIEW|RP_SKYVIEW )))
		return;

	CParticleSystem *pSystem;
	CParticleSystem *pLast = NULL;
	pSystem = m_pFirstSystem;

	while( pSystem )
	{
		state = pSystem->UpdateSystem( tr.frametime );

		if( state != AURORA_REMOVE )
		{
			if( state == AURORA_DRAW )
				pSystem->DrawSystem();

			pLast = pSystem;
			pSystem = pSystem->m_pNextSystem;
		}
		else
		{
			// delete this system
			if( pLast )
			{
				pLast->m_pNextSystem = pSystem->m_pNextSystem;
				delete pSystem;
				pSystem = pLast->m_pNextSystem;
			}
			else
			{
				// deleting the first system
				m_pFirstSystem = pSystem->m_pNextSystem;
				delete pSystem;
				pSystem = m_pFirstSystem;
			}
		}
	}

	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
}

void CParticleSystemManager :: ClearSystems( void )
{
	CParticleSystem *pSystem = m_pFirstSystem;
	CParticleSystem *pTemp;

	while( pSystem )
	{
		pTemp = pSystem->m_pNextSystem;
		delete pSystem;
		pSystem = pTemp;
	}

	m_pFirstSystem = NULL;
}

CParticleType :: CParticleType( CParticleType *pNext )
{
	m_pSprayType = m_pOverlayType = NULL;
	m_StartAngle = RandomRange( 45.0f );
	m_pNext = pNext;
	m_szName[0] = 0;
	m_hSprite = 0;

	m_StartRed = m_StartGreen = m_StartBlue = m_StartAlpha = RandomRange( 1.0f );
	m_EndRed = m_EndGreen = m_EndBlue = m_EndAlpha = RandomRange( 1.0f );

	m_iRenderMode = kRenderTransAdd;
	m_iDrawCond = CONTENTS_NONE;
	m_bIsDefined = false;
	m_bEndFrame = false;
	m_bBouncing = false;
}

CParticle* CParticleType :: CreateParticle( CParticleSystem *pSys )
{
	if( !pSys ) return NULL;

	CParticle *pPart = pSys->ActivateParticle();
	if( !pPart ) return NULL;

	pPart->age = 0.0f;
	pPart->age_death = m_Life.GetInstance();

	InitParticle( pPart, pSys );

	return pPart;
}

void CParticleType :: InitParticle( CParticle *pPart, CParticleSystem *pSys )
{
	float fLifeRecip;

	if( pPart->age_death > 0.0f )
		fLifeRecip = 1.0f / pPart->age_death;
	else fLifeRecip = 1.0f;

	pPart->pType = this;

	pPart->velocity = pPart->origin = g_vecZero;
	pPart->accel.x = pPart->accel.y = 0.0f;
	pPart->accel.z = m_Gravity.GetInstance();
	pPart->m_pEntity = NULL;

	CParticle *pOverlay = NULL;

	if( m_pOverlayType )
	{
		// create an overlay for this particle
		pOverlay = pSys->ActivateParticle();

		if( pOverlay )
		{
			pOverlay->age = pPart->age;
			pOverlay->age_death = pPart->age_death;
			m_pOverlayType->InitParticle( pOverlay, pSys );
		}
	}

	pPart->m_pOverlay = pOverlay;

	if( m_pSprayType )
		pPart->age_spray = 1.0f / m_SprayRate.GetInstance();
	else pPart->age_spray = 0.0f;

	pPart->m_fSize = m_StartSize.GetInstance();

	if( m_EndSize.IsDefined( ))
		pPart->m_fSizeStep = m_EndSize.GetOffset( pPart->m_fSize ) * fLifeRecip;
	else pPart->m_fSizeStep = m_SizeDelta.GetInstance();

	pPart->frame = m_StartFrame.GetInstance();

	if( m_EndFrame.IsDefined( ))
		pPart->m_fFrameStep = m_EndFrame.GetOffset( pPart->frame ) * fLifeRecip;
	else pPart->m_fFrameStep = m_FrameRate.GetInstance();

	pPart->m_fAlpha = m_StartAlpha.GetInstance();
	pPart->m_fAlphaStep = m_EndAlpha.GetOffset( pPart->m_fAlpha ) * fLifeRecip;
	pPart->m_fRed = m_StartRed.GetInstance();
	pPart->m_fRedStep = m_EndRed.GetOffset( pPart->m_fRed ) * fLifeRecip;
	pPart->m_fGreen = m_StartGreen.GetInstance();
	pPart->m_fGreenStep = m_EndGreen.GetOffset( pPart->m_fGreen ) * fLifeRecip;
	pPart->m_fBlue = m_StartBlue.GetInstance();
	pPart->m_fBlueStep = m_EndBlue.GetOffset( pPart->m_fBlue ) * fLifeRecip;

	pPart->m_fAngle = m_StartAngle.GetInstance();
	pPart->m_fAngleStep = m_AngleDelta.GetInstance();

	pPart->m_fDrag = m_Drag.GetInstance();

	float fWindStrength = m_WindStrength.GetInstance();
	float fWindYaw = m_WindYaw.GetInstance();

	if( fWindStrength != 0 )
	{
		if( fWindYaw == 0 )
		{
			// angle = 0, sin 0, cos 1
			pPart->m_vecWind.x = 1.0f;
			pPart->m_vecWind.y = 0.0f;
			pPart->m_vecWind.z = 0.0f;
		}
		else
		{
                              float fSinWindYaw = CParticleSystem :: CosLookup( fWindYaw );
			float fCosWindYaw = CParticleSystem :: SinLookup( fWindYaw );

			pPart->m_vecWind.x = fCosWindYaw;
			pPart->m_vecWind.y = fSinWindYaw;
			pPart->m_vecWind.z = 0;
		}

		// rotate wind vector into world space
		pPart->m_vecWind = pSys->entityMatrix.VectorRotate( pPart->m_vecWind ) * fWindStrength;
	}
	else
	{
		pPart->m_vecWind = g_vecZero;
	}
}

float CParticleSystem :: c_fCosTable[360 + 90];
bool CParticleSystem :: c_bCosTableInit = false;

//============================================
CParticleSystem :: CParticleSystem( cl_entity_t *ent, const char *szFilename, int attachment, float lifetime )
{
	int iParticles = 100; // default
	m_iKillCondition = CONTENTS_NONE;
	m_iEntAttachment = attachment;
	m_pActiveParticle = NULL;
	m_pMainParticle = NULL;
	m_fLifeTime = lifetime;
	m_pNextSystem = NULL;
	m_iLightingModel = 0;
	m_fViewerDist = 0.0f;
	m_pFirstType = NULL;
	m_pEntity = ent;
	enable = true;

	entityMatrix.Identity();
	ClearBounds(m_vecAbsMin, m_vecAbsMax);

	if( !c_bCosTableInit )
	{
		for( int i = 0; i < 360 + 90; i++ )
			c_fCosTable[i] = cos( i * M_PI / 180.0f );
		c_bCosTableInit = true;
	}

	char *afile = (char *)gEngfuncs.COM_LoadFile( (char *)szFilename, 5, NULL );
	char szToken[1024];
	char *pfile = afile;

	if( !afile )
	{
		ALERT( at_error, "couldn't load %s.\n", szFilename );
		return;
	}
	else
	{
		pfile = COM_ParseFile( pfile, szToken );

		while( pfile )
		{
			if( !Q_stricmp( szToken, "particles" ))
			{
				pfile = COM_ParseFile( pfile, szToken );
				iParticles = Q_atoi( szToken );
			}
			else if( !Q_stricmp( szToken, "maintype" ))
			{
				pfile = COM_ParseFile( pfile, szToken );
				m_pMainType = AddPlaceholderType( szToken );
			}
			else if( !Q_stricmp( szToken, "attachment" ))
			{
				pfile = COM_ParseFile( pfile, szToken );
				m_iEntAttachment = Q_atoi( szToken );
			}
			else if( !Q_stricmp( szToken, "lightmodel" ))
			{
				pfile = COM_ParseFile( pfile, szToken );
				m_iLightingModel = Q_atoi( szToken );
			}
			else if( !Q_stricmp( szToken, "lifetime" ))
			{
				pfile = COM_ParseFile( pfile, szToken );
				m_fLifeTime = tr.time + Q_atof( szToken );
			}
			else if( !Q_stricmp( szToken, "killcondition" ))
			{
				pfile = COM_ParseFile( pfile, szToken );

				if( !Q_stricmp( szToken, "empty" ))
				{
					m_iKillCondition = CONTENTS_EMPTY;
				}
				else if( !Q_stricmp( szToken, "water" ))
				{
					m_iKillCondition = CONTENTS_WATER;
				}
				else if( !Q_stricmp( szToken, "solid" ))
				{
					m_iKillCondition = CONTENTS_SOLID;
				}
			}
			else if( !Q_stricmp( szToken, "{" ))
			{
				// parse new type
				this->ParseType( pfile );
			}

			pfile = COM_ParseFile( pfile, szToken );
		}
	}
		
	gEngfuncs.COM_FreeFile( afile );
	AllocateParticles( iParticles );
}

void CParticleSystem :: AllocateParticles( int iParticles )
{
	m_pAllParticles = new CParticle[iParticles];
	memset( m_pAllParticles, 0, sizeof( CParticle ) * iParticles );
	m_pFreeParticle = m_pAllParticles;
	m_pActiveParticle = NULL;
	m_pMainParticle = NULL;

	// initialise the linked list
	CParticle *pLast = m_pAllParticles;
	CParticle *pParticle = pLast + 1;

	for( int i = 1; i < iParticles; i++ )
	{
		pLast->nextpart = pParticle;
		pLast = pParticle;
		pParticle++;
	}

	pLast->nextpart = NULL;
}

CParticleSystem :: ~CParticleSystem( void )
{
	delete[] m_pAllParticles;

	CParticleType *pType = m_pFirstType;
	CParticleType *pNext;

	while( pType )
	{
		pNext = pType->m_pNext;
		delete pType;
		pType = pNext;
	}
}

// returns the CParticleType with the given name, if there is one
CParticleType *CParticleSystem :: GetType( const char *szName )
{
	for( CParticleType *pType = m_pFirstType; pType != NULL; pType = pType->m_pNext )
	{
		if( !Q_stricmp( pType->m_szName, szName ))
			return pType;
	}
	return NULL;
}

CParticleType *CParticleSystem :: AddPlaceholderType( const char *szName )
{
	m_pFirstType = new CParticleType( m_pFirstType );
	Q_strncpy( m_pFirstType->m_szName, szName, sizeof( m_pFirstType->m_szName ));

	return m_pFirstType;
}

// creates a new particletype from the given file
// NB: this changes the value of szFile.
CParticleType *CParticleSystem :: ParseType( char *&pfile )
{
	CParticleType *pType = new CParticleType();

	// parse the .aur file
	char szToken[1024];

	pfile = COM_ParseFile( pfile, szToken );

	while( Q_stricmp( szToken, "}" ))
	{
		if( !pfile ) break;

		if( !Q_stricmp( szToken, "name" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			Q_strncpy( pType->m_szName, szToken, sizeof( pType->m_szName ));

			CParticleType *pTemp = GetType( szToken );

			if( pTemp )
			{
				// there's already a type with this name
				if( pTemp->m_bIsDefined )
					ALERT( at_warning, "Particle type %s is defined more than once!\n", szToken );

				// copy all our data into the existing type, throw away the type we were making
				*pTemp = *pType;
				delete pType;
				pType = pTemp;
				pType->m_bIsDefined = true; // record the fact that it's defined, so we won't need to add it to the list
			}
		}
		else if( !Q_stricmp( szToken, "gravity" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_Gravity = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "windyaw" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_WindYaw = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "windstrength" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_WindStrength = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "sprite" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_hSprite = SPR_Load( szToken );
		}
		else if( !Q_stricmp( szToken, "startalpha" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_StartAlpha = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "endalpha" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_EndAlpha = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "startred" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_StartRed = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "endred" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_EndRed = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "startgreen" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_StartGreen = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "endgreen" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_EndGreen = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "startblue" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_StartBlue = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "endblue" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_EndBlue = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "startsize" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_StartSize = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "sizedelta" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_SizeDelta = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "endsize" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_EndSize = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "startangle" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_StartAngle = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "angledelta" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_AngleDelta = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "startframe" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_StartFrame = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "endframe" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_EndFrame = RandomRange( szToken );
			pType->m_bEndFrame = true;
		}
		else if( !Q_stricmp( szToken, "framerate" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_FrameRate = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "lifetime" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_Life = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "spraytype" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			CParticleType *pTemp = GetType( szToken );

			if( pTemp )
				pType->m_pSprayType = pTemp;
			else
				pType->m_pSprayType = AddPlaceholderType( szToken );
		}
		else if( !Q_stricmp( szToken, "overlaytype" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			CParticleType *pTemp = GetType( szToken );

			if( pTemp )
				pType->m_pOverlayType = pTemp;
			else
				pType->m_pOverlayType = AddPlaceholderType( szToken );
		}
		else if( !Q_stricmp( szToken, "sprayrate" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_SprayRate = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "sprayforce" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_SprayForce = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "spraypitch" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_SprayPitch = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "sprayyaw" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_SprayYaw = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "drag" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_Drag = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "bounce" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_Bounce = RandomRange( szToken );

			if( pType->m_Bounce.m_flMin != 0 || pType->m_Bounce.m_flMax != 0 )
				pType->m_bBouncing = true;
		}
		else if( !Q_stricmp( szToken, "bouncefriction" ))
		{
			pfile = COM_ParseFile( pfile, szToken );
			pType->m_BounceFriction = RandomRange( szToken );
		}
		else if( !Q_stricmp( szToken, "rendermode" ))
		{
			pfile = COM_ParseFile( pfile, szToken );

			if( !Q_stricmp( szToken, "additive" ))
			{
				pType->m_iRenderMode = kRenderTransAdd;
			}
			else if( !Q_stricmp( szToken, "solid" ))
			{
				pType->m_iRenderMode = kRenderTransAlpha;
			}
			else if( !Q_stricmp( szToken, "texture" ))
			{
				pType->m_iRenderMode = kRenderTransTexture;
			}
			else if( !Q_stricmp( szToken, "color" ))
			{
				pType->m_iRenderMode = kRenderTransColor;
			}
		}
		else if( !Q_stricmp( szToken, "drawcondition" ))
		{
			pfile = COM_ParseFile( pfile, szToken );

			if( !Q_stricmp( szToken, "empty" ))
			{
				pType->m_iDrawCond = CONTENTS_EMPTY;
			}
			else if( !Q_stricmp( szToken, "water" ))
			{
				pType->m_iDrawCond = CONTENTS_WATER;
			}
			else if( !Q_stricmp( szToken, "solid" ))
			{
				pType->m_iDrawCond = CONTENTS_SOLID;
			}
			else if( !Q_stricmp( szToken, "special" ) || !Q_stricmp( szToken, "special1" ))
			{
				pType->m_iDrawCond = CONTENTS_SPECIAL1;
			}
			else if( !Q_stricmp( szToken, "special2" ))
			{
				pType->m_iDrawCond = CONTENTS_SPECIAL2;
			}
			else if( !Q_stricmp( szToken, "special3" ))
			{
				pType->m_iDrawCond = CONTENTS_SPECIAL3;
			}
			else if( !Q_stricmp( szToken, "spotlight" ))
			{
				pType->m_iDrawCond = CONTENTS_SPOTLIGHT;
			}
		}

		// get the next token
		pfile = COM_ParseFile( pfile, szToken );
	}

	if( !pType->m_bIsDefined )
	{
		// if this is a newly-defined type, we need to add it to the list
		pType->m_pNext = m_pFirstType;
		m_pFirstType = pType;
		pType->m_bIsDefined = true;
	}

	return pType;
}

CParticle *CParticleSystem :: ActivateParticle( void )
{
	CParticle *pActivated = m_pFreeParticle;

	if( pActivated )
	{
		m_pFreeParticle = pActivated->nextpart;
		pActivated->nextpart = m_pActiveParticle;
		m_pActiveParticle = pActivated;
	}

	return pActivated;
}

void CParticleSystem :: MarkForDeletion( void )
{
	if( m_pMainParticle )
	{
		m_pMainParticle->age_death = 0.0f; // die now
		m_pMainParticle = NULL;
	}

	// completely remove for time-based systems
	if( m_fLifeTime != 0.0f ) m_pEntity = NULL;
}

void CParticleSystem :: CalculateDistance( void )
{
	if( !m_pActiveParticle )
		return;

	Vector offset = GetVieworg() - m_pActiveParticle->origin; // just pick one
	m_fViewerDist = DotProduct( offset, offset );
}

AURSTATE CParticleSystem :: UpdateSystem( float frametime )
{
	if( m_pEntity != NULL )
	{
		// don't update if the system is outside the player's PVS.
		if( m_pEntity->curstate.messagenum != r_currentMessageNum )
			return AURORA_INVISIBLE;

		// time-based particle system
		if( m_fLifeTime != 0.0f )
		{
			enable = (m_fLifeTime >= tr.time) ? true : false;
		}
		else
		{
			enable = (m_pEntity->curstate.body) ? true : false;
		}

		// check for contents to remove
		if( m_iKillCondition == POINT_CONTENTS( m_pEntity->origin ))
        {
			m_pEntity = NULL;
          	enable = false;
        }
	}
	else
	{
		enable = false;
	}

	if( m_pEntity != NULL )
	{
		Vector angles = m_pEntity->angles;

		// get the system entity matrix
		if( m_iEntAttachment && m_pEntity->model->type == mod_studio )
			angles = R_StudioAttachmentAng( m_pEntity, m_iEntAttachment - 1 );
		entityMatrix = matrix3x3( angles );
	}

	if( m_pMainParticle == NULL )
	{
		if( enable )
		{
			CParticleType *pType = m_pMainType;

			if( pType )
			{
				m_pMainParticle = pType->CreateParticle( this );

				if( m_pMainParticle )
				{
					// first origin initialize
					if( m_iEntAttachment && m_pEntity->model->type == mod_studio )
						m_pMainParticle->origin = R_StudioAttachmentPos( m_pEntity, m_iEntAttachment - 1 );
					else m_pMainParticle->origin = m_pEntity->origin;

					m_pMainParticle->m_pEntity = m_pEntity;
					m_pMainParticle->age_death = -1.0f; // never die
				}
			}
		}
	}
	else if( !enable )
	{
		MarkForDeletion();
	}

	// last particle is died, allow to remove partsystem
	if( !m_pEntity && !m_pActiveParticle )
		return AURORA_REMOVE;

	CParticle	*pParticle = m_pActiveParticle;
	CParticle	*pLast = NULL;

	if (tr.frametime != 0.0) {
		ClearBounds(m_vecAbsMin, m_vecAbsMax);
	}

	while( pParticle )
	{
		if( UpdateParticle( pParticle, frametime ))
		{
			pLast = pParticle;
			pParticle = pParticle->nextpart;
		}
		else
		{
			// deactivate it
			if( pLast )
			{
				pLast->nextpart = pParticle->nextpart;
				pParticle->nextpart = m_pFreeParticle;
				m_pFreeParticle = pParticle;
				pParticle = pLast->nextpart;
			}
			else
			{
				// deactivate the first CParticle in the list
				m_pActiveParticle = pParticle->nextpart;
				pParticle->nextpart = m_pFreeParticle;
				m_pFreeParticle = pParticle;
				pParticle = m_pActiveParticle;
			}
		}
	}

	if (!Mod_CheckBoxVisible(m_vecAbsMin, m_vecAbsMax)) {
		return AURORA_INVISIBLE; // culled with PVS check
	}
	else {
		return AURORA_DRAW;
	}
}

void CParticleSystem :: DrawSystem( void )
{
	CParticle *pParticle = m_pActiveParticle;

	if( m_pEntity != NULL )
	{
		// don't draw if the system is outside the player's PVS.
		if( m_pEntity->curstate.messagenum != r_currentMessageNum )
			return;
	}

	m_fHasProjectionLighting = HasDynamicLights();

	for( pParticle = m_pActiveParticle; pParticle; pParticle = pParticle->nextpart )
	{
		if( pParticle->m_fSize <= 0 || !ParticleIsVisible( pParticle ))
			continue;
		Vector vLeft = GetVLeft();
		Vector vUp = GetVUp();
		DrawParticle( pParticle, vLeft, vUp);
	}
}

bool CParticleSystem :: ParticleIsVisible( CParticle *part )
{
	if (CVAR_TO_BOOL(r_nocull))
		return true;

	if( R_CullSphere( part->origin, part->m_fSize + 1 ))
		return false;

	return true;
}

bool CParticleSystem :: UpdateParticle( CParticle *part, float frametime )
{
	if( frametime == 0 )
		return true;

	part->age += frametime;

	// is this particle bound to an entity?
	if( part->m_pEntity )
	{
		if( enable )
		{
			if( m_iEntAttachment && m_pEntity->model->type == mod_studio )
			{
				float flSpeed = (R_StudioAttachmentPos( m_pEntity, m_iEntAttachment - 1 ) - part->origin).Length() * frametime;
				part->velocity = entityMatrix.GetForward() * flSpeed;
				part->origin = R_StudioAttachmentPos( m_pEntity, m_iEntAttachment - 1 );
			}
			else
			{
				float flSpeed = (m_pEntity->origin - part->origin).Length() * frametime;
				part->velocity = entityMatrix.GetForward() * flSpeed;
				part->origin = m_pEntity->origin;
			}
		}
		else
		{
			// entity is switched off, die
			return false;
		}
	}
	else
	{
		// not tied to an entity, check whether it's time to die
		if( part->age_death >= 0.0f && part->age > part->age_death )
			return false;

		// apply acceleration and velocity
		if( part->m_fDrag )
			part->velocity += (part->velocity - part->m_vecWind) * (-part->m_fDrag * frametime);

		part->velocity += part->accel * frametime;
		part->origin += part->velocity * frametime;

		if( part->pType->m_bBouncing )
		{
			Vector vecTarget = part->origin + part->velocity * frametime;
			pmtrace_t *tr = gEngfuncs.PM_TraceLine( part->origin, vecTarget, PM_TRACELINE_PHYSENTSONLY, 2, -1 );

			if( tr->fraction < 1.0f )
			{
				part->origin = tr->endpos;
				float bounceforce = DotProduct( tr->plane.normal, part->velocity );
				float newspeed = (1.0f - part->pType->m_BounceFriction.GetInstance( ));
				part->velocity = part->velocity * newspeed;
				part->velocity += tr->plane.normal * (-bounceforce * (newspeed + part->pType->m_Bounce.GetInstance()));
			}
		}
	}

	// spray children
	if( part->age_spray && part->age > part->age_spray )
	{
		part->age_spray = part->age + 1.0f / part->pType->m_SprayRate.GetInstance();

		if( part->pType->m_pSprayType )
		{
			CParticle *pChild = part->pType->m_pSprayType->CreateParticle( this );

			if( pChild )
			{
				pChild->origin = part->origin;
				pChild->velocity = part->velocity;
				float fSprayForce = part->pType->m_SprayForce.GetInstance();

				if( fSprayForce )
				{
					Vector vecLocalAngles, vecSprayDir;
					vecLocalAngles.x = part->pType->m_SprayPitch.GetInstance();
					vecLocalAngles.y = part->pType->m_SprayYaw.GetInstance();
					vecLocalAngles.z = 0.0f;

					// setup particle local direction
					if( vecLocalAngles != g_vecZero )
						AngleVectors( vecLocalAngles, vecSprayDir, NULL, NULL );
					else vecSprayDir = Vector( 1, 0, 0 ); // fast case
					pChild->velocity += entityMatrix.VectorRotate( vecSprayDir ) * fSprayForce;
				}
			}
		}
	}

	part->m_fSize += part->m_fSizeStep * frametime;
	part->m_fAlpha += part->m_fAlphaStep * frametime;
	part->m_fRed += part->m_fRedStep * frametime;
	part->m_fGreen += part->m_fGreenStep * frametime;
	part->m_fBlue += part->m_fBlueStep * frametime;
	part->frame += part->m_fFrameStep * frametime;

	if (!part->m_pEntity && (part->m_fSize <= 0.0f || part->m_fAlpha <= 0.0f)) {
		return false;
	}

	if( part->m_fAngleStep )
	{
		part->m_fAngle += part->m_fAngleStep * frametime;
		while( part->m_fAngle < 0 ) part->m_fAngle += 360;
		while( part->m_fAngle > 360 ) part->m_fAngle -= 360;
	}

	if (tr.frametime != 0.0) {
		AddPointToBounds(part->origin, m_vecAbsMin, m_vecAbsMax);
	}

	return true;
}

void CParticleSystem :: DrawParticle( CParticle *part, const vec3_t &right, const vec3_t &up )
{
	float fSize = part->m_fSize;

	Vector point[4];
	Vector origin = part->origin;
	Vector absmin, absmax;
	Vector4D partColor; 

	float fCosSize = CosLookup( part->m_fAngle ) * fSize;
	float fSinSize = SinLookup( part->m_fAngle ) * fSize;

	// calculate the four corners of the sprite
	point[0] = origin + up * -fCosSize + right * -fSinSize;
	point[1] = origin + up * fSinSize + right * -fCosSize;
	point[2] = origin + up * fCosSize + right * fSinSize;
	point[3] = origin + up * -fSinSize + right * fCosSize;	

	ClearBounds( absmin, absmax );
	int i;
	for( i = 0; i < 4; i++ )
		AddPointToBounds( point[i], absmin, absmax );

	int iContents = CONTENTS_NONE;
	model_t *pModel;

	for( CParticle *pDraw = part; pDraw; pDraw = pDraw->m_pOverlay )
	{
		if( !pDraw->pType->m_hSprite )
			continue;

		if( pDraw->pType->m_iDrawCond )
		{
			if( pDraw->pType->m_iDrawCond == CONTENTS_SPOTLIGHT )
			{
				int i;
				if( !m_fHasProjectionLighting )
					continue;	// fast reject

				for( i = 0; i < MAX_DLIGHTS; i++ )
				{
					CDynLight *pl = &tr.dlights[i];

					if( !pl->Active( )) continue;

					if( !pl->frustum.CullSphere( part->origin, part->m_fSize + 1 ))
						break; // cone intersected with particle

				}

				if( i == MAX_DLIGHTS )
					continue;	// no intersection
			}
			else
			{
				if( iContents == CONTENTS_NONE )
					iContents = POINT_CONTENTS( origin );

				if( iContents != pDraw->pType->m_iDrawCond )
					continue;
			}
		}

		pModel = (model_t *)gEngfuncs.GetSpritePointer( pDraw->pType->m_hSprite );

		// if we've reached the end of the sprite's frames, loop back
		while( pDraw->frame > pModel->numframes )
			pDraw->frame -= pModel->numframes;

		while( pDraw->frame < 0 )
			pDraw->frame += pModel->numframes;

		if( m_iLightingModel >= 1 )
		{
			Vector lightColor;

			gEngfuncs.pTriAPI->LightAtPoint( part->origin, (float *)&lightColor );
			lightColor *= (1.0f / 255.0f);
			partColor.x = pDraw->m_fRed * lightColor.x;
			partColor.y = pDraw->m_fGreen * lightColor.y;
			partColor.z = pDraw->m_fBlue * lightColor.z;
			partColor.w = pDraw->m_fAlpha;
		}
		else
		{
			partColor = Vector4D( pDraw->m_fRed, pDraw->m_fGreen, pDraw->m_fBlue, pDraw->m_fAlpha );
		}
#if 0
		if( !gEngfuncs.pTriAPI->SpriteTexture( pModel, (int)pDraw->frame ))
			continue;

		gEngfuncs.pTriAPI->RenderMode( pDraw->pType->m_iRenderMode );
		pglColor4f( partColor.x, partColor.y, partColor.z, partColor.w );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

		pglBegin( GL_QUADS );
			pglTexCoord2f( 0.0f, 1.0f );
			pglVertex3fv( point[0] );

			pglTexCoord2f( 0.0f, 0.0f );
			pglVertex3fv( point[1] );

			pglTexCoord2f( 1.0f, 0.0f );
			pglVertex3fv( point[2] );

			pglTexCoord2f( 1.0f, 1.0f );
			pglVertex3fv( point[3] );
		pglEnd();
#else
		TextureHandle hTexture = R_GetSpriteTexture( pModel, pDraw->frame );
		if (hTexture == TextureHandle::Null()) {
			continue;
		}

		CTransEntry entry;
		entry.SetRenderPrimitive( point, partColor, hTexture, pDraw->pType->m_iRenderMode );
		entry.ComputeViewDistance( absmin, absmax );
		RI->frame.trans_list.AddToTail( entry );
#endif
	}
}
