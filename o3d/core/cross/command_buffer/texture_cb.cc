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


// Implementations of the abstract Texture2D and TextureCUBE classes using
// the OpenCB graphics API.

#include "core/cross/precompile.h"
#include "core/cross/error.h"
#include "core/cross/types.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "core/cross/command_buffer/texture_cb.h"

#include "command_buffer/common/cross/cmd_buffer_format.h"
#include "command_buffer/common/cross/resource.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"
#include "command_buffer/client/cross/fenced_allocator.h"

namespace o3d {

using command_buffer::CommandBufferEntry;
using command_buffer::CommandBufferHelper;
using command_buffer::FencedAllocatorWrapper;
using command_buffer::ResourceID;
namespace texture = command_buffer::texture;
namespace set_texture_data_cmd = command_buffer::set_texture_data_cmd;
namespace get_texture_data_cmd = command_buffer::get_texture_data_cmd;
namespace create_texture_2d_cmd = command_buffer::create_texture_2d_cmd;
namespace create_texture_cube_cmd = command_buffer::create_texture_cube_cmd;

namespace {

const Texture::RGBASwizzleIndices g_cb_abgr32f_swizzle_indices =
    {0, 1, 2, 3};

// Converts an O3D texture format to a command buffer texture format.
texture::Format CBFormatFromO3DFormat(Texture::Format format) {
  switch (format) {
    case Texture::XRGB8:
      return texture::XRGB8;
    case Texture::ARGB8:
      return texture::ARGB8;
    case Texture::ABGR16F:
      return texture::ABGR16F;
    case Texture::DXT1:
      return texture::DXT1;
      // TODO: DXT3/5. It's not yet supported by the command buffer
      // renderer, though it would be a simple addition.
    default:
      break;
  }
  // failed to find a matching format
  LOG(ERROR) << "Unrecognized Texture format type.";
  return texture::NUM_FORMATS;
}

// Checks that enums match in value, so that they can be directly used in
// set_texture_data_cmd::Face bitfields.
COMPILE_ASSERT(TextureCUBE::FACE_POSITIVE_X == texture::FACE_POSITIVE_X,
               FACE_POSITIVE_X_enums_don_t_match);
COMPILE_ASSERT(TextureCUBE::FACE_NEGATIVE_X == texture::FACE_NEGATIVE_X,
               FACE_NEGATIVE_X_enums_don_t_match);
COMPILE_ASSERT(TextureCUBE::FACE_POSITIVE_Y == texture::FACE_POSITIVE_Y,
               FACE_POSITIVE_Y_enums_don_t_match);
COMPILE_ASSERT(TextureCUBE::FACE_NEGATIVE_Y == texture::FACE_NEGATIVE_Y,
               FACE_NEGATIVE_Y_enums_don_t_match);
COMPILE_ASSERT(TextureCUBE::FACE_POSITIVE_Z == texture::FACE_POSITIVE_Z,
               FACE_POSITIVE_Z_enums_don_t_match);
COMPILE_ASSERT(TextureCUBE::FACE_NEGATIVE_Z == texture::FACE_NEGATIVE_Z,
               FACE_NEGATIVE_Z_enums_don_t_match);

// Updates a command buffer texture resource from a bitmap, rescaling if
// necessary.
void UpdateResourceFromBitmap(RendererCB *renderer,
                              ResourceID texture_id,
                              unsigned int level,
                              TextureCUBE::CubeFace face,
                              const Bitmap &bitmap,
                              bool resize_to_pot) {
  DCHECK(bitmap.image_data());
  FencedAllocatorWrapper *allocator = renderer->allocator();
  CommandBufferHelper *helper = renderer->helper();
  unsigned int mip_width = std::max(1U, bitmap.width() >> level);
  unsigned int mip_height = std::max(1U, bitmap.height() >> level);
  unsigned char *mip_data = bitmap.GetMipData(level, face);
  unsigned int mip_size =
      Bitmap::GetBufferSize(mip_width, mip_height, bitmap.format());
  if (resize_to_pot) {
    unsigned int pot_width =
        std::max(1U, Bitmap::GetPOTSize(bitmap.width()) >> level);
    unsigned int pot_height =
        std::max(1U, Bitmap::GetPOTSize(bitmap.height()) >> level);
    unsigned int pot_size = Bitmap::GetBufferSize(pot_width, pot_height,
                                                  bitmap.format());
    unsigned char *buffer = allocator->AllocTyped<unsigned char>(pot_size);
    // This should succeed for practical purposes: we don't store persistent
    // data in the transfer buffer, and the maximum texture size 2048x2048
    // makes 32MB for ABGR16F (the size of the transfer buffer).
    // TODO: 32MB for the transfer buffer can be big (e.g. if there are
    // multiple renderers). We'll want to implement a way to upload the texture
    // by bits that fit into an arbitrarily small buffer, but that is complex
    // for the NPOT->POT case.
    DCHECK(buffer);
    Bitmap::Scale(mip_width, mip_height, bitmap.format(), mip_data,
                  pot_width, pot_height, buffer);
    mip_width = pot_width;
    mip_height = pot_height;
    mip_size = pot_size;
    mip_data = buffer;
  } else {
    unsigned char *buffer = allocator->AllocTyped<unsigned char>(mip_size);
    DCHECK(buffer);
    memcpy(buffer, mip_data, mip_size);
    mip_data = buffer;
  }

  unsigned int pitch = Bitmap::GetBufferSize(mip_width, 1, bitmap.format());

  CommandBufferEntry args[10];
  args[0].value_uint32 = texture_id;
  args[1].value_uint32 =
      set_texture_data_cmd::X::MakeValue(0) |
      set_texture_data_cmd::Y::MakeValue(0);
  args[2].value_uint32 =
      set_texture_data_cmd::Width::MakeValue(mip_width) |
      set_texture_data_cmd::Height::MakeValue(mip_height);
  args[3].value_uint32 =
      set_texture_data_cmd::Z::MakeValue(0) |
      set_texture_data_cmd::Depth::MakeValue(1);
  args[4].value_uint32 =
      set_texture_data_cmd::Level::MakeValue(level) |
      set_texture_data_cmd::Face::MakeValue(face);
  args[5].value_uint32 = pitch;
  args[6].value_uint32 = 0;  // slice_pitch
  args[7].value_uint32 = mip_size;
  args[8].value_uint32 = renderer->transfer_shm_id();
  args[9].value_uint32 = allocator->GetOffset(mip_data);
  helper->AddCommand(command_buffer::SET_TEXTURE_DATA, 10, args);
  allocator->FreePendingToken(mip_data, helper->InsertToken());
}

// Copies back texture resource data into a bitmap.
void CopyBackResourceToBitmap(RendererCB *renderer,
                              ResourceID texture_id,
                              unsigned int level,
                              TextureCUBE::CubeFace face,
                              const Bitmap &bitmap) {
  DCHECK(bitmap.image_data());
  FencedAllocatorWrapper *allocator = renderer->allocator();
  CommandBufferHelper *helper = renderer->helper();
  unsigned int mip_width = std::max(1U, bitmap.width() >> level);
  unsigned int mip_height = std::max(1U, bitmap.height() >> level);
  unsigned int mip_size =
      Bitmap::GetBufferSize(mip_width, mip_height, bitmap.format());
  unsigned char *buffer = allocator->AllocTyped<unsigned char>(mip_size);
  DCHECK(buffer);

  unsigned int pitch = Bitmap::GetBufferSize(mip_width, 1, bitmap.format());

  CommandBufferEntry args[10];
  args[0].value_uint32 = texture_id;
  args[1].value_uint32 =
      get_texture_data_cmd::X::MakeValue(0) |
      get_texture_data_cmd::Y::MakeValue(0);
  args[2].value_uint32 =
      get_texture_data_cmd::Width::MakeValue(mip_width) |
      get_texture_data_cmd::Height::MakeValue(mip_height);
  args[3].value_uint32 =
      get_texture_data_cmd::Z::MakeValue(0) |
      get_texture_data_cmd::Depth::MakeValue(1);
  args[4].value_uint32 =
      get_texture_data_cmd::Level::MakeValue(level) |
      get_texture_data_cmd::Face::MakeValue(face);
  args[5].value_uint32 = pitch;
  args[6].value_uint32 = 0;  // slice_pitch
  args[7].value_uint32 = mip_size;
  args[8].value_uint32 = renderer->transfer_shm_id();
  args[9].value_uint32 = allocator->GetOffset(buffer);
  helper->AddCommand(command_buffer::GET_TEXTURE_DATA, 10, args);
  helper->Finish();
  memcpy(bitmap.GetMipData(level, face), buffer, mip_size);
  allocator->Free(buffer);
}

}  // anonymous namespace

static const unsigned int kMaxTextureSize = 2048;

// Texture2DCB -----------------------------------------------------------------

// Constructs a 2D texture object from an existing command-buffer 2D texture
// resource.
// NOTE: the Texture2DCB now owns the texture resource and will destroy it on
// exit.
Texture2DCB::Texture2DCB(ServiceLocator* service_locator,
                         ResourceID resource_id,
                         const Bitmap &bitmap,
                         bool resize_to_pot,
                         bool enable_render_surfaces)
    : Texture2D(service_locator,
                bitmap.width(),
                bitmap.height(),
                bitmap.format(),
                bitmap.num_mipmaps(),
                bitmap.CheckAlphaIsOne(),
                resize_to_pot,
                enable_render_surfaces),
      renderer_(static_cast<RendererCB*>(
                    service_locator->GetService<Renderer>())),
      resource_id_(resource_id),
      has_levels_(0) {
  DCHECK_NE(format(), Texture::UNKNOWN_FORMAT);
}

Texture2DCB::~Texture2DCB() {
  if (resource_id_ != command_buffer::kInvalidResource) {
    CommandBufferEntry args[1];
    args[0].value_uint32 = resource_id_;
    renderer_->helper()->AddCommand(command_buffer::DESTROY_TEXTURE, 1, args);
  }
}

// Creates a new texture object from scratch.
Texture2DCB* Texture2DCB::Create(ServiceLocator* service_locator,
                                 Bitmap *bitmap,
                                 bool enable_render_surfaces) {
  DCHECK_NE(bitmap->format(), Texture::UNKNOWN_FORMAT);
  DCHECK(!bitmap->is_cubemap());
  RendererCB *renderer = static_cast<RendererCB *>(
      service_locator->GetService<Renderer>());
  texture::Format cb_format = CBFormatFromO3DFormat(bitmap->format());
  if (cb_format == texture::NUM_FORMATS) {
    O3D_ERROR(service_locator)
        << "Unsupported format in Texture2DCB::Create.";
    return NULL;
  }
  if (bitmap->width() > kMaxTextureSize ||
      bitmap->height() > kMaxTextureSize) {
    O3D_ERROR(service_locator) << "Texture dimensions (" << bitmap->width()
                               << ", " << bitmap->height() << ") too big.";
    return NULL;
  }

  bool resize_to_pot = !renderer->supports_npot() && !bitmap->IsPOT();

  unsigned int mip_width = bitmap->width();
  unsigned int mip_height = bitmap->height();
  if (resize_to_pot) {
    mip_width = Bitmap::GetPOTSize(mip_width);
    mip_height = Bitmap::GetPOTSize(mip_height);
  }

  ResourceID texture_id = renderer->texture_ids().AllocateID();
  CommandBufferEntry args[3];
  args[0].value_uint32 = texture_id;
  args[1].value_uint32 =
      create_texture_2d_cmd::Width::MakeValue(mip_width) |
      create_texture_2d_cmd::Height::MakeValue(mip_height);
  args[2].value_uint32 =
      create_texture_2d_cmd::Levels::MakeValue(bitmap->num_mipmaps()) |
      create_texture_2d_cmd::Format::MakeValue(cb_format) |
      create_texture_2d_cmd::Flags::MakeValue(0);
  renderer->helper()->AddCommand(command_buffer::CREATE_TEXTURE_2D, 3, args);
  if (bitmap->image_data()) {
    for (unsigned int i = 0; i < bitmap->num_mipmaps(); ++i) {
      UpdateResourceFromBitmap(renderer, texture_id, i,
                               TextureCUBE::FACE_POSITIVE_X, *bitmap, true);
    }
  }

  Texture2DCB *texture = new Texture2DCB(service_locator, texture_id,
                                         *bitmap, resize_to_pot,
                                         enable_render_surfaces);

  // Setup the backing bitmap.
  texture->backing_bitmap_.SetFrom(bitmap);
  if (texture->backing_bitmap_.image_data()) {
    if (resize_to_pot) {
      texture->has_levels_ = (1 << bitmap->num_mipmaps()) - 1;
    } else {
      texture->backing_bitmap_.FreeData();
    }
  }
  return texture;
}

// Locks the given mipmap level of this texture for loading from main memory,
// and returns a pointer to the buffer.
bool Texture2DCB::Lock(int level, void** data) {
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to lock inexistent level " << level
        << " on Texture \"" << name();
    return false;
  }
  if (IsLocked(level)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " of texture \"" << name()
        << "\" is already locked.";
    return false;
  }
  if (!backing_bitmap_.image_data()) {
    DCHECK_EQ(has_levels_, 0);
    backing_bitmap_.Allocate(format(), width(), height(), levels(), false);
  }
  *data = backing_bitmap_.GetMipData(level, TextureCUBE::FACE_POSITIVE_X);
  if (!HasLevel(level)) {
    DCHECK(!resize_to_pot_);
    DCHECK_EQ(backing_bitmap_.width(), width());
    DCHECK_EQ(backing_bitmap_.height(), height());
    DCHECK_EQ(backing_bitmap_.format(), format());
    DCHECK_GT(backing_bitmap_.num_mipmaps(), level);
    DCHECK(!backing_bitmap_.is_cubemap());
    CopyBackResourceToBitmap(renderer_, resource_id_, level,
                             TextureCUBE::FACE_POSITIVE_X,
                             backing_bitmap_);
    has_levels_ |= 1 << level;
  }
  locked_levels_ |= 1 << level;
  return true;
}

