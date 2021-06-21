#pragma once
#ifndef __ASUI_LOCAL_H__
#define __ASUI_LOCAL_H__

#include "../gameshared/q_angeliface.h"
#include "../gameshared/q_shared.h"
#include "../gameshared/q_comref.h"

// DEBUG
#define __FUNCTIONPTR_CALL_THROW__
#include "as/asbind.h"

// this is exported to AS

namespace ASUI
{
// Shortcuts
using namespace Rml::Core;

typedef WSWUI::UI_Main UI_Main;

// string -> string helpers
// we use asstring_t* explicitly in AS API so we have to convert all String's to that

// asstring_t <-> std::string
inline asstring_t *ASSTR( const std::string &str ) { return UI_Main::Get()->getAS()->createString( str.c_str(), str.length() ); }

// rather cast to const char* that works for both String and std::string
inline const char *ASSTR( const asstring_t &str ) { return str.buffer; }

// const char*
inline asstring_t *ASSTR( const char *s ) { return UI_Main::Get()->getAS()->createString( s, strlen( s ) ); }

//==========================================================

// ui_scriptevent.cpp
EventListener *CreateScriptEventCaller( ASInterface *as, asIScriptFunction *func );

//
//	BINDING STUFF

// Helper in binding overloaded functions and methods
	#define OVERLOAD( F,R,P ) ( static_cast<R(*) P>( F ) )
	#define OVERLOAD_METHOD( C,F,R,P ) ( static_cast<R(C::*) P>( &C::F ) )

// asui_dom.cpp
void PrebindEvent( ASInterface *as );
void BindEvent( ASInterface *as );

void PrebindEventListener( ASInterface *as );

void PrebindElement( ASInterface *as );
void BindElement( ASInterface *as );

// as_bind_options.cpp
void PrebindOptionsForm( ASInterface *as );
void BindOptionsForm( ASInterface *as );

// as_bind_serverbrowser.cpp
void PrebindServerbrowser( ASInterface *as );
void BindServerbrowser( ASInterface *as );
void BindServerbrowserGlobal( ASInterface *as );

// as_bind_demoinfo.cpp
void PrebindDemoInfo( ASInterface *as );
void BindDemoInfo( ASInterface *as );

// as_bind_downloadinfo.cpp
void PrebindDownloadInfo( ASInterface *as );
void BindDownloadInfo( ASInterface *as );

// as_bind_datasouce.cpp
void PrebindDataSource( ASInterface *as );
void BindDataSource( ASInterface *as );

// as_bind_console.cpp
void PrebindConsole( ASInterface *as );
void BindConsole( ASInterface *as );
void BindConsoleGlobal( ASInterface *as );

// as_bind_game.cpp
void PrebindGame( ASInterface *as );
void BindGame( ASInterface *as );
void BindGameGlobal( ASInterface *as );

// as_bind_window.cpp
void PrebindWindow( ASInterface *as );
void BindWindow( ASInterface *as );
void BindWindowGlobal( ASInterface *as );
void RunWindowFrame( void );
void UnbindWindow( void );

// as_bind_url.cpp
void PrebindURL( ASInterface *as );
void BindURL( ASInterface *as );

// as_bind_mm.cpp
void PrebindMatchMaker( ASInterface *as );
void BindMatchMaker( ASInterface *as );
void BindMatchMakerGlobal( ASInterface *as );
void RunMatchMakerFrame( void );
void UnbindMatchMaker( void );

// as_bind_l10n.cpp
void PrebindL10n( ASInterface *as );
void BindL10n( ASInterface *as );
}

// type definitions required by ASBind
ASBIND_TYPE( asstring_t, String )      // Still required by ASBind
ASBIND_TYPE( CScriptDictionaryInterface, Dictionary )
ASBIND_TYPE( CScriptAnyInterface, any )

ASBIND_TYPE( Rml::Core::Element, Element )
ASBIND_TYPE( Rml::Core::ElementDocument, ElementDocument )

ASBIND_TYPE( Rml::Core::Event, Event )
ASBIND_TYPE( Rml::Core::EventListener, EventListener )

ASBIND_TYPE( WSWUI::DemoInfo, DemoInfo )

ASBIND_TYPE( WSWUI::DownloadInfo, DownloadInfo )

#endif
