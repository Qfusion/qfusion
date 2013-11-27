/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * Sometimes we treat GLfloats as GLints.  On x86 systems, moving a float
 * as a int (thereby using integer registers instead of FP registers) is
 * a performance win.  Typically, this can be done with ordinary casts.
 * But with gcc's -fstrict-aliasing flag (which defaults to on in gcc 3.0)
 * these casts generate warnings.
 * The following union typedef is used to solve that.
 */
typedef union { GLfloat f; GLint i; GLuint u; } fi_type;

/**
 * Convert float to int by rounding to nearest integer, away from zero.
 */
static inline int _mesa_IROUND(float f)
{
   return (int) ((f >= 0.0F) ? (f + 0.5F) : (f - 0.5F));
}

/* Using C99 rounding functions for roundToEven() implementation is
 * difficult, because round(), rint, and nearbyint() are affected by
 * fesetenv(), which the application may have done for its own
 * purposes.  Mesa's IROUND macro is close to what we want, but it
 * rounds away from 0 on n + 0.5.
 */
static inline int
_mesa_round_to_even(float val)
{
   int rounded = _mesa_IROUND(val);

   if (val - floor(val) == 0.5) {
      if (rounded % 2 != 0)
         rounded += val > 0 ? -1 : 1;
   }

   return rounded;
}

/**
 * Convert a 4-byte float to a 2-byte half float.
 *
 * Not all float32 values can be represented exactly as a float16 value. We
 * round such intermediate float32 values to the nearest float16. When the
 * float32 lies exactly between to float16 values, we round to the one with
 * an even mantissa.
 *
 * This rounding behavior has several benefits:
 *   - It has no sign bias.
 *
 *   - It reproduces the behavior of real hardware: opcode F32TO16 in Intel's
 *     GPU ISA.
 *
 *   - By reproducing the behavior of the GPU (at least on Intel hardware),
 *     compile-time evaluation of constant packHalf2x16 GLSL expressions will
 *     result in the same value as if the expression were executed on the GPU.
 */
static inline GLhalfARB
_mesa_float_to_half(float val)
{
   const fi_type fi = {val};
   const int flt_m = fi.i & 0x7fffff;
   const int flt_e = (fi.i >> 23) & 0xff;
   const int flt_s = (fi.i >> 31) & 0x1;
   int s, e, m = 0;
   GLhalfARB result;
   
   /* sign bit */
   s = flt_s;

   /* handle special cases */
   if ((flt_e == 0) && (flt_m == 0)) {
      /* zero */
      /* m = 0; - already set */
      e = 0;
   }
   else if ((flt_e == 0) && (flt_m != 0)) {
      /* denorm -- denorm float maps to 0 half */
      /* m = 0; - already set */
      e = 0;
   }
   else if ((flt_e == 0xff) && (flt_m == 0)) {
      /* infinity */
      /* m = 0; - already set */
      e = 31;
   }
   else if ((flt_e == 0xff) && (flt_m != 0)) {
      /* NaN */
      m = 1;
      e = 31;
   }
   else {
      /* regular number */
      const int new_exp = flt_e - 127;
      if (new_exp < -14) {
         /* The float32 lies in the range (0.0, min_normal16) and is rounded
          * to a nearby float16 value. The result will be either zero, subnormal,
          * or normal.
          */
         e = 0;
         m = _mesa_round_to_even((1 << 24) * fabsf(fi.f));
      }
      else if (new_exp > 15) {
         /* map this value to infinity */
         /* m = 0; - already set */
         e = 31;
      }
      else {
         /* The float32 lies in the range
          *   [min_normal16, max_normal16 + max_step16)
          * and is rounded to a nearby float16 value. The result will be
          * either normal or infinite.
          */
         e = new_exp + 15;
         m = _mesa_round_to_even(flt_m / (float) (1 << 13));
      }
   }

   assert(0 <= m && m <= 1024);
   if (m == 1024) {
      /* The float32 was rounded upwards into the range of the next exponent,
       * so bump the exponent. This correctly handles the case where f32
       * should be rounded up to float16 infinity.
       */
      ++e;
      m = 0;
   }

   result = (s << 15) | (e << 10) | m;
   return result;
}


/**
 * Convert a 2-byte half float to a 4-byte float.
 * Based on code from:
 * http://www.opengl.org/discussion_boards/ubb/Forum3/HTML/008786.html
 */
static inline float
_mesa_half_to_float(GLhalfARB val)
{
   /* XXX could also use a 64K-entry lookup table */
   const int m = val & 0x3ff;
   const int e = (val >> 10) & 0x1f;
   const int s = (val >> 15) & 0x1;
   int flt_m, flt_e, flt_s;
   fi_type fi;
   float result;

   /* sign bit */
   flt_s = s;

   /* handle special cases */
   if ((e == 0) && (m == 0)) {
      /* zero */
      flt_m = 0;
      flt_e = 0;
   }
   else if ((e == 0) && (m != 0)) {
      /* denorm -- denorm half will fit in non-denorm single */
      const float half_denorm = 1.0f / 16384.0f; /* 2^-14 */
      float mantissa = ((float) (m)) / 1024.0f;
      float sign = s ? -1.0f : 1.0f;
      return sign * mantissa * half_denorm;
   }
   else if ((e == 31) && (m == 0)) {
      /* infinity */
      flt_e = 0xff;
      flt_m = 0;
   }
   else if ((e == 31) && (m != 0)) {
      /* NaN */
      flt_e = 0xff;
      flt_m = 1;
   }
   else {
      /* regular */
      flt_e = e + 112;
      flt_m = m << 13;
   }

   fi.i = (flt_s << 31) | (flt_e << 23) | flt_m;
   result = fi.f;
   return result;
}
