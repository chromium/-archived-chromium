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


// This file contains the definition of the RendererGL class that provides
// low-level access for O3D to graphics hardware using the OpenGL API
// and Cg Runtime.

#ifndef O3D_CORE_CROSS_GL_RENDERER_GL_H_
#define O3D_CORE_CROSS_GL_RENDERER_GL_H_

#include <Cg/cg.h>
#include <build/build_config.h>
#include "core/cross/renderer.h"
#include "core/cross/renderer_platform.h"
#include "core/cross/types.h"
#include "core/cross/state.h"

namespace o3d {

class Material;
class Effect;
class DrawEffect;
class SemanticManager;

// Implements the genereric Renderer interface using OpenGL and the Cg Runtime.
class RendererGL : public Renderer {
 public:
  // Creates a default Renderer.
  static RendererGL* CreateDefault(ServiceLocator* service_locator);
  virtual ~RendererGL();

  // Initialises the renderer for use, claiming hardware resources.
  virtual InitStatus InitPlatformSpecific(const DisplayWindow& display,
                                          bool off_screen);

  // Released all hardware resources.
  virtual void Destroy();

  // This method should be called before any draw calls take place in a
  // frame. It clears the back buffer, stencil and depth buffers.
  // Returns true on success.
  virtual bool BeginDraw();

  // Finalizes the drawing of the frame.
  virtual void EndDraw();

  // Does any pre-rendering preparation
  virtual bool StartRendering();

  // Presents the results of the draw calls for this frame.
  virtual void FinishRendering();

  // Turns fullscreen display on or off.
  // Parameters:
  //  fullscreen: true for fullscreen, false for in-browser display
  //  display: a platform-specific display identifier
  //  mode_id: a mode returned by GetDisplayModes, for fullscreen use.
  //  (Ignored in non-fullscreen mode.)
  // Returns true on success, false on failure.
  virtual bool SetFullscreen(bool fullscreen,
                             const DisplayWindow& display,
                             int mode_id);

  // Tells whether we're currently displayed fullscreen or not.
  virtual bool fullscreen() const {
    return fullscreen_;
  }

  // Resizes the viewport in OpenGL.
  virtual void Resize(int width, int height);

  // clears the current buffers
  virtual void Clear(const Float4 &color,
                     bool color_flag,
                     float depth,
                     bool depth_flag,
                     int stencil,
                     bool stencil_flag);

  // Draws a Element.
  // Parameters:
  //   element: Element to draw draw_element: DrawElement to override
  //   params with.
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

  // Creates and returns a GL specific float buffer.
  virtual VertexBuffer::Ref CreateVertexBuffer();

  // Creates and returns a GL specific integer buffer.
  virtual IndexBuffer::Ref CreateIndexBuffer();

  // Creates and returns a GL specific Effect object.
  virtual Effect::Ref CreateEffect();

  // Creates and returns a GL specific Sampler object.
  virtual Sampler::Ref CreateSampler();

  // Creates and returns a platform-specific RenderDepthStencilSurface object
  // for use as a depth-stencil render target.
  virtual RenderDepthStencilSurface::Ref CreateDepthStencilSurface(
      int width,
      int height);

  // Saves a png screenshot 'file_name.png'.
  // Returns true on success and false on failure.
  virtual bool SaveScreen(const String& file_name);

  // Overridden from Renderer.
  virtual const int* GetRGBAUByteNSwizzleTable();

  // Makes this renderer active on the current thread if it is not active
  // already.
  void MakeCurrentLazy() {
    if (!IsCurrent()) MakeCurrent();
  }

  // Returns whether or not this renderer is active on the current thread.
  // In the Mac case, also requires the correct GL context to be active.
  // Don't worry, the "get" calls are el cheapo.
  bool IsCurrent() {
#ifdef OS_MACOSX
    if ((mac_agl_context_ != NULL) &&
        (mac_agl_context_ != aglGetCurrentContext())) {
      return false;
    } else if ((mac_cgl_context_ != NULL) &&
               (mac_cgl_context_ != CGLGetCurrentContext())) {
      return false;
    }
#endif
    return this == current_renderer_;
  }

