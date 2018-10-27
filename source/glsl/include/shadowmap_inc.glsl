	{
		float f;

		vec3 shadowmaptc = vec3(v_ShadowProjVector[SHADOW_INDEX].xyz / v_ShadowProjVector[SHADOW_INDEX].w);

		// this keeps shadows from appearing on surfaces behind frustum's nearplane
		float d = step(v_ShadowProjVector[SHADOW_INDEX].w, 0.0);

		# ifdef APPLY_SHADOW_NORMAL_CHECK
		f = dot(u_ShadowDir[SHADOW_INDEX].xyz, v_Normal);
		d += step(0.0, f);
		# endif
		
		d += length(v_Position[SHADOW_INDEX].xyz - u_ShadowEntityDist[SHADOW_INDEX].xyz) / u_ShadowDir[SHADOW_INDEX].w;

		//shadowmaptc = (shadowmaptc + vec3 (1.0)) * vec3 (0.5);
		shadowmaptc.xy = shadowmaptc.xy * u_ShadowmapTextureParams[SHADOW_INDEX].xy; // .x - texture width
		shadowmaptc.z = clamp(shadowmaptc.z, 0.0, 1.0);
		shadowmaptc.xy = vec2(clamp(shadowmaptc.x, 0.0, u_ShadowmapTextureParams[SHADOW_INDEX].x), clamp(shadowmaptc.y, 0.0, u_ShadowmapTextureParams[SHADOW_INDEX].y));

		vec2 ShadowMap_TextureScale = u_ShadowmapTextureParams[SHADOW_INDEX].zw;

		#ifdef APPLY_DITHER

		# ifdef APPLY_PCF
		#  define texval(x, y) dshadow2D(SHADOW_TEXTURE, vec3(center + vec2(x, y)*ShadowMap_TextureScale, shadowmaptc.z))

		// this method can be described as a 'dithered pinwheel' (4 texture lookups)
		// which is a combination of the 'pinwheel' filter suggested by eihrul and dithered 4x4 PCF,
		// described here: http://http.developer.nvidia.com/GPUGems/gpugems_ch11.html 

		vec2 offset_dither = mod(floor(gl_FragCoord.xy), 2.0);
		offset_dither.y += offset_dither.x;  // y ^= x in floating point
		offset_dither.y *= step(offset_dither.y, 1.1);

		vec2 center = (shadowmaptc.xy + offset_dither.xy) * ShadowMap_TextureScale;
		float group1 = texval(-0.4,  1.0);
		float group2 = texval(-1.0, -0.4);
		float group3 = texval( 0.4, -1.0);
		float group4 = texval( 1.0,  0.4);

		#  undef texval

		f = dot(vec4(0.25), vec4(group1, group2, group3, group4));
		# else
		f = dshadow2D(SHADOW_TEXTURE, vec3(shadowmaptc.xy*ShadowMap_TextureScale, shadowmaptc.z));
		# endif // APPLY_PCF
	
		#else
		// an essay by eihrul:
		// now think of bilinear filtering as a 1x1 weighted box filter
		// that is, it's sampling over a 2x2 area, but only collecting the portion of each pixel it actually steps on
		// with a linear shadowmap filter, you are getting that, like normal bilinear sampling
		// only its doing the shadowmap test on each pixel first, to generate a new little 2x2 area, then its doing
		// the bilinear filtering on that
		// so now if you consider your 2x2 filter you have
		// each of those taps is actually using linear filtering as you've configured it
		// so you are literally sampling almost 16 pixels as is and all you are getting for it is 2x2
		// the trick is to realize that in essence you could instead be sampling a 4x4 area of pixels
		// and running a 3x3 weighted box filter on it
		// but you would need some way to get the shadowmap to simply return the 4 pixels covered by each
		// tap, rather than the filtered result
		// which is what the ARB_texture_gather extension is for
		// NOTE: we're using emulation of texture_gather now

		# ifdef APPLY_PCF
		#  define texval(off) dshadow2D(SHADOW_TEXTURE, vec3(off,shadowmaptc.z))

		#  ifdef APPLY_SHADOW_SAMPLERS
		vec2 offset = fract(shadowmaptc.xy - 0.5);
		vec4 size = vec4(offset + 1.0, 2.0 - offset), weight = (vec4(2.0 - 1.0 / size.xy, 1.0 / size.zw - 1.0) + (shadowmaptc.xy - offset).xyxy)*ShadowMap_TextureScale.xyxy;
		f = (1.0/9.0)*dot(size.zxzx*size.wwyy, vec4(texval(weight.zw), texval(weight.xw), texval(weight.zy), texval(weight.xy)));

		#  else
		vec2 origin = floor(shadowmaptc.xy) * ShadowMap_TextureScale;
		vec4 offsets = ShadowMap_TextureScale.xyxy * vec4(-0.5, -0.5, 0.5, 0.5);
		float texNN = texval(origin + offsets.xy);
		float texPN = texval(origin + offsets.zy);
		float texNP = texval(origin + offsets.xw);
		float texPP = texval(origin + offsets.zw);
		vec2 mixFactors = fract(shadowmaptc.xy);
		f = mix(mix(texNN, texPN, mixFactors.x), mix(texNP, texPP, mixFactors.x), mixFactors.y);
		#  endif // APPLY_SHADOW_SAMPLERS

		#  undef texval
		
		# else
		
		f = dshadow2D(SHADOW_TEXTURE, vec3(shadowmaptc.xy * ShadowMap_TextureScale, shadowmaptc.z));
		
		# endif // APPLY_PCF
		
		#endif // APPLY_DITHER

		finalcolor *= clamp(max(max(f, d), u_ShadowAlpha[SHADOW_INDEX / 4].SHADOW_INDEX_COMPONENT), 0.0, 1.0);
	}
