uniform sampler2D	u_ScreenMap;
uniform sampler2D	u_TexVelocity;

varying vec2	var_TexCoord;
float uVelocityScale = 1; //currentFps / targetFps

int  MAX_SAMPLES = 16; //32

void main(void) {
	vec2 tx = var_TexCoord;
	vec4 sum = texture2D( u_TexVelocity, tx ) * 0.134598;
	vec2 dx = vec2( 0.0 ); // assume no blur

	dx = vec2( 0.01953, 0.0 ) * 2;
	dx = vec2( 0.0, 0.01953 ) * 2;
	vec2 sdx = dx;
	sum += (texture2D( u_TexVelocity, tx + sdx ) + texture2D( u_TexVelocity, tx - sdx )) * 0.127325;
	sdx += dx;
	sum += (texture2D( u_TexVelocity, tx + sdx ) + texture2D( u_TexVelocity, tx - sdx )) * 0.107778;
	sdx += dx;
	sum += (texture2D( u_TexVelocity, tx + sdx ) + texture2D( u_TexVelocity, tx - sdx )) * 0.081638;
	sdx += dx;
	sum += (texture2D( u_TexVelocity, tx + sdx ) + texture2D( u_TexVelocity, tx - sdx )) * 0.055335;
	sdx += dx;
	sum += (texture2D( u_TexVelocity, tx + sdx ) + texture2D( u_TexVelocity, tx - sdx )) * 0.033562;
	sdx += dx;
	sum += (texture2D( u_TexVelocity, tx + sdx ) + texture2D( u_TexVelocity, tx - sdx )) * 0.018216;
	sdx += dx;
	sum += (texture2D( u_TexVelocity, tx + sdx ) + texture2D( u_TexVelocity, tx - sdx )) * 0.008847;
	sdx += dx;

	vec2 velocity = sum.rg;
  velocity = velocity * 2.0 - 1.0;
	velocity *= uVelocityScale;

	//get the size of on pixel (texel)
	vec2 texelSize = 1.0 / vec2(textureSize(u_TexVelocity, 0));
	//mprove performance by adapting the number of samples according to the velocity
	float speed = length(velocity / texelSize);
	int nSamples = clamp(int(speed), 1, MAX_SAMPLES);

	//the actual blurring of the current pixel
	gl_FragColor = texture(u_ScreenMap, var_TexCoord);
	for (int i = 1; i < nSamples; ++i) 
	{
    vec2 offset = velocity * (float(i) / float(nSamples - 1) - 0.5) * 0.2;
		gl_FragColor += texture(u_ScreenMap, var_TexCoord + offset);
	}

	gl_FragColor /= float(nSamples);
}