// Unlocks the given mipmap level of this texture, uploading the main memory
// data buffer to the command buffer service.
bool Texture2DCB::Unlock(int level) {
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to unlock inexistent level " << level
        << " on Texture \"" << name();
    return false;
  }
  if (!IsLocked(level)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " of texture \"" << name()
        << "\" is not locked.";
    return false;
  }
  DCHECK(backing_bitmap_.image_data());
  DCHECK_EQ(backing_bitmap_.width(), width());
  DCHECK_EQ(backing_bitmap_.height(), height());
  DCHECK_EQ(backing_bitmap_.format(), format());
  DCHECK_GT(backing_bitmap_.num_mipmaps(), level);
  DCHECK(!backing_bitmap_.is_cubemap());
  DCHECK(HasLevel(level));
  UpdateResourceFromBitmap(renderer_, resource_id_, level,
                           TextureCUBE::FACE_POSITIVE_X,
                           backing_bitmap_, resize_to_pot_);
  locked_levels_ &= ~(1 << level);
  if (!resize_to_pot_ && (locked_levels_ == 0)) {
    backing_bitmap_.FreeData();
    has_levels_ = 0;
  }
  return true;
}

RenderSurface::Ref Texture2DCB::GetRenderSurface(int mip_level, Pack *pack) {
  DCHECK_LT(mip_level, levels());
  // TODO: Provide an implementation for render surface extraction.
  return RenderSurface::Ref(NULL);
}

