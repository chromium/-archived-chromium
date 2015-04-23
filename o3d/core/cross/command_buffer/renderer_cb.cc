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


// This file contains the implementation of the command buffer Renderer.

#include "core/cross/precompile.h"

#include "command_buffer/client/cross/cmd_buffer_helper.h"
#include "command_buffer/client/cross/fenced_allocator.h"
#include "command_buffer/common/cross/gapi_interface.h"
#include "core/cross/command_buffer/buffer_cb.h"
#include "core/cross/command_buffer/effect_cb.h"
#include "core/cross/command_buffer/param_cache_cb.h"
#include "core/cross/command_buffer/primitive_cb.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "core/cross/command_buffer/sampler_cb.h"
#include "core/cross/command_buffer/states_cb.h"
#include "core/cross/command_buffer/stream_bank_cb.h"
#include "core/cross/command_buffer/texture_cb.h"
#include "core/cross/renderer_platform.h"

#ifdef OS_WIN
#include "core/win/command_buffer/win32_cb_server.h"
#endif

namespace o3d {
using command_buffer::GAPIInterface;
using command_buffer::CommandBufferHelper;

RendererCB::RendererCB(ServiceLocator* service_locator,
                       unsigned int command_buffer_size,
                       unsigned int transfer_memory_size)
    : Renderer(service_locator),
      cmd_buffer_size_(command_buffer_size),
      transfer_memory_size_(transfer_memory_size),
      transfer_shm_(command_buffer::kRPCInvalidHandle),
      transfer_shm_id_(0),
      transfer_shm_address_(NULL),
      sync_interface_(NULL),
      helper_(NULL),
      allocator_(NULL),
      cb_server_(NULL),
      frame_token_(0),
      state_manager_(new StateManager) {
  DCHECK_GT(command_buffer_size, 0);
  DCHECK_GT(transfer_memory_size, 0);
  transfer_shm_ = command_buffer::CreateShm(transfer_memory_size);
  transfer_shm_address_ = command_buffer::MapShm(transfer_shm_,
                                                 transfer_memory_size);
  state_manager_->AddStateHandlers(this);
}

RendererCB::~RendererCB() {
  Destroy();
  command_buffer::UnmapShm(transfer_shm_address_, transfer_memory_size_);
  command_buffer::DestroyShm(transfer_shm_);
}

static const unsigned int kDefaultCommandBufferSize = 256 << 10;
// This should be enough to hold the biggest possible buffer
// (2048x2048xABGR16F texture = 32MB)
static const unsigned int kDefaultTransferMemorySize = 32 << 20;

RendererCB *RendererCB::CreateDefault(ServiceLocator* service_locator) {
  return new RendererCB(service_locator, kDefaultCommandBufferSize,
                        kDefaultTransferMemorySize);
}

Renderer::InitStatus RendererCB::InitPlatformSpecific(
    const DisplayWindow& display,
    bool off_screen) {
  if (off_screen) {
    // TODO: Off-screen support ?
    return UNINITIALIZED;  // equivalent to 0/false
  }

#ifdef OS_WIN
  const DisplayWindowWindows &display_platform =
      static_cast<const DisplayWindowWindows&>(display);
  // Creates a Win32CBServer based on the HWND, and creates the
  // command buffer helper, and initializes it. Also, create the
  // FencedAllocator for the transfer memory.
  cb_server_ = new Win32CBServer(display_platform.hwnd());
  sync_interface_ = cb_server_->GetInterface();

  RECT windowRect;
  ::GetWindowRect(display_platform.hwnd(), &windowRect);
  int width = windowRect.right - windowRect.left;
  int height = windowRect.bottom - windowRect.top;
  InitCommon(width, height);
  return SUCCESS;
#else
  // TODO: Implement Mac/Linux support before shipping
  // command buffers.
  return UNINITIALIZED;
#endif
}

void RendererCB::InitCommon(unsigned int width, unsigned int height) {
  sync_interface_->InitConnection();
  transfer_shm_id_ = sync_interface_->RegisterSharedMemory(
      transfer_shm_, transfer_memory_size_);
  helper_ = new CommandBufferHelper(sync_interface_);
  helper_->Init(cmd_buffer_size_);
  frame_token_ = helper_->InsertToken();
  allocator_ = new FencedAllocatorWrapper(transfer_memory_size_,
                                          helper_,
                                          transfer_shm_address_);
  SetClientSize(width, height);
}

void RendererCB::Destroy() {
  if (allocator_) {
    delete allocator_;
    allocator_ = NULL;
  }
  if (helper_) {
    helper_->Finish();
    if (sync_interface_) {
      sync_interface_->CloseConnection();
      sync_interface_->UnregisterSharedMemory(transfer_shm_id_);
      sync_interface_ = NULL;
    }
    delete helper_;
    helper_ = NULL;
  }
#ifdef OS_WIN
  if (cb_server_) {
    delete cb_server_;
    cb_server_ = NULL;
  }
#endif
}

void RendererCB::ApplyDirtyStates() {
  state_manager_->ValidateStates(helper_);
}

void RendererCB::Resize(int width, int height) {
  // TODO: the Resize event won't be coming to the client, but
  // potentially to the server, so that function doesn't directly make sense
  // in the command buffer implementation.
  SetClientSize(width, height);
}

// Adds the BEGIN_FRAME command to the command buffer.
bool RendererCB::BeginDraw() {
  ++render_frame_count_;
  DCHECK(helper_);
  helper_->AddCommand(command_buffer::BEGIN_FRAME, 0 , NULL);
  // Clear the client if we need to.
  if (clear_client_) {
    clear_client_ = false;
    Clear(Float4(0.5f, 0.5f, 0.5f, 1.0f), true, 1.0f, true, 0, true);
  }
  return true;
}

// Adds the CLEAR command to the command buffer.
void RendererCB::Clear(const Float4 &color,
                       bool color_flag,
                       float depth,
                       bool depth_flag,
                       int stencil,
                       bool stencil_flag) {
  ApplyDirtyStates();
  uint32 buffers = (color_flag ? GAPIInterface::COLOR : 0) |
      (depth_flag ? GAPIInterface::DEPTH : 0) |
      (stencil_flag ? GAPIInterface::STENCIL : 0);
  command_buffer::CommandBufferEntry args[7];
  args[0].value_uint32 = buffers;
  args[1].value_float = color[0];
  args[2].value_float = color[1];
  args[3].value_float = color[2];
  args[4].value_float = color[3];
  args[5].value_float = depth;
  args[6].value_uint32 = stencil;
  helper_->AddCommand(command_buffer::CLEAR, 7, args);
}

// Adds the END_FRAME command to the command buffer, and flushes the commands.
void RendererCB::EndDraw() {
  ApplyDirtyStates();
  helper_->AddCommand(command_buffer::END_FRAME, 0 , NULL);
  helper_->WaitForToken(frame_token_);
  frame_token_ = helper_->InsertToken();
}

bool RendererCB::StartRendering() {
  ++render_frame_count_;
  transforms_culled_ = 0;
  transforms_processed_ = 0;
  draw_elements_culled_ = 0;
  draw_elements_processed_ = 0;
  draw_elements_rendered_ = 0;
  primitives_rendered_ = 0;

  // Any device issues are handled in the command buffer backend
  return true;
}

void RendererCB::FinishRendering() {
  // Any device issues are handled in the command buffer backend
}

void RendererCB::RenderElement(Element* element,
                               DrawElement* draw_element,
                               Material* material,
                               ParamObject* override,
                               ParamCache* param_cache) {
  ++draw_elements_rendered_;
  State *current_state = material ? material->state() : NULL;
  PushRenderStates(current_state);
  ApplyDirtyStates();
  element->Render(this, draw_element, material, override, param_cache);
  PopRenderStates();
}

// Assign the surface arguments to the renderer, and update the stack
// of pushed surfaces.
void RendererCB::SetRenderSurfacesPlatformSpecific(
    RenderSurface* surface,
    RenderDepthStencilSurface* surface_depth) {
  // TODO: Provide an implementation for this routine.
}

void RendererCB::SetBackBufferPlatformSpecific() {
  // TODO: Provide an implementation for this routine.
}

// Creates a StreamBank, returning a platform specific implementation class.
StreamBank::Ref RendererCB::CreateStreamBank() {
  return StreamBank::Ref(new StreamBankCB(service_locator(), this));
}

// Creates a Primitive, returning a platform specific implementation class.
Primitive::Ref RendererCB::CreatePrimitive() {
  return Primitive::Ref(new PrimitiveCB(service_locator(), this));
}

// Creates a DrawElement, returning a platform specific implementation
// class.
DrawElement::Ref RendererCB::CreateDrawElement() {
  return DrawElement::Ref(new DrawElement(service_locator()));
}

Sampler::Ref RendererCB::CreateSampler() {
  return SamplerCB::Ref(new SamplerCB(service_locator(), this));
}

// Creates and returns a platform-specific float buffer
VertexBuffer::Ref RendererCB::CreateVertexBuffer() {
  return VertexBuffer::Ref(new VertexBufferCB(service_locator(), this));
}

// Creates and returns a platform-specific integer buffer
IndexBuffer::Ref RendererCB::CreateIndexBuffer() {
  return IndexBuffer::Ref(new IndexBufferCB(service_locator(), this));
}

// Creates and returns a platform-specific effect object
Effect::Ref RendererCB::CreateEffect() {
  return Effect::Ref(new EffectCB(service_locator(), this));
}

// Attempts to create a Texture with the given bitmap, automatically
// determining whether the to create a 2D texture, cube texture, etc. If
// creation fails the method returns NULL.
// Parameters:
//  bitmap: The bitmap specifying the dimensions, format and content of the
//          new texture. The created texture takes ownership of the bitmap
//          data.
// Returns:
//  A ref-counted pointer to the texture or NULL if it did not load.
Texture::Ref RendererCB::CreatePlatformSpecificTextureFromBitmap(
    Bitmap *bitmap) {
  if (bitmap->is_cubemap()) {
    return Texture::Ref(TextureCUBECB::Create(service_locator(), bitmap,
                                              false));
  } else {
    return Texture::Ref(Texture2DCB::Create(service_locator(), bitmap, false));
  }
}

// Creates and returns a platform-specific Texture2D object.  It allocates
// the necessary resources to store texture data for the given image size
// and format.
Texture2D::Ref RendererCB::CreatePlatformSpecificTexture2D(
    int width,
    int height,
    Texture::Format format,
    int levels,
    bool enable_render_surfaces) {
  Bitmap bitmap;
  bitmap.set_format(format);
  bitmap.set_width(width);
  bitmap.set_height(height);
  bitmap.set_num_mipmaps(levels);
  return Texture2D::Ref(Texture2DCB::Create(service_locator(), &bitmap,
                                            enable_render_surfaces));
}

// Creates and returns a platform-specific Texture2D object.  It allocates
// the necessary resources to store texture data for the given image size
// and format.
TextureCUBE::Ref RendererCB::CreatePlatformSpecificTextureCUBE(
    int edge,
    Texture::Format format,
    int levels,
    bool enable_render_surfaces) {
  Bitmap bitmap;
  bitmap.set_format(format);
  bitmap.set_width(edge);
  bitmap.set_height(edge);
  bitmap.set_num_mipmaps(levels);
  bitmap.set_is_cubemap(true);
  return TextureCUBE::Ref(TextureCUBECB::Create(service_locator(), &bitmap,
                                                enable_render_surfaces));
}

// Creates a platform specific ParamCache.
ParamCache* RendererCB::CreatePlatformSpecificParamCache() {
  return new ParamCacheCB();
}

void RendererCB::SetViewportInPixels(int left,
                                     int top,
                                     int width,
                                     int height,
                                     float min_z,
                                     float max_z) {
  command_buffer::CommandBufferEntry args[6];
  args[0].value_uint32 = left;
  args[1].value_uint32 = top;
  args[2].value_uint32 = width;
  args[3].value_uint32 = height;
  args[4].value_float = min_z;
  args[5].value_float = max_z;
  helper_->AddCommand(command_buffer::SET_VIEWPORT, 6, args);
}

bool RendererCB::SaveScreen(const String& file_name) {
  // TODO
  return false;
}

const int* RendererCB::GetRGBAUByteNSwizzleTable() {
  static int swizzle_table[] = { 0, 1, 2, 3, };
  return swizzle_table;
}

// This is a factory function for creating Renderer objects.  Since
// we're implementing command buffers, we only ever return a CB renderer.
Renderer* Renderer::CreateDefaultRenderer(ServiceLocator* service_locator) {
  return RendererCB::CreateDefault(service_locator);
}
}  // namespace o3d
