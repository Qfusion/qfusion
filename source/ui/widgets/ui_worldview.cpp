/*
Copyright (C) 2012 Victor Luchits

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
#include "widgets/ui_widgets.h"

#define MODELVIEW_EPSILON       1.0f

namespace WSWUI
{

using namespace Rml::Core;

// forward-declare the instancer for keyselects
class UI_WorldviewWidgetInstancer;

class UI_WorldviewWidget : public Element, EventListener
{
private:
	refdef_t refdef;
	vec3_t baseAngles;
	vec3_t aWaveAmplitude;
	vec3_t aWavePhase;
	vec3_t aWaveFrequency;
	float fovY;
	float mouseSensitivity;
	vec3_t anglesMove;
	vec3_t anglesClamp;
	String mapName;
	String colorCorrection;
	struct shader_s *colorCorrectionShader;
	bool Initialized;
	String lightStyles[MAX_LIGHTSTYLES];

public:
	UI_WorldviewWidget( const String &tag )
		: Element( tag ),
		mapName( "" ), colorCorrection( "" ), colorCorrectionShader( NULL ),
		Initialized( false ) {
		memset( &refdef, 0, sizeof( refdef ) );
		refdef.areabits = 0;

		VectorClear( baseAngles );
		VectorClear( aWaveAmplitude );
		VectorClear( aWavePhase );
		VectorClear( aWaveFrequency );

		VectorClear( anglesMove );
		VectorClear( anglesClamp );

		// Some default values
		Matrix3_Copy( axis_identity, refdef.viewaxis );
		fovY = 100.0f;
		mouseSensitivity = 0.0f;

		InitLightStyles();
	}

	virtual void OnRender() {
		bool firstRender = false;
		Rml::Core::Dictionary parameters;
		auto *ui_main = UI_Main::Get();
		int mousedx, mousedy;
		vec3_t viewAngles;

		Element::OnRender();

		if( !Initialized ) {
			// lazily register the world model
			colorCorrectionShader = NULL;
			firstRender = true;
			Initialized = true;

			if( !colorCorrection.empty() ) {
				colorCorrectionShader = trap::R_RegisterLinearPic( colorCorrection.c_str() );
			}

			if( mapName.empty() ) {
				return;
			}

			trap::R_RegisterWorldModel( mapName.c_str() );

			this->DispatchEvent( "registerworldmodel", parameters, false );
		}

		ui_main->getMouseMoveDelta( &mousedx, &mousedy );

		// refdef setup
		Rml::Core::Vector2f box = GetBox().GetSize( Rml::Core::Box::CONTENT );
		refdef.width = box.x;
		refdef.height = box.y;
		refdef.fov_y = WidescreenFov( fovY );
		refdef.fov_x = CalcHorizontalFov( refdef.fov_y, refdef.width, refdef.height );
		refdef.time = ui_main->getRefreshState().time;

		anglesMove[PITCH] += mousedy * mouseSensitivity * 0.022;
		anglesMove[YAW] -= mousedx * mouseSensitivity * 0.022;

		// angular movement for camera
		for( int i = 0; i < 3; i++ ) {
			float delta = anglesMove[i] + aWaveAmplitude[i] * sin( aWavePhase[i] + refdef.time * 0.001 * aWaveFrequency[i] * M_TWOPI );
			if( anglesClamp[i] ) {
				if( delta < -anglesClamp[i] ) {
					delta = -anglesClamp[i];
				}
				if( delta >  anglesClamp[i] ) {
					delta =  anglesClamp[i];
				}
			}
			viewAngles[i] = baseAngles[i] + delta;
		}

		AnglesToAxis( viewAngles, refdef.viewaxis );

		Rml::Core::Vector2f offset = GetAbsoluteOffset( Rml::Core::Box::CONTENT );
		refdef.x = offset.x;
		refdef.y = offset.y;

		// clip scissor region to parent
		int scissor_x, scissor_y, scissor_w, scissor_h;
		trap::R_GetScissor( &scissor_x, &scissor_y, &scissor_w, &scissor_h );
		refdef.scissor_x = std::max( scissor_x, refdef.x );
		refdef.scissor_y = std::max( scissor_y, refdef.y );
		refdef.scissor_width = std::min( scissor_w, refdef.width );
		refdef.scissor_height = std::min( scissor_h, refdef.height );
		refdef.colorCorrection = colorCorrectionShader;

		trap::R_ClearScene();

		AddLightStylesToScene();

		trap::R_RenderScene( &refdef );

		trap::R_Scissor( scissor_x, scissor_y, scissor_w, scissor_h );

		if( firstRender ) {
			this->DispatchEvent( "firstrender", parameters, false );
		}
	}

	virtual void OnAttributeChange( const Rml::Core::ElementAttributes& changed_attributes ) {
		Element::OnAttributeChange( changed_attributes );

		//GetAttribute< Rml::Core::String >("value", "");
		for( Rml::Core::ElementAttributes::const_iterator it = changed_attributes.begin(); it != changed_attributes.end(); ++it ) {
			if( it->first == "worldmodel" ) {
				mapName = GetAttribute< Rml::Core::String >( it->first, "" );
			} else if( it->first == "vieworigin-x" || it->first == "vieworigin-y" || it->first == "vieworigin-z" ) {
				char lastChar = it->first.back();
				refdef.vieworg[lastChar - 'x'] = GetAttribute< float >( it->first, 0.0 );
			} else if( it->first == "viewangle-pitch" ) {
				baseAngles[PITCH] = GetAttribute< float >( it->first, 0.0 );
			} else if( it->first == "viewangle-yaw" ) {
				baseAngles[YAW] = GetAttribute< float >( it->first, 0.0 );
			} else if( it->first == "viewangle-roll" ) {
				baseAngles[ROLL] = GetAttribute< float >( it->first, 0.0 );
			} else if( it->first == "wave-pitch-amplitude" ) {
				aWaveAmplitude[PITCH] = GetAttribute< float >( it->first, 0.0 );
			} else if( it->first == "wave-yaw-amplitude" ) {
				aWaveAmplitude[YAW] = GetAttribute< float >( it->first, 0.0 );
			} else if( it->first == "wave-roll-amplitude" ) {
				aWaveAmplitude[ROLL] = GetAttribute< float >( it->first, 0.0 );
			} else if( it->first == "wave-pitch-phase" ) {
				aWavePhase[PITCH] = GetAttribute< float >( it->first, 0.0 ) / 360.0f * M_TWOPI;
			} else if( it->first == "wave-yaw-phase" ) {
				aWavePhase[YAW] = GetAttribute< float >( it->first, 0.0 ) / 360.0f * M_TWOPI;
			} else if( it->first == "wave-roll-phase" ) {
				aWavePhase[ROLL] = GetAttribute< float >( it->first, 0.0 ) / 360.0f * M_TWOPI;
			} else if( it->first == "wave-pitch-frequency" ) {
				aWaveFrequency[PITCH] = GetAttribute< float >( it->first, 0.0 );
			} else if( it->first == "wave-yaw-frequency" ) {
				aWaveFrequency[YAW] = GetAttribute< float >( it->first, 0.0 );
			} else if( it->first == "wave-roll-frequency" ) {
				aWaveFrequency[ROLL] = GetAttribute< float >( it->first, 0.0 );
			} else if( it->first == "fov" ) {
				fovY = GetAttribute< float >( it->first, 100.0 );
			} else if( it->first == "color-correction" ) {
				colorCorrection = GetAttribute< Rml::Core::String >( it->first, "" );
			} else if( it->first == "mouse-sensitivity" ) {
				mouseSensitivity = GetAttribute< float >( it->first, 0.0 );
			} else if( it->first == "viewangle-pitch-clamp" ) {
				anglesClamp[0] = GetAttribute< float >( it->first, 0.0 );
			} else if( it->first == "viewangle-yaw-clamp" ) {
				anglesClamp[1] = GetAttribute< float >( it->first, 0.0 );
			} else if( it->first == "viewangle-roll-clamp" ) {
				anglesClamp[2] = GetAttribute< float >( it->first, 0.0 );
			} else if( it->first == "blur" ) {
				bool blur = HasAttribute( "blur ");
				if( blur ) {
					refdef.rdflags |= RDF_BLURRED;
				} else {
					refdef.rdflags &= ~RDF_BLURRED;
				}
			}
		}
	}

	// Called when the element is added into a hierarchy.
	void OnChildAdd( Element* element ) {
		Element::OnChildAdd( element );

		if( element == this ) {
			Element *document = GetOwnerDocument();
			if( document == NULL ) {
				return;
			}
			document->AddEventListener( "invalidate", this );
		}
	}

	// Called when the element is removed from a hierarchy.
	void OnChildRemove( Element* element ) {
		Element::OnChildRemove( element );

		if( element == this ) {
			Element *document = GetOwnerDocument();
			if( document == NULL ) {
				return;
			}
			document->RemoveEventListener( "invalidate", this );
		}
	}

	// Called for every event sent to this element or one of its descendants.
	void ProcessEvent( Rml::Core::Event& evt ) {
		if( evt == "invalidate" ) {
			Initialized = false;
		}
	}

	void InitLightStyles( void ) {
		for( int i = 0; i < MAX_LIGHTSTYLES; i++ ) {
			lightStyles[i] = "";
		}

		// this is constructed to match G_PrecacheMedia in g_spawn.cpp
		lightStyles[0] = LS_NORMAL;
		lightStyles[1] = LS_FLICKER1;
		lightStyles[2] = LS_SLOW_STRONG_PULSE;
		lightStyles[3] = LS_CANDLE1;
		lightStyles[4] = LS_FAST_STROBE;
		lightStyles[5] = LS_GENTLE_PULSE_1;
		lightStyles[6] = LS_FLICKER2;
		lightStyles[7] = LS_CANDLE2;
		lightStyles[8] = LS_CANDLE3;
		lightStyles[9] = LS_SLOW_STROBE;
		lightStyles[10] = LS_FLUORESCENT_FLICKER;
		lightStyles[11] = LS_SLOW_PULSE_NOT_FADE;

		// styles 32-62 are assigned by the light program for switchable lights
		lightStyles[63] = "a";
	}

	// Interpolates lightstyles and adds them to the scene
	void AddLightStylesToScene( void ) {
		float f;
		int ofs;
		const RefreshState &state = UI_Main::Get()->getRefreshState();

		f = float( state.time ) / 100.0f;
		ofs = (int)floor( f );
		f = f - float( ofs );

		for( int i = 0; i < MAX_LIGHTSTYLES; i++ ) {
			const String &ls = lightStyles[i];

			auto len = ls.size();
			if( len == 0 ) {
				trap::R_AddLightStyleToScene( i, 0, 0, 0 );
				continue;
			}

			auto charToIntensity = []( char c ) -> float {
									   return (float)( c - 'a' ) / (float)( 'm' - 'a' );
								   };

			auto l1 = charToIntensity( ls[ofs % len] );
			auto l2 = charToIntensity( ls[( ofs - 1 ) % len] );
			auto l = l1 * f + ( 1 - f ) * l2;

			trap::R_AddLightStyleToScene( i, l, l, l );
		}
	}

	virtual ~UI_WorldviewWidget() {
	}
};

//==============================================================

class UI_WorldviewWidgetInstancer : public ElementInstancer
{
public:
	UI_WorldviewWidgetInstancer() : ElementInstancer() {
	}

	// Rocket overrides
	virtual ElementPtr InstanceElement( Element *parent, const String &tag, const XMLAttributes &attr ) override {
		UI_WorldviewWidget *worldview = __new__( UI_WorldviewWidget )( tag );
		UI_Main::Get()->getRocket()->registerElementDefaults( worldview );
		return ElementPtr(worldview);
	}

	virtual void ReleaseElement( Element *element ) override {
		// then delete
		__delete__( element );
	}

private:
};

//============================================

ElementInstancer *GetWorldviewInstancer( void ) {
	ElementInstancer *instancer = __new__( UI_WorldviewWidgetInstancer )();
	return instancer;
}

}