const Texture::RGBASwizzleIndices& Texture2DCB::GetABGR32FSwizzleIndices() {
  return g_cb_abgr32f_swizzle_indices;
}
// TextureCUBECB ---------------------------------------------------------------

// Creates a texture from a pre-existing texture resource.
TextureCUBECB::TextureCUBECB(ServiceLocator* service_locator,
                             ResourceID resource_id,
                             const Bitmap &bitmap,
                             bool resize_to_pot,
                             bool enable_render_surfaces)
    : TextureCUBE(service_locator,
                  bitmap.width(),
                  bitmap.format(),
                  bitmap.num_mipmaps(),
                  bitmap.CheckAlphaIsOne(),
                  resize_to_pot,
                  enable_render_surfaces),
      renderer_(static_cast<RendererCB*>(
                    service_locator->GetService<Renderer>())),
      resource_id_(resource_id) {
  for (unsigned int i = 0; i < 6; ++i) {
    has_levels_[i] = 0;
  }
}

TextureCUBECB::~TextureCUBECB() {
  if (resource_id_ != command_buffer::kInvalidResource) {
    CommandBufferEntry args[1];
    args[0].value_uint32 = resource_id_;
    renderer_->helper()->AddCommand(command_buffer::DESTROY_TEXTURE, 1, args);
  }
}

