uniform sampler2D u_BlueNoiseTexture;

vec3 dither() {
	return ( texture( u_BlueNoiseTexture, gl_FragCoord.xy / textureSize( u_BlueNoiseTexture, 0 ) ).xxx - vec3( 0.5 ) ) / 256.0;
}
