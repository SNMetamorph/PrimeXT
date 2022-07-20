
#include "skinning.h"
#include "skinningPrev.h"

uniform mat4 u_PrevModelViewProjectionMatrix;

out vec4 vertexPosition;
out vec4 previousVertexPosition;

attribute vec3	attr_Position;
uniform mat4	u_ModelMatrix;
uniform vec3	u_ViewOrigin;	// already in modelspace

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 ); // in object space
	mat4 boneMatrix = ComputeSkinningMatrix();
	mat4 prevBoneMatrix = ComputePrevSkinningMatrix();

  vertexPosition = gl_ModelViewProjectionMatrix * boneMatrix * position;
  previousVertexPosition = u_PrevModelViewProjectionMatrix * prevBoneMatrix * position;

	gl_Position = vertexPosition;
}
