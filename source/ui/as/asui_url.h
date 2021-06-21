#pragma once

#ifndef __ASUI_URL_H__
#define __ASUI_URL_H__

#include "asui_local.h"

namespace ASUI
{

typedef Rml::Core::URL RocketURL;

class ASURL
{
public:
	ASURL( void );
	ASURL( const char *url );
	ASURL( const asstring_t &url );
	/// copy constructor
	ASURL( const ASURL &other );

	/// Assignment operator, required by the AS.
	ASURL &operator =( const ASURL &other );

	/// Returns the entire URL string.
	asstring_t *GetURL( void ) const;

	/// Assigns a new URL to the object. This will return false if the URL is malformed.
	bool SetUrl( const asstring_t &url );

	/// Returns the URL's schema.
	asstring_t *GetSchema( void ) const;

	/// Sets the URL's schema.
	bool SetSchema( const asstring_t &schema );

	/// Returns the URL's login.
	asstring_t *GetLogin( void ) const;

	/// Sets the URL's login.
	bool SetLogin( const asstring_t &login );

	/// Returns the URL's password.
	asstring_t *GetPassword( void ) const;

	/// Sets the URL's password.
	bool SetPassword( const asstring_t &password );

	/// Returns the URL's host name.
	asstring_t *GetHost( void ) const;

	/// Sets the URL's host name.
	bool SetHost( const asstring_t &host );

	/// Returns the URL's port number.
	unsigned int GetPort( void ) const;

	/// Sets the URL's port number.
	bool SetPort( unsigned int port );

	/// Returns the URL's path.
	asstring_t *GetPath( void ) const;

	/// Sets the URL's path.
	bool SetPath( const asstring_t &filePath );

	/// Prefixes the URL's existing path with the given prefix.
	bool PrefixPath( const asstring_t &prefix );

	/// Returns the URL's file name.
	asstring_t *GetFileName( void ) const;

	/// Sets the URL's file name.
	bool SetFileName( const asstring_t &fileName );

	/// Returns the URL's path, file name and extension.
	asstring_t *GetFullFileName( void ) const;

	/// Returns the URL's file extension.
	asstring_t *GetFileExtension( void ) const;

	/// Sets the URL's file extension.
	bool SetFileExtension( const asstring_t &extension );

	/// Builds and returns a url query string ( key=value&key2=value2 )
	asstring_t *GetQueryString( void ) const;

	/// Returns dictionary of query string parameters passed in the URL.
	CScriptDictionaryInterface *GetParameters( void ) const;

	void SetParameter( const asstring_t& name, const asstring_t& value );
	void ClearParameters( void );

	/// Assignment operator, required by the AS
	static asstring_t *CastToString( const ASURL &url );

	/// Assignment operator, required by the AS.
	static ASURL CastFromString( const asstring_t &str );

private:
	RocketURL rocketURL;
};

}

ASBIND_TYPE( ASUI::ASURL, URL )

#endif
