/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the declaration of the RendererD3D9 class.

#ifndef O3D_CORE_WIN_D3D9_RENDERER_D3D9_H_
#define O3D_CORE_WIN_D3D9_RENDERER_D3D9_H_

#include <atlbase.h>
#include <d3d9.h>

#include <vector>

#include "core/cross/display_mode.h"
#include "core/cross/display_window.h"
#include "core/cross/renderer.h"
#include "core/cross/state.h"
#include "core/cross/texture.h"
#include "core/cross/timer.h"
#include "core/cross/types.h"

interface ID3DXFont;
interface ID3DXLine;

namespace o3d {

class Material;
class Texture2D;
class ObjectManager;
class ParamCache;
class SemanticManager;

// The RenderedD9D9 class implements the genereric Renderer interface using
// DirectX9.
class RendererD3D9 : public Renderer {
 public:
  // Creates a default Renderer.
  static RendererD3D9* CreateDefault(ServiceLocator* service_locator);
  virtual ~RendererD3D9();

  // Initialises the renderer for use, claiming hardware resources.
  virtual InitStatus InitPlatformSpecific(const DisplayWindow& display,
                                          bool off_screen);

  // Released all hardware resources.
  virtual void Destroy();

  // This method should be called before any draw calls take place in a
  // frame. It clears the back buffer, stencil and depth buffers.
  virtual bool BeginDraw();

  // Finalizes the drawing of the frame.
  virtual void EndDraw();

  // Does any pre-rendering preparation
  virtual bool StartRendering();

  // Presents the results of the draw calls for this frame.
  virtual void FinishRendering();

  // Attempts to reset the back buffer to its new dimensions.
  virtual void Resize(int width, int height);

  // Turns fullscreen display on or off.
  // Parameters:
  //  fullscreen: true for fullscreen, false for in-plugin display
  //  display: a platform-specific display identifier
  //  mode_id: a mode returned by GetDisplayModes, for fullscreen use.  Ignored
  //      in non-fullscreen mode.
  // Returns true on success, false on failure.
  virtual bool SetFullscreen(bool fullscreen, const DisplayWindow& display,
                             int mode_id);

  // Tells whether we're currently displayed fullscreen or not.
  virtual bool fullscreen() const {
    return fullscreen_;
  }

  // Get a vector of the available fullscreen display modes.
  // Clears *modes on error.
  virtual void GetDisplayModes(std::vector<DisplayMode> *modes);

  // Get a single fullscreen display mode by id.
  // Returns true on success, false on error.
  virtual bool GetDisplayMode(int id, DisplayMode *mode);

  // clears the current buffers
  virtual void Clear(const Float4 &color,
                     bool color_flag,
                     float depth,
                     bool depth_flag,
                     int stencil,
                     bool stencil_flag);

  // Draws a Element.
  virtual void RenderElement(Element* element,
                             DrawElement* draw_element,
                             Material* material,
                             ParamObject* override,
                             ParamCache* param_cache);

  // Creates a StreamBank, returning a platform specific implementation class.
  virtual StreamBank::Ref CreateStreamBank();

  // Creates a Primitive, returning a platform specific implementation class.
  virtual Primitive::Ref CreatePrimitive();

  // Creates a DrawElement, returning a platform specific implementation
  // class.
  virtual DrawElement::Ref CreateDrawElement();

  // Creates and returns a D3D9 specific float buffer.
  virtual VertexBuffer::Ref CreateVertexBuffer();

  // Creates and returns a D3D9 specific integer buffer.
  virtual IndexBuffer::Ref CreateIndexBuffer();

  // Creates and returns a D3D9 specific Effect object.
  virtual Effect::Ref CreateEffect();

  // Creates and returns a D3D9 specific Sampler object.
  virtual Sampler::Ref CreateSampler();

  // Creates and returns a platform-specific RenderDepthStencilSurface object
  // for use as a depth-stencil render target.
  virtual RenderDepthStencilSurface::Ref CreateDepthStencilSurface(
      int width,
      int height);

  // Saves a png screenshot 'file_name.png'.
  // Returns true on success and false on failure.
  virtual bool SaveScreen(const String& file_name);

