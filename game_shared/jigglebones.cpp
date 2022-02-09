//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "jigglebones.h"

//-----------------------------------------------------------------------------
JiggleData * CJiggleBones::GetJiggleData( int bone, float currenttime, const Vector &initBasePos, const Vector &initTipPos )
{
	FOR_EACH_LL( m_jiggleBoneState, it )
	{
		if ( m_jiggleBoneState[it].bone == bone )
		{
			return &m_jiggleBoneState[it];
		}
	}

	JiggleData data;
	data.Init( bone, currenttime, initBasePos, initTipPos );

	int idx = m_jiggleBoneState.AddToHead( data );
	if ( idx == m_jiggleBoneState.InvalidIndex() )
		return NULL;

	return &m_jiggleBoneState[idx];
}


//-----------------------------------------------------------------------------
/**
 * Do spring physics calculations and update "jiggle bone" matrix
 * (Michael Booth, Turtle Rock Studios)
 */
void CJiggleBones::BuildJiggleTransformations( int boneIndex, float currenttime, const mstudiojigglebone_t *jiggleInfo, const matrix3x4 &goalMX, matrix3x4 &boneMX )
{
	Vector goalBasePosition = goalMX[3];

	Vector goalForward = goalMX[2];
	Vector goalUp = goalMX[1];
	Vector goalLeft = goalMX[0];

	// compute goal tip position
	Vector goalTip = goalBasePosition + jiggleInfo->length * goalForward;

	JiggleData *data = GetJiggleData( boneIndex, currenttime, goalBasePosition, goalTip );
	if( !data ) return;

	if( currenttime - data->lastUpdate > 0.5f )
	{
		data->Init( boneIndex, currenttime, goalBasePosition, goalTip );
	}

	// limit maximum deltaT to avoid simulation blowups
	// if framerate gets very low, jiggle will run in slow motion
	const float thirtyHZ = 0.0333f;
	const float thousandHZ = 0.001f;
	float deltaT = bound( thousandHZ, currenttime - data->lastUpdate, thirtyHZ );
	data->lastUpdate = currenttime;

	//
	// Bone tip flex
	//
	if (jiggleInfo->flags & (JIGGLE_IS_FLEXIBLE | JIGGLE_IS_RIGID))
	{
		// apply gravity in global space
		data->tipAccel.z -= jiggleInfo->tipMass;

		if (jiggleInfo->flags & JIGGLE_IS_FLEXIBLE)
		{
			// decompose into local coordinates
			Vector error = goalTip - data->tipPos;

			Vector localError;
			localError.x = DotProduct( goalLeft, error );
			localError.y = DotProduct( goalUp, error );
			localError.z = DotProduct( goalForward, error );

			Vector localVel;
			localVel.x = DotProduct( goalLeft, data->tipVel );
			localVel.y = DotProduct( goalUp, data->tipVel );

			// yaw spring
			float yawAccel = jiggleInfo->yawStiffness * localError.x - jiggleInfo->yawDamping * localVel.x;

			// pitch spring
			float pitchAccel = jiggleInfo->pitchStiffness * localError.y - jiggleInfo->pitchDamping * localVel.y;

			if (jiggleInfo->flags & JIGGLE_HAS_LENGTH_CONSTRAINT)
			{
				// drive tip towards goal tip position	
				data->tipAccel += yawAccel * goalLeft + pitchAccel * goalUp;
			}
			else
			{
				// allow flex along length of spring
				localVel.z = DotProduct( goalForward, data->tipVel );

				// along spring
				float alongAccel = jiggleInfo->alongStiffness * localError.z - jiggleInfo->alongDamping * localVel.z;

				// drive tip towards goal tip position	
				data->tipAccel += yawAccel * goalLeft + pitchAccel * goalUp + alongAccel * goalForward;
			}
		}


		// simple euler integration		
		data->tipVel += data->tipAccel * deltaT;
		data->tipPos += data->tipVel * deltaT;

		// clear this timestep's accumulated accelerations
		data->tipAccel = g_vecZero;		

		//
		// Apply optional constraints
		//
		if (jiggleInfo->flags & (JIGGLE_HAS_YAW_CONSTRAINT | JIGGLE_HAS_PITCH_CONSTRAINT))
		{
			// find components of spring vector in local coordinate system
			Vector along = data->tipPos - goalBasePosition;
			Vector localAlong;
			localAlong.x = DotProduct( goalLeft, along );
			localAlong.y = DotProduct( goalUp, along );
			localAlong.z = DotProduct( goalForward, along );

			Vector localVel;
			localVel.x = DotProduct( goalLeft, data->tipVel );
			localVel.y = DotProduct( goalUp, data->tipVel );
			localVel.z = DotProduct( goalForward, data->tipVel );

			if (jiggleInfo->flags & JIGGLE_HAS_YAW_CONSTRAINT)
			{
				// enforce yaw constraints in local XZ plane
				float yawError = atan2( localAlong.x, localAlong.z );

				bool isAtLimit = false;
				float yaw = 0.0f;

				if (yawError < jiggleInfo->minYaw)
				{
					// at angular limit
					isAtLimit = true;
					yaw = jiggleInfo->minYaw;
				}
				else if (yawError > jiggleInfo->maxYaw)
				{
					// at angular limit
					isAtLimit = true;
					yaw = jiggleInfo->maxYaw;
				}

				if (isAtLimit)
				{
					float sy, cy;
					SinCos( yaw, &sy, &cy );

					// yaw matrix
					matrix3x4 yawMatrix;

					yawMatrix.SetForward( Vector( cy, 0, -sy ));
					yawMatrix.SetRight( Vector( 0.0f, 1.0f, 0.0f ));
					yawMatrix.SetUp( Vector( sy, 0.0f, cy ));
					yawMatrix.SetOrigin( g_vecZero );

					// global coordinates of limit
					matrix3x4 limitMatrix = goalMX.ConcatTransforms( yawMatrix );

					Vector limitLeft( limitMatrix[0] );
					Vector limitUp( limitMatrix[1] );
					Vector limitForward( limitMatrix[2] );
					Vector limitAlong( DotProduct( limitLeft, along ), DotProduct( limitUp, along ), DotProduct( limitForward, along ));

					// clip to limit plane
					data->tipPos = goalBasePosition + limitAlong.y * limitUp + limitAlong.z * limitForward;

					// yaw friction - rubbing along limit plane
					Vector limitVel;
					limitVel.y = DotProduct( limitUp, data->tipVel );
					limitVel.z = DotProduct( limitForward, data->tipVel );

					data->tipAccel -= jiggleInfo->yawFriction * (limitVel.y * limitUp + limitVel.z * limitForward);

					// update velocity reaction to hitting constraint
					data->tipVel = -jiggleInfo->yawBounce * limitVel.x * limitLeft + limitVel.y * limitUp + limitVel.z * limitForward;

					// update along vectors for use by pitch constraint
					along = data->tipPos - goalBasePosition;
					localAlong.x = DotProduct( goalLeft, along );
					localAlong.y = DotProduct( goalUp, along );
					localAlong.z = DotProduct( goalForward, along );

					localVel.x = DotProduct( goalLeft, data->tipVel );
					localVel.y = DotProduct( goalUp, data->tipVel );
					localVel.z = DotProduct( goalForward, data->tipVel );
				}
			}


			if (jiggleInfo->flags & JIGGLE_HAS_PITCH_CONSTRAINT)
			{
				// enforce pitch constraints in local YZ plane
				float pitchError = atan2( localAlong.y, localAlong.z );

				bool isAtLimit = false;
				float pitch = 0.0f;

				if (pitchError < jiggleInfo->minPitch)
				{
					// at angular limit
					isAtLimit = true;
					pitch = jiggleInfo->minPitch;
				}
				else if (pitchError > jiggleInfo->maxPitch)
				{
					// at angular limit
					isAtLimit = true;
					pitch = jiggleInfo->maxPitch;
				}

				if (isAtLimit)
				{
					float sp, cp;
					SinCos( pitch, &sp, &cp );

					// pitch matrix
					matrix3x4 pitchMatrix;

					pitchMatrix.SetForward( Vector( 1.0f, 0.0, 0.0f ));
					pitchMatrix.SetRight( Vector( 0, cp, -sp ));
					pitchMatrix.SetUp( Vector( 0, sp, cp ));
					pitchMatrix.SetOrigin( g_vecZero );

					// global coordinates of limit
					matrix3x4 limitMatrix = goalMX.ConcatTransforms( pitchMatrix );

					Vector limitLeft( limitMatrix[0] );
					Vector limitUp( limitMatrix[1] );
					Vector limitForward( limitMatrix[2] );

					Vector limitAlong( DotProduct( limitLeft, along ), DotProduct( limitUp, along ), DotProduct( limitForward, along ));

					// clip to limit plane
					data->tipPos = goalBasePosition + limitAlong.x * limitLeft + limitAlong.z * limitForward;

					// removed friction and velocity clipping against constraint - was causing simulation blowups
					data->tipVel.Init();
				}
			}
		}

		// needed for matrix assembly below
		Vector forward = (data->tipPos - goalBasePosition).Normalize();

		if (jiggleInfo->flags & JIGGLE_HAS_ANGLE_CONSTRAINT)
		{
			// enforce max angular error
			Vector error = goalTip - data->tipPos;
			float dot = DotProduct( forward, goalForward );
			float angleBetween = acos( dot );
			if (dot < 0.0f)
			{
				angleBetween = 2.0f * M_PI - angleBetween;
			}

			if (angleBetween > jiggleInfo->angleLimit)
			{
				// at angular limit
				float maxBetween = jiggleInfo->length * sin( jiggleInfo->angleLimit );

				Vector delta = (goalTip - data->tipPos).Normalize();

				data->tipPos = goalTip - maxBetween * delta;

				forward = (data->tipPos - goalBasePosition).Normalize();
			}
		}

		if (jiggleInfo->flags & JIGGLE_HAS_LENGTH_CONSTRAINT)
		{
			// enforce spring length
			data->tipPos = goalBasePosition + jiggleInfo->length * forward;

			// zero velocity along forward bone axis
			data->tipVel -= DotProduct( data->tipVel, forward ) * forward;
		}

		//
		// Build bone matrix to align along current tip direction
		//
		Vector left = CrossProduct( goalUp, forward ).Normalize();
		if (DotProduct(left, data->lastLeft) < 0.0f)
		{
			// The bone has rotated so far its on the other side of the up vector
			// resulting in the cross product result flipping 180 degrees around the up
			// vector.  Flip it back.
			left = -left;
		}
		data->lastLeft = left;

		Vector up = CrossProduct( forward, left );
		boneMX.SetForward( left );
		boneMX.SetRight( up );
		boneMX.SetUp( forward );
		boneMX.SetOrigin( goalBasePosition );
	}

	//
	// Bone base flex
	//
	if (jiggleInfo->flags & JIGGLE_HAS_BASE_SPRING)
	{
		// gravity
		data->baseAccel.z -= jiggleInfo->baseMass;

		// simple spring
		Vector error = goalBasePosition - data->basePos;
		data->baseAccel += jiggleInfo->baseStiffness * error - jiggleInfo->baseDamping * data->baseVel;

		data->baseVel += data->baseAccel * deltaT;
		data->basePos += data->baseVel * deltaT;

		// clear this timestep's accumulated accelerations
		data->baseAccel = g_vecZero;		

		// constrain to limits
		error = data->basePos - goalBasePosition;
		Vector localError;
		localError.x = DotProduct( goalLeft, error );
		localError.y = DotProduct( goalUp, error );
		localError.z = DotProduct( goalForward, error );

		Vector localVel;
		localVel.x = DotProduct( goalLeft, data->baseVel );
		localVel.y = DotProduct( goalUp, data->baseVel );
		localVel.z = DotProduct( goalForward, data->baseVel );

		// horizontal constraint
		if (localError.x < jiggleInfo->baseMinLeft)
		{
			localError.x = jiggleInfo->baseMinLeft;

			// friction
			data->baseAccel -= jiggleInfo->baseLeftFriction * (localVel.y * goalUp + localVel.z * goalForward);
		}
		else if (localError.x > jiggleInfo->baseMaxLeft)
		{
			localError.x = jiggleInfo->baseMaxLeft;

			// friction
			data->baseAccel -= jiggleInfo->baseLeftFriction * (localVel.y * goalUp + localVel.z * goalForward);
		}

		if (localError.y < jiggleInfo->baseMinUp)
		{
			localError.y = jiggleInfo->baseMinUp;

			// friction
			data->baseAccel -= jiggleInfo->baseUpFriction * (localVel.x * goalLeft + localVel.z * goalForward);
		}
		else if (localError.y > jiggleInfo->baseMaxUp)
		{
			localError.y = jiggleInfo->baseMaxUp;

			// friction
			data->baseAccel -= jiggleInfo->baseUpFriction * (localVel.x * goalLeft + localVel.z * goalForward);
		}

		if (localError.z < jiggleInfo->baseMinForward)
		{
			localError.z = jiggleInfo->baseMinForward;

			// friction
			data->baseAccel -= jiggleInfo->baseForwardFriction * (localVel.x * goalLeft + localVel.y * goalUp);
		}
		else if (localError.z > jiggleInfo->baseMaxForward)
		{
			localError.z = jiggleInfo->baseMaxForward;

			// friction
			data->baseAccel -= jiggleInfo->baseForwardFriction * (localVel.x * goalLeft + localVel.y * goalUp);
		}

		data->basePos = goalBasePosition + localError.x * goalLeft + localError.y * goalUp + localError.z * goalForward;


		// fix up velocity
		data->baseVel = (data->basePos - data->baseLastPos) / deltaT;
		data->baseLastPos = data->basePos;


		if (!(jiggleInfo->flags & (JIGGLE_IS_FLEXIBLE | JIGGLE_IS_RIGID)))
		{
			// no tip flex - use bone's goal orientation
			boneMX = goalMX;							
		}

		// update bone position
		boneMX.SetOrigin( data->basePos );
	}
	else if ( jiggleInfo->flags & JIGGLE_IS_BOING )
	{
		// estimate velocity
		Vector vel = goalBasePosition - data->lastBoingPos;

		data->lastBoingPos = goalBasePosition;

		float speed = vel.Length();
		vel = vel.Normalize();

		if( speed < 0.00001f )
		{
			vel = Vector( 0.0f, 0.0f, 1.0f );
			speed = 0.0f;
		}
		else
		{
			speed /= deltaT;
		}

		data->boingTime += deltaT;

		// if velocity changed a lot, we impacted and should *boing*
		const float minSpeed = 5.0f; // 15.0f;
		const float minReBoingTime = 0.5f;

		if(( speed > minSpeed || data->boingSpeed > minSpeed ) && data->boingTime > minReBoingTime )
		{
			if( fabs( data->boingSpeed - speed ) > jiggleInfo->boingImpactSpeed || DotProduct( vel, data->boingVelDir ) < jiggleInfo->boingImpactAngle )
			{
				data->boingTime = 0.0f;
				data->boingDir = -vel;
			}
		}

		data->boingVelDir = vel;
		data->boingSpeed = speed;

		float damping = 1.0f - ( jiggleInfo->boingDampingRate * data->boingTime );
		if ( damping < 0.01f )
		{
			// boing has entirely damped out
			boneMX = goalMX;
		}
		else
		{
			damping *= damping;
			damping *= damping;

			float flex = jiggleInfo->boingAmplitude * cos( jiggleInfo->boingFrequency * data->boingTime ) * damping;
 			float squash = 1.0f + flex;
 			float stretch = 1.0f - flex;

			boneMX.SetForward( goalLeft );
			boneMX.SetRight( goalUp );
			boneMX.SetUp( goalForward );
			boneMX.SetOrigin( g_vecZero );

			// build transform into "boing space", where Z is along primary boing axis
			Vector boingSide;
			if( fabs( data->boingDir.x ) < 0.9f )
			{
				boingSide = CrossProduct( data->boingDir, Vector( 1.0f, 0.0f, 0.0f )).Normalize();
			}
			else
			{
				boingSide = CrossProduct( data->boingDir, Vector( 0.0f, 0.0f, 1.0f )).Normalize();
			}

			Vector boingOtherSide = CrossProduct( data->boingDir, boingSide );

			matrix3x4 xfrmToBoingCoordsMX, xfrmFromBoingCoordsMX;

			xfrmToBoingCoordsMX.SetForward( boingSide );
			xfrmToBoingCoordsMX.SetRight( boingOtherSide );
			xfrmToBoingCoordsMX.SetUp( data->boingDir );
			xfrmToBoingCoordsMX.SetOrigin( g_vecZero );

			// transform back from boing space (inverse is transpose since orthogonal)
			xfrmFromBoingCoordsMX = xfrmToBoingCoordsMX;
			xfrmToBoingCoordsMX = xfrmToBoingCoordsMX.Transpose();

			// build squash and stretch transform in "boing space"
			matrix3x4 boingMX;

			boingMX.SetForward( Vector( squash, 0.0f, 0.0f ));
			boingMX.SetRight( Vector( 0.0f, squash, 0.0f ));
			boingMX.SetUp( Vector( 0.0f, 0.0f, stretch ));
			boingMX.SetOrigin( g_vecZero );

			// put it all together
			matrix3x4 xfrmMX;
			xfrmMX = xfrmToBoingCoordsMX.ConcatTransforms( boingMX );
			xfrmMX = xfrmMX.ConcatTransforms( xfrmFromBoingCoordsMX );
			boneMX = boneMX.ConcatTransforms( xfrmMX );
			boneMX.SetOrigin( goalBasePosition );
		}
	}
	else if (!(jiggleInfo->flags & (JIGGLE_IS_FLEXIBLE | JIGGLE_IS_RIGID)))
	{
		// no flex at all - just use goal matrix
		boneMX = goalMX;
	}
}