// Create a new Cube texture from scratch.
TextureCUBECB* TextureCUBECB::Create(ServiceLocator* service_locator,
                                     Bitmap *bitmap,
                                     bool enable_render_surfaces) {
  DCHECK_NE(bitmap->format(), Texture::UNKNOWN_FORMAT);
  DCHECK(bitmap->is_cubemap());
  DCHECK_EQ(bitmap->width(), bitmap->height());
  RendererCB *renderer = static_cast<RendererCB *>(
      service_locator->GetService<Renderer>());
  texture::Format cb_format = CBFormatFromO3DFormat(bitmap->format());
  if (cb_format == texture::NUM_FORMATS) {
    O3D_ERROR(service_locator)
        << "Unsupported format in Texture2DCB::Create.";
    return NULL;
  }
  if (bitmap->width() > kMaxTextureSize) {
    O3D_ERROR(service_locator) << "Texture dimensions (" << bitmap->width()
                               << ", " << bitmap->height() << ") too big.";
    return NULL;
  }

  bool resize_to_pot = !renderer->supports_npot() && !bitmap->IsPOT();

  unsigned int mip_width = bitmap->width();
  unsigned int mip_height = bitmap->height();
  if (resize_to_pot) {
    mip_width = Bitmap::GetPOTSize(mip_width);
    mip_height = Bitmap::GetPOTSize(mip_height);
  }

  ResourceID texture_id = renderer->texture_ids().AllocateID();
  CommandBufferEntry args[3];
  args[0].value_uint32 = texture_id;
  args[1].value_uint32 = create_texture_cube_cmd::Side::MakeValue(mip_width);
  args[2].value_uint32 =
      create_texture_cube_cmd::Levels::MakeValue(bitmap->num_mipmaps()) |
      create_texture_cube_cmd::Format::MakeValue(cb_format) |
      create_texture_cube_cmd::Flags::MakeValue(0);
  renderer->helper()->AddCommand(command_buffer::CREATE_TEXTURE_CUBE, 3, args);
  if (bitmap->image_data()) {
    for (unsigned int face = 0; face < 6; ++face) {
      for (unsigned int i = 0; i < bitmap->num_mipmaps(); ++i) {
        UpdateResourceFromBitmap(renderer, texture_id, i,
                                 static_cast<CubeFace>(face), *bitmap, true);
      }
    }
  }

  TextureCUBECB* texture =
      new TextureCUBECB(service_locator, texture_id, *bitmap,
                        resize_to_pot, enable_render_surfaces);

  // Setup the backing bitmap.
  texture->backing_bitmap_.SetFrom(bitmap);
  if (texture->backing_bitmap_.image_data()) {
    if (resize_to_pot) {
      for (unsigned int face = 0; face < 6; ++face) {
        texture->has_levels_[face] = (1 << bitmap->num_mipmaps()) - 1;
      }
    } else {
      texture->backing_bitmap_.FreeData();
    }
  }
  return texture;
}