  inline LPDIRECT3DDEVICE9 d3d_device() const { return d3d_device_; }
  inline LPDIRECT3D9 d3d() const { return d3d_; }

  inline DWORD supported_depth_format() const {
    return supported_depth_format_;
  }

  // Overridden from Renderer.
  virtual const int* GetRGBAUByteNSwizzleTable();

 protected:
  // Keep the constructor protected so only factory methods can create
  // renderers.
  explicit RendererD3D9(ServiceLocator* service_locator);

  // Overridden from Renderer.
  virtual ParamCache* CreatePlatformSpecificParamCache();

  // Overridden from Renderer.
  virtual void SetBackBufferPlatformSpecific();

  // Overridden from Renderer.
  virtual void SetRenderSurfacesPlatformSpecific(
      RenderSurface* surface,
      RenderDepthStencilSurface* depth_surface);

  // Sets the viewport. This is the platform specific version.
  void SetViewportInPixels(int left,
                           int top,
                           int width,
                           int height,
                           float min_z,
                           float max_z);

  // Overridden from Renderer.
  virtual Texture::Ref CreatePlatformSpecificTextureFromBitmap(Bitmap* bitmap);

  // Overridden from Renderer.
  virtual Texture2D::Ref CreatePlatformSpecificTexture2D(
      int width,
      int height,
      Texture::Format format,
      int levels,
      bool enable_render_surfaces);

  // Overridden from Renderer.
  virtual TextureCUBE::Ref CreatePlatformSpecificTextureCUBE(
      int edge_length,
      Texture::Format format,
      int levels,
      bool enable_render_surfaces);

 private:
  ServiceDependency<ObjectManager> object_manager_;
  ServiceDependency<SemanticManager> semantic_manager_;

  // Instance of the D3D9 interface.
  CComPtr<IDirect3D9> d3d_;

  // Instance of the D3DDevice9 interface.
  CComPtr<IDirect3DDevice9> d3d_device_;

  // D3DFORMAT value of the depth surface type supported.
  DWORD supported_depth_format_;

  // Pointer to interface of the off-screen-surface used for off-screen
  // rendering.  Is non-null when off-screen rendering is enabled.
  CComPtr<IDirect3DSurface9> off_screen_surface_;

  CComPtr<IDirect3DSurface9> back_buffer_surface_;
  CComPtr<IDirect3DSurface9> back_buffer_depth_surface_;

  // D3DPresent parameters (for initializing and resetting the device.)
  D3DPRESENT_PARAMETERS d3d_present_parameters_;

  // Flag to tell use we need to use small index buffers.
  bool use_small_index_buffers_;

  // Flag to tell us whether we have or lost the device.
  bool have_device_;

  // Indicates we're rendering fullscreen rather than in the plugin region.
  bool fullscreen_;
  // Indicates we're showing the "Press Escape..." banner.
  bool showing_fullscreen_message_;
  // We want to show the message for about 3 seconds.
  ElapsedTimeTimer fullscreen_message_timer_;
  // Draws the actual message.
  void ShowFullscreenMessage(float elapsedTime, float display_duration);

  // Invalidates all resources which are in D3DPOOL_DEFAULT.
  // Used before we try to reset the device, when the device is lost.
  // (ie when suspending the computer, locking it, etc.)
  // Returns true on success and false on failure.
  bool InvalidateDeviceObjects();

  // Restore all resources which are in D3DPOOL_DEFAULT.
  // Used after we reset the direct3d device to restore these resources.
  // Returns true on success and false on failure.
  bool RestoreDeviceObjects();

  // This function tests if the device is lost and sets the have_device_ flag
  // appropriately.
  // It attempts to reset the device if it is lost by calling ResetDevice().
  void TestLostDevice();

  // Attempts to reset the device.
  // Returns true on success.
  bool ResetDevice();

  // The font to use to display the message when we go to fullscreen.
  CComPtr<ID3DXFont> fullscreen_message_font_;
  // The line used to draw the background for the message.
  CComPtr<ID3DXLine> fullscreen_message_line_;
};

}  // namespace o3d

#endif  // O3D_CORE_WIN_D3D9_RENDERER_D3D9_H_
