/*
Copyright (C) 1997-2001 Id Software, Inc.

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
    Copyright (C) 1999-2000  Brian Paul, All Rights Reserved.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
    BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
    AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


    The Mesa OpenGL headers were originally adapted in 2001 for dynamic OpenGL
    binding by Zephaniah E. Hull and later rewritten by Joseph Carter.  This
    version of the file is for the generation 3 DynGL code, and has been
    adapted by Joseph Carter.  He and Zeph have decided to hereby disclaim all
    Copyright of this work.  It is released to the Public Domain WITHOUT ANY
    WARRANTY whatsoever, express or implied, in the hopes that others will use
    it instead of other less-evolved hacks which usually don't work right.  ;)
*/

/*
    The following code is loosely based on DynGL code by Joseph Carter
    and Zephaniah E. Hull. Adapted by Victor Luchits for qfusion project.
*/

/*
** QGL.H
*/
#ifndef QGL_H
#define QGL_H

#define GL_GLEXT_LEGACY
#define GLX_GLXEXT_LEGACY

#if !defined ( __MACOSX__ ) && !defined ( __ANDROID__ )
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#endif

#if defined ( __ANDROID__ )
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#elif defined ( __MACOSX__ )
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#elif !defined ( _WIN32 )
#include <GL/glx.h>
#endif

#undef GL_GLEXT_LEGACY
#undef GLX_GLXEXT_LEGACY

typedef struct qgl_driverinfo_s {
	const char *dllname;        // default driver DLL name
	const char *dllcvarname;    // custom driver DLL cvar name, NULL if can't override driver
} qgl_driverinfo_t;

typedef enum {
	qgl_initerr_ok,
	qgl_initerr_invalid_driver,
	qgl_initerr_unknown
} qgl_initerr_t;

QGL_EXTERN const qgl_driverinfo_t  *QGL_GetDriverInfo( void );
QGL_EXTERN qgl_initerr_t           QGL_Init( const char *dllname );
QGL_EXTERN void                    QGL_Shutdown( void );

QGL_EXTERN void                    *qglGetProcAddress( const GLubyte * );
QGL_EXTERN const char              *(*qglGetGLWExtensionsString)( void );

/*
** extension constants
*/

#ifndef GL_INT
#define GL_INT                                              0x1404
#endif
#ifndef GL_FLOAT
#define GL_FLOAT                                            0x1406
#endif

#ifndef GL_UNSIGNED_SHORT_4_4_4_4
#define GL_UNSIGNED_SHORT_4_4_4_4                           0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1                           0x8034
#define GL_UNSIGNED_SHORT_5_6_5                             0x8363
#endif

#ifndef GL_LOW_FLOAT
#define GL_LOW_FLOAT                                        0x8DF0
#define GL_MEDIUM_FLOAT                                     0x8DF1
#define GL_HIGH_FLOAT                                       0x8DF2
#define GL_LOW_INT                                          0x8DF3
#define GL_MEDIUM_INT                                       0x8DF4
#define GL_HIGH_INT                                         0x8DF5
#endif

#define GL_TEXTURE0_ARB                                     0x84C0
#define GL_MAX_TEXTURE_IMAGE_UNITS_ARB                      0x8872

/* GL_ARB_texture_compression */
#ifndef GL_ARB_texture_compression
#define GL_ARB_texture_compression

#define GL_COMPRESSED_ALPHA_ARB                             0x84E9
#define GL_COMPRESSED_LUMINANCE_ARB                         0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA_ARB                   0x84EB
#define GL_COMPRESSED_INTENSITY_ARB                         0x84EC
#define GL_COMPRESSED_RGB_ARB                               0x84ED
#define GL_COMPRESSED_RGBA_ARB                              0x84EE
#define GL_TEXTURE_COMPRESSION_HINT_ARB                     0x84EF
#define GL_TEXTURE_IMAGE_SIZE_ARB                           0x86A0
#define GL_TEXTURE_COMPRESSED_ARB                           0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB               0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS_ARB                   0x86A3
#endif /* GL_ARB_texture_compression */

/* GL_OES_compressed_ETC1_RGB8_texture */
#ifndef GL_OES_compressed_ETC1_RGB8_texture
#define GL_OES_compressed_ETC1_RGB8_texture

#define GL_ETC1_RGB8_OES                                    0x8D64
#endif /* GL_OES_compressed_ETC1_RGB8_texture */

/* GL_EXT_texture_filter_anisotropic */
#ifndef GL_EXT_texture_filter_anisotropic
#define GL_EXT_texture_filter_anisotropic

#define GL_TEXTURE_MAX_ANISOTROPY_EXT                       0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT                   0x84FF
#endif /* GL_EXT_texture_filter_anisotropic */

/* GL_EXT_texture_edge_clamp */
#ifndef GL_EXT_texture_edge_clamp
#define GL_EXT_texture_edge_clamp

#define GL_CLAMP_TO_EDGE                                    0x812F
#endif /* GL_EXT_texture_edge_clamp */

/* GL_ARB_vertex_buffer_object */
#ifndef GL_ARB_vertex_buffer_object
#define GL_ARB_vertex_buffer_object

typedef int GLintptrARB;
typedef int GLsizeiptrARB;

#define GL_ARRAY_BUFFER_ARB                                 0x8892
#define GL_ELEMENT_ARRAY_BUFFER_ARB                         0x8893
#define GL_ARRAY_BUFFER_BINDING_ARB                         0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB                 0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING_ARB                  0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING_ARB                  0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING_ARB                   0x8898
#define GL_INDEX_ARRAY_BUFFER_BINDING_ARB                   0x8899
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB           0x889A
#define GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB               0x889B
#define GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB         0x889C
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB          0x889D
#define GL_WEIGHT_ARRAY_BUFFER_BINDING_ARB                  0x889E
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB           0x889F
#define GL_STREAM_DRAW_ARB                                  0x88E0
#define GL_STREAM_READ_ARB                                  0x88E1
#define GL_STREAM_COPY_ARB                                  0x88E2
#define GL_STATIC_DRAW_ARB                                  0x88E4
#define GL_STATIC_READ_ARB                                  0x88E5
#define GL_STATIC_COPY_ARB                                  0x88E6
#define GL_DYNAMIC_DRAW_ARB                                 0x88E8
#define GL_DYNAMIC_READ_ARB                                 0x88E9
#define GL_DYNAMIC_COPY_ARB                                 0x88EA
#define GL_READ_ONLY_ARB                                    0x88B8
#define GL_WRITE_ONLY_ARB                                   0x88B9
#define GL_READ_WRITE_ARB                                   0x88BA
#define GL_BUFFER_SIZE_ARB                                  0x8764
#define GL_BUFFER_USAGE_ARB                                 0x8765
#define GL_BUFFER_ACCESS_ARB                                0x88BB
#define GL_BUFFER_MAPPED_ARB                                0x88BC
#define GL_BUFFER_MAP_POINTER_ARB                           0x88BD
#endif /* GL_ARB_vertex_buffer_object */

/* GL_ARB_texture_cube_map */
#ifndef GL_ARB_texture_cube_map
#define GL_ARB_texture_cube_map

#define GL_NORMAL_MAP_ARB                                   0x8511
#define GL_REFLECTION_MAP_ARB                               0x8512
#define GL_TEXTURE_CUBE_MAP_ARB                             0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP_ARB                     0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB                  0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB                  0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB                  0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB                  0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB                  0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB                  0x851A
#define GL_PROXY_TEXTURE_CUBE_MAP_ARB                       0x851B
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB                    0x851C
#endif /* GL_ARB_texture_cube_map */

/* GL_EXT_bgra */
#ifndef GL_EXT_bgra
#define GL_EXT_bgra

#define GL_BGR_EXT                                          0x80E0
#define GL_BGRA_EXT                                         0x80E1
#endif /* GL_EXT_bgra */

/* GL_ARB_shader_objects */
#ifndef GL_ARB_shader_objects
#define GL_ARB_shader_objects

typedef char GLcharARB;
typedef unsigned int GLhandleARB;