// Locks the given face and mipmap level of this texture for loading from
// main memory, and returns a pointer to the buffer.
bool TextureCUBECB::Lock(CubeFace face, int level, void** data) {
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to lock inexistent level " << level
        << " on Texture \"" << name();
    return false;
  }
  if (IsLocked(level, face)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " Face " << face
        << " of texture \"" << name()
        << "\" is already locked.";
    return false;
  }
  if (!backing_bitmap_.image_data()) {
    for (unsigned int i = 0; i < 6; ++i) {
      DCHECK_EQ(has_levels_[i], 0);
    }
    backing_bitmap_.Allocate(format(), edge_length(), edge_length(),
                             levels(), true);
  }
  *data = backing_bitmap_.GetMipData(level, face);
  if (!HasLevel(level, face)) {
    // TODO: add some API so we don't have to copy back the data if we
    // will rewrite it all.
    DCHECK(!resize_to_pot_);
    DCHECK_EQ(backing_bitmap_.width(), edge_length());
    DCHECK_EQ(backing_bitmap_.height(), edge_length());
    DCHECK_EQ(backing_bitmap_.format(), format());
    DCHECK_GT(backing_bitmap_.num_mipmaps(), level);
    DCHECK(backing_bitmap_.is_cubemap());
    CopyBackResourceToBitmap(renderer_, resource_id_, level,
                             TextureCUBE::FACE_POSITIVE_X,
                             backing_bitmap_);
    has_levels_[face] |= 1 << level;
  }
  locked_levels_[face] |= 1 << level;
  return false;
}

