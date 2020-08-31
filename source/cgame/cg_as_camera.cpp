/*
Copyright (C) 2018 Victor Luchits

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

#include "cg_as_local.h"

//=======================================================================

static const gs_asEnumVal_t asCameraTypeEnumVals[] =
{
	ASLIB_ENUM_VAL( VIEWDEF_DEMOCAM ),
	ASLIB_ENUM_VAL( VIEWDEF_PLAYERVIEW ),
	ASLIB_ENUM_VAL( VIEWDEF_OVERHEAD ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asRefdefFlagsEnumVals[] =
{
	ASLIB_ENUM_VAL( RDF_UNDERWATER ),
	ASLIB_ENUM_VAL( RDF_NOWORLDMODEL ),
	ASLIB_ENUM_VAL( RDF_SKYPORTALINVIEW ),
	ASLIB_ENUM_VAL( RDF_FLIPPED ),
	ASLIB_ENUM_VAL( RDF_WORLDOUTLINES ),
	ASLIB_ENUM_VAL( RDF_CROSSINGWATER ),
	ASLIB_ENUM_VAL( RDF_USEORTHO ),
	ASLIB_ENUM_VAL( RDF_BLURRED ),

	ASLIB_ENUM_VAL_NULL
};

const gs_asEnum_t asCGameCameraEnums[] =
{
	{ "cg_cameratype_e", asCameraTypeEnumVals },
	{ "cg_rdflags_e", asRefdefFlagsEnumVals },

	ASLIB_ENUM_VAL_NULL
};

//=======================================================================

static int objectViewport_getAspectRatio( vrect_t *rect ) {
	return (float)rect->width / (float)rect->height;
}

static int objectViewport_getScreenWidth( vrect_t *rect ) {
	return cgs.vidWidth;
}

static int objectViewport_getScreenHeight( vrect_t *rect ) {
	return cgs.vidHeight;
}

static float objectViewport_getScreenAspectRatio( vrect_t *rect ) {
	return (float)cgs.vidWidth / cgs.vidHeight;
}

static float objectViewport_getScreenPixelRatio( vrect_t *rect ) {
	return cgs.pixelRatio;
}

static const gs_asFuncdef_t asviewport_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const gs_asBehavior_t asviewport_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL
};

static const gs_asMethod_t asviewport_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( int, get_aspectRatio, ( ) const ), asFUNCTION( objectViewport_getAspectRatio ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( int, get_screenWidth, ( ) const ), asFUNCTION( objectViewport_getScreenWidth ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( int, get_screenHeight, ( ) const ), asFUNCTION( objectViewport_getScreenHeight ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( float, get_screenAspectRatio, ( ) const ), asFUNCTION( objectViewport_getScreenAspectRatio ), asCALL_CDECL_OBJLAST },
	{ ASLIB_FUNCTION_DECL( float, get_screenPixelRatio, ( ) const ), asFUNCTION( objectViewport_getScreenPixelRatio ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL
};

static const gs_asProperty_t asviewport_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( const int, x ), ASLIB_FOFFSET( vrect_t, x ) },
	{ ASLIB_PROPERTY_DECL( const int, y ), ASLIB_FOFFSET( vrect_t, y ) },
	{ ASLIB_PROPERTY_DECL( const int, width ), ASLIB_FOFFSET( vrect_t, width ) },
	{ ASLIB_PROPERTY_DECL( const int, height ), ASLIB_FOFFSET( vrect_t, height ) },

	ASLIB_PROPERTY_NULL
};

static const gs_asClassDescriptor_t asViewportClassDescriptor =
{
	"Viewport",                  /* name */
	asOBJ_REF | asOBJ_NOCOUNT,   /* object type flags */
	sizeof( vrect_t ),           /* size */
	asviewport_Funcdefs,         /* funcdefs */
	asviewport_ObjectBehaviors,  /* object behaviors */
	asviewport_Methods,          /* methods */
	asviewport_Properties,       /* properties */
	NULL, NULL                   /* string factory hack */
};