#define GL_PROGRAM_OBJECT_ARB                               0x8B40
#define GL_OBJECT_TYPE_ARB                                  0x8B4E
#define GL_OBJECT_SUBTYPE_ARB                               0x8B4F
#define GL_OBJECT_DELETE_STATUS_ARB                         0x8B80
#define GL_OBJECT_COMPILE_STATUS_ARB                        0x8B81
#define GL_OBJECT_LINK_STATUS_ARB                           0x8B82
#define GL_OBJECT_VALIDATE_STATUS_ARB                       0x8B83
#define GL_OBJECT_INFO_LOG_LENGTH_ARB                       0x8B84
#define GL_OBJECT_ATTACHED_OBJECTS_ARB                      0x8B85
#define GL_OBJECT_ACTIVE_UNIFORMS_ARB                       0x8B86
#define GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB             0x8B87
#define GL_OBJECT_SHADER_SOURCE_LENGTH_ARB                  0x8B88
#define GL_SHADER_OBJECT_ARB                                0x8B48
#define GL_FLOAT_VEC2_ARB                                   0x8B50
#define GL_FLOAT_VEC3_ARB                                   0x8B51
#define GL_FLOAT_VEC4_ARB                                   0x8B52
#define GL_INT_VEC2_ARB                                     0x8B53
#define GL_INT_VEC3_ARB                                     0x8B54
#define GL_INT_VEC4_ARB                                     0x8B55
#define GL_BOOL_ARB                                         0x8B56
#define GL_BOOL_VEC2_ARB                                    0x8B57
#define GL_BOOL_VEC3_ARB                                    0x8B58
#define GL_BOOL_VEC4_ARB                                    0x8B59
#define GL_FLOAT_MAT2_ARB                                   0x8B5A
#define GL_FLOAT_MAT3_ARB                                   0x8B5B
#define GL_FLOAT_MAT4_ARB                                   0x8B5C
#define GL_SAMPLER_1D_ARB                                   0x8B5D
#define GL_SAMPLER_2D_ARB                                   0x8B5E
#define GL_SAMPLER_3D_ARB                                   0x8B5F
#define GL_SAMPLER_CUBE_ARB                                 0x8B60
#define GL_SAMPLER_1D_SHADOW_ARB                            0x8B61
#define GL_SAMPLER_2D_SHADOW_ARB                            0x8B62
#define GL_SAMPLER_2D_RECT_ARB                              0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW_ARB                       0x8B64
#endif /* GL_ARB_shader_objects */

/* GL_ARB_vertex_shader */
#ifndef GL_ARB_vertex_shader
#define GL_ARB_vertex_shader

#define GL_VERTEX_SHADER_ARB                                0x8B31
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB                0x8B4A
#define GL_MAX_VARYING_FLOATS_ARB                           0x8B4B
#define GL_MAX_VERTEX_ATTRIBS_ARB                           0x8869
#define GL_MAX_TEXTURE_IMAGE_UNITS_ARB                      0x8872
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB               0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB             0x8B4D
#define GL_MAX_TEXTURE_COORDS_ARB                           0x8871
#define GL_VERTEX_PROGRAM_POINT_SIZE_ARB                    0x8642
#define GL_VERTEX_PROGRAM_TWO_SIDE_ARB                      0x8643
#define GL_OBJECT_ACTIVE_ATTRIBUTES_ARB                     0x8B89
#define GL_OBJECT_ACTIVE_ATTRIBUTE_MAX_LENGTH_ARB           0x8B8A
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB                  0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB                     0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB                   0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB                     0x8625
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB               0x886A
#define GL_CURRENT_VERTEX_ATTRIB_ARB                        0x8626
#define GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB                  0x8645
#define GL_FLOAT_VEC2_ARB                                   0x8B50
#define GL_FLOAT_VEC3_ARB                                   0x8B51
#define GL_FLOAT_VEC4_ARB                                   0x8B52
#define GL_FLOAT_MAT2_ARB                                   0x8B5A
#define GL_FLOAT_MAT3_ARB                                   0x8B5B
#define GL_FLOAT_MAT4_ARB                                   0x8B5C
#endif /* GL_ARB_vertex_shader */

/* GL_ARB_fragment_shader */
#ifndef GL_ARB_fragment_shader
#define GL_ARB_fragment_shader

#define GL_FRAGMENT_SHADER_ARB                              0x8B30
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB              0x8B49
#define GL_MAX_TEXTURE_COORDS_ARB                           0x8871
#define GL_MAX_TEXTURE_IMAGE_UNITS_ARB                      0x8872
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_ARB              0x8B8B
#endif /* GL_ARB_fragment_shader */

/* GL_ARB_shading_language_100 */
#ifndef GL_ARB_shading_language_100
#define GL_ARB_shading_language_100

#define GL_SHADING_LANGUAGE_VERSION_ARB                     0x8B8C
#endif /* GL_ARB_shading_language_100 */

/* ARB_depth_texture */
#ifndef ARB_depth_texture
#define ARB_depth_texture

#ifndef GL_DEPTH_COMPONENT16
#define GL_DEPTH_COMPONENT16                                0x81A5
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24                                0x81A6
#endif
#ifndef GL_DEPTH_COMPONENT32
#define GL_DEPTH_COMPONENT32                                0x81A7
#endif
#ifndef GL_TEXTURE_DEPTH_SIZE
#define GL_TEXTURE_DEPTH_SIZE                               0x884A
#endif
#ifndef GL_DEPTH_TEXTURE_MODE
#define GL_DEPTH_TEXTURE_MODE                               0x884B
#endif
#endif /* ARB_depth_texture */

/* GL_ARB_shadow */
#ifndef GL_ARB_shadow
#define GL_ARB_shadow

#define GL_DEPTH_TEXTURE_MODE_ARB                           0x884B
#define GL_TEXTURE_COMPARE_MODE_ARB                         0x884C
#define GL_TEXTURE_COMPARE_FUNC_ARB                         0x884D
#define GL_COMPARE_R_TO_TEXTURE_ARB                         0x884E
#define GL_TEXTURE_COMPARE_FAIL_VALUE_ARB                   0x80BF
#endif /* GL_ARB_shadow */

/* GL_EXT_framebuffer_object */
#ifndef GL_EXT_framebuffer_object
#define GL_EXT_framebuffer_object

#define GL_INVALID_FRAMEBUFFER_OPERATION_EXT                0x0506
#define GL_MAX_RENDERBUFFER_SIZE_EXT                        0x84E8
#define GL_FRAMEBUFFER_BINDING_EXT                          0x8CA6
#define GL_RENDERBUFFER_BINDING_EXT                         0x8CA7
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT           0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT           0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT         0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT 0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_EXT    0x8CD4
#define GL_FRAMEBUFFER_COMPLETE_EXT                         0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT            0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT    0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT_EXT  0x8CD8
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT            0x8CD9
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT               0x8CDA
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT           0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT           0x8CDC
#define GL_FRAMEBUFFER_UNSUPPORTED_EXT                      0x8CDD
#define GL_MAX_COLOR_ATTACHMENTS_EXT                        0x8CDF
#define GL_COLOR_ATTACHMENT0_EXT                            0x8CE0
#define GL_COLOR_ATTACHMENT1_EXT                            0x8CE1
#define GL_COLOR_ATTACHMENT2_EXT                            0x8CE2
#define GL_COLOR_ATTACHMENT3_EXT                            0x8CE3
#define GL_COLOR_ATTACHMENT4_EXT                            0x8CE4
#define GL_COLOR_ATTACHMENT5_EXT                            0x8CE5
#define GL_COLOR_ATTACHMENT6_EXT                            0x8CE6
#define GL_COLOR_ATTACHMENT7_EXT                            0x8CE7
#define GL_COLOR_ATTACHMENT8_EXT                            0x8CE8
#define GL_COLOR_ATTACHMENT9_EXT                            0x8CE9
#define GL_COLOR_ATTACHMENT10_EXT                           0x8CEA
#define GL_COLOR_ATTACHMENT11_EXT                           0x8CEB
#define GL_COLOR_ATTACHMENT12_EXT                           0x8CEC
#define GL_COLOR_ATTACHMENT13_EXT                           0x8CED
#define GL_COLOR_ATTACHMENT14_EXT                           0x8CEE
#define GL_COLOR_ATTACHMENT15_EXT                           0x8CEF
#define GL_DEPTH_ATTACHMENT_EXT                             0x8D00
#define GL_STENCIL_ATTACHMENT_EXT                           0x8D20
#define GL_FRAMEBUFFER_EXT                                  0x8D40
#define GL_RENDERBUFFER_EXT                                 0x8D41
#define GL_RENDERBUFFER_WIDTH_EXT                           0x8D42
#define GL_RENDERBUFFER_HEIGHT_EXT                          0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT_EXT                 0x8D44
#define GL_STENCIL_INDEX1_EXT                               0x8D46
#define GL_STENCIL_INDEX4_EXT                               0x8D47
#define GL_STENCIL_INDEX8_EXT                               0x8D48
#define GL_STENCIL_INDEX16_EXT                              0x8D49
#define GL_RENDERBUFFER_RED_SIZE_EXT                        0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE_EXT                      0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE_EXT                       0x8D52
#define GL_RENDERBUFFER_ALPHA_SIZE_EXT                      0x8D53
#define GL_RENDERBUFFER_DEPTH_SIZE_EXT                      0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE_EXT                    0x8D55
#endif /* GL_EXT_framebuffer_object */

