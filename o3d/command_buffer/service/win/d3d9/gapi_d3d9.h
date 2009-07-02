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


// This file contains the GAPID3D9 class, implementing the GAPI interface for
// D3D9.

#ifndef O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_GAPI_D3D9_H__
#define O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_GAPI_D3D9_H__

#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/service/win/d3d9/d3d9_utils.h"
#include "command_buffer/service/win/d3d9/geometry_d3d9.h"
#include "command_buffer/service/win/d3d9/effect_d3d9.h"
#include "command_buffer/service/win/d3d9/texture_d3d9.h"
#include "command_buffer/service/win/d3d9/sampler_d3d9.h"

namespace o3d {
namespace command_buffer {

// This class implements the GAPI interface for D3D9.
class GAPID3D9 : public GAPIInterface {
 public:
  GAPID3D9();
  virtual ~GAPID3D9();

  void set_hwnd(HWND hwnd) { hwnd_ = hwnd; }

  // Initializes the graphics context, bound to a window.
  // Returns:
  //   true if successful.
  virtual bool Initialize();

  // Destroys the graphics context.
  virtual void Destroy();

  // Implements the BeginFrame function for D3D9.
  virtual void BeginFrame();

  // Implements the EndFrame function for D3D9.
  virtual void EndFrame();

  // Implements the Clear function for D3D9.
  virtual void Clear(unsigned int buffers,
                     const RGBA &color,
                     float depth,
                     unsigned int stencil);

  // Implements the SetViewport function for D3D9.
  virtual void SetViewport(unsigned int x,
                           unsigned int y,
                           unsigned int width,
                           unsigned int height,
                           float z_min,
                           float z_max);

  // Implements the CreateVertexBuffer function for D3D9.
  virtual ParseError CreateVertexBuffer(ResourceID id,
                                        unsigned int size,
                                        unsigned int flags);

  // Implements the DestroyVertexBuffer function for D3D9.
  virtual ParseError DestroyVertexBuffer(ResourceID id);

  // Implements the SetVertexBufferData function for D3D9.
  virtual ParseError SetVertexBufferData(ResourceID id,
                                         unsigned int offset,
                                         unsigned int size,
                                         const void *data);

  // Implements the GetVertexBufferData function for D3D9.
  virtual ParseError GetVertexBufferData(ResourceID id,
                                         unsigned int offset,
                                         unsigned int size,
                                         void *data);

  // Implements the CreateIndexBuffer function for D3D9.
  virtual ParseError CreateIndexBuffer(ResourceID id,
                                       unsigned int size,
                                       unsigned int flags);

  // Implements the DestroyIndexBuffer function for D3D9.
  virtual ParseError DestroyIndexBuffer(ResourceID id);

  // Implements the SetIndexBufferData function for D3D9.
  virtual ParseError SetIndexBufferData(ResourceID id,
                                        unsigned int offset,
                                        unsigned int size,
                                        const void *data);

  // Implements the GetIndexBufferData function for D3D9.
  virtual ParseError GetIndexBufferData(ResourceID id,
                                        unsigned int offset,
                                        unsigned int size,
                                        void *data);

  // Implements the CreateVertexStruct function for D3D9.
  virtual ParseError CreateVertexStruct(ResourceID id,
                                        unsigned int input_count);

  // Implements the DestroyVertexStruct function for D3D9.
  virtual ParseError DestroyVertexStruct(ResourceID id);

  // Implements the SetVertexInput function for D3D9.
  virtual ParseError SetVertexInput(ResourceID vertex_struct_id,
                                    unsigned int input_index,
                                    ResourceID vertex_buffer_id,
                                    unsigned int offset,
                                    unsigned int stride,
                                    vertex_struct::Type type,
                                    vertex_struct::Semantic semantic,
                                    unsigned int semantic_index);

  // Implements the SetVertexStruct function for D3D9.
  virtual ParseError SetVertexStruct(ResourceID id);

  // Implements the Draw function for D3D9.
  virtual ParseError Draw(PrimitiveType primitive_type,
                          unsigned int first,
                          unsigned int count);

