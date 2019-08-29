/*
Copyright (C) 2011 Victor Luchits

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
#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "as/asui.h"
#include "as/asui_local.h"
#include "as/asui_url.h"

namespace ASUI
{

typedef Rml::Core::URL RocketURL;

ASURL::ASURL( void ) : rocketURL( "" ) { }

ASURL::ASURL( const char *url ) : rocketURL( url ) { }

ASURL::ASURL( const asstring_t &url ) : rocketURL( url.buffer ) { }

ASURL::ASURL( const ASURL &other ) : rocketURL( other.rocketURL ) { }

ASURL &ASURL::operator =( const ASURL &other ) {
	rocketURL = other.rocketURL;
	return *this;
}

asstring_t *ASURL::GetURL( void ) const {
	return ASSTR( rocketURL.GetURL() );
}

bool ASURL::SetUrl( const asstring_t &url ) {
	return rocketURL.SetURL( url.buffer );
}

asstring_t *ASURL::GetSchema( void ) const {
	return ASSTR( rocketURL.GetProtocol() );
}

bool ASURL::SetSchema( const asstring_t &schema ) {
	return rocketURL.SetProtocol( schema.buffer );
}

asstring_t *ASURL::GetLogin( void ) const {
	return ASSTR( rocketURL.GetLogin() );
}

bool ASURL::SetLogin( const asstring_t &login ) {
	return rocketURL.SetLogin( login.buffer );
}

asstring_t *ASURL::GetPassword( void ) const {
	return ASSTR( rocketURL.GetPassword() );
}

bool ASURL::SetPassword( const asstring_t &password ) {
	return rocketURL.SetPassword( password.buffer );
}

asstring_t *ASURL::GetHost( void ) const {
	return ASSTR( rocketURL.GetHost() );
}

bool ASURL::SetHost( const asstring_t &host ) {
	return rocketURL.SetHost( host.buffer );
}

unsigned int ASURL::GetPort( void ) const {
	return rocketURL.GetPort();
}

bool ASURL::SetPort( unsigned int port ) {
	return rocketURL.SetPort( port );
}

asstring_t *ASURL::GetPath( void ) const {
	return ASSTR( rocketURL.GetPath() );
}

bool ASURL::SetPath( const asstring_t &filePath ) {
	return rocketURL.SetPath( filePath.buffer );
}

bool ASURL::PrefixPath( const asstring_t &prefix ) {
	return rocketURL.PrefixPath( prefix.buffer );
}

asstring_t *ASURL::GetFileName( void ) const {
	return ASSTR( rocketURL.GetFileName() );
}

bool ASURL::SetFileName( const asstring_t &fileName ) {
	return rocketURL.SetFileName( fileName.buffer );
}

asstring_t *ASURL::GetFullFileName( void ) const {
	return ASSTR( rocketURL.GetPathedFileName() );
}

asstring_t *ASURL::GetFileExtension( void ) const {
	return ASSTR( rocketURL.GetExtension() );
}

bool ASURL::SetFileExtension( const asstring_t &extension ) {
	return rocketURL.SetExtension( extension.buffer );
}

asstring_t *ASURL::GetQueryString( void ) const {
	return ASSTR( rocketURL.GetQueryString() );
}

CScriptDictionaryInterface *ASURL::GetParameters( void ) const {
	CScriptDictionaryInterface *dict = UI_Main::Get()->getAS()->createDictionary();
	int stringObjectTypeId = UI_Main::Get()->getAS()->getStringObjectType()->GetTypeId(); // FIXME: cache this?

	RocketURL::Parameters parameters = rocketURL.GetParameters();
	for( RocketURL::Parameters::const_iterator it = parameters.begin(); it != parameters.end(); ++it ) {
		dict->Set( *( ASSTR( it->first ) ), ASSTR( it->second ), stringObjectTypeId );
	}
	return dict;
}

void ASURL::SetParameter( const asstring_t& name, const asstring_t& value ) {
	rocketURL.SetParameter( ASSTR( name ), ASSTR( value ) );
}

void ASURL::ClearParameters( void ) {
	rocketURL.ClearParameters();
}

/// This makes AS aware of this class so other classes may reference
/// it in their properties and methods
void PrebindURL( ASInterface *as ) {
	ASBind::Class<ASURL, ASBind::class_class>( as->getEngine() );
}

void BindURL( ASInterface *as ) {
	ASBind::GetClass<ASURL>( as->getEngine() )
	.constructor<void()>()
	.constructor<void(const asstring_t &url)>()
	.constructor<void(const ASURL &other)>()
	.destructor()

	.method( &ASURL::operator =, "opAssign" )

	.method( &ASURL::GetURL, "getURL" )
	.method( &ASURL::SetUrl, "setURL" )

	.method( &ASURL::GetSchema, "getSchema" )
	.method( &ASURL::SetSchema, "setSchema" )

	.method( &ASURL::GetLogin, "getLogin" )
	.method( &ASURL::SetLogin, "setLogin" )

	.method( &ASURL::GetPassword, "getPassword" )
	.method( &ASURL::SetPassword, "setPassword" )

	.method( &ASURL::GetHost, "getHost" )
	.method( &ASURL::SetHost, "setHost" )

	.method( &ASURL::GetPort, "getPort" )
	.method( &ASURL::SetPort, "setPort" )

	.method( &ASURL::GetPath, "getPath" )
	.method( &ASURL::SetPath, "setPath" )
	.method( &ASURL::PrefixPath, "prefixPath" )

	.method( &ASURL::GetFileName, "getFileName" )
	.method( &ASURL::SetFileName, "setFileName" )
	.method( &ASURL::GetFullFileName, "getFullFileName" )

	.method( &ASURL::GetFileExtension, "getFileExtension" )
	.method( &ASURL::SetFileExtension, "setFileExtension" )

	.method( &ASURL::GetParameters, "getParameters" )
	.method( &ASURL::SetParameter, "setParameter" )
	.method( &ASURL::ClearParameters, "clearParameters" )

	.method( &ASURL::GetQueryString, "getQueryString" )
	;
}

}
