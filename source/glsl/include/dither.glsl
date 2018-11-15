uniform sampler2D u_BlueNoiseTexture;
uniform vec2 u_BlueNoiseTextureSize;

vec3 dither() {
	return ( qf_texture( u_BlueNoiseTexture, gl_FragCoord.xy / u_BlueNoiseTextureSize ).xxx - vec3( 0.5 ) ) / 256.0;
}
