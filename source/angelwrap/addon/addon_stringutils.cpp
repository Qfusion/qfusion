/*
Copyright (C) 2012 Chasseur de bots

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

#include <stdarg.h>
#include "../qas_precompiled.h"
#include "addon_string.h"
#include "addon_scriptarray.h"
#include "addon_stringutils.h"

namespace StringUtils
{

// lifted from AngelScript source code

static std::string FormatInt( asINT64 value, const std::string &options, asUINT width ) {
	bool leftJustify = options.find( "l" ) != std::string::npos;
	bool padWithZero = options.find( "0" ) != std::string::npos;
	bool alwaysSign  = options.find( "+" ) != std::string::npos;
	bool spaceOnSign = options.find( " " ) != std::string::npos;
	bool hexSmall    = options.find( "h" ) != std::string::npos;
	bool hexLarge    = options.find( "H" ) != std::string::npos;

	std::string fmt = "%";
	if( leftJustify ) {
		fmt += "-";
	}
	if( alwaysSign ) {
		fmt += "+";
	}
	if( spaceOnSign ) {
		fmt += " ";
	}
	if( padWithZero ) {
		fmt += "0";
	}

	fmt += "*";

	if( hexSmall ) {
		fmt += "x";
	} else if( hexLarge ) {
		fmt += "X";
	} else {
		fmt += "d";
	}

	std::string buf;
	buf.resize( width + 20 );

	Q_snprintfz( &buf[0], buf.size(), fmt.c_str(), width, value );
	buf.resize( strlen( &buf[0] ) );

	return buf;
}

static std::string FormatFloat( double value, const std::string &options, size_t width, size_t precision ) {
	bool leftJustify = options.find( "l" ) != std::string::npos;
	bool padWithZero = options.find( "0" ) != std::string::npos;
	bool alwaysSign  = options.find( "+" ) != std::string::npos;
	bool spaceOnSign = options.find( " " ) != std::string::npos;
	bool expSmall    = options.find( "e" ) != std::string::npos;
	bool expLarge    = options.find( "E" ) != std::string::npos;

	std::string fmt = "%";
	if( leftJustify ) {
		fmt += "-";
	}
	if( alwaysSign ) {
		fmt += "+";
	}
	if( spaceOnSign ) {
		fmt += " ";
	}
	if( padWithZero ) {
		fmt += "0";
	}

	fmt += "*.*";

	if( expSmall ) {
		fmt += "e";
	} else if( expLarge ) {
		fmt += "E";
	} else {
		fmt += "f";
	}

	std::string buf;
	buf.resize( width + precision + 50 );

	Q_snprintfz( &buf[0], buf.size(), fmt.c_str(), width, precision, value );
	buf.resize( strlen( &buf[0] ) );

	return buf;
}

static asstring_t *QAS_FormatInt( asINT64 value, const asstring_t &options, asUINT width ) {
	std::string s( options.buffer );
	std::string ret = FormatInt( value, s, width );
	return objectString_FactoryBuffer( ret.c_str(), ret.length() );
}

static asstring_t *QAS_FormatFloat( double value, const asstring_t &options, asUINT width, asUINT precision ) {
	std::string s( options.buffer );
	std::string ret = FormatFloat( value, s, width, precision );
	return objectString_FactoryBuffer( ret.c_str(), ret.length() );
}

static asstring_t *QAS_FormatStringHelper( const char *format, ... ) {
	char buf[256];
	va_list args;
	const int buf_size = int(sizeof( buf ) );

	va_start( args, format );

	int ret = Q_vsnprintfz( buf, buf_size, format, args );
	if( ret < 0 ) {
		va_end( args );
		return objectString_FactoryBuffer( "", 0 );
	}

	if( ret < buf_size ) {
		va_end( args );
		return objectString_FactoryBuffer( buf, strlen( buf ) );
	}

	asstring_t *formatted = objectString_FactoryBuffer( NULL, ret );
	Q_vsnprintfz( formatted->buffer, formatted->size, format, args );
	va_end( args );

	return formatted;
}

static asstring_t *QAS_FormatString1( const asstring_t &format, const asstring_t &arg1 ) {
	return QAS_FormatStringHelper( format.buffer, arg1.buffer );
}

static asstring_t *QAS_FormatString2( const asstring_t &format, const asstring_t &arg1, const asstring_t &arg2 ) {
	return QAS_FormatStringHelper( format.buffer, arg1.buffer, arg2.buffer );
}

static asstring_t *QAS_FormatString3( const asstring_t &format, const asstring_t &arg1, const asstring_t &arg2, const asstring_t &arg3 ) {
	return QAS_FormatStringHelper( format.buffer, arg1.buffer, arg2.buffer, arg3.buffer );
}

static asstring_t *QAS_FormatString4( const asstring_t &format, const asstring_t &arg1, const asstring_t &arg2, const asstring_t &arg3,
									  const asstring_t &arg4 ) {
	return QAS_FormatStringHelper( format.buffer, arg1.buffer, arg2.buffer, arg3.buffer, arg4.buffer );
}

static asstring_t *QAS_FormatString5( const asstring_t &format, const asstring_t &arg1, const asstring_t &arg2, const asstring_t &arg3,
									  const asstring_t &arg4, const asstring_t &arg5 ) {
	return QAS_FormatStringHelper( format.buffer, arg1.buffer, arg2.buffer, arg3.buffer, arg4.buffer, arg5.buffer );
}

static asstring_t *QAS_FormatString6( const asstring_t &format, const asstring_t &arg1, const asstring_t &arg2, const asstring_t &arg3,
									  const asstring_t &arg4, const asstring_t &arg5, const asstring_t &arg6 ) {
	return QAS_FormatStringHelper( format.buffer, arg1.buffer, arg2.buffer, arg3.buffer, arg4.buffer, arg5.buffer, arg6.buffer );
}

static asstring_t *QAS_FormatString7( const asstring_t &format, const asstring_t &arg1, const asstring_t &arg2, const asstring_t &arg3,
									  const asstring_t &arg4, const asstring_t &arg5, const asstring_t &arg6, const asstring_t &arg7 ) {
	return QAS_FormatStringHelper( format.buffer, arg1.buffer, arg2.buffer, arg3.buffer, arg4.buffer, arg5.buffer, arg6.buffer, arg7.buffer );
}

static asstring_t *QAS_FormatString8( const asstring_t &format, const asstring_t &arg1, const asstring_t &arg2, const asstring_t &arg3,
									  const asstring_t &arg4, const asstring_t &arg5, const asstring_t &arg6, const asstring_t &arg7, const asstring_t &arg8 ) {
	return QAS_FormatStringHelper( format.buffer, arg1.buffer, arg2.buffer, arg3.buffer, arg4.buffer, arg5.buffer, arg6.buffer, arg7.buffer, arg8.buffer );
}

static CScriptArrayInterface *QAS_SplitString( const asstring_t &str, const asstring_t &delim ) {
	asIScriptContext *ctx = asGetActiveContext();
	asIScriptEngine *engine = ctx->GetEngine();
	asITypeInfo *ot = engine->GetTypeInfoById( engine->GetTypeIdByDecl( "array<String @>" ) );

	CScriptArrayInterface *arr = qasCreateArrayCpp( 0, ot );
	const char *pdelim = delim.buffer;
	const size_t delim_len = strlen( pdelim );

	const char *pbuf = str.buffer;
	const char *prev_pbuf = pbuf;

	// find all occurences of the delimiter in source string
	unsigned int count = 0;
	while( 1 ) {
		pbuf = strstr( prev_pbuf, pdelim );
		if( !pbuf ) {
			break;
		}

		arr->Resize( count + 1 );
		*( (asstring_t **)arr->At( count ) ) = objectString_FactoryBuffer( prev_pbuf, pbuf - prev_pbuf );

		prev_pbuf = pbuf + delim_len;
		count++;
	}

	// append the remaining part
	arr->Resize( count + 1 );
	*( (asstring_t **)arr->At( count ) ) = objectString_FactoryBuffer( prev_pbuf, strlen( prev_pbuf ) );

	return arr;
}

static asstring_t *QAS_JoinString( const CScriptArrayInterface &arr, const asstring_t &delim ) {
	std::string ret( "" );

	unsigned int arr_size = arr.GetSize();
	if( arr_size > 0 ) {
		unsigned int i;
		asstring_t *str;

		for( i = 0; i < arr_size - 1; i++ ) {
			str = *( (asstring_t **)arr.At( i ) );

			ret += str->buffer;
			ret += delim.buffer;
		}

		// append the last element
		str = *( (asstring_t **)arr.At( i ) );
		ret += str->buffer;
	}

	return objectString_FactoryBuffer( ret.c_str(), ret.length() );
}

static unsigned int QAS_Strtol( const asstring_t &str, unsigned int base ) {
	return strtol( str.buffer, NULL, base );
}

static asstring_t *QAS_StringFromCharCode( unsigned int charCode ) {
	return objectString_FactoryBuffer( Q_WCharToUtf8Char( charCode ), Q_WCharUtf8Length( charCode ) );
}

static asstring_t *QAS_StringFromCharCodes( const CScriptArrayInterface &arr ) {
	unsigned int arr_size = arr.GetSize();
	unsigned int i;

	size_t str_len = 0;
	for( i = 0; i < arr_size; i++ ) {
		str_len += Q_WCharUtf8Length( *( (asUINT *)arr.At( i ) ) );
	}
	str_len++;

	int buf_len = str_len + 1;
	char *str = new char[buf_len];

	char *p = str;
	int char_len;
	for( i = 0; i < arr_size; i++ ) {
		char_len = Q_WCharToUtf8( *( (asUINT *)arr.At( i ) ), p, buf_len );
		p += char_len;
		buf_len -= char_len;
	}
	*p = '\0';

	asstring_t *ret = objectString_FactoryBuffer( str, str_len );

	delete[] str;

	return ret;
}

}

using namespace StringUtils;

void PreRegisterStringUtilsAddon( asIScriptEngine *engine ) {
}

void RegisterStringUtilsAddon( asIScriptEngine *engine ) {
	int r;

	r = engine->SetDefaultNamespace( "StringUtils" ); assert( r >= 0 );

	r = engine->RegisterGlobalFunction( "String @FormatInt(int64 val, const String &in options, uint width = 0)", asFUNCTION( QAS_FormatInt ), asCALL_CDECL );  assert( r >= 0 );
	r = engine->RegisterGlobalFunction( "String @FormatFloat(double val, const String &in options, uint width = 0, uint precision = 0)", asFUNCTION( QAS_FormatFloat ), asCALL_CDECL ); assert( r >= 0 );

	r = engine->RegisterGlobalFunction( "String @Format(const String &in format, const String &in arg1)", asFUNCTION( QAS_FormatString1 ), asCALL_CDECL ); assert( r >= 0 );
	r = engine->RegisterGlobalFunction( "String @Format(const String &in format, const String &in arg1, const String &in arg2)", asFUNCTION( QAS_FormatString2 ), asCALL_CDECL ); assert( r >= 0 );
	r = engine->RegisterGlobalFunction( "String @Format(const String &in format, const String &in arg1, const String &in arg2, "
										"const String &in arg3)", asFUNCTION( QAS_FormatString3 ), asCALL_CDECL ); assert( r >= 0 );
	r = engine->RegisterGlobalFunction( "String @Format(const String &in format, const String &in arg1, const String &in arg2, "
										"const String &in arg3, const String &in arg4)", asFUNCTION( QAS_FormatString4 ), asCALL_CDECL ); assert( r >= 0 );
	r = engine->RegisterGlobalFunction( "String @Format(const String &in format, const String &in arg1, const String &in arg2, "
										"const String &in arg3, const String &in arg4, const String &in arg5)", asFUNCTION( QAS_FormatString5 ), asCALL_CDECL ); assert( r >= 0 );
	r = engine->RegisterGlobalFunction( "String @Format(const String &in format, const String &in arg1, const String &in arg2, "
										"const String &in arg3, const String &in arg4, const String &in arg5, const String &in arg6)", asFUNCTION( QAS_FormatString6 ), asCALL_CDECL ); assert( r >= 0 );
	r = engine->RegisterGlobalFunction( "String @Format(const String &in format, const String &in arg1, const String &in arg2, "
										"const String &in arg3, const String &in arg4, const String &in arg5, const String &in arg6, const String &in arg7)", asFUNCTION( QAS_FormatString7 ), asCALL_CDECL ); assert( r >= 0 );
	r = engine->RegisterGlobalFunction( "String @Format(const String &in format, const String &in arg1, const String &in arg2, "
										"const String &in arg3, const String &in arg4, const String &in arg5, const String &in arg6, const String &in arg7, const String &in arg8)", asFUNCTION( QAS_FormatString8 ), asCALL_CDECL ); assert( r >= 0 );

	r = engine->RegisterGlobalFunction( "array<String @> @Split(const String &in string, const String &in delimiter)", asFUNCTION( QAS_SplitString ), asCALL_CDECL ); assert( r >= 0 );
	r = engine->RegisterGlobalFunction( "String @Join(array<String @> &in, const String &in delimiter)", asFUNCTION( QAS_JoinString ), asCALL_CDECL ); assert( r >= 0 );

	r = engine->RegisterGlobalFunction( "uint Strtol(const String &in string, uint base)", asFUNCTION( QAS_Strtol ), asCALL_CDECL ); assert( r >= 0 );

	r = engine->RegisterGlobalFunction( "String @FromCharCode(uint charCode)", asFUNCTION( QAS_StringFromCharCode ), asCALL_CDECL ); assert( r >= 0 );
	r = engine->RegisterGlobalFunction( "String @FromCharCode(array<uint> &in charCodes)", asFUNCTION( QAS_StringFromCharCodes ), asCALL_CDECL ); assert( r >= 0 );

	r = engine->SetDefaultNamespace( "" ); assert( r >= 0 );

	(void)sizeof( r ); // hush the compiler
}
