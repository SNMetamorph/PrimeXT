/* Interpolated texture coordinates */
uniform mat4 u_PrevModelViewProjectionMatrix;

out vec4 vertexPosition;
out vec4 previousVertexPosition;

void main( void )
{
	vertexPosition = gl_ModelViewProjectionMatrix * gl_Vertex;
  previousVertexPosition = u_PrevModelViewProjectionMatrix * gl_Vertex;

	gl_Position = vertexPosition;
}
