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

#include "cg_local.h"
#include "angelscript.h"
#include "../gameshared/q_angeliface.h"
#include "../gameshared/gs_ascript.h"

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#include <functional>
#include <map>
#include <string>

#define CG_SCRIPTS_GAME_MODULE_NAME "cgame"
#define CG_SCRIPTS_INPUT_MODULE_NAME "input"
#define CG_SCRIPTS_PMOVE_MODULE_NAME "cgpmove"

#define CGAME_AS_ENGINE() static_cast<asIScriptEngine *>( cgs.asEngine )

typedef std::map<std::string, asIScriptFunction *> scriptCommandMap_t;
typedef std::map<std::string, scriptCommandMap_t> moduleCommands_t;

typedef struct {
	const char * const decl;
	void **ptr;
	bool mandatory;
} cg_asApiFuncPtr_t;

extern std::function<void(asIScriptContext *)> cg_empty_as_cb;

void			 CG_asUnloadScriptModule( const char *moduleName, cg_asApiFuncPtr_t *api );
asIScriptModule *CG_asLoadScriptModule(
	const char *moduleName, const char *dir, const char *filename, cg_asApiFuncPtr_t *api, time_t *mtime );

bool CG_asExecutionErrorReport( int error );
bool CG_asCallScriptFunc(
	void *ptr, std::function<void( asIScriptContext * )> setArgs, std::function<void( asIScriptContext * )> getResult );

//
// cg_as_camera.cpp
//
extern const gs_asEnum_t				   asCGameCameraEnums[];
extern const gs_asClassDescriptor_t *const asCGameCameraClassesDescriptors[];
extern const gs_asglobfuncs_t			   asCGameCameraGlobalFuncs[];


//
// cg_as_input.cpp
//
extern const gs_asEnum_t				   asCGameInputEnums[];
extern const gs_asClassDescriptor_t *const asCGameInputClassesDescriptors[];
extern const gs_asglobfuncs_t			   asCGameInputGlobalFuncs[];


//
// cg_as_cmds.cpp
//
extern const gs_asFuncdef_t	  asCGameCmdFuncdefs[];
extern const gs_asglobfuncs_t asCGameCmdGlobalFuncs[];


//
// cg_as_refscene.cpp
//
extern const gs_asEnum_t				   asCGameRefSceneEnums[];
extern const gs_asClassDescriptor_t *const asCGameRefSceneClassesDescriptors[];


//
// cg_as_screen.cpp
//
extern const gs_asEnum_t	  asCGameScreenEnums[];
extern const gs_asglobfuncs_t asCGameScreenGlobalFuncs[];

void CG_asReleaseModuleCommands( const char *moduleName );
