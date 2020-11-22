/*
Copyright (C) 2008 German Garcia

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

#include "qas_precompiled.h"
#include "addon/addon_string.h"
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>

enum DiagMessageType {
	Diagnostics,
	RequestDebugDatabase,
	DebugDatabase,

	StartDebugging,
	StopDebugging,
	Pause,
	Continue,

	RequestCallStack,
	CallStack,

	ClearBreakpoints,
	SetBreakpoint,

	HasStopped,
	HasContinued,

	StepOver,
	StepIn,
	StepOut,

	EngineBreak,

	RequestVariables,
	Variables,

	RequestEvaluate,
	Evaluate,
	GoToDefinition,

	BreakOptions,
	RequestBreakFilters,
	BreakFilters,

	Disconnect,
};

enum DebugAction {
	CONTINUE,  // continue until next break point
	STEP_INTO, // stop at next instruction
	STEP_OVER, // stop at next instruction, skipping called functions
	STEP_OUT   // run until returning from current function
};
DebugAction		   m_action;
asUINT			   m_lastCommandAtStackLevel;

struct BreakPoint {
	BreakPoint( std::string f, int n, int id ) : name( f ), lineNbr( n ),  id( id ), needsAdjusting( true ) {
		file = f;
		std::replace( file.begin(), file.end(), '\\', '/' );
	}

	std::string name;
	int			lineNbr;
	int			id;
	bool		needsAdjusting;
	std::string file;
};

typedef struct {
	int	  severity;
	int	  line, col;
	std::string text;
} diag_message_t;

typedef struct {
	std::vector<diag_message_t> messages;
} diag_messagelist_t;

typedef struct angelwrap_stack_frame_s {
	std::string file;
	int			line;
	std::string func;
} angelwrap_stack_frame_t;

typedef struct angelwrap_variable_s {
	std::string name;
	std::string value;
	std::string type;

	bool  hasProperties;
} angelwrap_variable_t;

static std::map < std::string, diag_messagelist_t *> diag_messages;

static bool diag_debugging;
static bool diag_paused;
static bool diag_send_exceptions;

static asIScriptContext *diag_context;
static asIScriptFunction *diag_function;

std::vector<BreakPoint> m_breakPoints;

static void Diag_RespBreakFilters( qstreambuf_t *stream );
static void Diag_RespCallStack( qstreambuf_t *stream );
static void Diag_RespVariables( qstreambuf_t *stream, int level, const char *scope );
static void Diag_HasContinued( void );
static void	Diag_SetBreakpoint( BreakPoint *bp );

static std::string VarToString(
	void *value, asUINT typeId, int expandMembers, asIScriptEngine *engine, asUINT stringTypeId )
{
	if( value == 0 )
		return "<null>";

	std::stringstream s;
	if( typeId == asTYPEID_VOID )
		return "<void>";
	else if( typeId == asTYPEID_BOOL )
		return *(bool *)value ? "true" : "false";
	else if( typeId == asTYPEID_INT8 )
		s << (int)*(signed char *)value;
	else if( typeId == asTYPEID_INT16 )
		s << (int)*(signed short *)value;
	else if( typeId == asTYPEID_INT32 )
		s << *(signed int *)value;
	else if( typeId == asTYPEID_INT64 )
#if defined( _MSC_VER ) && _MSC_VER <= 1200
		s << "{...}"; // MSVC6 doesn't like the << operator for 64bit integer
#else
		s << *(asINT64 *)value;
#endif
	else if( typeId == asTYPEID_UINT8 )
		s << (unsigned int)*(unsigned char *)value;
	else if( typeId == asTYPEID_UINT16 )
		s << (unsigned int)*(unsigned short *)value;
	else if( typeId == asTYPEID_UINT32 )
		s << *(unsigned int *)value;
	else if( typeId == asTYPEID_UINT64 )
#if defined( _MSC_VER ) && _MSC_VER <= 1200
		s << "{...}"; // MSVC6 doesn't like the << operator for 64bit integer
#else
		s << *(asQWORD *)value;
#endif
	else if( typeId == asTYPEID_FLOAT )
		s << *(float *)value;
	else if( typeId == asTYPEID_DOUBLE )
		s << *(double *)value;
	else if( typeId == stringTypeId )
		s << "\"" << ( (asstring_t *)value )->buffer << "\"";
	else if( ( typeId & asTYPEID_MASK_OBJECT ) == 0 ) {
		// The type is an enum
		s << *(asUINT *)value;
	} else if( typeId & asTYPEID_SCRIPTOBJECT ) {
		// Dereference handles, so we can see what it points to
		if( typeId & asTYPEID_OBJHANDLE )
			value = *(void **)value;

		// Print the address of the object
		s << "0x" << value;
	} else {
		// Dereference handles, so we can see what it points to
		if( typeId & asTYPEID_OBJHANDLE )
			value = *(void **)value;

		// Print the address for reference types so it will be
		// possible to see when handles point to the same object
		if( engine ) {
			asITypeInfo *type = engine->GetTypeInfoById( typeId );

			if( type ) {
				if( type->GetFlags() & asOBJ_REF )
					s << "0x" << value;

				qasObjToString_t toString = (qasObjToString_t)type->GetUserData( 33 );
				if( toString ) {
					asstring_t *str = toString( value );
					s << ( ( type->GetFlags() & asOBJ_REF ) ? " " : "" ) << str->buffer;
					objectString_Free( str );
				}
			}
		} else {
			s << "{no engine}";
		}
	}

	return s.str();
}

static bool ScriptVarHasProperties( void *ptr, int typeId )
{
	if( !( typeId & asTYPEID_SCRIPTOBJECT ) )
		return false;

	// Dereference handles, so we can see what it points to
	if( typeId & asTYPEID_OBJHANDLE )
		ptr = *(void **)ptr;

	asIScriptObject *obj = (asIScriptObject *)ptr;
	return ( obj && obj->GetPropertyCount() > 0 );
}

static void FillScriptVarInfo(
	void *ptr, int typeId, asIScriptEngine *engine, asUINT stringTypeId, angelwrap_variable_t &var )
{
	var.value = VarToString( ptr, typeId, 0, engine, stringTypeId ).c_str();
	var.type = engine->GetTypeDeclaration( typeId );
	var.hasProperties = ScriptVarHasProperties( ptr, typeId );
}

static void GetScriptVarProperty(
	asUINT n, void *bPtr, asITypeInfo *type, const char **ppropName, void **pptr, int *ptypeId )
{
	void *		ptr = NULL;
	int			typeId = 0;
	int			offset = 0;
	bool		isReference = 0;
	int			compositeOffset = 0;
	bool		isCompositeIndirect = false;
	const char *propName = 0;

	if( n >= type->GetPropertyCount() ) {
		return;
	}

	type->GetProperty( n, &propName, &typeId, 0, 0, &offset, &isReference, 0, &compositeOffset, &isCompositeIndirect );

	ptr = (void *)( ( (asBYTE *)bPtr ) + compositeOffset );
	if( isCompositeIndirect )
		ptr = *(void **)ptr;
	ptr = (void *)( ( (asBYTE *)ptr ) + offset );
	if( isReference )
		ptr = *(void **)ptr;

	*ppropName = propName;
	*pptr = ptr;
	*ptypeId = typeId;
}

static void ListScriptVarProperties( void *ptr, int typeId, std::vector<angelwrap_variable_t> &res,
	asIScriptEngine *engine, std::vector<std::string> &path )
{
	const char *propName = 0;
	void *		pPtr = 0;
	int			pTypeId = 0;

	while( !path.empty() ) {
		if( ( typeId & asTYPEID_SCRIPTOBJECT ) != asTYPEID_SCRIPTOBJECT ) {
			return;
		}
		if( typeId & asTYPEID_OBJHANDLE ) {
			ptr = *(void **)ptr;
		}

		asITypeInfo *type = engine->GetTypeInfoById( typeId );

		asUINT n;
		for( n = 0; n < type->GetPropertyCount(); n++ ) {
			GetScriptVarProperty( n, ptr, type, &propName, &pPtr, &pTypeId );
			if( propName != path[0] )
				continue;

			ptr = pPtr;
			typeId = pTypeId;
			path.erase( path.begin() );
			break;
		}

		if( n == type->GetPropertyCount() ) {
			return;
		}
	}

	if( ( typeId & asTYPEID_SCRIPTOBJECT ) != asTYPEID_SCRIPTOBJECT ) {
		return;
	}
	if( typeId & asTYPEID_OBJHANDLE ) {
		ptr = *(void **)ptr;
	}

	asUINT stringTypeId = engine->GetStringFactoryReturnTypeId();

	asITypeInfo *type = engine->GetTypeInfoById( typeId );
	for( asUINT n = 0; n < type->GetPropertyCount(); n++ ) {
		GetScriptVarProperty( n, ptr, type, &propName, &pPtr, &pTypeId );

		angelwrap_variable_t var;
		var.name = propName;
		FillScriptVarInfo( pPtr, pTypeId, engine, stringTypeId, var );
		res.push_back( var );
	}
}

static void ListScriptFuncLocals( asIScriptContext *ctx, int stackLevel, asIScriptFunction *func,
	std::vector<angelwrap_variable_t> &res, asIScriptEngine *engine )
{
	asUINT stringTypeId = engine->GetStringFactoryReturnTypeId();

	for( asUINT n = func->GetVarCount(); n-- > 0; ) {
		if( ctx->IsVarInScope( n, stackLevel ) ) {
			void *ptr = ctx->GetAddressOfVar( n, stackLevel );
			int	  typeId = ctx->GetVarTypeId( n, stackLevel );

			angelwrap_variable_t var;
			var.name = ctx->GetVarName( n, stackLevel );
			FillScriptVarInfo( ptr, typeId, engine, stringTypeId, var );
			res.push_back( var );
		}
	}
}

static void ListScriptFuncLocalVarProperties( asIScriptContext *ctx, int stackLevel, asIScriptFunction *func,
	std::vector<angelwrap_variable_t> &res, asIScriptEngine *engine, std::vector<std::string> &path )
{
	if( path.empty() ) {
		return;
	}

	for( asUINT n = func->GetVarCount(); n-- > 0; ) {
		if( !ctx->IsVarInScope( n, stackLevel ) ) {
			continue;
		}
		if( path[0] != ctx->GetVarName( n, stackLevel ) ) {
			continue;
		}

		void *ptr = ctx->GetAddressOfVar( n, stackLevel );
		int	  typeId = ctx->GetVarTypeId( n, stackLevel );
		path.erase( path.begin() );

		ListScriptVarProperties( ptr, typeId, res, engine, path );
		return;
	}
}

static void ListScriptModuleGlobals(
	asIScriptModule *mod, std::vector<angelwrap_variable_t> &res, asIScriptEngine *engine )
{
	asUINT stringTypeId = engine->GetStringFactoryReturnTypeId();

	for( asUINT n = 0; n < mod->GetGlobalVarCount(); n++ ) {
		void *		ptr = mod->GetAddressOfGlobalVar( n );
		int			typeId = 0;
		const char *name;

		mod->GetGlobalVar( n, &name, NULL, &typeId );

		angelwrap_variable_t var;
		var.name = name;
		FillScriptVarInfo( ptr, typeId, engine, stringTypeId, var );
		res.push_back( var );
	}
}

static void ListScriptModuleGlobalVarProperties( asIScriptModule *mod, std::vector<angelwrap_variable_t> &res,
	asIScriptEngine *engine, std::vector<std::string> &path )
{
	for( asUINT n = 0; n < mod->GetGlobalVarCount(); n++ ) {
		void *		ptr = mod->GetAddressOfGlobalVar( n );
		int			typeId = 0;
		const char *name;

		mod->GetGlobalVar( n, &name, NULL, &typeId );
		if( path[0] != name ) {
			continue;
		}

		path.erase( path.begin() );
		ListScriptVarProperties( ptr, typeId, res, engine, path );
		return;
	}
}

std::vector<angelwrap_variable_t> QAS_asGetVariables( int stackLevel, const char *scope_ )
{
	//asIScriptContext *ctx = qasGetActiveContext();
	asIScriptContext *ctx = 0;
	std::vector<angelwrap_variable_t> res;

	if( ctx == 0 ) {
		ctx = diag_context;
	}
	if( ctx == 0 ) {
		return res;
	}

	asIScriptEngine *  engine = ctx->GetEngine();
	asIScriptFunction *func = ctx->GetFunction( stackLevel );
	asIScriptModule *  mod = func->GetModule();

	std::string				 scope( scope_ );
	std::vector<std::string> path;
	size_t					 pos = scope.find( "%", 1 );
	if( pos != std::string::npos ) {
		pos = scope.find( ".", pos + 1 );
	}

	if( pos != std::string::npos ) {
		std::string s = scope.substr( pos + 1 );
		for( pos = s.find( "." ); pos != std::string::npos; ) {
			path.push_back( s.substr( 0, pos ) );
			s.erase( 0, pos + 1 );
			pos = s.find( "." );
		}

		if( !s.empty() ) {
			path.push_back( s );
		}
	}


	if( scope.rfind( "%local%" ) == 0 ) {
		if( path.empty() ) {
			ListScriptFuncLocals( ctx, stackLevel, func, res, engine );
		} else {
			ListScriptFuncLocalVarProperties( ctx, stackLevel, func, res, engine, path );
		}
	} else if( scope.rfind( "%this%" ) == 0 ) {
		path.clear();

		if( func->GetObjectType() ) {
			ListScriptVarProperties(
				ctx->GetThisPointer( stackLevel ), ctx->GetThisTypeId( stackLevel ), res, engine, path );
		}
	} else if( scope.rfind( "%module%" ) == 0 ) {
		if( mod ) {
			if( path.empty() ) {
				ListScriptModuleGlobals( mod, res, engine );
			} else {
				ListScriptModuleGlobalVarProperties( mod, res, engine, path );
			}
		}
	}

	return res;
}

std::vector<angelwrap_stack_frame_t > QAS_GetCallstack( void )
{
	asIScriptContext *ctx = qasGetActiveContext();
	std::vector<angelwrap_stack_frame_t> stack;

	if( ctx == 0 ) {
		ctx = qasGetExceptionContext();
	}
	if( ctx == 0 ) {
		return stack;
	}

	asUINT ss = ctx->GetCallstackSize();

	for( asUINT n = 0; n < ss; n++ ) {
		const char *file = 0;
		int			lineNbr = ctx->GetLineNumber( n, 0, &file );
		const char *decl = ctx->GetFunction( n )->GetDeclaration();

		angelwrap_stack_frame_t frame;
		frame.file = file;
		frame.line = lineNbr;
		frame.func = decl;
		stack.push_back( frame );
	}

	return stack;
}

bool CheckBreakPoint( asIScriptContext *ctx )
{
	if( ctx == 0 )
		return false;

	const char *tmp = 0;
	int			lineNbr = ctx->GetLineNumber( 0, 0, &tmp );
	const char *file = tmp ? tmp : "";

	// Determine if there is a breakpoint at the current line
	for( size_t n = 0; n < m_breakPoints.size(); n++ ) {
		if( m_breakPoints[n].lineNbr == lineNbr ) {
			if( !Q_stricmp( m_breakPoints[n].file.c_str(), file ) ) {
				return true;
			}
		}
	}

	return false;
}

void LineCallback( asIScriptContext *ctx )
{
	// This should never happen, but it doesn't hurt to validate it
	if( ctx == 0 ) {
		assert( ctx );
		return;
	}

	// By default we ignore callbacks when the context is not active.
	// An application might override this to for example disconnect the
	// debugger as the execution finished.
	if( ctx->GetState() != asEXECUTION_ACTIVE ) {
		return;
	}

	const char *tmp = 0;
	int			lineNbr = ctx->GetLineNumber( 0, 0, &tmp );
	const char *file = tmp ? tmp : "";

	asIScriptFunction *func = ctx->GetFunction();
	if( diag_function != func ) {
		for( auto it = m_breakPoints.begin(); it != m_breakPoints.end(); ++it ) {
			BreakPoint &bp = *it;

			if( !bp.needsAdjusting ) {
				continue;
			}
			if( Q_stricmp( bp.file.c_str(), file ) ) {
				continue;
			}

			int line = func->FindNextLineWithCode( bp.lineNbr );
			if( line >= 0 ) {
				bp.needsAdjusting = false;

				if( bp.lineNbr != line ) {
					// Move the breakpoint to the next line
					bp.lineNbr = line;
					Diag_SetBreakpoint( &bp );
				}
			}
		}
		diag_function = func;
	}

	if( m_action == CONTINUE ) {
		if( !CheckBreakPoint( ctx ) ) {
			return;
		}
	} else if( m_action == STEP_OVER ) {
		if( ctx->GetCallstackSize() > m_lastCommandAtStackLevel ) {
			if( !CheckBreakPoint( ctx ) ) {
				return;
			}
		}
	} else if( m_action == STEP_OUT ) {
		if( ctx->GetCallstackSize() >= m_lastCommandAtStackLevel ) {
			if( !CheckBreakPoint( ctx ) ) {
				return;
			}
		}
	} else if( m_action == STEP_INTO ) {
		if( CheckBreakPoint( ctx ) ) {
			return;
		}

		// Always break, but we call the check break point anyway
		// to tell user when break point has been reached
	}

	Diag_Breakpoint( ctx, file, lineNbr );
}

static int Diag_ReadInt8( uint8_t *buf, uint8_t *end, int *res )
{
	if( buf + 1 > end )
		return 0;
	*res = buf[0];
	return 1;
}

static int Diag_ReadInt32( uint8_t *buf, uint8_t *end, int *res )
{
	if( buf + 4 > end )
		return 0;
	*res = buf[0] | ( buf[1] << 8 ) | ( buf[2] << 16 ) | ( buf[3] << 24 );
	return 4;
}

static int Diag_ReadString( uint8_t *buf, uint8_t *end, char **pres )
{
	int32_t len;
	int32_t off = 0;
	char *	res;

	*pres = NULL;
	if( buf + 4 > end )
		return 0;

	off += Diag_ReadInt32( buf, end, &len );

	if( buf + off + len > end )
		return 0;

	res = (char *)QAS_Malloc( (size_t)len + 1 );
	memcpy( res, (char *)( buf + off ), len );
	res[len] = 0;
	*pres = res;

	off += len;

	return off;
}

static int Diag_WriteInt8( uint8_t *buf, uint8_t *end, int val )
{
	if( buf + 1 > end )
		return 0;
	buf[0] = (uint8_t)val;
	return 1;
}

static int Diag_WriteInt32( uint8_t *buf, uint8_t *end, int val )
{
	if( buf + 4 > end )
		return 0;
	buf[0] = ( uint8_t )( val & 0xff );
	buf[1] = ( uint8_t )( ( val >> 8 ) & 0xff );
	buf[2] = ( uint8_t )( ( val >> 16 ) & 0xff );
	buf[3] = ( uint8_t )( val >> 24 );
	return 4;
}

static int Diag_WriteString( uint8_t *buf, uint8_t *end, char *str, size_t len )
{
	int32_t off = 0;

	if( !str )
		return 0;

	if( buf + 4 > end )
		return 0;

	off += Diag_WriteInt32( buf, end, len );

	if( buf + off + len > end )
		return 0;

	memcpy( buf + off, str, len );
	off += len;

	return off;
}

bool Diag_PeekMessage( qstreambuf_t *rb )
{
	int		 len;
	size_t	 s, off;
	uint8_t *data;

	s = rb->datalength( rb );
	if( s < 4 ) {
		return false;
	}

	data = rb->data( rb );
	off = Diag_ReadInt32( data, data + s, &len );
	return s - off >= len;
}

void Diag_ReadMessage( qstreambuf_t *rb, qstreambuf_t *resp )
{
	int		 len;
	uint8_t *data, *end;
	size_t	 s, off;

	s = rb->datalength( rb );
	if( s < 4 ) {
		assert( s >= 4 );
		return;
	}

	off = 0;
	data = rb->data( rb );
	end = data + s;

	off += Diag_ReadInt32( data + off, end, &len );

	if( len != 0 ) {
		int mt;

		off += Diag_ReadInt8( data + off, end, &mt );

		switch( mt ) {
			case StartDebugging:
				Diag_Start();
				break;
			case StopDebugging:
				Diag_Stop();
				break;
			case Disconnect:
				break;
			case RequestBreakFilters:
				Diag_RespBreakFilters( resp );
				break;
			case RequestCallStack:
				Diag_RespCallStack( resp );
				break;
			case Pause:
				m_action = STEP_INTO;
				//Diag_Pause( true );
				break;
			case Continue:
				if( diag_context ) {
					diag_context->Release();
					diag_context = NULL;
				}
				m_action = CONTINUE;
				Diag_Pause( false );
				break;
			case StepIn:
				m_action = STEP_INTO;
				Diag_Pause( false );
				break;
			case StepOut:
				m_action = STEP_OUT;
				m_lastCommandAtStackLevel = diag_context ? diag_context->GetCallstackSize() : 0;
				Diag_Pause( false );
				break;
			case StepOver:
				m_action = STEP_OVER;
				m_lastCommandAtStackLevel = diag_context ? diag_context->GetCallstackSize() : 1;
				Diag_Pause( false );
				break;
			case ClearBreakpoints:
				m_breakPoints.clear();
				break;
			case SetBreakpoint: {
				char *fn;
				int	  line, id;

				off += Diag_ReadString( data + off, end, &fn );
				off += Diag_ReadInt32( data + off, end, &line );
				off += Diag_ReadInt32( data + off, end, &id );

				if( fn != NULL && id != 0 ) {
					BreakPoint bp( fn, line, id );
					m_breakPoints.push_back( bp );

					//Diag_RespSetBreakpoint( resp, bp.name.c_str(), bp.lineNbr, bp.id );
				}

				QAS_Free( fn );
			} break;
			case RequestVariables: {
				char *sep, *str;

				off += Diag_ReadString( data + off, end, &str );

				if( !str ) {
					break;
				}

				sep = strchr( str, ':' );
				if( sep ) {
					*sep = '\0';
					Diag_RespVariables( resp, atoi( str ), sep + 1 );
				}

				QAS_Free( str );
			} break;
			case BreakOptions: {
				int count;

				diag_send_exceptions = false;

				off += Diag_ReadInt32( data + off, end, &count );
				for( int i = 0; i < count; i++ ) {
					char *filter;

					off += Diag_ReadString( data + off, end, &filter );

					if( !strcmp( filter, "uncaught" ) ) {
						diag_send_exceptions = true;
					}

					QAS_Free( filter );
				}

			} break;
		}
	}

	rb->consume( rb, (size_t)len + 4 );
}

static size_t Diag_BeginEncodeMsg( qstreambuf_t *stream )
{
	size_t head_pos;

	stream->prepare( stream, 5 );
	head_pos = stream->datalength( stream );
	stream->commit( stream, 5 );

	return head_pos;
}

static void Diag_EncodeMsg( qstreambuf_t *stream, const char *fmt, ... )
{
	int	   j;
	size_t total_len = 0;

	for( j = 0; j < 2; j++ ) {
		int			i = 0;
		char *		s = NULL;
		size_t		s_len;
		size_t		len = 0, off = 0;
		va_list		argp;
		const char *p;
		uint8_t *	buf = NULL, *end = NULL;

		if( j != 0 ) {
			stream->prepare( stream, total_len );
			buf = stream->buffer( stream );
			end = buf + stream->size( stream );
			off = 0;
		}

		va_start( argp, fmt );

		for( p = fmt; *p != '\0'; p++ ) {
			if( *p != '%' ) {
				continue;
			}

			switch( *++p ) {
				case 'c':
					i = va_arg( argp, int );
					len += 1;
					if( j != 0 ) {
						off += Diag_WriteInt8( buf + off, end, i );
					}
					break;
				case 's':
					s = va_arg( argp, char * );
					s_len = strlen( s );
					len += 4;
					len += s_len;
					if( j != 0 ) {
						off += Diag_WriteString( buf + off, end, s, s_len );
					}
					break;
				case 'i':
				case 'd':
					i = va_arg( argp, int );
					len += 4;
					if( j != 0 ) {
						off += Diag_WriteInt32( buf + off, end, i );
					}
					break;
			}
		}

		va_end( argp );

		if( j != 0 ) {
			stream->commit( stream, off );
			break;
		}

		total_len = len;
	}
}

static void Diag_FinishEncodeMsg( qstreambuf_t *stream, size_t head_pos, int type )
{
	uint8_t *buf = stream->data( stream ) + head_pos;
	uint8_t *end = buf + 5;
	size_t	 off = 0;

	off += Diag_WriteInt32( buf + off, end, ( stream->datalength( stream ) - head_pos - 5 ) );
	off += Diag_WriteInt8( buf + off, end, type );
}

void Diag_BeginBuild( const char **filenames )
{
	int i;

	if( !diag_messages.empty() ) {
		assert( diag_messages.empty() );
		return;
	}

	for( i = 0; filenames[i]; i++ ) {
		diag_messages[filenames[i]] = QAS_NEW( diag_messagelist_t );
	}
}

void Diag_Message( int severity, const char *filename, int line, int col, const char *text )
{
	diag_message_t newmsg;
	diag_messagelist_t *ml;

	if( diag_messages.empty() )
		return;

	auto it = diag_messages.find( filename );
	if( it == diag_messages.end() ) {
		return;
	}

	newmsg.line = line;
	newmsg.col = col;
	newmsg.text = text;
	newmsg.severity = severity;

	ml = it->second;
	ml->messages.push_back( newmsg );
}

void Diag_EndBuild( void )
{
	unsigned int		j;
	qstreambuf_t		stream;

	if( diag_messages.empty() ) {
		return;
	}

	QStreamBuf_Init( &stream );

	for( auto it = diag_messages.begin(); it != diag_messages.end(); ++it ) {
		const char *		filename = it->first.c_str();
		diag_messagelist_t *ml = it->second;
		size_t				head_pos;

		head_pos = Diag_BeginEncodeMsg( &stream );

		Diag_EncodeMsg( &stream, "%s%i", filename, (int)ml->messages.size() );

		for( j = 0; j < ml->messages.size(); j++ ) {
			diag_message_t *dm = &ml->messages[j];

			Diag_EncodeMsg( &stream, "%s%i%i%i%i", dm->text.c_str(), dm->line, dm->col, dm->severity > 1, dm->severity == 0 );
		}

		Diag_FinishEncodeMsg( &stream, head_pos, Diagnostics );

		QAS_DELETE( ml, diag_messagelist_t );
	}

	diag_messages.clear();

	trap_Diag_Broadcast( &stream );

	stream.clear( &stream );
}

static void Diag_Pause_( asIScriptContext *ctx,
	const char *sectionName, int line, int col, const char *reason, const char *reasonString )
{
	qstreambuf_t stream;
	size_t		 head_pos;

	if( !diag_debugging ) {
		return;
	}
	if( diag_paused ) {
		return;
	}

	diag_context = ctx;
	ctx->AddRef();

	QStreamBuf_Init( &stream );

	head_pos = Diag_BeginEncodeMsg( &stream );

	Diag_EncodeMsg( &stream, "%s%i%s", reason, 0, reasonString );

	Diag_FinishEncodeMsg( &stream, head_pos, HasStopped );

	trap_Diag_Broadcast( &stream );

	stream.clear( &stream );

	Diag_Pause( true );

	trap_Diag_Break();
}

void Diag_Exception( asIScriptContext *ctx, const char *sectionName, int line, int col, const char *exceptionString )
{
	if( !diag_send_exceptions ) {
		return;
	}
	Diag_Pause_( ctx, sectionName, line, col, "exception", exceptionString );
}

void Diag_Breakpoint( asIScriptContext *ctx, const char *sectionName, int line )
{
	Diag_Pause_( ctx, sectionName, line, 0, "breakpoint", "" );
}

static void Diag_HasContinued( void )
{
	qstreambuf_t stream;
	size_t		 head_pos;

	QStreamBuf_Init( &stream );

	head_pos = Diag_BeginEncodeMsg( &stream );

	Diag_FinishEncodeMsg( &stream, head_pos, HasContinued );

	trap_Diag_Broadcast( &stream );

	stream.clear( &stream );
}

static void Diag_SetBreakpoint( BreakPoint *bp )
{
	qstreambuf_t stream;
	size_t		 head_pos;

	QStreamBuf_Init( &stream );

	head_pos = Diag_BeginEncodeMsg( &stream );

	Diag_EncodeMsg( &stream, "%s%i%i", bp->name.c_str(), bp->lineNbr, bp->id );

	Diag_FinishEncodeMsg( &stream, head_pos, SetBreakpoint );

	trap_Diag_Broadcast( &stream );

	stream.clear( &stream );
}

static void Diag_RespBreakFilters( qstreambuf_t *stream )
{
	size_t head_pos = Diag_BeginEncodeMsg( stream );

	Diag_EncodeMsg( stream, "%i%s%s", 1, "uncaught", "Uncaught Exceptions" );

	Diag_FinishEncodeMsg( stream, head_pos, BreakFilters );
}

static void Diag_RespCallStack( qstreambuf_t *stream )
{
	size_t head_pos;
	int	   i, stack_size;

	head_pos = Diag_BeginEncodeMsg( stream );

	auto stack = QAS_GetCallstack();
	stack_size = stack.size();

	Diag_EncodeMsg( stream, "%i", stack_size );
	for( i = 0; i < stack_size; i++ ) {
		Diag_EncodeMsg( stream, "%s%s%i", stack[i].func.c_str(), stack[i].file.c_str(), stack[i].line );
	}

	Diag_FinishEncodeMsg( stream, head_pos, CallStack );
}

static void Diag_RespVariables( qstreambuf_t *stream, int level, const char *scope )
{
	size_t head_pos;
	int	   i, num_vars = 0;

	head_pos = Diag_BeginEncodeMsg( stream );

	auto vars = QAS_asGetVariables( level, scope );
	num_vars = vars.size();

	Diag_EncodeMsg( stream, "%i", num_vars );
	for( i = 0; i < num_vars; i++ ) {
		Diag_EncodeMsg( stream, "%s%s%s%i", vars[i].name.c_str(), vars[i].value.c_str(), vars[i].type.c_str(), (int)vars[i].hasProperties );
	}

	Diag_FinishEncodeMsg( stream, head_pos, Variables );
}

void Diag_Start( void )
{
	if( diag_debugging ) {
		return;
	}

	diag_debugging = true;
	qasSetLineCallback( asFUNCTION( LineCallback ), asCALL_CDECL );
}

void Diag_Pause( bool pause )
{
	if( !diag_debugging ) {
		return;
	}
	if( diag_paused == pause ) {
		return;
	}

	diag_paused = pause;
	if( !pause ) {
		Diag_HasContinued();
	}
}

void Diag_Stop( void )
{
	if( !diag_debugging ) {
		return;
	}

	if( diag_context ) {
		diag_context->Release();
		diag_context = NULL;
	}

	qasClearLineCallback();
	diag_debugging = false;
}

bool Diag_Paused( void )
{
	return diag_paused;
}