/* GL_EXT_framebuffer_blit */
#ifndef GL_EXT_framebuffer_blit
#define GL_EXT_framebuffer_blit

#define GL_READ_FRAMEBUFFER_EXT                             0x8CA8
#define GL_DRAW_FRAMEBUFFER_EXT                             0x8CA9
#define GL_DRAW_FRAMEBUFFER_BINDING_EXT                     0x8CA6 // alias FRAMEBUFFER_BINDING_EXT
#define GL_READ_FRAMEBUFFER_BINDING_EXT                     0x8CAA
#endif /* GL_EXT_framebuffer_object */

/* GL_NVX_gpu_memory_info */
#ifndef GL_NVX_gpu_memory_info
#define GL_NVX_gpu_memory_info

#define GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX                0x9047
#define GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX          0x9048
#define GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX        0x9049
#define GPU_MEMORY_INFO_EVICTION_COUNT_NVX                  0x904A
#define GPU_MEMORY_INFO_EVICTED_MEMORY_NVX                  0x904B
#endif /* GL_NVX_gpu_memory_info */

/* GL_ATI_meminfo */
#ifndef GL_ATI_meminfo
#define GL_ATI_meminfo

#define VBO_FREE_MEMORY_ATI                                 0x87FB
#define TEXTURE_FREE_MEMORY_ATI                             0x87FC
#define RENDERBUFFER_FREE_MEMORY_ATI                        0x87FD
#endif /* GL_ATI_meminfo */

/* GL_ARB_half_float_vertex */
#ifndef GL_ARB_half_float_vertex
#define GL_ARB_half_float_vertex

typedef unsigned short GLhalfARB;

#ifdef GL_HALF_FLOAT
#undef GL_HALF_FLOAT
#endif
#ifdef GL_ES_VERSION_2_0
#define GL_HALF_FLOAT                                       0x8D61
#else
#define GL_HALF_FLOAT                                       0x140B
#endif
#endif /* GL_ARB_half_float_vertex */


/* GL_ARB_texture_float */
#ifndef GL_ARB_texture_float
#define GL_ARB_texture_float

#define GL_TEXTURE_RED_TYPE_ARB           0x8C10
#define GL_TEXTURE_GREEN_TYPE_ARB         0x8C11
#define GL_TEXTURE_BLUE_TYPE_ARB          0x8C12
#define GL_TEXTURE_ALPHA_TYPE_ARB         0x8C13
#define GL_TEXTURE_LUMINANCE_TYPE_ARB     0x8C14
#define GL_TEXTURE_INTENSITY_TYPE_ARB     0x8C15
#define GL_TEXTURE_DEPTH_TYPE_ARB         0x8C16
#define GL_UNSIGNED_NORMALIZED_ARB        0x8C17
#define GL_RGBA32F_ARB                    0x8814
#define GL_RGB32F_ARB                     0x8815
#define GL_ALPHA32F_ARB                   0x8816
#define GL_INTENSITY32F_ARB               0x8817
#define GL_LUMINANCE32F_ARB               0x8818
#define GL_LUMINANCE_ALPHA32F_ARB         0x8819
#define GL_RGBA16F_ARB                    0x881A
#define GL_RGB16F_ARB                     0x881B
#define GL_ALPHA16F_ARB                   0x881C
#define GL_INTENSITY16F_ARB               0x881D
#define GL_LUMINANCE16F_ARB               0x881E
#define GL_LUMINANCE_ALPHA16F_ARB         0x881F
#endif

/* GL_EXT_texture_sRGB */
#ifndef GL_EXT_texture_sRGB
#define GL_EXT_texture_sRGB
#define GL_SRGB_EXT                       0x8C40
#define GL_SRGB8_EXT                      0x8C41
#define GL_SRGB_ALPHA_EXT                 0x8C42
#define GL_SRGB8_ALPHA8_EXT               0x8C43
#define GL_SLUMINANCE_ALPHA_EXT           0x8C44
#define GL_SLUMINANCE8_ALPHA8_EXT         0x8C45
#define GL_SLUMINANCE_EXT                 0x8C46
#define GL_SLUMINANCE8_EXT                0x8C47
#define GL_COMPRESSED_SRGB_EXT            0x8C48
#define GL_COMPRESSED_SRGB_ALPHA_EXT      0x8C49
#define GL_COMPRESSED_SLUMINANCE_EXT      0x8C4A
#define GL_COMPRESSED_SLUMINANCE_ALPHA_EXT 0x8C4B
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT  0x8C4C
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif

/* GL_EXT_texture_sRGB_decode */
#ifndef GL_EXT_texture_sRGB_decode
#define GL_EXT_texture_sRGB_decode
#define GL_TEXTURE_SRGB_DECODE_EXT        0x8A48
#define GL_DECODE_EXT                     0x8A49
#define GL_SKIP_DECODE_EXT                0x8A4A
#endif

/* GL_ARB_get_program_binary */
#ifndef GL_ARB_get_program_binary
#define GL_PROGRAM_BINARY_RETRIEVABLE_HINT                  0x8257
#define GL_PROGRAM_BINARY_LENGTH                            0x8741
#define GL_NUM_PROGRAM_BINARY_FORMATS                       0x87FE
#define GL_PROGRAM_BINARY_FORMATS                           0x87FF
#endif /* GL_ARB_get_program_binary */

/* GL_ARB_ES3_compatibility */
#ifndef GL_ARB_ES3_compatibility
#define GL_ARB_ES3_compatibility

#define GL_COMPRESSED_RGB8_ETC2                             0x9274
#define GL_COMPRESSED_SRGB8_ETC2                            0x9275
#define GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2         0x9276
#define GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2        0x9277
#define GL_COMPRESSED_RGBA8_ETC2_EAC                        0x9278
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC                 0x9279
#define GL_COMPRESSED_R11_EAC                               0x9270
#define GL_COMPRESSED_SIGNED_R11_EAC                        0x9271
#define GL_COMPRESSED_RG11_EAC                              0x9272
#define GL_COMPRESSED_SIGNED_RG11_EAC                       0x9273
#define GL_PRIMITIVE_RESTART_FIXED_INDEX                    0x8D69
#define GL_ANY_SAMPLES_PASSED_CONSERVATIVE                  0x8D6A
#define GL_MAX_ELEMENT_INDEX                                0x8D6B
#endif

/* GL_NV_depth_nonlinear */
#ifndef GL_NV_depth_nonlinear
#define GL_NV_depth_nonlinear

#define GL_DEPTH_COMPONENT16_NONLINEAR_NV                   0x8E2C
#define EGL_DEPTH_ENCODING_NV                               0x30E2
#define EGL_DEPTH_ENCODING_NONE_NV                          0
#define EGL_DEPTH_ENCODING_NONLINEAR_NV                     0x30E3
#endif

/* GL_EXT_texture3D */
#ifndef GL_EXT_texture3D
#define GL_EXT_texture3D

#define GL_TEXTURE_3D_EXT                                   0x806F
#define GL_TEXTURE_WRAP_R_EXT                               0x8072
#define GL_MAX_3D_TEXTURE_SIZE_EXT                          0x8073
#define GL_TEXTURE_BINDING_3D_EXT                           0x806A
#endif

/* GL_EXT_texture_array */
#ifndef GL_EXT_texture_array
#define GL_EXT_texture_array

#define GL_TEXTURE_2D_ARRAY_EXT                             0x8C1A
#define GL_TEXTURE_BINDING_2D_ARRAY_EXT                     0x8C1D
#define GL_MAX_ARRAY_TEXTURE_LAYERS_EXT                     0x88FF
#endif

/* GL_EXT_packed_depth_stencil */
#ifndef GL_EXT_packed_depth_stencil
#define GL_EXT_packed_depth_stencil

