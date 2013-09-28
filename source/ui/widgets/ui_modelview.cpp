/*
 * ui_modelview.cpp
 *
 *  Created on: 29.6.2011
 *      Author: hc
 */

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "kernel/ui_boneposes.h"
#include "widgets/ui_widgets.h"

#define MODELVIEW_EPSILON		1.0f

namespace WSWUI
{

using namespace Rocket::Core;

// forward-declare the instancer for keyselects
class UI_ModelviewWidgetInstancer;

class UI_ModelviewWidget : public Element
{
public:
	entity_t entity;
	refdef_t refdef;
	vec3_t baseangles;
	vec3_t angles;
	vec3_t anglespeed;
	unsigned int time;
	bool AutoRotationCenter;
	bool Initialized;
	UI_BonePoses BonePoses;
	cgs_skeleton_t *skel;

	UI_ModelviewWidget( const String &tag )
		: Element( tag ), 
		time( 0 ), AutoRotationCenter( false), Initialized( false ), skel( NULL )
	{
		memset( &entity, 0, sizeof( entity ) );
		memset( &refdef, 0, sizeof( refdef ) );
		entity.renderfx = RF_NOSHADOW | RF_FORCENOLOD | RF_MINLIGHT;
		refdef.rdflags = RDF_NOWORLDMODEL | RDF_NOFOVADJUSTMENT;
		refdef.areabits = 0;
		refdef.minLight = 0.7;
		Matrix3_Copy( axis_identity, refdef.viewaxis );

		// Some default values
		VectorSet(baseangles, 0, 0, 0);
		VectorSet(anglespeed, 0, 0, 0);
		refdef.fov_x = 30.0f;
		entity.scale = 1.0f;
		entity.outlineHeight = 0.3f;
		Vector4Set(entity.outlineRGBA, 0.25f, 0.25f, 0.25f, 1.0f);
		Vector4Set(entity.shaderRGBA, 1.0f, 1.0f, 1.0f, 1.0f);
	}

	void ComputePosition()
	{
		if (!entity.model)
			return;

		// refdef setup
		Rocket::Core::Vector2f box = GetBox().GetSize(Rocket::Core::Box::CONTENT);
		refdef.x = 0;
		refdef.y = 0;
		refdef.width = box.x;
		refdef.height = box.y;
		refdef.fov_y = CalcFov( refdef.fov_x, refdef.width, refdef.height );

		skel = NULL;
		if (trap::R_SkeletalGetNumBones( entity.model, NULL ))
		{
			skel = BonePoses.SkeletonForModel( entity.model );
			BonePoses.SetBoneposesForTemporaryEntity( &entity );
		}

		// entity setup
		// Set origin to fit the viewport according to initial rotation
		//trap::R_FitModelPositionInViewport( entity.model, baseangles, refdef.fov_x, refdef.fov_y, entity.origin);

		vec3_t mins, maxs;
		trap::R_ModelFrameBounds( entity.model, entity.frame, mins, maxs );

		entity.origin[0] = 0.5 * ( maxs[2] - mins[2] ) * ( 1.0 / 0.220 );
		entity.origin[1] = 0.5 * ( mins[1] + maxs[1] );
		entity.origin[2] = -0.5 * ( mins[2] + maxs[2] );

		VectorCopy( entity.origin, entity.origin2 );
		VectorCopy( baseangles, angles );

		Initialized = true;
	}

	virtual void OnRender()
	{
		Element::OnRender();

		if (!Initialized || !entity.model)
		{
			return;
		}

		unsigned int curtime = UI_Main::Get()->getRefreshState().time;
		float deltatime = curtime - time;

		refdef.time = curtime;

		for (int i = 0; i < 3; ++i)
			angles[i] += deltatime * anglespeed[i] / 1000.0f;

		AnglesToAxis( angles, entity.axis );

		// TODO: 1 and 39 are the value of cvars ui_playermodel_firstframe and ui_playermodel_lastframe
		entity.oldframe = entity.frame;
		entity.frame = 1 + (entity.frame % 39);

		if (AutoRotationCenter)
		{
			// Update origin to make the rotation centered into viewport
			vec3_t mins, maxs;
			trap::R_ModelBounds( entity.model, mins, maxs );
			vec3_t buf;
			buf[0] = -0.5 * (mins[0] + maxs[0]);
			buf[1] = -0.5 * (mins[1] + maxs[1]);
			buf[2] = -0.5 * (mins[2] + maxs[2]);

			// Just keep the x component
			float xoffset = entity.origin[0];
			mat3_t localAxis;
			Matrix3_Transpose( entity.axis, localAxis );
			Matrix3_TransformVector( localAxis, buf, entity.origin );
			entity.origin[0] = xoffset;
		}

		Rocket::Core::Vector2f offset = GetAbsoluteOffset(Rocket::Core::Box::CONTENT);
		refdef.x = offset.x;
		refdef.y = offset.y;

		// clip scissor region to parent
		int scissor_x, scissor_y, scissor_w, scissor_h;
		trap::R_GetScissorRegion( &scissor_x, &scissor_y, &scissor_w, &scissor_h );
		refdef.scissor_x = std::max( scissor_x, refdef.x );
		refdef.scissor_y = std::max( scissor_y, refdef.y );
		refdef.scissor_width = std::min( scissor_w, refdef.width );
		refdef.scissor_height = std::min( scissor_h, refdef.height );

		trap::R_ClearScene();

		trap::R_AddEntityToScene( &entity );

		trap::R_RenderScene( &refdef );

		// TODO: Should this be done here or in ComputePosition?
		BonePoses.ResetTemporaryBoneposesCache();
		time = curtime;

	}

