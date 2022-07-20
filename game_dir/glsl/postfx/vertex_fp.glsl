 in vec4 vertexPosition;
 in vec4 previousVertexPosition;

   void main(void) {
     vec2 a = (vertexPosition.xy / vertexPosition.w) * 0.5 + 0.5;
     vec2 b = (previousVertexPosition.xy / previousVertexPosition.w) * 0.5 + 0.5;
     vec2 oVelocity = (a - b);// * 10;
     //oVelocity = oVelocity * abs(oVelocity) * abs(oVelocity) / 10;
     oVelocity = oVelocity * 0.5 + 0.5;

    gl_FragColor = vec4(oVelocity.x, oVelocity.y, 0, 1);
   }