#define GL_DEPTH_STENCIL_EXT                                0x84F9
#define GL_UNSIGNED_INT_24_8_EXT                            0x84FA
#define GL_DEPTH24_STENCIL8_EXT                             0x88F0
#endif

#ifndef GL_DEPTH_STENCIL_ATTACHMENT_EXT
#define GL_DEPTH_STENCIL_ATTACHMENT_EXT                     0x821A
#endif

/* GL_SGIS_texture_lod */
#ifndef GL_SGIS_texture_lod
#define GL_SGIS_texture_lod

#define GL_TEXTURE_MIN_LOD_SGIS                             0x813A
#define GL_TEXTURE_MAX_LOD_SGIS                             0x813B
#define GL_TEXTURE_BASE_LEVEL_SGIS                          0x813C
#define GL_TEXTURE_MAX_LEVEL_SGIS                           0x813D
#endif

/* GL_ARB_draw_buffers */
#ifndef GL_ARB_draw_buffers
#define GL_ARB_draw_buffers

#define GL_MAX_DRAW_BUFFERS_ARB           0x8824
#define GL_DRAW_BUFFER0_ARB               0x8825
#define GL_DRAW_BUFFER1_ARB               0x8826
#define GL_DRAW_BUFFER2_ARB               0x8827
#define GL_DRAW_BUFFER3_ARB               0x8828
#define GL_DRAW_BUFFER4_ARB               0x8829
#define GL_DRAW_BUFFER5_ARB               0x882A
#define GL_DRAW_BUFFER6_ARB               0x882B
#define GL_DRAW_BUFFER7_ARB               0x882C
#define GL_DRAW_BUFFER8_ARB               0x882D
#define GL_DRAW_BUFFER9_ARB               0x882E
#define GL_DRAW_BUFFER10_ARB              0x882F
#define GL_DRAW_BUFFER11_ARB              0x8830
#define GL_DRAW_BUFFER12_ARB              0x8831
#define GL_DRAW_BUFFER13_ARB              0x8832
#define GL_DRAW_BUFFER14_ARB              0x8833
#define GL_DRAW_BUFFER15_ARB              0x8834
#endif /* GL_ARB_draw_buffers */

#ifndef GL_ARB_multisample
#define GL_ARB_multisample

#define GL_MULTISAMPLE_ARB                0x809D
#define GL_SAMPLE_ALPHA_TO_COVERAGE_ARB   0x809E
#define GL_SAMPLE_ALPHA_TO_ONE_ARB        0x809F
#define GL_SAMPLE_COVERAGE_ARB            0x80A0
#define GL_SAMPLE_BUFFERS_ARB             0x80A8
#define GL_SAMPLES_ARB                    0x80A9
#define GL_SAMPLE_COVERAGE_VALUE_ARB      0x80AA
#define GL_SAMPLE_COVERAGE_INVERT_ARB     0x80AB
#define GL_MULTISAMPLE_BIT_ARB            0x20000000
#endif /* GL_ARB_multisample */

#ifndef WGL_ARB_pixel_format
#define WGL_ARB_pixel_format

#define WGL_NUMBER_PIXEL_FORMATS_ARB      0x2000
#define WGL_DRAW_TO_WINDOW_ARB            0x2001
#define WGL_DRAW_TO_BITMAP_ARB            0x2002
#define WGL_ACCELERATION_ARB              0x2003
#define WGL_NEED_PALETTE_ARB              0x2004
#define WGL_NEED_SYSTEM_PALETTE_ARB       0x2005
#define WGL_SWAP_LAYER_BUFFERS_ARB        0x2006
#define WGL_SWAP_METHOD_ARB               0x2007
#define WGL_NUMBER_OVERLAYS_ARB           0x2008
#define WGL_NUMBER_UNDERLAYS_ARB          0x2009
#define WGL_TRANSPARENT_ARB               0x200A
#define WGL_TRANSPARENT_RED_VALUE_ARB     0x2037
#define WGL_TRANSPARENT_GREEN_VALUE_ARB   0x2038
#define WGL_TRANSPARENT_BLUE_VALUE_ARB    0x2039
#define WGL_TRANSPARENT_ALPHA_VALUE_ARB   0x203A
#define WGL_TRANSPARENT_INDEX_VALUE_ARB   0x203B
#define WGL_SHARE_DEPTH_ARB               0x200C
#define WGL_SHARE_STENCIL_ARB             0x200D
#define WGL_SHARE_ACCUM_ARB               0x200E
#define WGL_SUPPORT_GDI_ARB               0x200F
#define WGL_SUPPORT_OPENGL_ARB            0x2010
#define WGL_DOUBLE_BUFFER_ARB             0x2011
#define WGL_STEREO_ARB                    0x2012
#define WGL_PIXEL_TYPE_ARB                0x2013
#define WGL_COLOR_BITS_ARB                0x2014
#define WGL_RED_BITS_ARB                  0x2015
#define WGL_RED_SHIFT_ARB                 0x2016
#define WGL_GREEN_BITS_ARB                0x2017
#define WGL_GREEN_SHIFT_ARB               0x2018
#define WGL_BLUE_BITS_ARB                 0x2019
#define WGL_BLUE_SHIFT_ARB                0x201A
#define WGL_ALPHA_BITS_ARB                0x201B
#define WGL_ALPHA_SHIFT_ARB               0x201C
#define WGL_ACCUM_BITS_ARB                0x201D
#define WGL_ACCUM_RED_BITS_ARB            0x201E
#define WGL_ACCUM_GREEN_BITS_ARB          0x201F
#define WGL_ACCUM_BLUE_BITS_ARB           0x2020
#define WGL_ACCUM_ALPHA_BITS_ARB          0x2021
#define WGL_DEPTH_BITS_ARB                0x2022
#define WGL_STENCIL_BITS_ARB              0x2023
#define WGL_AUX_BUFFERS_ARB               0x2024
#define WGL_NO_ACCELERATION_ARB           0x2025
#define WGL_GENERIC_ACCELERATION_ARB      0x2026
#define WGL_FULL_ACCELERATION_ARB         0x2027
#define WGL_SWAP_EXCHANGE_ARB             0x2028
#define WGL_SWAP_COPY_ARB                 0x2029
#define WGL_SWAP_UNDEFINED_ARB            0x202A
#define WGL_TYPE_RGBA_ARB                 0x202B
#define WGL_TYPE_COLORINDEX_ARB           0x202C
#endif /* WGL_ARB_pixel_format */

#ifndef WGL_ARB_multisample
#define WGL_ARB_multisample

#define WGL_SAMPLE_BUFFERS_ARB            0x2041
#define WGL_SAMPLES_ARB                   0x2042
#endif /* WGL_ARB_multisample */

#ifndef GL_EXT_framebuffer_multisample
#define GL_EXT_framebuffer_multisample

#define GL_RENDERBUFFER_SAMPLES_EXT       0x8CAB
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT 0x8D56
#define GL_MAX_SAMPLES_EXT                0x8D57
#endif /* GL_EXT_framebuffer_multisample */

#endif // QGL_H

#ifndef APIENTRY
#define APIENTRY
#endif

#ifndef QGL_FUNC
#define QGL_FUNC
#endif

// WGL Functions
QGL_WGL( PROC, wglGetProcAddress, ( LPCSTR ) )
QGL_WGL( int, wglChoosePixelFormat, ( HDC, CONST PIXELFORMATDESCRIPTOR * ) )
QGL_WGL( int, wglDescribePixelFormat, ( HDC, int, UINT, LPPIXELFORMATDESCRIPTOR ) )
QGL_WGL( BOOL, wglSetPixelFormat, ( HDC, int, CONST PIXELFORMATDESCRIPTOR * ) )
QGL_WGL( BOOL, wglSwapBuffers, ( HDC ) )
QGL_WGL( HGLRC, wglCreateContext, ( HDC ) )
QGL_WGL( BOOL, wglDeleteContext, ( HGLRC ) )
QGL_WGL( BOOL, wglMakeCurrent, ( HDC, HGLRC ) )
QGL_WGL( BOOL, wglShareLists, ( HGLRC, HGLRC ) )