	virtual void OnPropertyChange(const Rocket::Core::PropertyNameList& changed_properties)
	{
		bool RecomputePosition = false;

		Element::OnPropertyChange(changed_properties);

		for (Rocket::Core::PropertyNameList::const_iterator it = changed_properties.begin(); it != changed_properties.end(); ++it)
		{
			if (*it == "model-modelpath")
			{
				if (GetProperty(*it)->Get<Rocket::Core::String>().Length())
					entity.model = trap::R_RegisterModel(GetProperty(*it)->Get<Rocket::Core::String>().CString());
				else
					entity.model = NULL;
				RecomputePosition = true;
			}
			else if (*it == "model-skinpath" && GetProperty(*it)->Get<Rocket::Core::String>().Length() > 0)
			{
				if (GetProperty(*it)->Get<Rocket::Core::String>().Length())
					entity.customSkin = trap::R_RegisterSkinFile(GetProperty(*it)->Get<Rocket::Core::String>().CString());
				else
					entity.customSkin = NULL;
			}
			else if (*it == "model-scale")
			{
				entity.scale = GetProperty(*it)->Get<float>();
			}
			else if (*it == "model-outline-height")
			{
				entity.outlineHeight = GetProperty(*it)->Get<float>();
			}
			else if (*it == "model-outline-color")
			{
				Rocket::Core::Colourb color = GetProperty(*it)->Get<Rocket::Core::Colourb>();
				Vector4Set(entity.outlineRGBA, color.red, color.green, color.blue, color.alpha);
			}
			else if (*it == "model-shader-color")
			{
				Rocket::Core::Colourb color = GetProperty(*it)->Get<Rocket::Core::Colourb>();
				Vector4Set(entity.shaderRGBA, color.red, color.green, color.blue, color.alpha);
			}
			else if (*it == "model-fov-x")
			{
				refdef.fov_x = GetProperty(*it)->Get<float>();
				clamp(refdef.fov_x, 1, 179);
				RecomputePosition = true;
			}
			else if (*it == "model-rotation-pitch")
			{
				baseangles[0] = GetProperty(*it)->Get<float>();
				RecomputePosition = true;
			}
			else if (*it == "model-rotation-yaw")
			{
				baseangles[1] = GetProperty(*it)->Get<float>();
				RecomputePosition = true;
			}
			else if (*it == "model-rotation-roll")
			{
				baseangles[2] = GetProperty(*it)->Get<float>();
				RecomputePosition = true;
			}
			else if (*it == "model-rotation-speed-pitch")
			{
				anglespeed[0] = GetProperty(*it)->Get<float>();
			}
			else if (*it == "model-rotation-speed-yaw")
			{
				anglespeed[1] = GetProperty(*it)->Get<float>();
			}
			else if (*it == "model-rotation-speed-roll")
			{
				anglespeed[2] = GetProperty(*it)->Get<float>();
			}
			else if (*it == "model-rotation-autocenter")
			{
				AutoRotationCenter = (GetProperty(*it)->Get<Rocket::Core::String>().ToLower() == "true");
			}
		}

		if (abs(refdef.width - GetClientWidth()) >= MODELVIEW_EPSILON || abs(refdef.height - GetClientHeight()) >= MODELVIEW_EPSILON)
			RecomputePosition = true;

		if ((refdef.x - GetAbsoluteLeft() + GetClientLeft()) >= MODELVIEW_EPSILON || (refdef.y - GetAbsoluteTop() + GetClientTop()) >= MODELVIEW_EPSILON)
			RecomputePosition = true;

		if (RecomputePosition)
			ComputePosition();
	}

	virtual ~UI_ModelviewWidget()
	{
	
	}
};

//==============================================================

class UI_ModelviewWidgetInstancer : public ElementInstancer
{
public:
	UI_ModelviewWidgetInstancer() : ElementInstancer()
	{
		StyleSheetSpecification::RegisterProperty("model-modelpath", "", false).AddParser("string");
		StyleSheetSpecification::RegisterProperty("model-skinpath", "", false).AddParser("string");
		StyleSheetSpecification::RegisterProperty("model-fov-x", "30", false).AddParser("number");
		StyleSheetSpecification::RegisterProperty("model-scale", "1", false).AddParser("number");
		StyleSheetSpecification::RegisterProperty("model-outline-height", "0.3", false).AddParser("number"); // DEFAULT_OUTLINE_HEIGHT
		StyleSheetSpecification::RegisterProperty("model-outline-color", "#404040FF", false).AddParser("color");
		StyleSheetSpecification::RegisterProperty("model-shader-color", "#FFFFFFFF", false).AddParser("color");
		StyleSheetSpecification::RegisterProperty("model-rotation-pitch", "0", false).AddParser("number");
		StyleSheetSpecification::RegisterProperty("model-rotation-yaw", "0", false).AddParser("number");
		StyleSheetSpecification::RegisterProperty("model-rotation-roll", "0", false).AddParser("number");
		StyleSheetSpecification::RegisterProperty("model-rotation-speed-pitch", "0", false).AddParser("number");
		StyleSheetSpecification::RegisterProperty("model-rotation-speed-yaw", "0", false).AddParser("number");
		StyleSheetSpecification::RegisterProperty("model-rotation-speed-roll", "0", false).AddParser("number");
	}

	// Rocket overrides
	virtual Element *InstanceElement( Element *parent, const String &tag, const XMLAttributes &attr )
	{
		UI_ModelviewWidget *modelview = __new__( UI_ModelviewWidget )( tag );
		UI_Main::Get()->getRocket()->registerElementDefaults( modelview );
		return modelview;
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

ElementInstancer *GetModelviewInstancer( void )
{
	ElementInstancer *instancer = __new__( UI_ModelviewWidgetInstancer )();
	// instancer->RemoveReference();
	return instancer;
}

}