// Unlocks the given face and mipmap level of this texture.
bool TextureCUBECB::Unlock(CubeFace face, int level) {
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to unlock inexistent level " << level
        << " on Texture \"" << name();
    return false;
  }
  if (!IsLocked(level, face)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " Face " << face
        << " of texture \"" << name()
        << "\" is not locked.";
    return false;
  }
  DCHECK(backing_bitmap_.image_data());
  DCHECK_EQ(backing_bitmap_.width(), edge_length());
  DCHECK_EQ(backing_bitmap_.height(), edge_length());
  DCHECK_EQ(backing_bitmap_.format(), format());
  DCHECK_GT(backing_bitmap_.num_mipmaps(), level);
  DCHECK(backing_bitmap_.is_cubemap());
  DCHECK(HasLevel(level, face));
  UpdateResourceFromBitmap(renderer_, resource_id_, level, face,
                           backing_bitmap_, resize_to_pot_);
  locked_levels_[face] &= ~(1 << level);
  if (!resize_to_pot_) {
    bool has_locked_level = false;
    for (unsigned int i = 0; i < 6; ++i) {
      if (locked_levels_[i]) {
        has_locked_level = true;
        break;
      }
    }
    if (!has_locked_level) {
      backing_bitmap_.FreeData();
      for (unsigned int i = 0; i < 6; ++i) {
        has_levels_[i] = 0;
      }
    }
  }
  return false;
}

RenderSurface::Ref TextureCUBECB::GetRenderSurface(TextureCUBE::CubeFace face,
                                                   int mip_level,
                                                   Pack *pack) {
  DCHECK_LT(mip_level, levels());
  // TODO: Provide an implementation for render surface extraction.
  return RenderSurface::Ref(NULL);
}

const Texture::RGBASwizzleIndices& TextureCUBECB::GetABGR32FSwizzleIndices() {
  return g_cb_abgr32f_swizzle_indices;
}

}  // namespace o3d