// GLX Functions
QGL_GLX( void *, glXGetProcAddressARB, ( const GLubyte * procName ) )
QGL_GLX( XVisualInfo *, glXChooseVisual, ( Display * dpy, int screen, int *attribList ) )
QGL_GLX( GLXContext, glXCreateContext, ( Display * dpy, XVisualInfo * vis, GLXContext shareList, Bool direct ) )
QGL_GLX( void, glXDestroyContext, ( Display * dpy, GLXContext ctx ) )
QGL_GLX( Bool, glXMakeCurrent, ( Display * dpy, GLXDrawable drawable, GLXContext ctx ) )
QGL_GLX( Bool, glXCopyContext, ( Display * dpy, GLXContext src, GLXContext dst, GLuint mask ) )
QGL_GLX( Bool, glXSwapBuffers, ( Display * dpy, GLXDrawable drawable ) )
QGL_GLX( Bool, glXQueryVersion, ( Display * dpy, int *major, int *minor ) )
QGL_GLX( const char *, glXQueryExtensionsString, ( Display * dpy, int screen ) )

// EGL Functions
#ifdef EGL_VERSION_1_0
QGL_EGL( void *, eglGetProcAddress, ( const char *procname ) )
QGL_EGL( EGLBoolean, eglChooseConfig, ( EGLDisplay dpy, const EGLint * attrib_list, EGLConfig * configs, EGLint config_size, EGLint * num_config ) )
QGL_EGL( EGLContext, eglCreateContext, ( EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint * attrib_list ) )
QGL_EGL( EGLSurface, eglCreatePbufferSurface, ( EGLDisplay dpy, EGLConfig config, const EGLint * attrib_list ) )
QGL_EGL( EGLSurface, eglCreateWindowSurface, ( EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint * attrib_list ) )
QGL_EGL( EGLBoolean, eglDestroyContext, ( EGLDisplay dpy, EGLContext ctx ) )
QGL_EGL( EGLBoolean, eglDestroySurface, ( EGLDisplay dpy, EGLSurface surface ) )
QGL_EGL( EGLBoolean, eglGetConfigAttrib, ( EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint * value ) )
QGL_EGL( EGLContext, eglGetCurrentContext, ( void ) )
QGL_EGL( EGLDisplay, eglGetCurrentDisplay, ( void ) )
QGL_EGL( EGLDisplay, eglGetDisplay, ( EGLNativeDisplayType display_id ) )
QGL_EGL( EGLint, eglGetError, ( void ) )
QGL_EGL( EGLBoolean, eglInitialize, ( EGLDisplay dpy, EGLint * major, EGLint * minor ) )
QGL_EGL( EGLBoolean, eglMakeCurrent, ( EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx ) )
QGL_EGL( const char *, eglQueryString, ( EGLDisplay dpy, EGLint name ) )
QGL_EGL( EGLBoolean, eglSwapBuffers, ( EGLDisplay dpy, EGLSurface surface ) )
QGL_EGL( EGLBoolean, eglSwapInterval, ( EGLDisplay dpy, EGLint interval ) )
QGL_EGL( EGLBoolean, eglTerminate, ( EGLDisplay dpy ) )
#endif

// GL Functions
QGL_FUNC( void, glBindTexture, ( GLenum target, GLuint texture ) )
QGL_FUNC( void, glClear, ( GLbitfield mask ) )
QGL_FUNC( void, glClearColor, ( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha ) )
QGL_FUNC( void, glClearStencil, ( GLint s ) )
QGL_FUNC( void, glColorMask, ( GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha ) )
QGL_FUNC( void, glCullFace, ( GLenum mode ) )
QGL_FUNC( void, glGenTextures, ( GLsizei n, const GLuint * textures ) )
QGL_FUNC( void, glDeleteTextures, ( GLsizei n, const GLuint * textures ) )
QGL_FUNC( void, glDepthFunc, ( GLenum func ) )
QGL_FUNC( void, glDepthMask, ( GLboolean flag ) )
QGL_FUNC( void, glDisable, ( GLenum cap ) )
QGL_FUNC( void, glDrawElements, ( GLenum, GLsizei, GLenum, const GLvoid * ) )
QGL_FUNC( void, glEnable, ( GLenum cap ) )
QGL_FUNC( void, glFinish, ( void ) )
QGL_FUNC( void, glFlush, ( void ) )
QGL_FUNC( void, glFrontFace, ( GLenum mode ) )
QGL_FUNC( GLenum, glGetError, ( void ) )
QGL_FUNC( void, glGetIntegerv, ( GLenum pname, GLint * params ) )
QGL_FUNC( const GLubyte *, glGetString, ( GLenum name ) )
QGL_FUNC( void, glPixelStorei, ( GLenum pname, GLint param ) )
QGL_FUNC( void, glPolygonOffset, ( GLfloat factor, GLfloat units ) )
QGL_FUNC( void, glReadPixels, ( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid * pixels ) )
QGL_FUNC( void, glScissor, ( GLint x, GLint y, GLsizei width, GLsizei height ) )
QGL_FUNC( void, glStencilFunc, ( GLenum func, GLint ref, GLuint mask ) )
QGL_FUNC( void, glStencilMask, ( GLuint mask ) )
QGL_FUNC( void, glStencilOp, ( GLenum fail, GLenum zfail, GLenum zpass ) )
QGL_FUNC( void, glTexImage2D, ( GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * pixels ) )
QGL_FUNC( void, glTexParameteri, ( GLenum target, GLenum pname, GLint param ) )
QGL_FUNC( void, glTexSubImage2D, ( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels ) )
QGL_FUNC( void, glViewport, ( GLint x, GLint y, GLsizei width, GLsizei height ) )

#ifndef GL_ES_VERSION_2_0
QGL_FUNC( void, glClearDepth, ( GLclampd depth ) )
QGL_FUNC( void, glDepthRange, ( GLclampd zNear, GLclampd zFar ) )
QGL_FUNC( void, glDrawBuffer, ( GLenum mode ) )
QGL_FUNC( void, glReadBuffer, ( GLenum mode ) )
QGL_FUNC( void, glPolygonMode, ( GLenum face, GLenum mode ) )
#else
QGL_FUNC( void, glClearDepthf, ( GLclampf depth ) )
QGL_FUNC( void, glDepthRangef, ( GLclampf zNear, GLclampf zFar ) )
QGL_FUNC( void, glGetShaderPrecisionFormat, ( GLenum shaderType, GLenum precisionType, GLint * range, GLint * precision ) )
#ifndef qglClearDepth
#define qglClearDepth qglClearDepthf
#define qglDepthRange qglDepthRangef
#endif
#endif

#ifndef GL_ES_VERSION_2_0
QGL_EXT( void, glActiveTextureARB, ( GLenum ) )
QGL_EXT( void, glClientActiveTextureARB, ( GLenum ) )
QGL_EXT( void, glDrawRangeElementsEXT, ( GLenum, GLuint, GLuint, GLsizei, GLenum, const GLvoid * ) )
QGL_EXT( void, glBindBufferARB, ( GLenum target, GLuint buffer ) )
QGL_EXT( void, glDeleteBuffersARB, ( GLsizei n, const GLuint * buffers ) )
QGL_EXT( void, glGenBuffersARB, ( GLsizei n, GLuint * buffers ) )
QGL_EXT( void, glBufferDataARB, ( GLenum target, GLsizeiptrARB size, const GLvoid * data, GLenum usage ) )
QGL_EXT( void, glBufferSubDataARB, ( GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid * data ) )
#else
QGL_FUNC( void, glActiveTexture, ( GLenum ) )
QGL_FUNC_OPT( void, glDrawRangeElements, ( GLenum, GLuint, GLuint, GLsizei, GLenum, const GLvoid * ) )
QGL_FUNC( void, glBindBuffer, ( GLenum target, GLuint buffer ) )
QGL_FUNC( void, glDeleteBuffers, ( GLsizei n, const GLuint * buffers ) )
QGL_FUNC( void, glGenBuffers, ( GLsizei n, GLuint * buffers ) )
QGL_FUNC( void, glBufferData, ( GLenum target, GLsizeiptrARB size, const GLvoid * data, GLenum usage ) )
QGL_FUNC( void, glBufferSubData, ( GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid * data ) )
#ifndef qglActiveTextureARB
#define qglActiveTextureARB qglActiveTexture
#define qglDrawRangeElementsEXT qglDrawRangeElements
#define qglBindBufferARB qglBindBuffer
#define qglDeleteBuffersARB qglDeleteBuffers
#define qglGenBuffersARB qglGenBuffers
#define qglBufferDataARB qglBufferData
#define qglBufferSubDataARB qglBufferSubData
#endif
#endif