//=======================================================================

static refdef_t *asCamera_getRefdef( cg_viewdef_t *view )
{
	return &view->refdef;
}

static const gs_asFuncdef_t ascamera_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL,
};

static const gs_asBehavior_t ascamera_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL,
};

static const gs_asMethod_t ascamera_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( Refdef @, get_refdef, () const ), asFUNCTION( asCamera_getRefdef ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL,
};

static const gs_asProperty_t ascamera_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( int, type ), ASLIB_FOFFSET( cg_viewdef_t, type ) },
	{ ASLIB_PROPERTY_DECL( int, POVent ), ASLIB_FOFFSET( cg_viewdef_t, POVent ) },
	{ ASLIB_PROPERTY_DECL( bool, flipped ), ASLIB_FOFFSET( cg_viewdef_t, flipped ) },
	{ ASLIB_PROPERTY_DECL( bool, thirdPerson ), ASLIB_FOFFSET( cg_viewdef_t, thirdperson ) },
	{ ASLIB_PROPERTY_DECL( bool, playerPrediction ), ASLIB_FOFFSET( cg_viewdef_t, playerPrediction ) },
	{ ASLIB_PROPERTY_DECL( bool, drawWeapon ), ASLIB_FOFFSET( cg_viewdef_t, drawWeapon ) },
	{ ASLIB_PROPERTY_DECL( bool, draw2D ), ASLIB_FOFFSET( cg_viewdef_t, draw2D ) },
	{ ASLIB_PROPERTY_DECL( float, fovX ), ASLIB_FOFFSET( cg_viewdef_t, fov_x ) },
	{ ASLIB_PROPERTY_DECL( float, fovY ), ASLIB_FOFFSET( cg_viewdef_t, fov_y ) },
	{ ASLIB_PROPERTY_DECL( float, stereoSeparation ), ASLIB_FOFFSET( cg_viewdef_t, stereoSeparation ) },
	{ ASLIB_PROPERTY_DECL( Vec3, origin ), ASLIB_FOFFSET( cg_viewdef_t, origin ) },
	{ ASLIB_PROPERTY_DECL( Vec3, angles ), ASLIB_FOFFSET( cg_viewdef_t, angles ) },
	{ ASLIB_PROPERTY_DECL( Vec3, velocity ), ASLIB_FOFFSET( cg_viewdef_t, velocity ) },
	{ ASLIB_PROPERTY_DECL( Mat3, axis ), ASLIB_FOFFSET( cg_viewdef_t, axis ) },

	ASLIB_PROPERTY_NULL
};

static const gs_asClassDescriptor_t asCameraClassDescriptor =
{
	"Camera",                    /* name */
	asOBJ_REF | asOBJ_NOCOUNT,   /* object type flags */
	sizeof( cg_viewdef_t ),      /* size */
	ascamera_Funcdefs,           /* funcdefs */
	ascamera_ObjectBehaviors,    /* object behaviors */
	ascamera_Methods,            /* methods */
	ascamera_Properties,         /* properties */
	NULL, NULL                   /* string factory hack */
};

//=======================================================================

static asvec3_t asRefdef_transformToScreen( asvec3_t *vec, refdef_t *rd )
{
	asvec3_t res;
	trap_R_TransformVectorToScreen( rd, vec->v, res.v );
	return res;
}

static const gs_asFuncdef_t asrefdef_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL,
};

static const gs_asBehavior_t asrefdef_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL,
};