  // Implements the DrawIndexed function for D3D9.
  virtual ParseError DrawIndexed(PrimitiveType primitive_type,
                                 ResourceID index_buffer_id,
                                 unsigned int first,
                                 unsigned int count,
                                 unsigned int min_index,
                                 unsigned int max_index);

  // Implements the CreateEffect function for D3D9.
  virtual ParseError CreateEffect(ResourceID id,
                                  unsigned int size,
                                  const void *data);

  // Implements the DestroyEffect function for D3D9.
  virtual ParseError DestroyEffect(ResourceID id);

  // Implements the SetEffect function for D3D9.
  virtual ParseError SetEffect(ResourceID id);

  // Implements the GetParamCount function for D3D9.
  virtual ParseError GetParamCount(ResourceID id,
                                   unsigned int size,
                                   void *data);

  // Implements the CreateParam function for D3D9.
  virtual ParseError CreateParam(ResourceID param_id,
                                 ResourceID effect_id,
                                 unsigned int index);

  // Implements the CreateParamByName function for D3D9.
  virtual ParseError CreateParamByName(ResourceID param_id,
                                       ResourceID effect_id,
                                       unsigned int size,
                                       const void *name);

  // Implements the DestroyParam function for D3D9.
  virtual ParseError DestroyParam(ResourceID id);

  // Implements the SetParamData function for D3D9.
  virtual ParseError SetParamData(ResourceID id,
                                  unsigned int size,
                                  const void *data);

  // Implements the GetParamDesc function for D3D9.
  virtual ParseError GetParamDesc(ResourceID id,
                                  unsigned int size,
                                  void *data);

  // Implements the GetStreamCount function for D3D9.
  virtual ParseError GetStreamCount(ResourceID id,
                                    unsigned int size,
                                    void *data);

  // Implements the GetStreamDesc function for D3D9.
  virtual ParseError GetStreamDesc(ResourceID id,
                                   unsigned int index,
                                   unsigned int size,
                                   void *data);

  // Implements the CreateTexture2D function for D3D9.
  virtual ParseError CreateTexture2D(ResourceID id,
                                     unsigned int width,
                                     unsigned int height,
                                     unsigned int levels,
                                     texture::Format format,
                                     unsigned int flags);

  // Implements the CreateTexture3D function for D3D9.
  virtual ParseError CreateTexture3D(ResourceID id,
                                     unsigned int width,
                                     unsigned int height,
                                     unsigned int depth,
                                     unsigned int levels,
                                     texture::Format format,
                                     unsigned int flags);

  // Implements the CreateTextureCube function for D3D9.
  virtual ParseError CreateTextureCube(ResourceID id,
                                       unsigned int side,
                                       unsigned int levels,
                                       texture::Format format,
                                       unsigned int flags);

  // Implements the SetTextureData function for D3D9.
  virtual ParseError SetTextureData(ResourceID id,
                                    unsigned int x,
                                    unsigned int y,
                                    unsigned int z,
                                    unsigned int width,
                                    unsigned int height,
                                    unsigned int depth,
                                    unsigned int level,
                                    texture::Face face,
                                    unsigned int pitch,
                                    unsigned int slice_pitch,
                                    unsigned int size,
                                    const void *data);

  // Implements the GetTextureData function for D3D9.
  virtual ParseError GetTextureData(ResourceID id,
                                    unsigned int x,
                                    unsigned int y,
                                    unsigned int z,
                                    unsigned int width,
                                    unsigned int height,
                                    unsigned int depth,
                                    unsigned int level,
                                    texture::Face face,
                                    unsigned int pitch,
                                    unsigned int slice_pitch,
                                    unsigned int size,
                                    void *data);

  // Implements the DestroyTexture function for D3D9.
  virtual ParseError DestroyTexture(ResourceID id);

  // Implements the CreateSampler function for D3D9.
  virtual ParseError CreateSampler(ResourceID id);

  // Implements the DestroySampler function for D3D9.
  virtual ParseError DestroySampler(ResourceID id);

  // Implements the SetSamplerStates function for D3D9.
  virtual ParseError SetSamplerStates(ResourceID id,
                                      sampler::AddressingMode addressing_u,
                                      sampler::AddressingMode addressing_v,
                                      sampler::AddressingMode addressing_w,
                                      sampler::FilteringMode mag_filter,
                                      sampler::FilteringMode min_filter,
                                      sampler::FilteringMode mip_filter,
                                      unsigned int max_anisotropy);