#ifndef GL_ES_VERSION_2_0
QGL_EXT( void, glDeleteObjectARB, ( GLhandleARB obj ) )
QGL_EXT( void, glDetachObjectARB, ( GLhandleARB containerObj, GLhandleARB attachedObj ) )
QGL_EXT( GLhandleARB, glCreateShaderObjectARB, ( GLenum shaderType ) )
QGL_EXT( void, glShaderSourceARB, ( GLhandleARB shaderObj, GLsizei count, const GLcharARB **string, const GLint * length ) )
QGL_EXT( void, glCompileShaderARB, ( GLhandleARB shaderObj ) )
QGL_EXT( GLhandleARB, glCreateProgramObjectARB, ( void ) )
QGL_EXT( void, glAttachObjectARB, ( GLhandleARB containerObj, GLhandleARB obj ) )
QGL_EXT( void, glLinkProgramARB, ( GLhandleARB programObj ) )
QGL_EXT( void, glUseProgramObjectARB, ( GLhandleARB programObj ) )
QGL_EXT( void, glValidateProgramARB, ( GLhandleARB programObj ) )
QGL_EXT( void, glUniform1fARB, ( GLint location, GLfloat v0 ) )
QGL_EXT( void, glUniform2fARB, ( GLint location, GLfloat v0, GLfloat v1 ) )
QGL_EXT( void, glUniform3fARB, ( GLint location, GLfloat v0, GLfloat v1, GLfloat v2 ) )
QGL_EXT( void, glUniform4fARB, ( GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3 ) )
QGL_EXT( void, glUniform1iARB, ( GLint location, GLint v0 ) )
QGL_EXT( void, glUniform2iARB, ( GLint location, GLint v0, GLint v1 ) )
QGL_EXT( void, glUniform3iARB, ( GLint location, GLint v0, GLint v1, GLint v2 ) )
QGL_EXT( void, glUniform4iARB, ( GLint location, GLint v0, GLint v1, GLint v2, GLint v3 ) )
QGL_EXT( void, glUniform1fvARB, ( GLint location, GLsizei count, const GLfloat * value ) )
QGL_EXT( void, glUniform2fvARB, ( GLint location, GLsizei count, const GLfloat * value ) )
QGL_EXT( void, glUniform3fvARB, ( GLint location, GLsizei count, const GLfloat * value ) )
QGL_EXT( void, glUniform4fvARB, ( GLint location, GLsizei count, const GLfloat * value ) )
QGL_EXT( void, glUniform1ivARB, ( GLint location, GLsizei count, const GLint * value ) )
QGL_EXT( void, glUniform2ivARB, ( GLint location, GLsizei count, const GLint * value ) )
QGL_EXT( void, glUniform3ivARB, ( GLint location, GLsizei count, const GLint * value ) )
QGL_EXT( void, glUniform4ivARB, ( GLint location, GLsizei count, const GLint * value ) )
QGL_EXT( void, glUniformMatrix2fvARB, ( GLint location, GLsizei count, GLboolean transpose, const GLfloat * value ) )
QGL_EXT( void, glUniformMatrix3fvARB, ( GLint location, GLsizei count, GLboolean transpose, const GLfloat * value ) )
QGL_EXT( void, glUniformMatrix4fvARB, ( GLint location, GLsizei count, GLboolean transpose, const GLfloat * value ) )
QGL_EXT( void, glGetObjectParameterivARB, ( GLhandleARB obj, GLenum pname, GLint * params ) )
QGL_EXT( void, glGetInfoLogARB, ( GLhandleARB obj, GLsizei maxLength, GLsizei * length, GLcharARB * infoLog ) )
QGL_EXT( void, glGetAttachedObjectsARB, ( GLhandleARB containerObj, GLsizei maxCount, GLsizei * count, GLhandleARB * obj ) )
QGL_EXT( GLint, glGetUniformLocationARB, ( GLhandleARB programObj, const GLcharARB * name ) )
QGL_EXT( void, glGetActiveUniformARB, ( GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei * length, GLint * size, GLenum * type, GLcharARB * name ) )
QGL_EXT( void, glGetUniformfvARB, ( GLhandleARB programObj, GLint location, GLfloat * params ) )
QGL_EXT( void, glGetUniformivARB, ( GLhandleARB programObj, GLint location, GLint * params ) )
QGL_EXT( void, glGetShaderSourceARB, ( GLhandleARB obj, GLsizei maxLength, GLsizei * length, GLcharARB * source ) )

QGL_EXT( void, glDeleteProgram, ( GLhandleARB programObj ) )
QGL_EXT( void, glDeleteShader, ( GLhandleARB shaderObj ) )
QGL_EXT( void, glDetachShader, ( GLhandleARB programObj, GLhandleARB shaderObj ) )
QGL_EXT( GLhandleARB, glCreateShader, ( GLenum shaderType ) )
QGL_EXT( GLhandleARB, glCreateProgram, ( void ) )
QGL_EXT( void, glAttachShader, ( GLhandleARB programObj, GLhandleARB shaderObj ) )
QGL_EXT( void, glUseProgram, ( GLhandleARB programObj ) )
QGL_EXT( void, glGetProgramiv, ( GLhandleARB programObj, GLenum pname, GLint * params ) )
QGL_EXT( void, glGetShaderiv, ( GLhandleARB shaderObj, GLenum pname, GLint * params ) )
QGL_EXT( void, glGetProgramInfoLog, ( GLhandleARB programObj, GLsizei maxLength, GLsizei * length, GLcharARB * infoLog ) )
QGL_EXT( void, glGetShaderInfoLog, ( GLhandleARB shaderObj, GLsizei maxLength, GLsizei * length, GLcharARB * infoLog ) )
QGL_EXT( void, glGetAttachedShaders, ( GLhandleARB programObj, GLsizei maxCount, GLsizei * count, GLhandleARB * shaders ) )
#else
QGL_FUNC( void, glDeleteProgram, ( GLhandleARB programObj ) )
QGL_FUNC( void, glDeleteShader, ( GLhandleARB shaderObj ) )
QGL_FUNC( void, glDetachShader, ( GLhandleARB programObj, GLhandleARB shaderObj ) )
QGL_FUNC( GLhandleARB, glCreateShader, ( GLenum shaderType ) )
QGL_FUNC( void, glShaderSource, ( GLhandleARB shaderObj, GLsizei count, const GLcharARB **string, const GLint * length ) )
QGL_FUNC( void, glCompileShader, ( GLhandleARB shaderObj ) )
QGL_FUNC( GLhandleARB, glCreateProgram, ( void ) )
QGL_FUNC( void, glAttachShader, ( GLhandleARB programObj, GLhandleARB shaderObj ) )
QGL_FUNC( void, glLinkProgram, ( GLhandleARB programObj ) )
QGL_FUNC( void, glUseProgram, ( GLhandleARB programObj ) )
QGL_FUNC( void, glValidateProgram, ( GLhandleARB programObj ) )
QGL_FUNC( void, glUniform1f, ( GLint location, GLfloat v0 ) )
QGL_FUNC( void, glUniform2f, ( GLint location, GLfloat v0, GLfloat v1 ) )
QGL_FUNC( void, glUniform3f, ( GLint location, GLfloat v0, GLfloat v1, GLfloat v2 ) )
QGL_FUNC( void, glUniform4f, ( GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3 ) )
QGL_FUNC( void, glUniform1i, ( GLint location, GLint v0 ) )
QGL_FUNC( void, glUniform2i, ( GLint location, GLint v0, GLint v1 ) )
QGL_FUNC( void, glUniform3i, ( GLint location, GLint v0, GLint v1, GLint v2 ) )
QGL_FUNC( void, glUniform4i, ( GLint location, GLint v0, GLint v1, GLint v2, GLint v3 ) )
QGL_FUNC( void, glUniform1fv, ( GLint location, GLsizei count, const GLfloat * value ) )
QGL_FUNC( void, glUniform2fv, ( GLint location, GLsizei count, const GLfloat * value ) )
QGL_FUNC( void, glUniform3fv, ( GLint location, GLsizei count, const GLfloat * value ) )
QGL_FUNC( void, glUniform4fv, ( GLint location, GLsizei count, const GLfloat * value ) )
QGL_FUNC( void, glUniform1iv, ( GLint location, GLsizei count, const GLint * value ) )
QGL_FUNC( void, glUniform2iv, ( GLint location, GLsizei count, const GLint * value ) )
QGL_FUNC( void, glUniform3iv, ( GLint location, GLsizei count, const GLint * value ) )
QGL_FUNC( void, glUniform4iv, ( GLint location, GLsizei count, const GLint * value ) )
QGL_FUNC( void, glUniformMatrix2fv, ( GLint location, GLsizei count, GLboolean transpose, const GLfloat * value ) )
QGL_FUNC( void, glUniformMatrix3fv, ( GLint location, GLsizei count, GLboolean transpose, const GLfloat * value ) )
QGL_FUNC( void, glUniformMatrix4fv, ( GLint location, GLsizei count, GLboolean transpose, const GLfloat * value ) )
QGL_FUNC( void, glGetProgramiv, ( GLhandleARB programObj, GLenum pname, GLint * params ) )
QGL_FUNC( void, glGetShaderiv, ( GLhandleARB shaderObj, GLenum pname, GLint * params ) )
QGL_FUNC( void, glGetProgramInfoLog, ( GLhandleARB programObj, GLsizei maxLength, GLsizei * length, GLcharARB * infoLog ) )
QGL_FUNC( void, glGetShaderInfoLog, ( GLhandleARB shaderObj, GLsizei maxLength, GLsizei * length, GLcharARB * infoLog ) )
QGL_FUNC( void, glGetAttachedShaders, ( GLhandleARB programObj, GLsizei maxCount, GLsizei * count, GLhandleARB * shaders ) )
QGL_FUNC( GLint, glGetUniformLocation, ( GLhandleARB programObj, const GLcharARB * name ) )
QGL_FUNC( void, glGetActiveUniform, ( GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei * length, GLint * size, GLenum * type, GLcharARB * name ) )
QGL_FUNC( void, glGetUniformfv, ( GLhandleARB programObj, GLint location, GLfloat * params ) )
QGL_FUNC( void, glGetUniformiv, ( GLhandleARB programObj, GLint location, GLint * params ) )
QGL_FUNC( void, glGetShaderSource, ( GLhandleARB obj, GLsizei maxLength, GLsizei * length, GLcharARB * source ) )
#ifndef qglShaderSourceARB
#define qglShaderSourceARB qglShaderSource
#define qglCompileShaderARB qglCompileShader
#define qglLinkProgramARB qglLinkProgram
#define qglValidateProgramARB qglValidateProgram
#define qglUniform1fARB qglUniform1f
#define qglUniform2fARB qglUniform2f
#define qglUniform3fARB qglUniform3f
#define qglUniform4fARB qglUniform4f
#define qglUniform1iARB qglUniform1i
#define qglUniform2iARB qglUniform2i
#define qglUniform3iARB qglUniform3i
#define qglUniform4iARB qglUniform4i
#define qglUniform1fvARB qglUniform1fv
#define qglUniform2fvARB qglUniform2fv
#define qglUniform3fvARB qglUniform3fv
#define qglUniform4fvARB qglUniform4fv
#define qglUniform1ivARB qglUniform1iv
#define qglUniform2ivARB qglUniform2iv
#define qglUniform3ivARB qglUniform3iv
#define qglUniform4ivARB qglUniform4iv
#define qglUniformMatrix2fvARB qglUniformMatrix2fv
#define qglUniformMatrix3fvARB qglUniformMatrix3fv
#define qglUniformMatrix4fvARB qglUniformMatrix4fv
#define qglGetUniformLocationARB qglGetUniformLocation
#define qglGetActiveUniformARB qglGetActiveUniform
#define qglGetUniformfvARB qglGetUniformfv
#define qglGetUniformivARB qglGetUniformiv
#define qglGetShaderSourceARB qglGetShaderSource
#endif
#endif

