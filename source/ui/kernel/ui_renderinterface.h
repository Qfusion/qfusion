/*
 * ui_renderinterface.h
 *
 *  Created on: 25.6.2011
 *      Author: hc
 *
 * Implements the RenderInterface for libRmlUi
 */
#pragma once
#ifndef UI_RENDERINTERFACE_H_
#define UI_RENDERINTERFACE_H_

#include <map>
#include "kernel/ui_polyallocator.h"
#include <RmlUi/Core/RenderInterface.h>

namespace WSWUI
{
class UI_RenderInterface : public Rml::Core::RenderInterface
{
public:
	UI_RenderInterface( int vidWidth, int vidHeight, float pixelRatio );
	virtual ~UI_RenderInterface();

	//// Implement the RenderInterface

	/// Called by RmlUi when it wants to render geometry that it does not wish to optimise.
	virtual void RenderGeometry( Rml::Core::Vertex *vertices, int num_vertices, int *indices, int num_indices, Rml::Core::TextureHandle texture, const Rml::Core::Vector2f &translation );

	/// Called by RmlUi when it wants to compile geometry it believes will be static for the forseeable future.
	virtual Rml::Core::CompiledGeometryHandle CompileGeometry( Rml::Core::Vertex *vertices, int num_vertices, int *indices, int num_indices, Rml::Core::TextureHandle texture );

	/// Called by RmlUi when it wants to render application-compiled geometry.
	virtual void RenderCompiledGeometry( Rml::Core::CompiledGeometryHandle geometry, const Rml::Core::Vector2f &translation );
	/// Called by RmlUi when it wants to release application-compiled geometry.
	virtual void ReleaseCompiledGeometry( Rml::Core::CompiledGeometryHandle geometry );

	/// Called by RmlUi when it wants to enable or disable scissoring to clip content.
	virtual void EnableScissorRegion( bool enable );
	/// Called by RmlUi when it wants to change the scissor region.
	virtual void SetScissorRegion( int x, int y, int width, int height );

	/// Called by RmlUi when a texture is required by the library.
	virtual bool LoadTexture( Rml::Core::TextureHandle &texture_handle, Rml::Core::Vector2i &texture_dimensions, const Rml::Core::String &source );
	/// Called by RmlUi when a texture is required to be built from an internally-generated sequence of pixels.
	virtual bool GenerateTexture( Rml::Core::TextureHandle &texture_handle, const Rml::Core::byte *source, const Rml::Core::Vector2i &source_dimensions/*, int source_samples*/ );
	/// Called by RmlUi when a loaded texture is no longer required.
	virtual void ReleaseTexture( Rml::Core::TextureHandle texture_handle );

	/// Returns the number of pixels per inch.
	virtual float GetPixelsPerInch( void );

	/// Called by RmlUi when it wants to set the current transform matrix to a new matrix.
	virtual void SetTransform( const Rml::Core::Matrix4f* transform );

	//// Methods
	int GetWidth( void ) const { return this->vid_width; }
	int GetHeight( void ) const { return this->vid_height; }
	float GetPixelRatio( void ) const { return pixelRatio; }

	void AddShaderToCache( const Rml::Core::String &shader );
	void ClearShaderCache( void );
	void TouchAllCachedShaders( void );

   private:
	const float basePixelsPerInch = Q_BASE_DPI;

	int vid_width;
	int vid_height;

	float pixelsPerInch;
	float pixelRatio;

	int texCounter;

	bool scissorEnabled;
	int scissorX, scissorY;
	int scissorWidth, scissorHeight;

	PolyAllocator polyAlloc;
	struct shader_s *whiteShader;

	typedef std::map<std::string, char> ShaderMap;
	ShaderMap shaderMap;

	poly_t *RmlUiGeometry2Poly( bool temp, Rml::Core::Vertex *vertices, int num_vertices, int *indices, int num_indices, Rml::Core::TextureHandle texture );
};

} // namespace WSWUI

#endif /* UI_RENDERINTERFACE_H_ */
