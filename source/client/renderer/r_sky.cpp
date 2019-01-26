#include "r_local.h"
#include "r_backend_local.h"

static mesh_vbo_t * sky_vbo = NULL;
static shader_t * sky_shader = NULL;

// w = 0 projects to infinity
static vec4_t cube_verts[] = {
       { -1.0f,  1.0f,  1.0f, 0.0f },
       {  1.0f,  1.0f,  1.0f, 0.0f },
       { -1.0f, -1.0f,  1.0f, 0.0f },
       {  1.0f, -1.0f,  1.0f, 0.0f },
       { -1.0f,  1.0f, -1.0f, 0.0f },
       {  1.0f,  1.0f, -1.0f, 0.0f },
       { -1.0f, -1.0f, -1.0f, 0.0f },
       {  1.0f, -1.0f, -1.0f, 0.0f },
};

static uint16_t cube_indices[] = { 7, 6, 3, 2, 0, 6, 4, 7, 5, 3, 1, 0, 5, 4 };

void R_InitSky() {
	if( sky_vbo != NULL ) {
		R_TouchMeshVBO( sky_vbo );
		R_TouchShader( sky_shader );
		return;
	}

	mesh_t mesh = { };
	mesh.numVerts = ARRAY_COUNT( cube_verts );
	mesh.numElems = ARRAY_COUNT( cube_indices );
	mesh.xyzArray = cube_verts;
	mesh.elems = cube_indices;

	sky_vbo = R_CreateMeshVBO( NULL, ARRAY_COUNT( cube_verts ), ARRAY_COUNT( cube_indices ), 0, VATTRIB_POSITION_BIT, VBO_TAG_NONE, 0 );
	R_UploadVBOVertexData( sky_vbo, 0, VATTRIB_POSITION_BIT, &mesh, 0 );
	R_UploadVBOElemData( sky_vbo, 0, 0, &mesh );

	sky_shader = R_RegisterPic( "sky" );
}

void R_DrawSky( const refdef_t * rd ) {
	mat4_t oldprojection;
	mat4_t oldobject;
	Matrix4_Copy( rb.projectionMatrix, oldprojection );
	Matrix4_Copy( rb.objectMatrix, oldobject );

	mat4_t world_to_clip;
	Matrix4_InfinitePerspectiveProjection( rd->fov_x, rd->fov_y, rn.nearClip, world_to_clip );
	RB_LoadProjectionMatrix( world_to_clip );

	mat4_t model_to_world;
	Matrix4_ObjectMatrix( rn.viewOrigin, rn.viewAxis, 1.0f, model_to_world );
	RB_LoadObjectMatrix( model_to_world );

	RB_BindShader( NULL, sky_shader );
	RB_BindVBO( sky_vbo->index, GL_TRIANGLE_STRIP );
	RB_DrawElements( 0, ARRAY_COUNT( cube_verts ), 0, ARRAY_COUNT( cube_indices ) );

	RB_LoadProjectionMatrix( oldprojection );
	RB_LoadObjectMatrix( oldobject );
}