#ifndef GL_ES_VERSION_2_0
QGL_EXT( void, glVertexAttribPointerARB, ( GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer ) )
QGL_EXT( void, glEnableVertexAttribArrayARB, ( GLuint index ) )
QGL_EXT( void, glDisableVertexAttribArrayARB, ( GLuint index ) )
QGL_EXT( void, glBindAttribLocationARB, ( GLhandleARB programObj, GLuint index, const GLcharARB * name ) )
QGL_EXT( void, glGetActiveAttribARB, ( GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei * length, GLint * size, GLenum * type, GLcharARB * name ) )
QGL_EXT( GLint, glGetAttribLocationARB, ( GLhandleARB programObj, const GLcharARB * name ) )
#else
QGL_FUNC( void, glVertexAttribPointer, ( GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer ) )
QGL_FUNC( void, glEnableVertexAttribArray, ( GLuint index ) )
QGL_FUNC( void, glDisableVertexAttribArray, ( GLuint index ) )
QGL_FUNC( void, glBindAttribLocation, ( GLhandleARB programObj, GLuint index, const GLcharARB * name ) )
QGL_FUNC( void, glGetActiveAttrib, ( GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei * length, GLint * size, GLenum * type, GLcharARB * name ) )
QGL_FUNC( GLint, glGetAttribLocation, ( GLhandleARB programObj, const GLcharARB * name ) )
#ifndef qglVertexAttribPointerARB
#define qglVertexAttribPointerARB qglVertexAttribPointer
#define qglEnableVertexAttribArrayARB qglEnableVertexAttribArray
#define qglDisableVertexAttribArrayARB qglDisableVertexAttribArray
#define qglBindAttribLocationARB qglBindAttribLocation
#define qglGetActiveAttribARB qglGetActiveAttrib
#define qglGetAttribLocationARB qglGetAttribLocationARB
#endif
#endif

#ifndef GL_ES_VERSION_2_0
QGL_EXT( void, glBindFragDataLocation, ( GLuint programObj, GLuint index, const GLcharARB * name ) )
#endif

#ifndef GL_ES_VERSION_2_0
QGL_EXT( void, glDrawArraysInstancedARB, ( GLenum mode, GLint first, GLsizei count, GLsizei primcount ) )
QGL_EXT( void, glDrawElementsInstancedARB, ( GLenum mode, GLsizei count, GLenum type, const GLvoid * indices, GLsizei primcount ) )
QGL_EXT( void, glVertexAttribDivisorARB, ( GLuint index, GLuint divisor ) )
#else
QGL_FUNC_OPT( void, glDrawArraysInstanced, ( GLenum mode, GLint first, GLsizei count, GLsizei primcount ) )
QGL_FUNC_OPT( void, glDrawElementsInstanced, ( GLenum mode, GLsizei count, GLenum type, const GLvoid * indices, GLsizei primcount ) )
QGL_FUNC_OPT( void, glVertexAttribDivisor, ( GLuint index, GLuint divisor ) )
#ifndef qglDrawArraysInstancedARB
#define qglDrawArraysInstancedARB qglDrawArraysInstanced
#define qglDrawElementsInstancedARB qglDrawElementsInstanced
#define qglVertexAttribDivisorARB qglVertexAttribDivisor
#endif
#endif