  // Makes this renderer active on the current thread.
  bool MakeCurrent();

  inline CGcontext cg_context() const { return cg_context_; }
  inline CGprofile cg_vertex_profile() const { return cg_vertex_profile_; }
  inline CGprofile cg_fragment_profile() const { return cg_fragment_profile_; }

 protected:
  // Keep the constructor protected so only factory methods can create
  // renderers.
  explicit RendererGL(ServiceLocator* service_locator);

  // Overridden from Renderer.
  virtual ParamCache* CreatePlatformSpecificParamCache();

  // Sets the viewport. This is the platform specific version.
  void SetViewportInPixels(int left,
                           int top,
                           int width,
                           int height,
                           float min_z,
                           float max_z);

  // Overridden from Renderer.
  virtual void SetBackBufferPlatformSpecific();

  // Overridden from Renderer.
  virtual void SetRenderSurfacesPlatformSpecific(
      RenderSurface* surface,
      RenderDepthStencilSurface* depth_surface);

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
  // Platform-independent GL initialization
  InitStatus InitCommonGL();

  // Platform-independent GL destruction
  void DestroyCommonGL();

  // Updates the helper constant used to remap D3D clip coordinates to GL ones.
  void UpdateHelperConstant(float width, float height);

  ServiceDependency<SemanticManager> semantic_manager_;

  // Current renderer, tracking which renderer has last called wglMakeCurrent
  // (or equivalent on other platforms).
  // NOTE: this should really be thread-local, but since we don't handle
  // multiple threads currently, this is enough.
  static RendererGL *current_renderer_;

  // Indicates we're rendering fullscreen rather than in the plugin region.
  bool fullscreen_;


#ifdef OS_WIN
  // Handle to the GL device.
  HWND window_;
  HDC device_context_;
  HGLRC gl_context_;
#endif

#ifdef OS_MACOSX
  AGLContext    mac_agl_context_;
  CGLContextObj mac_cgl_context_;
#endif

#ifdef OS_LINUX
  Display *display_;
  Window window_;
  GLXContext context_;
#endif

  // Handle to the framebuffer-object used while rendering to off-screen
  // targets.
  GLuint render_surface_framebuffer_;

  // Cg Runtime variables.
  CGcontext cg_context_;
  CGprofile cg_vertex_profile_;
  CGprofile cg_fragment_profile_;

  // Sets the states that need muliple arguments in GL.
  void SetChangedStates();

  friend class AlphaReferenceHandler;
  bool alpha_function_ref_changed_;
  GLenum alpha_function_;
  GLclampf alpha_ref_;

  friend class BlendFunctionHandler;
  friend class BlendEquationHandler;
  bool alpha_blend_settings_changed_;
  bool separate_alpha_blend_enable_;
  enum {
    RGB,
    ALPHA,
  };
  enum {
    SRC,
    DST,
  };
  GLenum blend_function_[2][2];  // SRC/DST, RGB/ALPHA
  GLenum blend_equation_[2];  //  RGB/ALPHA

  bool stencil_settings_changed_;
  bool separate_stencil_settings_enable_;

  // States for Stencils
  friend class StencilOperationHandler;
  friend class StencilRefHandler;
  friend class StencilMaskHandler;
  struct StencilStates {
    GLenum func_;
    enum {
      FAIL_OP,
      ZFAIL_OP,
      PASS_OP,
    };
    int op_[3];
  };

  enum {
    FRONT,
    BACK,
  };
  StencilStates stencil_settings_[2];

  enum {
    READ_MASK,
    WRITE_MASK,
  };
  int stencil_mask_[2];
  int stencil_ref_;

  // States for PolygonOffset
  friend class PolygonOffset1Handler;
  friend class PolygonOffset2Handler;
  bool polygon_offset_changed_;
  float polygon_offset_factor_;
  float polygon_offset_bias_;

  // Sets the stencils states for either front, back or both facing polys.
  void SetStencilStates(GLenum face, const StencilStates& stencil_states);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_GL_RENDERER_GL_H_
