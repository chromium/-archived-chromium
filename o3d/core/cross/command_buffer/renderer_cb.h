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


// This file contains the implementation of the command buffer renderer.

#ifndef O3D_CORE_CROSS_COMMAND_BUFFER_RENDERER_CB_H_
#define O3D_CORE_CROSS_COMMAND_BUFFER_RENDERER_CB_H_

#include "core/cross/precompile.h"
#include "core/cross/renderer.h"
#include "command_buffer/common/cross/rpc.h"
#include "command_buffer/common/cross/resource.h"
#include "command_buffer/client/cross/id_allocator.h"

namespace o3d {
namespace command_buffer {
class CommandBufferHelper;
class BufferSyncInterface;
class FencedAllocatorWrapper;
}  // namespace command_buffer

class Material;

// TODO: change the Init API to not rely on direct HWND so we don't need
// to create a command buffer locally.
class Win32CBServer;

// This is the implementation of the Renderer interface for command buffers.
class RendererCB : public Renderer {
 public:
  typedef command_buffer::IdAllocator IdAllocator;
  typedef command_buffer::FencedAllocatorWrapper FencedAllocatorWrapper;

  // Creates a default RendererCB.
  // The default command buffer is 256K entries.
  // The default transfer buffer is 16MB.
  static RendererCB *CreateDefault(ServiceLocator* service_locator);
  ~RendererCB();

  // Initialises the renderer for use, claiming hardware resources.
  virtual InitStatus InitPlatformSpecific(const DisplayWindow& display_window,
                                          bool off_screen);

  // Handles the plugin resize event.
  virtual void Resize(int width, int height);

  // Releases all hardware resources. This should be called before destroying
  // the window used for rendering. It will be called automatically from the
  // destructor.
  // Destroy() should be called before Init() is called again.
  virtual void Destroy();

  // Prepares the rendering device for subsequent draw calls.
  virtual bool BeginDraw();

  // Clears the current buffers.
  virtual void Clear(const Float4 &color,
                     bool color_flag,
                     float depth,
                     bool depth_flag,
                     int stencil,
                     bool stencil_flag);

  // Notifies the renderer that the draw calls for this frame are completed.
  virtual void EndDraw();

  // Does any pre-rendering preparation
  virtual bool StartRendering();

  // Presents the results of the draw calls for this frame.
  virtual void FinishRendering();

  // Renders this Element using the parameters from override first, followed by
  // the draw_element, followed by params on this Primitive and material.
  // Parameters:
  //   element: Element to draw
  //   draw_element: DrawElement to override params with.
  //   material: Material to render with.
  //   override: Override to render with.
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

  // Creates and returns a platform specific float buffer
  virtual VertexBuffer::Ref CreateVertexBuffer();

  // Creates and returns a platform specific integer buffer
  virtual IndexBuffer::Ref CreateIndexBuffer();

  // Creates and returns a platform specific effect object
  virtual Effect::Ref CreateEffect();

  // Creates and returns a platform specific Sampler object.
  virtual Sampler::Ref CreateSampler();

  // TODO: Fill this in
  virtual RenderDepthStencilSurface::Ref CreateDepthStencilSurface(
      int width,
      int height) {
    return RenderDepthStencilSurface::Ref();
  }

  // Saves a png screenshot.
  // Returns true on success and false on failure.
  virtual bool SaveScreen(const String& file_name);

  // Gets the allocator for vertex buffer IDs.
  IdAllocator &vertex_buffer_ids() { return vertex_buffer_ids_; }

  // Gets the allocator for index buffer IDs.
  IdAllocator &index_buffer_ids() { return index_buffer_ids_; }

  // Gets the allocator for vertex struct IDs.
  IdAllocator &vertex_structs_ids() { return vertex_structs_ids_; }

  // Gets the allocator for effect IDs.
  IdAllocator &effect_ids() { return effect_ids_; }

  // Gets the allocator for effect param IDs.
  IdAllocator &effect_param_ids() { return effect_param_ids_; }

  // Gets the allocator for texture IDs.
  IdAllocator &texture_ids() { return texture_ids_; }

  // Gets the allocator for sampler IDs.
  IdAllocator &sampler_ids() { return sampler_ids_; }

  // Gets the command buffer helper.
  command_buffer::CommandBufferHelper *helper() const { return helper_; }

  // Gets the sync interface.
  command_buffer::BufferSyncInterface *sync_interface() const {
    return sync_interface_;
  }

  // Gets the registered ID of the transfer shared memory.
  unsigned int transfer_shm_id() const { return transfer_shm_id_; }

  // Gets the base address of the transfer shared memory.
  void *transfer_shm_address() const { return transfer_shm_address_; }

  // Gets the allocator of the transfer shared memory.
  FencedAllocatorWrapper *allocator() const {
    return allocator_;
  }

  // Overridden from Renderer.
  virtual const int* GetRGBAUByteNSwizzleTable();

 protected:
  // Protected so that callers are forced to call the factory method.
  RendererCB(ServiceLocator* service_locator, unsigned int command_buffer_size,
             unsigned int transfer_memory_size);

  // Creates a platform specific ParamCache.
  virtual ParamCache* CreatePlatformSpecificParamCache();

  // Sets the viewport. This is the platform specific version.
  virtual void SetViewportInPixels(int left,
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
  // Applies states that have been modified (marked dirty).
  void ApplyDirtyStates();

  // Performs cross-platform initialization.
  void InitCommon(unsigned int width, unsigned int height);

  unsigned int cmd_buffer_size_;
  unsigned int transfer_memory_size_;
  command_buffer::RPCShmHandle transfer_shm_;
  unsigned int transfer_shm_id_;
  void *transfer_shm_address_;
  command_buffer::BufferSyncInterface *sync_interface_;
  command_buffer::CommandBufferHelper *helper_;
  FencedAllocatorWrapper *allocator_;
  Win32CBServer *cb_server_;

  IdAllocator vertex_buffer_ids_;
  IdAllocator index_buffer_ids_;
  IdAllocator vertex_structs_ids_;
  IdAllocator effect_ids_;
  IdAllocator effect_param_ids_;
  IdAllocator texture_ids_;
  IdAllocator sampler_ids_;
  unsigned int frame_token_;

  class StateManager;
  scoped_ptr<StateManager> state_manager_;

  DISALLOW_COPY_AND_ASSIGN(RendererCB);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_COMMAND_BUFFER_RENDERER_CB_H_
