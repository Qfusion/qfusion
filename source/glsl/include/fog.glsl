vec3 apply_fog( vec3 color, float dist ) {
	vec3 fog_color = vec3( 1.0 );
	float fog_strength = 0.00006;
	float fog_amount = 1.0 - exp( -fog_strength * dist );
	return mix( color, fog_color, fog_amount );
}