#ifndef GL_ES_VERSION_2_0
QGL_EXT( GLboolean, glIsRenderbufferEXT, ( GLuint ) )
QGL_EXT( void, glBindRenderbufferEXT, ( GLenum, GLuint ) )
QGL_EXT( void, glDeleteRenderbuffersEXT, ( GLsizei, const GLuint * ) )
QGL_EXT( void, glGenRenderbuffersEXT, ( GLsizei, GLuint * ) )
QGL_EXT( void, glRenderbufferStorageEXT, ( GLenum, GLenum, GLsizei, GLsizei ) )
QGL_EXT( void, glGetRenderbufferParameterivEXT, ( GLenum, GLenum, GLint * ) )
QGL_EXT( GLboolean, glIsFramebufferEXT, ( GLuint ) )
QGL_EXT( void, glBindFramebufferEXT, ( GLenum, GLuint ) )
QGL_EXT( void, glDeleteFramebuffersEXT, ( GLsizei, const GLuint * ) )
QGL_EXT( void, glGenFramebuffersEXT, ( GLsizei, GLuint * ) )
QGL_EXT( GLenum, glCheckFramebufferStatusEXT, ( GLenum ) )
QGL_EXT( void, glFramebufferTexture1DEXT, ( GLenum, GLenum, GLenum, GLuint, GLint ) )
QGL_EXT( void, glFramebufferTexture2DEXT, ( GLenum, GLenum, GLenum, GLuint, GLint ) )
QGL_EXT( void, glFramebufferRenderbufferEXT, ( GLenum, GLenum, GLenum, GLuint ) )
QGL_EXT( void, glGetFramebufferAttachmentParameterivEXT, ( GLenum, GLenum, GLenum, GLint * ) )
QGL_EXT( void, glGenerateMipmapEXT, ( GLenum ) )
#else
QGL_FUNC( GLboolean, glIsRenderbuffer, ( GLuint ) )
QGL_FUNC( void, glBindRenderbuffer, ( GLenum, GLuint ) )
QGL_FUNC( void, glDeleteRenderbuffers, ( GLsizei, const GLuint * ) )
QGL_FUNC( void, glGenRenderbuffers, ( GLsizei, GLuint * ) )
QGL_FUNC( void, glRenderbufferStorage, ( GLenum, GLenum, GLsizei, GLsizei ) )
QGL_FUNC( void, glGetRenderbufferParameteriv, ( GLenum, GLenum, GLint * ) )
QGL_FUNC( GLboolean, glIsFramebuffer, ( GLuint ) )
QGL_FUNC( void, glBindFramebuffer, ( GLenum, GLuint ) )
QGL_FUNC( void, glDeleteFramebuffers, ( GLsizei, const GLuint * ) )
QGL_FUNC( void, glGenFramebuffers, ( GLsizei, GLuint * ) )
QGL_FUNC( GLenum, glCheckFramebufferStatus, ( GLenum ) )
QGL_FUNC( void, glFramebufferTexture2D, ( GLenum, GLenum, GLenum, GLuint, GLint ) )
QGL_FUNC( void, glFramebufferRenderbuffer, ( GLenum, GLenum, GLenum, GLuint ) )
QGL_FUNC( void, glGetFramebufferAttachmentParameteriv, ( GLenum, GLenum, GLenum, GLint * ) )
QGL_FUNC( void, glGenerateMipmap, ( GLenum ) )
#ifndef qglIsRenderbufferEXT
#define qglIsRenderbufferEXT qglIsRenderbuffer
#define qglBindRenderbufferEXT qglBindRenderbuffer
#define qglDeleteRenderbuffersEXT qglDeleteRenderbuffers
#define qglGenRenderbuffersEXT qglGenRenderbuffers
#define qglRenderbufferStorageEXT qglRenderbufferStorage
#define qglGetRenderbufferParameterivEXT qglGetRenderbufferParameteriv
#define qglIsFramebufferEXT qglIsFramebuffer
#define qglBindFramebufferEXT qglBindFramebuffer
#define qglDeleteFramebuffersEXT qglDeleteFramebuffers
#define qglGenFramebuffersEXT qglGenFramebuffers
#define qglCheckFramebufferStatusEXT qglCheckFramebufferStatus
#define qglFramebufferTexture2DEXT qglFramebufferTexture2D
#define qglFramebufferRenderbufferEXT qglFramebufferRenderbuffer
#define qglGetFramebufferAttachmentParameterivEXT qglGetFramebufferAttachmentParameteriv
#define qglGenerateMipmapEXT qglGenerateMipmap
#endif
#endif

#ifndef GL_ES_VERSION_2_0
QGL_EXT( void, glBlitFramebufferEXT, ( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum ) )
#else
QGL_FUNC_OPT( void, glBlitFramebuffer, ( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum ) )
QGL_EXT( void, glBlitFramebufferANGLE, ( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum ) )
QGL_EXT( void, glBlitFramebufferNV, ( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum ) )
#ifndef qglBlitFramebufferEXT
#define qglBlitFramebufferEXT qglBlitFramebuffer
#endif
#endif

#ifndef GL_ES_VERSION_2_0
QGL_EXT( void, glRenderbufferStorageMultisampleEXT, ( GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height ) )
#endif

#ifdef GL_ES_VERSION_2_0
QGL_EXT( void, glReadBufferIndexedEXT, ( GLenum, GLint ) )
QGL_EXT( void, glDrawBuffersIndexedEXT, ( GLint, const GLenum *, const GLint * ) )
#endif

#ifndef GL_ES_VERSION_2_0
QGL_EXT( void, glProgramParameteri, ( GLuint program, GLenum pname, GLint value ) )
QGL_EXT( void, glGetProgramBinary, ( GLuint program, GLsizei bufSize, GLsizei * length, GLenum * binaryFormat, GLvoid * binary ) )
QGL_EXT( void, glProgramBinary, ( GLuint program, GLenum binaryFormat, const GLvoid * binary, GLsizei length ) )
#else
QGL_EXT( void, glGetProgramBinaryOES, ( GLuint program, GLsizei bufSize, GLsizei * length, GLenum * binaryFormat, GLvoid * binary ) )
QGL_EXT( void, glProgramBinaryOES, ( GLuint program, GLenum binaryFormat, const GLvoid * binary, GLsizei length ) )
QGL_FUNC_OPT( void, glProgramParameteri, ( GLuint program, GLenum pname, GLint value ) )
QGL_FUNC_OPT( void, glGetProgramBinary, ( GLuint program, GLsizei bufSize, GLsizei * length, GLenum * binaryFormat, GLvoid * binary ) )
QGL_FUNC_OPT( void, glProgramBinary, ( GLuint program, GLenum binaryFormat, const GLvoid * binary, GLsizei length ) )
#endif

#ifndef GL_ES_VERSION_2_0
QGL_EXT( void, glCompressedTexImage2DARB, ( GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid * ) )
QGL_EXT( void, glCompressedTexSubImage2DARB, ( GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid * ) )
#else
QGL_FUNC( void, glCompressedTexImage2D, ( GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid * ) )
QGL_FUNC( void, glCompressedTexSubImage2D, ( GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid * ) )
#ifndef qglCompressedTexImage2DARB
#define qglCompressedTexImage2DARB qglCompressedTexImage2D
#define qglCompressedTexSubImage2DARB qglCompressedTexSubImage2D
#endif
#endif

#ifndef GL_ES_VERSION_2_0
QGL_EXT( void, glBlendFuncSeparateEXT, ( GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha ) )
#else
QGL_FUNC( void, glBlendFuncSeparate, ( GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha ) )
#ifndef qglBlendFuncSeparateEXT
#define qglBlendFuncSeparateEXT qglBlendFuncSeparate
#endif
#endif

#ifndef GL_ES_VERSION_2_0
QGL_EXT( void, glTexImage3DEXT, ( GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid * pixels ) )
QGL_EXT( void, glTexSubImage3DEXT, ( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels ) )
#else
QGL_EXT( void, glTexImage3DOES, ( GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid * pixels ) )
QGL_EXT( void, glTexSubImage3DOES, ( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels ) )
QGL_FUNC_OPT( void, glTexImage3D, ( GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid * pixels ) )
QGL_FUNC_OPT( void, glTexSubImage3D, ( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels ) )
#ifndef qglTexImage3DEXT
#define qglTexImage3DEXT qglTexImage3DOES
#define qglTexSubImage3DEXT qglTexSubImage3DOES
#endif

#endif

#ifndef GL_ES_VERSION_2_0
QGL_EXT( void, glDrawBuffersARB, ( GLsizei n, const GLenum * bufs ) )
#endif

#ifndef GL_ES_VERSION_2_0
QGL_EXT( void, glSampleCoverageARB, ( GLfloat value, GLboolean invert ) )
#endif

// WGL_EXT Functions
QGL_WGL_EXT( const char *, wglGetExtensionsStringEXT, ( void ) )
QGL_WGL_EXT( BOOL, wglGetDeviceGammaRamp3DFX, ( HDC, WORD * ) )
QGL_WGL_EXT( BOOL, wglSetDeviceGammaRamp3DFX, ( HDC, WORD * ) )
QGL_WGL_EXT( BOOL, wglSwapIntervalEXT, ( int interval ) )

QGL_WGL_EXT( BOOL, wglGetPixelFormatAttribivARB, ( HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues ) )
QGL_WGL_EXT( BOOL, wglGetPixelFormatAttribfvARB, ( HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, FLOAT * pfValues ) )
QGL_WGL_EXT( BOOL, wglChoosePixelFormatARB, ( HDC hdc, const int *piAttribIList, const FLOAT * pfAttribFList, UINT nMaxFormats, int *piFormats, UINT * nNumFormats ) )

// GLX_EXT Functions
QGL_GLX_EXT( int, glXSwapIntervalSGI, ( int interval ) )
