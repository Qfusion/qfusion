/*
Copyright (C) 2017 Victor Luchits

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

#pragma once

#include "gs_public.h"
#include "angelscript.h"
#include "gameshared/q_angeliface.h"

typedef struct gs_asEnumVal_s {
	const char * name;
	int value;
} gs_asEnumVal_t;

typedef struct gs_asEnum_s {
	const char * name;
	const gs_asEnumVal_t * values;
} gs_asEnum_t;

typedef struct asFuncdef_s {
	const char * declaration;
} gs_asFuncdef_t;

typedef struct gs_asBehavior_s {
	asEBehaviours behavior;
	const char * declaration;
	asSFuncPtr funcPointer;
	asECallConvTypes callConv;
} gs_asBehavior_t;

typedef struct gs_asMethod_s {
	const char * declaration;
	asSFuncPtr funcPointer;
	asECallConvTypes callConv;
} gs_asMethod_t;

typedef struct gs_asProperty_s {
	const char * declaration;
	unsigned int offset;
} gs_asProperty_t;

typedef struct gs_asClassDescriptor_s {
	const char * name;
	asDWORD typeFlags;
	size_t size;
	const gs_asFuncdef_t * funcdefs;
	const gs_asBehavior_t * objBehaviors;
	const gs_asMethod_t * objMethods;
	const gs_asProperty_t * objProperties;
	const void * stringFactory;
	const void * stringFactory_asGeneric;
} gs_asClassDescriptor_t;

typedef struct gs_asglobfuncs_s {
	const char *declaration;
	asSFuncPtr pointer;
	void **asFuncPtr;
} gs_asglobfuncs_t;

typedef struct gs_asglobproperties_s {
	const char *declaration;
	void *pointer;
} gs_asglobproperties_t;

void gs_asemptyfunc( void );

#define ASLIB_LOCAL_CLASS_DESCR( x )

#define ASLIB_FOFFSET( s,m )                      offsetof( s,m )

#define ASLIB_ENUM_VAL( name )                    { #name,(int)name }
#define ASLIB_ENUM_VAL_NULL                     { NULL, 0 }

#define ASLIB_ENUM_NULL                         { NULL, NULL }

#define ASLIB_FUNCTION_DECL( type,name,params )   (#type " " #name #params )

#define ASLIB_PROPERTY_DECL( type,name )          #type " " #name

#define ASLIB_FUNCTION_NULL                     NULL
#define ASLIB_FUNCDEF_NULL                      { ASLIB_FUNCTION_NULL }
#define ASLIB_BEHAVIOR_NULL                     { asBEHAVE_CONSTRUCT, ASLIB_FUNCTION_NULL, asFUNCTION( gs_asemptyfunc ), asCALL_CDECL }
#define ASLIB_METHOD_NULL                       { ASLIB_FUNCTION_NULL, asFUNCTION( gs_asemptyfunc ), asCALL_CDECL }
#define ASLIB_PROPERTY_NULL                     { NULL, 0 }

void GS_asInitializeEngine( asIScriptEngine *asEngine );
void GS_asRegisterEnums( asIScriptEngine *asEngine, const gs_asEnum_t *asEnums, const char *nameSpace );
void GS_asRegisterFuncdefs( asIScriptEngine *asEngine, const gs_asFuncdef_t *asFuncdefs, const char *nameSpace );
void GS_asRegisterObjectClassNames( asIScriptEngine *asEngine, const gs_asClassDescriptor_t *const *asClassesDescriptors, const char *nameSpace );
void GS_asRegisterObjectClasses( asIScriptEngine *asEngine, const gs_asClassDescriptor_t *const *asClassesDescriptors, const char *nameSpace );
void GS_asRegisterGlobalFunctions( asIScriptEngine *asEngine, const gs_asglobfuncs_t *funcs, const char *nameSpace );
void GS_asRegisterGlobalProperties( asIScriptEngine *asEngine, const gs_asglobproperties_t *props, const char *nameSpace );
