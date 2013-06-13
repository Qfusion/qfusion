/*
 * ui_renderinterface.h
 *
 *  Created on: 25.6.2011
 *      Author: hc
 *
 * Implements the RenderInterface for libRocket
 */
#pragma once
#ifndef UI_RENDERINTERFACE_H_
#define UI_RENDERINTERFACE_H_

#include <Rocket/Core/RenderInterface.h>
#include "kernel/ui_polyallocator.h"

namespace WSWUI
{

class UI_RenderInterface : public Rocket::Core::RenderInterface
{
public:
	UI_RenderInterface( int vidWidth, int vidHeight );
	virtual ~UI_RenderInterface();

	//// Implement the RenderInterface

	/// Called by Rocket when it wants to render geometry that it does not wish to optimise.
	virtual void RenderGeometry(Rocket::Core::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rocket::Core::TextureHandle texture, const Rocket::Core::Vector2f& translation);

	/// Called by Rocket when it wants to compile geometry it believes will be static for the forseeable future.
	virtual Rocket::Core::CompiledGeometryHandle CompileGeometry(Rocket::Core::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rocket::Core::TextureHandle texture);

	/// Called by Rocket when it wants to render application-compiled geometry.
	virtual void RenderCompiledGeometry(Rocket::Core::CompiledGeometryHandle geometry, const Rocket::Core::Vector2f& translation);
	/// Called by Rocket when it wants to release application-compiled geometry.
	virtual void ReleaseCompiledGeometry(Rocket::Core::CompiledGeometryHandle geometry);

	/// Called by Rocket when it wants to enable or disable scissoring to clip content.
	virtual void EnableScissorRegion(bool enable);
	/// Called by Rocket when it wants to change the scissor region.
	virtual void SetScissorRegion(int x, int y, int width, int height);

	/// Called by Rocket when a texture is required by the library.
	virtual bool LoadTexture(Rocket::Core::TextureHandle& texture_handle, Rocket::Core::Vector2i& texture_dimensions, const Rocket::Core::String& source);
	/// Called by Rocket when a texture is required to be built from an internally-generated sequence of pixels.
	virtual bool GenerateTexture(Rocket::Core::TextureHandle& texture_handle, const Rocket::Core::byte* source, const Rocket::Core::Vector2i& source_dimensions);
	/// Called by Rocket when a loaded texture is no longer required.
	virtual void ReleaseTexture(Rocket::Core::TextureHandle texture_handle);

	//// Methods
	int GetWidth( void );
	int GetHeight( void );

private:
	int vid_width;
	int vid_height;

	int texCounter;

	bool scissorEnabled;
	int scissorX, scissorY;
	int scissorWidth, scissorHeight;

	PolyAllocator polyAlloc;
	struct shader_s *whiteShader;

	poly_t *RocketGeometry2Poly(bool temp, Rocket::Core::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rocket::Core::TextureHandle texture);
};

}

#endif /* UI_RENDERINTERFACE_H_ */
