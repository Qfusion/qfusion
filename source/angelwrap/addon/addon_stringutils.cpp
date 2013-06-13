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

#include "../qas_precompiled.h"
#include "addon_string.h"
#include "addon_scriptarray.h"
#include "addon_stringutils.h"

namespace StringUtils {

static asIObjectType *StringsArrayObjectType;

// lifted from AngelScript source code

static std::string FormatInt( asINT64 value, const std::string &options, asUINT width )
{
	bool leftJustify = options.find("l") != std::string::npos;
	bool padWithZero = options.find("0") != std::string::npos;
	bool alwaysSign  = options.find("+") != std::string::npos;
	bool spaceOnSign = options.find(" ") != std::string::npos;
	bool hexSmall    = options.find("h") != std::string::npos;
	bool hexLarge    = options.find("H") != std::string::npos;

	std::string fmt = "%";
	if( leftJustify ) fmt += "-";
	if( alwaysSign )  fmt += "+";
	if( spaceOnSign ) fmt += " ";
	if( padWithZero ) fmt += "0";

	fmt += "*";

	if( hexSmall ) fmt += "x";
	else if( hexLarge ) fmt += "X";
	else fmt += "d";

	std::string buf;
	buf.resize(width+20);

	Q_snprintfz(&buf[0], buf.size(), fmt.c_str(), width, value);
	buf.resize(strlen(&buf[0]));

	return buf;
}

static std::string FormatFloat( double value, const std::string &options, size_t width, size_t precision )
{
	bool leftJustify = options.find("l") != std::string::npos;
	bool padWithZero = options.find("0") != std::string::npos;
	bool alwaysSign  = options.find("+") != std::string::npos;
	bool spaceOnSign = options.find(" ") != std::string::npos;
	bool expSmall    = options.find("e") != std::string::npos;
	bool expLarge    = options.find("E") != std::string::npos;

	std::string fmt = "%";
	if( leftJustify ) fmt += "-";
	if( alwaysSign ) fmt += "+";
	if( spaceOnSign ) fmt += " ";
	if( padWithZero ) fmt += "0";

	fmt += "*.*";

	if( expSmall ) fmt += "e";
	else if( expLarge ) fmt += "E";
	else fmt += "f";

	std::string buf;
	buf.resize(width+precision+50);

	Q_snprintfz(&buf[0], buf.size(), fmt.c_str(), width, precision, value);
	buf.resize(strlen(&buf[0]));

	return buf;
}

static asstring_t *QAS_FormatInt( asINT64 value, const asstring_t &options, asUINT width )
{
	std::string s( options.buffer );
	std::string ret = FormatInt( value, s, width );
	return objectString_FactoryBuffer( ret.c_str(), ret.length() );
}

static asstring_t *QAS_FormatFloat( double value, const asstring_t &options, asUINT width, asUINT precision )
{
	std::string s( options.buffer );
	std::string ret = FormatFloat( value, s, width, precision );
	return objectString_FactoryBuffer( ret.c_str(), ret.length() );
}

static CScriptArrayInterface *QAS_SplitString( const asstring_t &str, const asstring_t &delim )
{
	CScriptArrayInterface *arr = QAS_NEW(CScriptArray)(0, StringsArrayObjectType);
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
		*((asstring_t **)arr->At(count)) = objectString_FactoryBuffer( prev_pbuf, pbuf - prev_pbuf );

		prev_pbuf = pbuf + delim_len;
		count++;
	}

	// append the remaining part
	arr->Resize( count + 1 );
	*((asstring_t **)arr->At(count)) = objectString_FactoryBuffer( prev_pbuf, strlen( prev_pbuf ) );

	return arr;
}

static asstring_t *QAS_JoinString( const CScriptArrayInterface &arr, const asstring_t &delim )
{
	std::string ret("");

	unsigned int arr_size = arr.GetSize();
	if( arr_size > 0 ) {
		unsigned int i;
		asstring_t *str;

		for( i = 0; i < arr_size - 1; i++ ) {
			str = *((asstring_t **)arr.At(i));

			ret += str->buffer;
			ret += delim.buffer;
		}

		// append the last element
		str = *((asstring_t **)arr.At(i));
		ret += str->buffer;
	}

	return objectString_FactoryBuffer( ret.c_str(), ret.length() );
}

static unsigned int QAS_Strtol( const asstring_t &str, unsigned int base )
{
	return strtol( str.buffer, NULL, base );
}

}

using namespace StringUtils;

void PreRegisterStringUtilsAddon( asIScriptEngine *engine )
{
}

static void CacheObjectTypes( asIScriptEngine *engine )
{
	StringsArrayObjectType = engine->GetObjectTypeById(engine->GetTypeIdByDecl("array<String @>"));
}

void RegisterStringUtilsAddon( asIScriptEngine *engine )
{
	int r;

	CacheObjectTypes( engine );

	r = engine->SetDefaultNamespace( "StringUtils" ); assert( r >= 0 );

	r = engine->RegisterGlobalFunction( "String @FormatInt(int64 val, const String &in options, uint width = 0)", asFUNCTION( QAS_FormatInt ), asCALL_CDECL );  assert( r >= 0 );
	r = engine->RegisterGlobalFunction( "String @FormatFloat(double val, const String &in options, uint width = 0, uint precision = 0)", asFUNCTION( QAS_FormatFloat ), asCALL_CDECL ); assert( r >= 0 );

    r = engine->RegisterGlobalFunction( "array<String @> @Split(const String &in string, const String &in delimiter)", asFUNCTION( QAS_SplitString ), asCALL_CDECL ); assert( r >= 0 );
    r = engine->RegisterGlobalFunction( "String @Join(array<String @> &in, const String &in delimiter)", asFUNCTION( QAS_JoinString ), asCALL_CDECL ); assert( r >= 0 );

    r = engine->RegisterGlobalFunction( "uint Strtol(const String &in string, uint base)", asFUNCTION( QAS_Strtol ), asCALL_CDECL ); assert( r >= 0 );

	r = engine->SetDefaultNamespace( "" ); assert( r >= 0 );
}
