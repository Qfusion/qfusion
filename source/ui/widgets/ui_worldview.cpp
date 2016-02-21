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

#define MODELVIEW_EPSILON		1.0f

namespace WSWUI
{

using namespace Rocket::Core;

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
	float fovX;
	String mapName;
	String colorCorrection;
	struct shader_s *colorCorrectionShader;
	bool Initialized;

public:
	UI_WorldviewWidget( const String &tag )
		: Element( tag ), 
		mapName( "" ), colorCorrection( "" ), colorCorrectionShader( NULL ),
		Initialized( false )
	{
		memset( &refdef, 0, sizeof( refdef ) );
		refdef.areabits = 0;

		VectorClear( baseAngles );
		VectorClear( aWaveAmplitude );
		VectorClear( aWavePhase );
		VectorClear( aWaveFrequency );

		// Some default values
		Matrix3_Copy( axis_identity, refdef.viewaxis );
		fovX = 100.0f;
	}

	virtual void OnRender()
	{
		bool firstRender = false;
		Rocket::Core::Dictionary parameters;

		Element::OnRender();

		if( !Initialized ) {
			// lazily register the world model
			if( mapName.Empty() ) {
				return;
			}

			colorCorrectionShader = NULL;
			firstRender = true;
			Initialized = true;

			if( !colorCorrection.Empty() ) {
				colorCorrectionShader = trap::R_RegisterPic( colorCorrection.CString() );
			}

			trap::R_RegisterWorldModel( mapName.CString() );

			this->DispatchEvent( "registerworldmodel", parameters, false );
		}

		// refdef setup
		Rocket::Core::Vector2f box = GetBox().GetSize(Rocket::Core::Box::CONTENT);
		refdef.width = box.x;
		refdef.height = box.y;
		refdef.fov_x = fovX;
		refdef.fov_y = CalcFov( refdef.fov_x, refdef.width, refdef.height );
		AdjustFov( &refdef.fov_x, &refdef.fov_y, refdef.width, refdef.height, false );
		refdef.time = UI_Main::Get()->getRefreshState().time;

		vec3_t viewAngles;
		for( int i = 0; i < 3; i++ ) {
			viewAngles[i] = baseAngles[i] + aWaveAmplitude[i] * 
				sin( aWavePhase[i] + refdef.time * 0.001 * aWaveFrequency[i] * M_TWOPI );
		}
		AnglesToAxis( viewAngles, refdef.viewaxis );

		Rocket::Core::Vector2f offset = GetAbsoluteOffset(Rocket::Core::Box::CONTENT);
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

		trap::R_RenderScene( &refdef );

		trap::R_Scissor( scissor_x, scissor_y, scissor_w, scissor_h );

		if( firstRender ) {			
			this->DispatchEvent( "firstrender", parameters, false );
		}
	}

	virtual void OnPropertyChange(const Rocket::Core::PropertyNameList& changed_properties)
	{
		Element::OnPropertyChange(changed_properties);

		for (Rocket::Core::PropertyNameList::const_iterator it = changed_properties.begin(); it != changed_properties.end(); ++it)
		{
			if (*it == "worldmodel")
			{
				mapName = GetProperty(*it)->Get<String>();
			}
			else if (*it == "vieworigin-x" || *it == "vieworigin-y" || *it == "vieworigin-z")
			{
				char lastChar = (*it)[it->Length() - 1];
				refdef.vieworg[lastChar - 'x'] = atof( GetProperty(*it)->Get<String>().CString() );
			}
			else if (*it == "viewangle-pitch")
			{
				baseAngles[PITCH] = atof( GetProperty(*it)->Get<String>().CString() );
			}
			else if (*it == "viewangle-yaw")
			{
				baseAngles[YAW] = atof( GetProperty(*it)->Get<String>().CString() );
			}
			else if (*it == "viewangle-roll")
			{
				baseAngles[ROLL] = atof( GetProperty(*it)->Get<String>().CString() );
			}

			else if (*it == "wave-pitch-amplitude")
			{
				aWaveAmplitude[PITCH] = atof( GetProperty(*it)->Get<String>().CString() );
			}
			else if (*it == "wave-yaw-amplitude")
			{
				aWaveAmplitude[YAW] = atof( GetProperty(*it)->Get<String>().CString() );
			}
			else if (*it == "wave-roll-amplitude")
			{
				aWaveAmplitude[ROLL] = atof( GetProperty(*it)->Get<String>().CString() );
			}

			else if (*it == "wave-pitch-phase")
			{
				aWavePhase[PITCH] = atof( GetProperty(*it)->Get<String>().CString() ) / 360.0f * M_TWOPI;
			}
			else if (*it == "wave-yaw-phase")
			{
				aWavePhase[YAW] = atof( GetProperty(*it)->Get<String>().CString() ) / 360.0f * M_TWOPI;
			}
			else if (*it == "wave-roll-phase")
			{
				aWavePhase[ROLL] = atof( GetProperty(*it)->Get<String>().CString() ) / 360.0f * M_TWOPI;
			}

			else if (*it == "wave-pitch-frequency")
			{
				aWaveFrequency[PITCH] = atof( GetProperty(*it)->Get<String>().CString() );
			}
			else if (*it == "wave-yaw-frequency")
			{
				aWaveFrequency[YAW] = atof( GetProperty(*it)->Get<String>().CString() );
			}
			else if (*it == "wave-roll-frequency")
			{
				aWaveFrequency[ROLL] = atof( GetProperty(*it)->Get<String>().CString() );
			}

			else if (*it == "fov")
			{
				fovX = atof( GetProperty(*it)->Get<String>().CString() );
			}

			else if (*it == "color-correction")
			{
				colorCorrection = GetProperty(*it)->Get<String>();
			}
		}
	}

