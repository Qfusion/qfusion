#include <stdint.h>

/*
 * https://gist.github.com/rygorous/2144712
 * https://gist.github.com/rygorous/2156668
 */

union FP32 {
	uint32_t u;
	float f;
};

uint16_t Com_FloatToHalf(float x) {
	FP32 f;
	f.f = x;
	FP32 f32infty = { 255 << 23 };
	FP32 f16max   = { (127 + 16) << 23 };
	FP32 denorm_magic = { ((127 - 15) + (23 - 10) + 1) << 23 };
	uint32_t sign_mask = 0x80000000u;
	uint16_t o = 0;

	uint32_t sign = f.u & sign_mask;
	f.u ^= sign;

	if (f.u >= f16max.u) {
		o = (f.u > f32infty.u) ? 0x7e00 : 0x7c00;
	}
	else {
		if (f.u < (113 << 23)) {
			f.f += denorm_magic.f;
			o = f.u - denorm_magic.u;
		}
		else {
			uint32_t mant_odd = (f.u >> 13) & 1;
			f.u += ((15 - 127) << 23) + 0xfff;
			f.u += mant_odd;
			o = f.u >> 13;
		}
	}

	o |= sign >> 16;
	return o;
}

float Com_HalfToFloat(uint16_t h) {
	constexpr FP32 magic = { 113 << 23 };
	constexpr uint32_t shifted_exp = 0x7c00 << 13;
	FP32 o;

	o.u = (h & 0x7fff) << 13;
	uint32_t exp = shifted_exp & o.u;
	o.u += (127 - 15) << 23;

	if (exp == shifted_exp) {
		o.u += (128 - 16) << 23;
	}
	else if (exp == 0) {
		o.u += 1 << 23;
		o.f -= magic.f;
	}

	o.u |= (h & 0x8000) << 16;
	return o.f;
}
