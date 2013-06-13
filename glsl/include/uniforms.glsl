uniform mat4 u_ModelViewMatrix;
uniform mat4 u_ModelViewProjectionMatrix;

uniform float u_ShaderTime;

uniform vec3 u_ViewOrigin;
uniform mat3 u_ViewAxis;

uniform vec3 u_EntityDist;
uniform vec3 u_EntityOrigin;
uniform myhalf4 u_EntityColor;

uniform myhalf4 u_ConstColor;
uniform myhalf4 u_RGBGenFuncArgs, u_AlphaGenFuncArgs;
uniform myhalf3 u_LightstyleColor[4]; // lightstyle colors

uniform myhalf3 u_LightAmbient;
uniform myhalf3 u_LightDiffuse;
uniform vec3 u_LightDir;

uniform vec2 u_TextureMatrix[3];
#define TextureMatrix2x3Mul(m2x3,tc) vec2(dot((m2x3)[0],(tc)) + (m2x3)[2][0], dot((m2x3)[1],(tc)) + (m2x3)[2][1])

uniform float u_MirrorSide;
