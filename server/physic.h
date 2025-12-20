/*
physic.h - an abstract class for physics subsystem
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#ifndef PHYSIC_H
#define PHYSIC_H

class Vector;
class CBaseEntity;

class IPhysicLayer
{
public:
	virtual void	InitPhysic( void ) = 0;
	virtual void	FreePhysic( void ) = 0;
	virtual void	Update( float flTime ) = 0;
	virtual void	EndFrame( void ) = 0;
	virtual bool	Initialized( void ) = 0;
	virtual void	RemoveBody( struct edict_s *pEdict ) = 0;
	virtual void	*CreateBodyFromEntity( CBaseEntity *pEntity ) = 0;
	virtual void	*CreateBoxFromEntity( CBaseEntity *pObject ) = 0;
	virtual void	*CreateKinematicBodyFromEntity( CBaseEntity *pEntity ) = 0;
	virtual void	*CreateStaticBodyFromEntity( CBaseEntity *pObject ) = 0;
	virtual void	*CreateVehicle( CBaseEntity *pObject, int scriptName = 0 ) = 0;
	virtual void	*CreateTriggerFromEntity( CBaseEntity *pEntity ) = 0;
	virtual void	*RestoreBody( CBaseEntity *pEntity ) = 0;
	virtual void	SaveBody( CBaseEntity *pObject ) = 0;
	virtual void	SetOrigin( CBaseEntity *pEntity, const Vector &origin ) = 0;
	virtual void	SetAngles( CBaseEntity *pEntity, const Vector &angles ) = 0;
	virtual void	SetVelocity( CBaseEntity *pEntity, const Vector &velocity ) = 0;
	virtual void	SetAvelocity( CBaseEntity *pEntity, const Vector &velocity ) = 0;
	virtual void	MoveObject( CBaseEntity *pEntity, const Vector &finalPos ) = 0;
	virtual void	RotateObject( CBaseEntity *pEntity, const Vector &finalAngle ) = 0;
	virtual void	SetLinearMomentum( CBaseEntity *pEntity, const Vector &velocity ) = 0;
	virtual void	AddImpulse( CBaseEntity *pEntity, const Vector &impulse, const Vector &position, float factor ) = 0;
	virtual void	AddForce( CBaseEntity *pEntity, const Vector &force ) = 0;
	virtual void	EnableCollision( CBaseEntity *pEntity, int fEnable ) = 0;
	virtual void	MakeKinematic( CBaseEntity *pEntity, int fEnable ) = 0;
	virtual void	UpdateVehicle( CBaseEntity *pObject ) = 0;
	virtual int		FLoadTree( char *szMapName ) = 0;
	virtual int		CheckBINFile( char *szMapName ) = 0;
	virtual int		BuildCollisionTree( char *szMapName ) = 0;
	virtual bool	UpdateEntityTransform( CBaseEntity *pEntity ) = 0;
	virtual void	UpdateEntityAABB( CBaseEntity *pEntity ) = 0;
	virtual bool	UpdateActorPos( CBaseEntity *pEntity ) = 0;
	virtual void	SetupWorld( void ) = 0;	
	virtual void	FreeWorld( void ) = 0;
	virtual void	DebugDraw( void ) = 0;
	virtual void	DrawPSpeeds( void ) = 0;
	virtual void	TeleportCharacter( CBaseEntity *pEntity ) = 0;
	virtual void	TeleportActor( CBaseEntity *pEntity ) = 0;
	virtual void	MoveCharacter( CBaseEntity *pEntity ) = 0;
	virtual void	MoveKinematic( CBaseEntity *pEntity ) = 0;
	virtual void	SweepTest( CBaseEntity *pTouch, const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, struct trace_s *tr ) = 0;
	virtual void	SweepEntity( CBaseEntity *pEntity, const Vector &start, const Vector &end, struct gametrace_s *tr ) = 0;
	virtual bool	IsBodySleeping( CBaseEntity *pEntity ) = 0;
	virtual void	*GetCookingInterface( void ) = 0;
	virtual void	*GetPhysicInterface( void ) = 0;
};

extern void GameInitNullPhysics( void ); // shutdown simulation for some reasons
extern IPhysicLayer	*WorldPhysic;

#endif // PHYSIC_H