	// Called when the element is added into a hierarchy.
	void OnChildAdd( Element* element )
	{
		Element::OnChildAdd( element );

		if( element == this ) {
			Element *document = GetOwnerDocument();
			if( document == NULL )
				return;
			document->AddEventListener( "invalidate", this );
		}
	}

	// Called when the element is removed from a hierarchy.
	void OnChildRemove(Element* element)
	{
		Element::OnChildRemove( element );

		if( element == this ) {
			Element *document = GetOwnerDocument();
			if( document == NULL )
				return;
			document->RemoveEventListener( "invalidate", this );
		}
	}

	// Called for every event sent to this element or one of its descendants.
	void ProcessEvent( Rocket::Core::Event& evt )
	{
		if( evt == "invalidate" ) {
			Initialized = false;
		}
	}

	virtual ~UI_WorldviewWidget()
	{
	}
};

//==============================================================

class UI_WorldviewWidgetInstancer : public ElementInstancer
{
public:
	UI_WorldviewWidgetInstancer() : ElementInstancer()
	{
		StyleSheetSpecification::RegisterProperty( "vieworigin-x", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterProperty( "vieworigin-y", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterProperty( "vieworigin-z", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterShorthand( "vieworigin", "vieworigin-x, vieworigin-y, vieworigin-z" );

		StyleSheetSpecification::RegisterProperty( "viewangle-pitch", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterProperty( "viewangle-yaw", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterProperty( "viewangle-roll", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterShorthand( "viewangles", "viewangle-pitch, viewangle-yaw, viewangle-roll" );

		StyleSheetSpecification::RegisterProperty( "wave-pitch-amplitude", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterProperty( "wave-pitch-phase", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterProperty( "wave-pitch-frequency", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterShorthand( "wave-pitch", "wave-pitch-amplitude, wave-pitch-phase, wave-pitch-frequency" );

		StyleSheetSpecification::RegisterProperty( "wave-yaw-amplitude", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterProperty( "wave-yaw-phase", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterProperty( "wave-yaw-frequency", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterShorthand( "wave-yaw", "wave-yaw-amplitude, wave-yaw-phase, wave-yaw-frequency" );

		StyleSheetSpecification::RegisterProperty( "wave-roll-amplitude", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterProperty( "wave-roll-phase", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterProperty( "wave-roll-frequency", "0", false ).AddParser( "number" );
		StyleSheetSpecification::RegisterShorthand( "wave-roll", "wave-roll-amplitude, wave-roll-phase, wave-roll-frequency" );

		StyleSheetSpecification::RegisterProperty( "fov", "100", false ).AddParser( "number" );

		StyleSheetSpecification::RegisterProperty( "worldmodel", "", false ).AddParser( "string" );

		StyleSheetSpecification::RegisterProperty( "color-correction", "", false ).AddParser( "string" );
	}

	// Rocket overrides
	virtual Element *InstanceElement( Element *parent, const String &tag, const XMLAttributes &attr )
	{
		UI_WorldviewWidget *worldview = __new__( UI_WorldviewWidget )( tag );
		UI_Main::Get()->getRocket()->registerElementDefaults( worldview );
		return worldview;
	}

	virtual void ReleaseElement( Element *element )
	{
		// then delete
		__delete__( element );
	}

	virtual void Release()
	{
		__delete__( this );
	}

private:
};

//============================================

ElementInstancer *GetWorldviewInstancer( void )
{
	ElementInstancer *instancer = __new__( UI_WorldviewWidgetInstancer )();
	// instancer->RemoveReference();
	return instancer;
}

}