static const gs_asMethod_t asrefdef_Methods[] =
{
	{ ASLIB_FUNCTION_DECL( Vec3, transformToScreen, ( const Vec3 &in ) const ), asFUNCTION( asRefdef_transformToScreen ), asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL,
};

static const gs_asProperty_t asrefdef_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( int, x ), ASLIB_FOFFSET( refdef_t, x ) },
	{ ASLIB_PROPERTY_DECL( int, y ), ASLIB_FOFFSET( refdef_t, y ) },
	{ ASLIB_PROPERTY_DECL( int, width ), ASLIB_FOFFSET( refdef_t, width ) },
	{ ASLIB_PROPERTY_DECL( int, height ), ASLIB_FOFFSET( refdef_t, height ) },
	{ ASLIB_PROPERTY_DECL( int, rdflags ), ASLIB_FOFFSET( refdef_t, rdflags ) },

	{ ASLIB_PROPERTY_DECL( int, scissorX ), ASLIB_FOFFSET( refdef_t, scissor_x ) },
	{ ASLIB_PROPERTY_DECL( int, scissorY ), ASLIB_FOFFSET( refdef_t, scissor_y ) },
	{ ASLIB_PROPERTY_DECL( int, scissorWidth ), ASLIB_FOFFSET( refdef_t, scissor_width ) },
	{ ASLIB_PROPERTY_DECL( int, scissorHeight ), ASLIB_FOFFSET( refdef_t, scissor_height ) },

	{ ASLIB_PROPERTY_DECL( int, orthoX ), ASLIB_FOFFSET( refdef_t, ortho_x ) },
	{ ASLIB_PROPERTY_DECL( int, orthoY ), ASLIB_FOFFSET( refdef_t, ortho_y ) },

	{ ASLIB_PROPERTY_DECL( Vec3, viewOrigin ), ASLIB_FOFFSET( refdef_t, vieworg ) },

	ASLIB_PROPERTY_NULL
};

static const gs_asClassDescriptor_t asRefdefClassDescriptor =
{
	"Refdef",                    /* name */
	asOBJ_REF | asOBJ_NOCOUNT,   /* object type flags */
	sizeof( refdef_t ),          /* size */
	asrefdef_Funcdefs,           /* funcdefs */
	asrefdef_ObjectBehaviors,    /* object behaviors */
	asrefdef_Methods,            /* methods */
	asrefdef_Properties,         /* properties */
	NULL, NULL                   /* string factory hack */
};

//=======================================================================

static cg_viewdef_t *CG_asGetMainCamera( void ) {
	return &cg.view;
}

static vrect_t *CG_asGetViewport( void ) {
	return &scr_vrect;
}

const gs_asClassDescriptor_t * const asCGameCameraClassesDescriptors[] =
{
	&asViewportClassDescriptor,
	&asCameraClassDescriptor,
	&asRefdefClassDescriptor,

	NULL
};

static asvec3_t CG_asViewSmoothPredictedSteps( asvec3_t v ) {
	asvec3_t p;
	VectorCopy( v.v, p.v );
	CG_ViewSmoothPredictedSteps( p.v );
	return p;
}

const gs_asglobfuncs_t asCGameCameraGlobalFuncs[] =
{
	{ "Viewport @GetViewport()", asFUNCTION( CG_asGetViewport ), NULL },
	{ "Camera @GetMainCamera()", asFUNCTION( CG_asGetMainCamera ), NULL },
	{ "float CalcVerticalFov( float fovX, float width, float height )", asFUNCTION( CalcVerticalFov ), NULL },
	{ "float CalcHorizontalFov( float fovY, float width, float height )", asFUNCTION( CalcHorizontalFov ), NULL },
	{ "Vec3 SmoothPredictedSteps( Vec3 &in org )", asFUNCTION( CG_asViewSmoothPredictedSteps ), NULL },

	{ NULL }
};

//======================================================================

/*
* CG_asSetupCamera
*/
void CG_asSetupCamera( cg_viewdef_t *view ) {
	CG_asCallScriptFunc( cgs.asCamera.setupCamera, [view](asIScriptContext *ctx)
		{
			ctx->SetArgObject( 0, view );
		},
		cg_empty_as_cb
	);
}

/*
* CG_asSetupRefdef
*/
void CG_asSetupRefdef( cg_viewdef_t *view ) {
	CG_asCallScriptFunc( cgs.asCamera.setupRefdef, [view](asIScriptContext *ctx)
		{
			ctx->SetArgObject( 0, view );
		},
		cg_empty_as_cb
	);
}