  // Implements the SetSamplerBorderColor function for D3D9.
  virtual ParseError SetSamplerBorderColor(ResourceID id, const RGBA &color);

  // Implements the SetSamplerTexture function for D3D9.
  virtual ParseError SetSamplerTexture(ResourceID id, ResourceID texture_id);

  // Implements the SetScissor function for D3D9.
  virtual void SetScissor(bool enable,
                          unsigned int x,
                          unsigned int y,
                          unsigned int width,
                          unsigned int height);

  // Implements the SetPointLineRaster function for D3D9.
  virtual void SetPointLineRaster(bool line_smooth,
                                  bool point_sprite,
                                  float point_size);

  // Implements the SetPolygonOffset function for D3D9.
  virtual void SetPolygonOffset(float slope_factor, float units);

  // Implements the SetPolygonRaster function for D3D9.
  virtual void SetPolygonRaster(PolygonMode fill_mode,
                                FaceCullMode cull_mode);

  // Implements the SetAlphaTest function for D3D9.
  virtual void SetAlphaTest(bool enable,
                            float reference,
                            Comparison comp);

  // Implements the SetDepthTest function for D3D9.
  virtual void SetDepthTest(bool enable,
                            bool write_enable,
                            Comparison comp);

  // Implements the SetStencilTest function for D3D9.
  virtual void SetStencilTest(bool enable,
                              bool separate_ccw,
                              unsigned int write_mask,
                              unsigned int compare_mask,
                              unsigned int ref,
                              Uint32 func_ops);

  // Implements the SetColorWritefunction for D3D9.
  virtual void SetColorWrite(bool red,
                             bool green,
                             bool blue,
                             bool alpha,
                             bool dither);

  // Implements the SetBlending function for D3D9.
  virtual void SetBlending(bool enable,
                           bool separate_alpha,
                           BlendEq color_eq,
                           BlendFunc color_src_func,
                           BlendFunc color_dst_func,
                           BlendEq alpha_eq,
                           BlendFunc alpha_src_func,
                           BlendFunc alpha_dst_func);

  // Implements the SetBlendingColor function for D3D9.
  virtual void SetBlendingColor(const RGBA &color);

  // Gets the D3D9 device.
  IDirect3DDevice9 *d3d_device() const { return d3d_device_; }

  // Gets a vertex buffer by resource ID.
  VertexBufferD3D9 *GetVertexBuffer(ResourceID id) {
    return vertex_buffers_.Get(id);
  }

  // Gets a texture by resource ID.
  TextureD3D9 *GetTexture(ResourceID id) {
    return textures_.Get(id);
  }

  // Gets a sampler by resource ID.
  SamplerD3D9 *GetSampler(ResourceID id) {
    return samplers_.Get(id);
  }

  EffectD3D9 *current_effect() { return current_effect_; }
 private:
  // Validates the current vertex struct to D3D, setting the streams.
  bool ValidateStreams();
  // Validates the current effect to D3D. This sends the effect states to D3D.
  bool ValidateEffect();
  // "Dirty" the current effect. This resets the effect states to D3D, and
  // requires ValidateEffect() to be called before further draws occur.
  void DirtyEffect();

  LPDIRECT3D9 d3d_;
  LPDIRECT3DDEVICE9 d3d_device_;
  HWND hwnd_;
  ResourceID current_vertex_struct_;
  bool validate_streams_;
  unsigned int max_vertices_;
  ResourceID current_effect_id_;
  bool validate_effect_;
  EffectD3D9 *current_effect_;

  ResourceMap<VertexBufferD3D9> vertex_buffers_;
  ResourceMap<IndexBufferD3D9> index_buffers_;
  ResourceMap<VertexStructD3D9> vertex_structs_;
  ResourceMap<EffectD3D9> effects_;
  ResourceMap<EffectParamD3D9> effect_params_;
  ResourceMap<TextureD3D9> textures_;
  ResourceMap<SamplerD3D9> samplers_;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_GAPI_D3D9_H__
