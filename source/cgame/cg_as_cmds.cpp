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

#include "cg_as_local.h"

static moduleCommands_t moduleConsoleCmds;

static void asFunc_AddCommand( const asstring_t *cmd, asIScriptFunction *f );
static void asFunc_RemoveCommand( const asstring_t *cmd );

// Handles addition and removal of Angelscript console commands.
// Passed script function handles are properly reference counted
// and called via proxy function.

/*
* asFunc_CmdProxyFunc
*
* This is what is really called upon execution of an AS-registered cmd.
* The real function handle is looked up in the command map and then
* executed in AngelScript context.
*/
static void asFunc_CmdProxyFunc( void ) {
	int error = 0;
	std::string cmdName = trap_Cmd_Argv( 0 );

	// scan all modules to find the matching command
	for( moduleCommands_t::const_iterator it = moduleConsoleCmds.begin(); 
		it != moduleConsoleCmds.end(); ++it ) {
		auto &cmds = it->second;
		auto fit = cmds.find( cmdName );

		if( fit != cmds.end() ) {
			// call the script function
			auto f = fit->second;
			auto ctx = cgs.asExport->asAcquireContext( CGAME_AS_ENGINE() );

			error = ctx->Prepare( f );
			if( error < 0 ) {
				break;
			}
			error = ctx->Execute();
			break;
		}
	}
}

/*
* asFunc_AddCommand
*
* This is exported to the AS. Registers the console command
* and stores the AS function handle in the module map.
*/
static void asFunc_AddCommand( const asstring_t *cmd, asIScriptFunction *f ) {
	std::string moduleName = f->GetModuleName();
	std::string cmdName = cmd->buffer;

	auto &cmds = moduleConsoleCmds[moduleName];
	auto oldf = cmds.find( cmdName );
	if( oldf != cmds.end() ) {
		oldf->second->Release();
	}

	f->AddRef();
	cmds[cmdName] = f;

	trap_Cmd_AddCommand( cmd->buffer, &asFunc_CmdProxyFunc );
}


/*
* asFunc_RemoveCommand
*
* Removes the command from the command map and also unreferences
* the AS function handle.
*/
static void asFunc_RemoveCommand( const asstring_t *cmd ) {
	asIScriptContext *ctx = cgs.asExport->asGetActiveContext();
	std::string moduleName = ctx->GetFunction()->GetModuleName();
	std::string cmdName = cmd->buffer;

	auto &cmds = moduleConsoleCmds[moduleName];
	auto fit = cmds.find( cmdName );
	if( fit != cmds.end() ) {
		fit->second->Release();
		cmds.erase( fit );
		trap_Cmd_RemoveCommand( cmd->buffer );
		return;
	}

	CG_Printf( S_COLOR_YELLOW "RemoveCommand: cmd '%s' doesn't exist in module '%s'\n", 
		cmd->buffer, moduleName.c_str() );
}

static asstring_t *asFunc_CmdArgv( int index ) {
	const char *buf = trap_Cmd_Argv( index );
	asstring_t *data = cgs.asExport->asStringFactoryBuffer( buf, strlen( buf ) );
	return data;
}

static asstring_t *asFunc_CmdArgs( void ) {
	const char *buf = trap_Cmd_Args();
	asstring_t *data = cgs.asExport->asStringFactoryBuffer( buf, strlen( buf ) );
	return data;
}

const gs_asFuncdef_t asCGameCmdFuncdefs[] =
{
	{ "void CmdFunction()" },

	ASLIB_FUNCDEF_NULL
};

const gs_asglobfuncs_t asCGameCmdGlobalFuncs[] =
{
	{ "void AddCommand( const String &in, CmdFunction @+f )", asFUNCTION( asFunc_AddCommand ), NULL },
	{ "void RemoveCommand( const String &in )", asFUNCTION( asFunc_RemoveCommand ), NULL },
	{ "uint Argc()", asFUNCTION( trap_Cmd_Argc ), NULL },
	{ "const String @Argv( uint index )", asFUNCTION( asFunc_CmdArgv ), NULL },
	{ "const String @Args()", asFUNCTION( asFunc_CmdArgs ), NULL },

	{ NULL }
};

//======================================================================

/*
* CG_asReleaseModuleCommands
*/
void CG_asReleaseModuleCommands( const char *moduleName ) {
	auto &cmds = moduleConsoleCmds[moduleName];

	for( scriptCommandMap_t::const_iterator fit = cmds.begin(); fit != cmds.end(); ++fit ) {
		trap_Cmd_RemoveCommand( fit->first.c_str() );
		fit->second->Release();
	}

	cmds.clear();
}
