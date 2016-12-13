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


// This file contains the definition of the RendererD3D9 class.

#include "core/cross/precompile.h"
#include "core/win/d3d9/renderer_d3d9.h"

#include <vector>
#include <d3dx9core.h>

#include "core/cross/ierror_status.h"
#include "core/cross/object_manager.h"
#include "core/cross/renderer_platform.h"
#include "core/cross/semantic_manager.h"
#include "core/cross/service_dependency.h"
#include "core/cross/client_info.h"
#include "core/cross/shape.h"
#include "core/cross/features.h"
#include "core/cross/types.h"
#include "core/win/d3d9/buffer_d3d9.h"
#include "core/win/d3d9/d3d_entry_points.h"
#include "core/win/d3d9/draw_element_d3d9.h"
#include "core/win/d3d9/effect_d3d9.h"
#include "core/win/d3d9/param_cache_d3d9.h"
#include "core/win/d3d9/primitive_d3d9.h"
#include "core/win/d3d9/render_surface_d3d9.h"
#include "core/win/d3d9/sampler_d3d9.h"
#include "core/win/d3d9/software_renderer_d3d9.h"
#include "core/win/d3d9/stream_bank_d3d9.h"
#include "core/win/d3d9/texture_d3d9.h"
#include "core/win/d3d9/utils_d3d9.h"

namespace o3d {

typedef std::vector<Effect*> EffectArray;
typedef std::vector<RenderSurfaceBase*> RenderSurfaceBaseArray;
typedef std::vector<State*> StateArray;
typedef std::vector<Texture*> TextureArray;

namespace {

D3DCMPFUNC ConvertCmpFunc(State::Comparison cmp) {
  switch (cmp) {
    case State::CMP_ALWAYS:
      return D3DCMP_ALWAYS;
    case State::CMP_NEVER:
      return D3DCMP_NEVER;
    case State::CMP_LESS:
      return D3DCMP_LESS;
    case State::CMP_GREATER:
      return D3DCMP_GREATER;
    case State::CMP_LEQUAL:
      return D3DCMP_LESSEQUAL;
    case State::CMP_GEQUAL:
      return D3DCMP_GREATEREQUAL;
    case State::CMP_EQUAL:
      return D3DCMP_EQUAL;
    case State::CMP_NOTEQUAL:
      return D3DCMP_NOTEQUAL;
    default:
      break;
  }
  return D3DCMP_ALWAYS;
}

D3DFILLMODE ConvertFillMode(State::Fill mode) {
  switch (mode) {
    case State::POINT:
      return D3DFILL_POINT;
    case State::WIREFRAME:
      return D3DFILL_WIREFRAME;
    case State::SOLID:
      return D3DFILL_SOLID;
    default:
      break;
  }
  return D3DFILL_SOLID;
}

D3DBLEND ConvertBlendFunc(State::BlendingFunction blend_func) {
  switch (blend_func) {
    case State::BLENDFUNC_ZERO:
      return D3DBLEND_ZERO;
    case State::BLENDFUNC_ONE:
      return D3DBLEND_ONE;
    case State::BLENDFUNC_SOURCE_COLOR:
      return D3DBLEND_SRCCOLOR;
    case State::BLENDFUNC_INVERSE_SOURCE_COLOR:
      return D3DBLEND_INVSRCCOLOR;
    case State::BLENDFUNC_SOURCE_ALPHA:
      return D3DBLEND_SRCALPHA;
    case State::BLENDFUNC_INVERSE_SOURCE_ALPHA:
      return D3DBLEND_INVSRCALPHA;
    case State::BLENDFUNC_DESTINATION_ALPHA:
      return D3DBLEND_DESTALPHA;
    case State::BLENDFUNC_INVERSE_DESTINATION_ALPHA:
      return D3DBLEND_INVDESTALPHA;
    case State::BLENDFUNC_DESTINATION_COLOR:
      return D3DBLEND_DESTCOLOR;
    case State::BLENDFUNC_INVERSE_DESTINATION_COLOR:
      return D3DBLEND_INVDESTCOLOR;
    case State::BLENDFUNC_SOURCE_ALPHA_SATUTRATE:
      return D3DBLEND_SRCALPHASAT;
    default:
      break;
  }
  return D3DBLEND_ONE;
}

D3DBLENDOP ConvertBlendEquation(State::BlendingEquation blend_equation) {
  switch (blend_equation) {
    case State::BLEND_ADD:
      return D3DBLENDOP_ADD;
    case State::BLEND_SUBTRACT:
      return D3DBLENDOP_SUBTRACT;
    case State::BLEND_REVERSE_SUBTRACT:
      return D3DBLENDOP_REVSUBTRACT;
    case State::BLEND_MIN:
      return D3DBLENDOP_MIN;
    case State::BLEND_MAX:
      return D3DBLENDOP_MAX;
    default:
      break;
  }
  return D3DBLENDOP_ADD;
}

D3DSTENCILOP ConvertStencilOp(State::StencilOperation stencil_func) {
  switch (stencil_func) {
    case State::STENCIL_KEEP:
      return D3DSTENCILOP_KEEP;
    case State::STENCIL_ZERO:
      return D3DSTENCILOP_ZERO;
    case State::STENCIL_REPLACE:
      return D3DSTENCILOP_REPLACE;
    case State::STENCIL_INCREMENT_SATURATE:
      return D3DSTENCILOP_INCRSAT;
    case State::STENCIL_DECREMENT_SATURATE:
      return D3DSTENCILOP_DECRSAT;
    case State::STENCIL_INVERT:
      return D3DSTENCILOP_INVERT;
    case State::STENCIL_INCREMENT:
      return D3DSTENCILOP_INCR;
    case State::STENCIL_DECREMENT:
      return D3DSTENCILOP_DECR;
    default:
      break;
  }
  return D3DSTENCILOP_KEEP;
}

// Checks that a device will be able to support the given texture formats.
bool CheckTextureFormatsSupported(LPDIRECT3D9 d3d,
                                  D3DFORMAT display_format,
                                  const D3DFORMAT* formats,
                                  int num_formats) {
  for (int i = 0; i < num_formats; ++i) {
    if (!SUCCEEDED(d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT,
                                          D3DDEVTYPE_HAL,
                                          display_format,
                                          0,
                                          D3DRTYPE_TEXTURE,
                                          formats[i]))) {
      LOG(ERROR) << "Device does not support all required texture formats.";
      return false;
    }
 }

 return true;
}

// Checks that the graphics device meets the necessary minimum requirements.
// Note that in the current implementation we're being very lenient with the
// capabilities we require.
bool CheckDeviceCaps(LPDIRECT3D9 d3d, Features* features) {
  D3DCAPS9 d3d_caps;
  if (!HR(d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3d_caps))) {
    LOG(ERROR) << "Failed to get device capabilities.";
    return false;
  }

  // Check the version of the pixel and vertex shader programs supported.
  DWORD pixel_shader_version = d3d_caps.PixelShaderVersion;
  if (pixel_shader_version < D3DPS_VERSION(2, 0)) {
    LOG(ERROR) << "Device only supports up to pixel shader version "
        << D3DSHADER_VERSION_MAJOR(pixel_shader_version) << "."
        << D3DSHADER_VERSION_MINOR(pixel_shader_version)
        << ".  Version 2.0 is required.";
    return false;
  }

  // Check that the device can support textures that are at least 2048x2048.
  DWORD max_texture_height = d3d_caps.MaxTextureHeight;
  DWORD max_texture_width = d3d_caps.MaxTextureWidth;
  DWORD required_texture_size = 2048;
  if (max_texture_height < required_texture_size ||
      max_texture_width < required_texture_size) {
    LOG(ERROR) << "Device only supports up to " << max_texture_height << "x"
        << max_texture_width << " textures.  " << required_texture_size << "x"
        << required_texture_size << " is required.";
    return false;
  }

  D3DDISPLAYMODE d3d_display_mode;
  if (!HR(d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3d_display_mode)))
    return false;

  // Check that the device supports all the texture formats needed.
  D3DFORMAT texture_formats[] = {
    D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_DXT1,
    D3DFMT_DXT3, D3DFMT_DXT5
  };
  if (!CheckTextureFormatsSupported(
      d3d,
      d3d_display_mode.Format,
      texture_formats,
      sizeof(texture_formats) / sizeof(texture_formats[0]))) {
    return false;
  }
  if (features->floating_point_textures()) {
    D3DFORMAT float_texture_formats[] = {
      D3DFMT_R32F, D3DFMT_A16B16G16R16F, D3DFMT_A32B32G32R32F
    };
    if (!CheckTextureFormatsSupported(
        d3d,
        d3d_display_mode.Format,
        float_texture_formats,
        sizeof(float_texture_formats) / sizeof(float_texture_formats[0]))) {
      return false;
    }
  }

  // Check the device supports the needed indices
  if (features->large_geometry()) {
    if (d3d_caps.MaxVertexIndex < Buffer::MAX_LARGE_INDEX) {
      return false;
    }
  }

  // Check render target formats.
  D3DFORMAT render_target_formats[] = { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8 };
  const int kNumRenderTargetFormats = sizeof(render_target_formats) /
                                      sizeof(render_target_formats[0]);
  for (int i = 0; i < kNumRenderTargetFormats; ++i) {
    if (!SUCCEEDED(d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT,
                                          D3DDEVTYPE_HAL,
                                          d3d_display_mode.Format,
                                          D3DUSAGE_RENDERTARGET,
                                          D3DRTYPE_TEXTURE,
                                          render_target_formats[i]))) {
      LOG(ERROR) << "Device does not support all required texture formats"
          << " for render targets.";
      return false;
    }
  }

  // Check depth stencil formats.
  D3DFORMAT depth_stencil_formats[] = { D3DFMT_D24S8 };
  const int kNumDepthStencilFormats = sizeof(depth_stencil_formats) /
                                      sizeof(depth_stencil_formats[0]);
  for (int i = 0; i < kNumDepthStencilFormats; ++i) {
    if (!SUCCEEDED(d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT,
                                          D3DDEVTYPE_HAL,
                                          d3d_display_mode.Format,
                                          D3DUSAGE_DEPTHSTENCIL,
                                          D3DRTYPE_SURFACE,
                                          depth_stencil_formats[i]))) {
      LOG(ERROR) << "Device does not support all required texture formats"
          << " for depth/stencil buffers.";
      return false;
    }
  }

  return true;
}

// Attempt to create a Direct3D9 object supporting the required caps. Return
// NULL if the object cannot be created or if it does not support the caps.
Renderer::InitStatus CreateDirect3D(Direct3DCreate9_Ptr d3d_create_function,
                                    LPDIRECT3D9* d3d,
                                    Features* features) {
  if (!d3d_create_function) {
    return Renderer::INITIALIZATION_ERROR;
  }

  *d3d = d3d_create_function(D3D_SDK_VERSION);
  if (NULL == *d3d) {
    return Renderer::INITIALIZATION_ERROR;
  }

  // Check that the graphics device meets the minimum capabilities.
  if (!CheckDeviceCaps(*d3d, features)) {
    (*d3d)->Release();
    *d3d = NULL;
    return Renderer::GPU_NOT_UP_TO_SPEC;
  }

  return Renderer::SUCCESS;
}

// For certain GPU drivers we need to force anti-aliasing off to avoid a
// a huge performance hit when certain types of windows are used on the same
// desktop as O3D. This function returns true if O3D is running on one
// of these GPUs/Drivers.
bool ForceAntiAliasingOff(LPDIRECT3D9* d3d) {
  D3DADAPTER_IDENTIFIER9 identifier;
  HRESULT hr = (*d3d)->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &identifier);

  unsigned int vendor_id = identifier.VendorId;
  unsigned int device_id = identifier.DeviceId;
  unsigned int product = HIWORD(identifier.DriverVersion.HighPart);
  unsigned int version = LOWORD(identifier.DriverVersion.HighPart);
  unsigned int subversion = HIWORD(identifier.DriverVersion.LowPart);
  unsigned int build = LOWORD(identifier.DriverVersion.LowPart);

  // Disable ATI drivers 6.14.10.x where x is 6800 or lower.
  if (vendor_id == 4098 &&   // ATI
      product == 6 &&
      version == 14 &&
      subversion == 10 &&
      build <= 6800) {
    return true;
  }

  return false;
}

namespace {
// Returns whether the ForceSoftwareRenderer value of the Software\Google\o3d
// key is non-zero.
bool IsForceSoftwareRendererEnabled() {
  HKEY key;
  if (FAILED(RegOpenKeyEx(HKEY_CURRENT_USER,
                          TEXT("Software\\Google\\o3d"),
                          0,
                          KEY_READ,
                          &key))) {
    return false;
  }

  bool enabled = false;
  DWORD type;
  DWORD value;
  DWORD size = sizeof(value);
  if (SUCCEEDED(RegQueryValueEx(key,
                                TEXT("ForceSoftwareRenderer"),
                                NULL,
                                &type,
                                reinterpret_cast<LPBYTE>(&value),
                                &size))) {
    if (type == REG_DWORD && size == sizeof(value) && value) {
      enabled = true;
    }
  }
  RegCloseKey(key);

  return enabled;
}
}

// Helper function that gets the D3D Interface, checks the available
// multisampling modes and selects the most advanced one available to create
// a D3D Device with a back buffer containing depth and stencil buffers that
// match the current display device.
Renderer::InitStatus InitializeD3D9Context(
    HWND window,
    LPDIRECT3D9* d3d,
    LPDIRECT3DDEVICE9* d3d_device,
    D3DPRESENT_PARAMETERS* d3d_present_parameters,
    bool fullscreen,
    Features* features,
    ServiceLocator* service_locator,
    int* out_width,
    int* out_height) {

  // Check registry to see if the developer has opted to force the software
  // renderer.
  Renderer::InitStatus status_hardware;
  if (IsForceSoftwareRendererEnabled()) {
    // Simulate GPU not up to spec.
    status_hardware = Renderer::GPU_NOT_UP_TO_SPEC;
  } else {
    // Create a hardware device.
    status_hardware = CreateDirect3D(Direct3DCreate9, d3d, features);
  }

  if (status_hardware != Renderer::SUCCESS) {
    Renderer::InitStatus status_software = CreateDirect3D(
        Direct3DCreate9Software, d3d, features);

    // We should not be requiring caps that are not supported by the software
    // renderer.
    DCHECK(status_software != Renderer::GPU_NOT_UP_TO_SPEC);

    if (status_software != Renderer::SUCCESS) {
      // Report the hardware error. An error with the software renderer should
      // only mean that it is not available, which is normal.
      if (status_hardware == Renderer::INITIALIZATION_ERROR) {
        LOG(ERROR) << "Failed to create the initial D3D9 Interface";
      }
      return status_hardware;
    }

    SetupSoftwareRenderer(*d3d);

    ClientInfoManager* client_info_manager =
        service_locator->GetService<ClientInfoManager>();
    client_info_manager->SetSoftwareRenderer(true);
  }

  D3DDISPLAYMODE d3ddm;
  if (!HR((*d3d)->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm)))
    return Renderer::GPU_NOT_UP_TO_SPEC;

  // NOTE: make sure the backbuffer matches this format, as it is
  // currently assumed to be 32-bit 8X8R8G8B

  ZeroMemory(d3d_present_parameters, sizeof(*d3d_present_parameters));
  d3d_present_parameters->Windowed               = !fullscreen;
  d3d_present_parameters->SwapEffect             = D3DSWAPEFFECT_DISCARD;
  d3d_present_parameters->BackBufferFormat       = d3ddm.Format;
  d3d_present_parameters->EnableAutoDepthStencil = FALSE;
  d3d_present_parameters->AutoDepthStencilFormat = D3DFMT_UNKNOWN;
  // wait for vsync
  d3d_present_parameters->PresentationInterval   = D3DPRESENT_INTERVAL_ONE;

  // Note: SwapEffect=DISCARD is req. for multisample to function
  // Note: AutoDepthStencilFormat is 16-bit (not the usual 8-bit)
  D3DFORMAT depth_stencil_formats[] = { D3DFMT_D24S8, };
  const int kNumFormats = sizeof(depth_stencil_formats) /
                          sizeof(depth_stencil_formats[0]);
  for (int i = 0; i < kNumFormats; ++i) {
    // Check if this depth/stencil combination is supported.
    if (SUCCEEDED((*d3d)->CheckDeviceFormat(D3DADAPTER_DEFAULT,
                                            D3DDEVTYPE_HAL,
                                            d3ddm.Format,
                                            D3DUSAGE_DEPTHSTENCIL,
                                            D3DRTYPE_SURFACE,
                                            depth_stencil_formats[i]))) {
      // Now check that it's compatible with the given backbuffer format.
      if (SUCCEEDED((*d3d)->CheckDepthStencilMatch(
                        D3DADAPTER_DEFAULT,
                        D3DDEVTYPE_HAL,
                        d3ddm.Format,
                        d3d_present_parameters->BackBufferFormat,
                        depth_stencil_formats[i]))) {
        d3d_present_parameters->AutoDepthStencilFormat =
            depth_stencil_formats[i];
        d3d_present_parameters->EnableAutoDepthStencil = TRUE;
        break;
      }
    }
  }

  if (features->not_anti_aliased() || ForceAntiAliasingOff(d3d)) {
    d3d_present_parameters->MultiSampleType = D3DMULTISAMPLE_NONE;
    d3d_present_parameters->MultiSampleQuality = 0;
  } else {
    // query multisampling
    D3DMULTISAMPLE_TYPE multisample_types[] = {
      D3DMULTISAMPLE_5_SAMPLES,
      D3DMULTISAMPLE_4_SAMPLES,
      D3DMULTISAMPLE_2_SAMPLES,
      D3DMULTISAMPLE_NONE
    };

    DWORD multisample_quality = 0;
    for (int i = 0; i < arraysize(multisample_types); ++i) {
      // check back-buffer for multisampling at level "i";
      // back buffer = 32-bit XRGB (i.e. no alpha)
      if (SUCCEEDED((*d3d)->CheckDeviceMultiSampleType(
                        D3DADAPTER_DEFAULT,
                        D3DDEVTYPE_HAL,
                        D3DFMT_X8R8G8B8,
                        true,  // result is windowed
                        multisample_types[i],
                        &multisample_quality))) {
        // back buffer succeeded, now check depth-buffer
        // depth buffer = 24-bit, stencil = 8-bit
        // NOTE: 8-bit not 16-bit like the D3DPRESENT_PARAMETERS
        if (SUCCEEDED((*d3d)->CheckDeviceMultiSampleType(
                          D3DADAPTER_DEFAULT,
                          D3DDEVTYPE_HAL,
                          D3DFMT_D24S8,
                          true,  // result is windowed
                          multisample_types[i],
                          &multisample_quality))) {
          d3d_present_parameters->MultiSampleType = multisample_types[i];
          d3d_present_parameters->MultiSampleQuality = multisample_quality - 1;
          break;
        }
      }
    }
  }

  // Check if the window size is zero. Some drivers will fail because of that
  // so we'll force a small size in that case.
  {
    RECT windowRect;
    ::GetWindowRect(window, &windowRect);
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;

    *out_width = width;
    *out_height = height;

    if (width == 0 || height == 0) {
      d3d_present_parameters->BackBufferWidth = 16;
      d3d_present_parameters->BackBufferHeight = 16;
      *out_width = 16;
      *out_height = 16;
    }
  }

  // create the D3D device
  DWORD d3d_behavior_flags = 0;

  // Check the device capabilities.
  D3DCAPS9 d3d_caps;
  HRESULT caps_result = (*d3d)->GetDeviceCaps(D3DADAPTER_DEFAULT,
                                              D3DDEVTYPE_HAL,
                                              &d3d_caps);
  if (!HR(caps_result)) {
    return Renderer::INITIALIZATION_ERROR;
  }

  // Check if the device supports HW vertex processing.
  if (d3d_caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
    d3d_behavior_flags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
  else
    d3d_behavior_flags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

  // D3DCREATE_FPU_PRESERVE is there because Firefox 3 relies on specific FPU
  // flags for its UI rendering. Apparently Firefox 2 is not, though we don't
  // currently propagate that info.
  // TODO: check if FPU_PRESERVE has a significant perf hit, in which
  // case find out if we can disable it for Firefox 2/other browsers, and/or if
  // it makes sense to switch FPU flags before/after every DX call.
  d3d_behavior_flags |= D3DCREATE_FPU_PRESERVE;
  if (!HR((*d3d)->CreateDevice(D3DADAPTER_DEFAULT,
                               D3DDEVTYPE_HAL,
                               window,
                               d3d_behavior_flags,
                               d3d_present_parameters,
                               d3d_device))) {
    return Renderer::OUT_OF_RESOURCES;
  }

  return Renderer::SUCCESS;
}

// Helper function that constructs an off-screen surface based on the
// current state of the d3d_device argument.
Renderer::InitStatus InitOffscreenSurface(
    LPDIRECT3DDEVICE9 d3d_device,
    IDirect3DSurface9** off_screen_surface) {
  CComPtr<IDirect3DSurface9> current_surface;
  if (!HR(d3d_device->GetRenderTarget(0, &current_surface))) {
    DLOG(ERROR) << "Failed to get offscreen render target.";
    return Renderer::OUT_OF_RESOURCES;
  }

  D3DSURFACE_DESC surface_description;
  if (!HR(current_surface->GetDesc(&surface_description))) {
    DLOG(ERROR) << "Failed to get offscreen surface description.";
    return Renderer::INITIALIZATION_ERROR;
  }

  CComPtr<IDirect3DSurface9> depth_stencil_surface;
  if (!HR(d3d_device->GetDepthStencilSurface(&depth_stencil_surface)))
    return Renderer::OUT_OF_RESOURCES;

  D3DSURFACE_DESC depth_stencil_description;
  if (!HR(depth_stencil_surface->GetDesc(
              &depth_stencil_description))) {
    DLOG(ERROR) << "Failed to get offscreen depth stencil.";
    return Renderer::INITIALIZATION_ERROR;
  }

  // create our surface as render target
  if (!HR(d3d_device->CreateRenderTarget(
              surface_description.Width,
              surface_description.Height,
              surface_description.Format,
              surface_description.MultiSampleType,
              surface_description.MultiSampleQuality,
              false,
              off_screen_surface,
              NULL))) {
    DLOG(ERROR) << "Failed to create offscreen renderer.";
    return Renderer::OUT_OF_RESOURCES;
  }

  return Renderer::SUCCESS;
}

namespace {
template <typename T>
T LocalMinMax(T value, T min_value, T max_value) {
  return (value < min_value) ? min_value :
      ((value > max_value) ? max_value : value);
}
}  // namespace anonymous

// Callback class used to construct depth-stencil RenderSurface objects during
// lost device events.  See usage in CreateDepthStencilSurface(...).
class DepthStencilSurfaceConstructor : public SurfaceConstructor {
 public:
  DepthStencilSurfaceConstructor(ServiceLocator* service_locator,
                                 int width,
                                 int height)
      : width_(width),
        height_(height),
        renderer_(service_locator) {}

  virtual HRESULT ConstructSurface(IDirect3DSurface9** surface) {
    if (!renderer_.IsAvailable()) {
      return E_FAIL;
    }

    RendererD3D9* renderer_d3d9 = static_cast<RendererD3D9*>(renderer_.Get());
    return renderer_d3d9->d3d_device()->CreateDepthStencilSurface(
        width_,
        height_,
        D3DFMT_D24S8,
        D3DMULTISAMPLE_NONE,
        0,
        FALSE,
        surface,
        NULL);
  }

 private:
  // The width and height of the surface to create, respectively.
  int width_;
  int height_;

  ServiceDependency<Renderer> renderer_;
  DISALLOW_COPY_AND_ASSIGN(DepthStencilSurfaceConstructor);
};

}  // unnamed namespace

// This class wraps StateHandler to make it typesafe.
template <typename T>
class TypedStateHandler : public RendererD3D9::StateHandler {
 public:
  typedef typename T ParamType;
  // Override this function to set a specific state.
  // Parameters:
  //   renderer: The platform specific renderer.
  //   param: A concrete param with state data.
  virtual void SetStateFromTypedParam(
      RendererD3D9* renderer, T* param) const = 0;

  // Gets Class of State's Parameter
  virtual const ObjectBase::Class* GetClass() const {
    return T::GetApparentClass();
  }

 private:
  // Calls SetStateFromTypedParam after casting the param to the correct type.
  // The State object guarntees that bad mis-matches do not happe,.
  // Parameters:
  //   renderer: The platform specific renderer.
  //   param: A param with state data.
  virtual void SetState(Renderer* renderer, Param* param) const {
    RendererD3D9 *renderer_d3d = down_cast<RendererD3D9 *>(renderer);
    // This is safe because State guarntees Params match by type.
    DCHECK(param->IsA(T::GetApparentClass()));
    SetStateFromTypedParam(renderer_d3d, down_cast<T*>(param));
  }
};

// A template the generates a handler for enable/disable states.
// Parameters:
//   state_constant: D3DRENDERSTATETYPE of state we want to enable/disable
template <D3DRENDERSTATETYPE state_constant>
class StateEnableHandler : public TypedStateHandler<ParamBoolean> {
 public:
  virtual void SetStateFromTypedParam(RendererD3D9* renderer,
                                      ParamBoolean* param) const {
    HR(renderer->d3d_device()->SetRenderState(state_constant,
                                              param->value()));
  }
};

// A template that generates a handler for stencil operation states.
// Parameters:
//   state_constant: D3DRENDERSTATETYPE of state we want to set.
template <D3DRENDERSTATETYPE state_constant>
class StencilOperationHandler : public TypedStateHandler<ParamInteger> {
 public:
  virtual void SetStateFromTypedParam(RendererD3D9* renderer,
                                      ParamInteger* param) const {
    HR(renderer->d3d_device()->SetRenderState(
           state_constant,
           ConvertStencilOp(static_cast<State::StencilOperation>(
                                param->value()))));
  }
};

// A template that generates a handler for blend function states.
// Parameters:
//   state_constant: D3DRENDERSTATETYPE of state we want to set.
template <D3DRENDERSTATETYPE state_constant>
class BlendFunctionHandler : public TypedStateHandler<ParamInteger> {
 public:
  virtual void SetStateFromTypedParam(RendererD3D9* renderer,
                                      ParamInteger* param) const {
    HR(renderer->d3d_device()->SetRenderState(
           state_constant,
           ConvertBlendFunc(static_cast<State::BlendingFunction>(
                                param->value()))));
  }
};

// A template that generates a handler for blend equation states.
// Parameters:
//   state_constant: D3DRENDERSTATETYPE of state we want to set.
template <D3DRENDERSTATETYPE state_constant>
class BlendEquationHandler : public TypedStateHandler<ParamInteger> {
 public:
  virtual void SetStateFromTypedParam(RendererD3D9* renderer,
                                      ParamInteger* param) const {
    HR(renderer->d3d_device()->SetRenderState(
           state_constant,
           ConvertBlendEquation(
               static_cast<State::BlendingEquation>(param->value()))));
  }
};

// A template that generates a handler for comparison function states.
// Parameters:
//   state_constant: D3DRENDERSTATETYPE of state we want to set.
template <D3DRENDERSTATETYPE state_constant>
class ComparisonFunctionHandler : public TypedStateHandler<ParamInteger> {
 public:
  virtual void SetStateFromTypedParam(RendererD3D9* renderer,
                                      ParamInteger* param) const {
    HR(renderer->d3d_device()->SetRenderState(
           state_constant,
           ConvertCmpFunc(static_cast<State::Comparison>(param->value()))));
  }
};

// A template that generates a handler for integer function states.
// Parameters:
//   state_constant: D3DRENDERSTATETYPE of state we want to set.
template <D3DRENDERSTATETYPE state_constant>
class IntegerStateHandler : public TypedStateHandler<ParamInteger> {
 public:
  virtual void SetStateFromTypedParam(RendererD3D9* renderer,
                                      ParamInteger* param) const {
    HR(renderer->d3d_device()->SetRenderState(state_constant,
                                              param->value()));
  }
};

class AlphaReferenceHandler : public TypedStateHandler<ParamFloat> {
 public:
  virtual void SetStateFromTypedParam(RendererD3D9* renderer,
                                      ParamFloat* param) const {
    float refFloat = LocalMinMax(param->value(), 0.0f, 1.0f);
    int ref = static_cast<int>(refFloat * 255.0f);

    HR(renderer->d3d_device()->SetRenderState(D3DRS_ALPHAREF, ref));
  }
};

class CullModeHandler : public TypedStateHandler<ParamInteger> {
 public:
  virtual void SetStateFromTypedParam(RendererD3D9* renderer,
                                      ParamInteger* param) const {
    State::Cull cull = static_cast<State::Cull>(param->value());
    switch (cull) {
      case State::CULL_NONE:
        HR(renderer->d3d_device()->SetRenderState(D3DRS_CULLMODE,
                                                  D3DCULL_NONE));
        break;
      case State::CULL_CW:
        HR(renderer->d3d_device()->SetRenderState(D3DRS_CULLMODE,
                                                  D3DCULL_CW));
        break;
      case State::CULL_CCW:
        HR(renderer->d3d_device()->SetRenderState(D3DRS_CULLMODE,
                                                  D3DCULL_CCW));
        break;
    }
  }
};

class PointSizeHandler : public TypedStateHandler<ParamFloat> {
 public:
  virtual void SetStateFromTypedParam(RendererD3D9* renderer,
                                      ParamFloat* param) const {
    Float2DWord offset;
    offset.my_float = param->value();

    HR(renderer->d3d_device()->SetRenderState(D3DRS_POINTSIZE,
                                              offset.my_dword));
  }
};

class PolygonOffset1Handler : public TypedStateHandler<ParamFloat> {
 public:
  virtual void SetStateFromTypedParam(RendererD3D9* renderer,
                                      ParamFloat* param) const {
    Float2DWord offset;
    offset.my_float = param->value();

    HR(renderer->d3d_device()->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,
                                              offset.my_dword));
  }
};

class PolygonOffset2Handler : public TypedStateHandler<ParamFloat> {
 public:
  virtual void SetStateFromTypedParam(RendererD3D9* renderer,
                                      ParamFloat* param) const {
    // TODO: this value is hard-coded currently because we only create a
    // 24-bit depth buffer. Move this to a member of RendererD3D9 if it changes.
    const float kUnitScale = 1.f / (1 << 24);
    Float2DWord offset;
    offset.my_float = param->value() * kUnitScale;

    HR(renderer->d3d_device()->SetRenderState(D3DRS_DEPTHBIAS,
                                              offset.my_dword));
  }
};

class FillModeHandler : public TypedStateHandler<ParamInteger> {
 public:
  virtual void SetStateFromTypedParam(RendererD3D9* renderer,
                                      ParamInteger* param) const {
    HR(renderer->d3d_device()->SetRenderState(
           D3DRS_FILLMODE,
           ConvertFillMode(static_cast<State::Fill>(param->value()))));
  }
};

RendererD3D9* RendererD3D9::CreateDefault(ServiceLocator* service_locator) {
  return new RendererD3D9(service_locator);
}

RendererD3D9::RendererD3D9(ServiceLocator* service_locator)
    : Renderer(service_locator),
      object_manager_(service_locator),
      semantic_manager_(service_locator),
      d3d_(NULL),
      d3d_device_(NULL),
      use_small_index_buffers_(false),
      off_screen_surface_(NULL),
      back_buffer_surface_(NULL),
      back_buffer_depth_surface_(NULL),
      have_device_(false),
      fullscreen_(false),
      fullscreen_message_font_(NULL),
      fullscreen_message_line_(NULL),
      showing_fullscreen_message_(false) {
  // Setup state handlers
  AddStateHandler(State::kAlphaTestEnableParamName,
                  new StateEnableHandler<D3DRS_ALPHATESTENABLE>);
  AddStateHandler(State::kAlphaReferenceParamName,
                  new AlphaReferenceHandler);
  AddStateHandler(State::kAlphaComparisonFunctionParamName,
                  new ComparisonFunctionHandler<D3DRS_ALPHAFUNC>);
  AddStateHandler(State::kCullModeParamName,
                  new CullModeHandler);
  AddStateHandler(State::kDitherEnableParamName,
                  new StateEnableHandler<D3DRS_DITHERENABLE>);
  AddStateHandler(State::kLineSmoothEnableParamName,
                  new StateEnableHandler<D3DRS_ANTIALIASEDLINEENABLE>);
  AddStateHandler(State::kPointSpriteEnableParamName,
                  new StateEnableHandler<D3DRS_POINTSPRITEENABLE>);
  AddStateHandler(State::kPointSizeParamName,
                  new PointSizeHandler);
  AddStateHandler(State::kPolygonOffset1ParamName,
                  new PolygonOffset1Handler);
  AddStateHandler(State::kPolygonOffset2ParamName,
                  new PolygonOffset2Handler);
  AddStateHandler(State::kFillModeParamName,
                  new FillModeHandler);
  AddStateHandler(State::kZEnableParamName,
                  new StateEnableHandler<D3DRS_ZENABLE>);
  AddStateHandler(State::kZWriteEnableParamName,
                  new StateEnableHandler<D3DRS_ZWRITEENABLE>);
  AddStateHandler(State::kZComparisonFunctionParamName,
                  new ComparisonFunctionHandler<D3DRS_ZFUNC>);
  AddStateHandler(State::kAlphaBlendEnableParamName,
                  new StateEnableHandler<D3DRS_ALPHABLENDENABLE>);
  AddStateHandler(State::kSourceBlendFunctionParamName,
                  new BlendFunctionHandler<D3DRS_SRCBLEND>);
  AddStateHandler(State::kDestinationBlendFunctionParamName,
                  new BlendFunctionHandler<D3DRS_DESTBLEND>);
  AddStateHandler(State::kStencilEnableParamName,
                  new StateEnableHandler<D3DRS_STENCILENABLE>);
  AddStateHandler(State::kStencilFailOperationParamName,
                  new StencilOperationHandler<D3DRS_STENCILFAIL>);
  AddStateHandler(State::kStencilZFailOperationParamName,
                  new StencilOperationHandler<D3DRS_STENCILZFAIL>);
  AddStateHandler(State::kStencilPassOperationParamName,
                  new StencilOperationHandler<D3DRS_STENCILPASS>);
  AddStateHandler(State::kStencilComparisonFunctionParamName,
                  new ComparisonFunctionHandler<D3DRS_STENCILFUNC>);
  AddStateHandler(State::kStencilReferenceParamName,
                  new IntegerStateHandler<D3DRS_STENCILREF>);
  AddStateHandler(State::kStencilMaskParamName,
                  new IntegerStateHandler<D3DRS_STENCILMASK>);
  AddStateHandler(State::kStencilWriteMaskParamName,
                  new IntegerStateHandler<D3DRS_STENCILWRITEMASK>);
  AddStateHandler(State::kColorWriteEnableParamName,
                  new IntegerStateHandler<D3DRS_COLORWRITEENABLE>);
  AddStateHandler(State::kBlendEquationParamName,
                  new BlendEquationHandler<D3DRS_BLENDOP>);
  AddStateHandler(State::kTwoSidedStencilEnableParamName,
                  new StateEnableHandler<D3DRS_TWOSIDEDSTENCILMODE>);
  AddStateHandler(State::kCCWStencilFailOperationParamName,
                  new StencilOperationHandler<D3DRS_CCW_STENCILFAIL>);
  AddStateHandler(State::kCCWStencilZFailOperationParamName,
                  new StencilOperationHandler<D3DRS_CCW_STENCILZFAIL>);
  AddStateHandler(State::kCCWStencilPassOperationParamName,
                  new StencilOperationHandler<D3DRS_CCW_STENCILPASS>);
  AddStateHandler(State::kCCWStencilComparisonFunctionParamName,
                  new ComparisonFunctionHandler<D3DRS_CCW_STENCILFUNC>);
  AddStateHandler(State::kSeparateAlphaBlendEnableParamName,
                  new StateEnableHandler<D3DRS_SEPARATEALPHABLENDENABLE>);
  AddStateHandler(State::kSourceBlendAlphaFunctionParamName,
                  new BlendFunctionHandler<D3DRS_SRCBLENDALPHA>);
  AddStateHandler(State::kDestinationBlendAlphaFunctionParamName,
                  new BlendFunctionHandler<D3DRS_DESTBLENDALPHA>);
  AddStateHandler(State::kBlendAlphaEquationParamName,
                  new BlendEquationHandler<D3DRS_BLENDOPALPHA>);
}

RendererD3D9::~RendererD3D9() {
  Destroy();
}

Renderer::InitStatus RendererD3D9::InitPlatformSpecific(
    const DisplayWindow& display,
    bool off_screen) {
  const DisplayWindowWindows& platform_display =
      static_cast<const DisplayWindowWindows&>(display);
  HWND window = platform_display.hwnd();
  int width;
  int height;
  d3d_ = NULL;
  d3d_device_ = NULL;
  InitStatus init_status = InitializeD3D9Context(
      window,
      &d3d_,
      &d3d_device_,
      &d3d_present_parameters_,
      fullscreen_,
      features(),
      service_locator(),
      &width,
      &height);
  if (init_status != SUCCESS) {
    DLOG(ERROR) << "Failed to initialize D3D9.";
    return init_status;
  }

  D3DCAPS9 d3d_caps;
  if (!HR(d3d_->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3d_caps))) {
    DLOG(ERROR) << "Failed to get device capabilities.";
    return INITIALIZATION_ERROR;
  }

  // Do we require small index buffers?
  use_small_index_buffers_ = d3d_caps.MaxVertexIndex < 0x10000;
  DCHECK(!use_small_index_buffers_ || !features()->large_geometry());

  const unsigned int kNpotFlags =
      D3DPTEXTURECAPS_POW2 | D3DPTEXTURECAPS_CUBEMAP_POW2;
  supports_npot_ = ((d3d_caps.TextureCaps & kNpotFlags) == 0);

  SetClientSize(width, height);
  have_device_ = true;

  if (!HR(d3d_->CheckDeviceFormat(D3DADAPTER_DEFAULT,
                                  D3DDEVTYPE_HAL,
                                  d3d_present_parameters_.BackBufferFormat,
                                  D3DUSAGE_DEPTHSTENCIL,
                                  D3DRTYPE_SURFACE,
                                  D3DFMT_D24S8))) {
    DLOG(ERROR) << "Failed to find compatible depth surface format.";
    Destroy();
    return GPU_NOT_UP_TO_SPEC;
  }

  if (off_screen) {
    init_status = InitOffscreenSurface(d3d_device_, &off_screen_surface_);
    if (init_status != SUCCESS) {
      Destroy();
      return init_status;
    }
  }

  if (S_OK !=
      D3DXCreateFont(d3d_device_,
                     27 /* font_height */,
                     0 /* font width--0 appears to be "don't care" */,
                     FW_BOLD,
                     1 /* MIP levels */,
                     FALSE,
                     DEFAULT_CHARSET,
                     OUT_TT_PRECIS,
                     PROOF_QUALITY,
                     DEFAULT_PITCH | FF_DONTCARE /* pitch and font family */,
                     L"Arial",
                     &fullscreen_message_font_)) {
    DLOG(ERROR) << "Failed to initialize font.";
    return INITIALIZATION_ERROR;
  }

  if (S_OK !=
      D3DXCreateLine(d3d_device_,
                     &fullscreen_message_line_)) {
    DLOG(ERROR) << "Failed to initialize line for message background.";
    return INITIALIZATION_ERROR;
  }

  return SUCCESS;
}

// Deletes the D3D9 Device and releases the D3D interface.
void RendererD3D9::Destroy() {
  off_screen_surface_ = NULL;
  d3d_device_ = NULL;
  d3d_ = NULL;
}

void RendererD3D9::Clear(const Float4 &color,
                         bool color_flag,
                         float depth,
                         bool depth_flag,
                         int stencil,
                         bool stencil_flag) {
  // is this safe to call inside BeginScene/EndScene?
  CComPtr<IDirect3DSurface9> current_surface;
  if (!HR(d3d_device()->GetRenderTarget(0, &current_surface)))
    return;

  CComPtr<IDirect3DSurface9> current_depth_surface;
  if (!HR(d3d_device()->GetDepthStencilSurface(&current_depth_surface)))
    return;

  // Conditionally clear the properties of the back buffer based on the
  // argument flags, and the existence of currently bound buffers.
  HR(d3d_device_->Clear(
         0,
         NULL,
         ((color_flag && current_surface) ? D3DCLEAR_TARGET  : 0) |
         ((depth_flag && current_depth_surface) ? D3DCLEAR_ZBUFFER : 0) |
         ((stencil_flag && current_depth_surface) ? D3DCLEAR_STENCIL : 0),
         D3DCOLOR_COLORVALUE(color[0],
                             color[1],
                             color[2],
                             color[3]),
         depth,
         stencil));
}

void RendererD3D9::SetViewportInPixels(int left,
                                       int top,
                                       int width,
                                       int height,
                                       float min_z,
                                       float max_z) {
  D3DVIEWPORT9 viewport;

  viewport.X      = left;
  viewport.Y      = top;
  viewport.Width  = width;
  viewport.Height = height;
  viewport.MinZ   = min_z;
  viewport.MaxZ   = max_z;

  HR(d3d_device_->SetViewport(&viewport));
}

// Resource allocation methods ------------------------------------------------

// Invalidates all resources which are in D3DPOOL_DEFAULT.
// Used before we try to reset the device, when the device is lost.
// (ie when suspending the computer, locking it, etc.)
// Returns true on success and false on failure.
bool RendererD3D9::InvalidateDeviceObjects() {
  back_buffer_surface_ = NULL;
  back_buffer_depth_surface_ = NULL;

  // Invalidate all effect objects.
  const EffectArray effect_array(object_manager_->GetByClass<Effect>());
  EffectArray::const_iterator p, end(effect_array.end());
  for (p = effect_array.begin(); p != end; ++p) {
    EffectD3D9* effect =  down_cast<EffectD3D9*>(*p);
    if (!HR(effect->OnLostDevice()))
      return false;
  }

  // Invalidate all Texture and RenderSurface objects.
  const RenderSurfaceBaseArray surface_array(
      object_manager_->GetByClass<RenderSurfaceBase>());
  RenderSurfaceBaseArray::const_iterator surface_iter(surface_array.begin()),
      surface_end(surface_array.end());
  for (;surface_iter != surface_end; ++surface_iter) {
    if ((*surface_iter)->IsA(RenderSurface::GetApparentClass())) {
      RenderSurfaceD3D9* render_surface_d3d9 =
          down_cast<RenderSurfaceD3D9*>(*surface_iter);
      if (!HR(render_surface_d3d9 ->OnLostDevice())) {
        return false;
      }
    } else if ((*surface_iter)->IsA(
                   RenderDepthStencilSurface::GetApparentClass())) {
      RenderDepthStencilSurfaceD3D9* render_depth_surface_d3d9 =
          down_cast<RenderDepthStencilSurfaceD3D9*>(*surface_iter);
      if (!HR(render_depth_surface_d3d9 ->OnLostDevice())) {
        return false;
      }
    }
  }

  const TextureArray texture_array(object_manager_->GetByClass<Texture>());
  TextureArray::const_iterator texture_iter(texture_array.begin()),
      texture_end(texture_array.end());
  for (;texture_iter != texture_end; ++texture_iter) {
    if ((*texture_iter)->IsA(Texture2D::GetApparentClass())) {
      Texture2DD3D9* texture2d_d3d9 = down_cast<Texture2DD3D9*>(*texture_iter);
      if (!HR(texture2d_d3d9->OnLostDevice())) {
        return false;
      }
    } else if ((*texture_iter)->IsA(TextureCUBE::GetApparentClass())) {
      TextureCUBED3D9* texture_cube_d3d9 =
          down_cast<TextureCUBED3D9*>(*texture_iter);
      if (!HR(texture_cube_d3d9->OnLostDevice())) {
        return false;
      }
    }
  }

  if (fullscreen_message_font_ &&
      S_OK != fullscreen_message_font_->OnLostDevice()) {
    return false;
  }
  if (fullscreen_message_line_ &&
      S_OK != fullscreen_message_line_->OnLostDevice()) {
    return false;
  }
  return true;
}

// Restore all resources which are in D3DPOOL_DEFAULT.
// Used after we reset the direct3d device to restore these resources.
// Returns true on success and false on failure.
bool RendererD3D9::RestoreDeviceObjects() {
  // Restore all Effect objects.
  const EffectArray effect_array(object_manager_->GetByClass<Effect>());
  EffectArray::const_iterator p, end(effect_array.end());
  for (p = effect_array.begin(); p != end; ++p) {
    EffectD3D9* effect =  down_cast<EffectD3D9*>(*p);
    if (!HR(effect->OnResetDevice()))
      return false;
  }

  // Restore all Texture objects.
  const TextureArray texture_array(object_manager_->GetByClass<Texture>());
  TextureArray::const_iterator texture_iter(texture_array.begin()),
      texture_end(texture_array.end());
  for (;texture_iter != texture_end; ++texture_iter) {
    if ((*texture_iter)->IsA(Texture2D::GetApparentClass())) {
      Texture2DD3D9* texture2d_d3d9 = down_cast<Texture2DD3D9*>(*texture_iter);
      if (!HR(texture2d_d3d9->OnResetDevice())) {
        return false;
      }
    } else if ((*texture_iter)->IsA(TextureCUBE::GetApparentClass())) {
      TextureCUBED3D9* texture_cube_d3d9 =
          down_cast<TextureCUBED3D9*>(*texture_iter);
      if (!HR(texture_cube_d3d9->OnResetDevice())) {
        return false;
      }
    }
  }

  // Restore all RenderSurface objects.  Note that this pass must happen
  // after the Textures have been restored.
  RenderSurfaceBaseArray surface_array(
      object_manager_->GetByClass<RenderSurfaceBase>());
  RenderSurfaceBaseArray::const_iterator surface_iter(surface_array.begin()),
      surface_end(surface_array.end());
  for (; surface_iter != surface_end; ++surface_iter) {
    if ((*surface_iter)->IsA(RenderSurface::GetApparentClass())) {
      RenderSurfaceD3D9* render_surface_d3d9 =
          down_cast<RenderSurfaceD3D9*>(*surface_iter);
      if (!HR(render_surface_d3d9->OnResetDevice())) {
        return false;
      }
    } else if ((*surface_iter)->IsA(
                   RenderDepthStencilSurface::GetApparentClass())) {
      RenderDepthStencilSurfaceD3D9* render_depth_stencil_surface_d3d9 =
          down_cast<RenderDepthStencilSurfaceD3D9*>(*surface_iter);
      if (!HR(render_depth_stencil_surface_d3d9->OnResetDevice())) {
        return false;
      }
    }
  }
  if (fullscreen_message_font_ &&
      S_OK != fullscreen_message_font_->OnResetDevice()) {
    return false;
  }
  if (fullscreen_message_line_ &&
      S_OK != fullscreen_message_line_->OnResetDevice()) {
    return false;
  }

  return true;
}

// Resets the d3d device properly and returns true on success.
bool RendererD3D9::ResetDevice() {
  // First update the flag if it hasn't been set yet.
  have_device_ = false;

  // Try to release all resources
  if (!InvalidateDeviceObjects())
    return false;

  // Attempt to reset the device
  if (!HR(d3d_device_->Reset(&d3d_present_parameters_)))
    return false;

  // Now try to restore our resources
  if (!RestoreDeviceObjects())
    return false;

  // If everything goes well, reset render states.
  SetInitialStates();

  // successful
  return true;
}

// This function tests if the device is lost and sets the have_device_ flag
// appropriately.
// It attempts to reset the device if it is lost.
void RendererD3D9::TestLostDevice() {
  HRESULT hr = 0;

  // We poll for the state of the device using TestCooperativeLevel().
  hr = d3d_device_->TestCooperativeLevel();

  // When hr == D3DERR_DEVICELOST, it means that we have lost the device
  // ie. a screensaver, or the computer is locked etc.
  // and there is nothing we can do to get back the device,
  // and display stuff normally.
  // In this case, we set the have_device_ flag to false to disable
  // all render calls and calls involving the device.

  // When hr == D3DERR_DEVICENOTRESET, we have lost the device BUT we can
  // reset it and restore our original display.
  // (ie. user has come out of his screensaver)
  // In this case, we attempt to invalidate all resources in D3DPOOL_DEFAULT,
  // reset the device, and then restore the resources.
  // This should succeed and we set the have_device_ flag to true.
  // If it fails, we do not set the flag to true so as

  if (hr == D3DERR_DEVICELOST) {
    // We've lost the device, update the flag so that render calls don't
    // get called.
    have_device_ = false;
    return;
  } else if (hr == D3DERR_DEVICENOTRESET) {
    // Direct3d tells us it is possible to reset the device now..
    // So let's attempt a reset!
    ResetDevice();
  }

  // TestCooperativeLevel doesn't give us a device lost error or variant
  // or the device has been successfully reset and reinitialized.
  // We can safely set our have_device_ flag to true.
  have_device_ = true;
}

// The window has been resized; change the size of our back buffer
// and do a reset.
void RendererD3D9::Resize(int width, int height) {
  if (width > 0 && height > 0) {
    // New size of back buffer
    d3d_present_parameters_.BackBufferWidth = width;
    d3d_present_parameters_.BackBufferHeight = height;

    // Attempt to do a reset if possible
    HRESULT hr = d3d_device_->TestCooperativeLevel();
    if (hr == D3DERR_DEVICENOTRESET || hr == D3D_OK) {
      // Reset device
      ResetDevice();
    }

    // Save this off.
    SetClientSize(width, height);
  }
}

void RendererD3D9::GetDisplayModes(std::vector<DisplayMode> *modes) {
  int num_modes =
      d3d_->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
  std::vector<DisplayMode> modes_found;
  for (int i = 0; i < num_modes; ++i) {
    D3DDISPLAYMODE mode;
    if (FAILED(d3d_->EnumAdapterModes(D3DADAPTER_DEFAULT,
                                      D3DFMT_X8R8G8B8,
                                      i,
                                      &mode))) {
      LOG(ERROR) << "Failed to enumerate adapter display modes.";
    } else {
      DCHECK(mode.Format == D3DFMT_X8R8G8B8);
      // Display mode IDs are one higher than D3D display modes.
      modes_found.push_back(
          DisplayMode(mode.Width, mode.Height, mode.RefreshRate, i + 1));
    }
  }
  modes->swap(modes_found);
}

bool RendererD3D9::GetDisplayMode(int id, DisplayMode *mode) {
  D3DDISPLAYMODE d3d_mode;
  bool success = false;
  if (id == DISPLAY_MODE_DEFAULT) {
    success = SUCCEEDED(d3d_->GetAdapterDisplayMode(D3DADAPTER_DEFAULT,
                                                    &d3d_mode));
  } else {
    // Display mode IDs are one higher than D3D display modes.
    success = SUCCEEDED(d3d_->EnumAdapterModes(D3DADAPTER_DEFAULT,
                                               D3DFMT_X8R8G8B8,
                                               id - 1,
                                               &d3d_mode));
  }
  if (success) {
    mode->Set(d3d_mode.Width, d3d_mode.Height, d3d_mode.RefreshRate, id);
  }
  return success;
}

bool RendererD3D9::SetFullscreen(bool fullscreen,
                                 const DisplayWindow& display,
                                 int mode_id) {
  if (fullscreen != fullscreen_) {
    if (d3d_device_) {  // Have we been initialized yet?
      const DisplayWindowWindows& platform_display =
          static_cast<const DisplayWindowWindows&>(display);
      HWND window = platform_display.hwnd();
      int refresh_rate = 0;
      if (fullscreen) {
        // Look up the refresh rate.
        DisplayMode mode;
        if (!GetDisplayMode(mode_id, &mode)) {
          LOG(ERROR) << "Failed to GetDisplayMode";
          return false;
        }
        refresh_rate = mode.refresh_rate();
        showing_fullscreen_message_ = true;
        fullscreen_message_timer_.GetElapsedTimeAndReset();  // Reset the timer.
      } else {
        showing_fullscreen_message_ = false;
      }
      d3d_present_parameters_.FullScreen_RefreshRateInHz = refresh_rate;
      d3d_present_parameters_.hDeviceWindow = window;
      d3d_present_parameters_.Windowed = !fullscreen;

      // Check if the window size is zero. Some drivers will fail because of
      // that so we'll force a small size in that case.
      RECT windowRect;
      ::GetWindowRect(window, &windowRect);
      int width = windowRect.right - windowRect.left;
      int height = windowRect.bottom - windowRect.top;

      if (width == 0 || height == 0) {
        width = 16;
        height = 16;
      }
      fullscreen_ = fullscreen;
      Resize(width, height);
    }
  }
  return true;
}

// Resets the rendering stats and
bool RendererD3D9::StartRendering() {
  ++render_frame_count_;
  transforms_culled_ = 0;
  transforms_processed_ = 0;
  draw_elements_culled_ = 0;
  draw_elements_processed_ = 0;
  draw_elements_rendered_ = 0;
  primitives_rendered_ = 0;

  // Attempt to reset the device if it is lost.
  if (!have_device_)
    TestLostDevice();

  // Only perform ops with the device if we have it.
  if (have_device_) {
    // Clear the client if we need to.
    if (clear_client_) {
      clear_client_ = false;
      Clear(Float4(0.5f, 0.5f, 0.5f, 1.0f), true, 1.0f, true, 0, true);
    }
    return true;
  } else {
    // Return false if we have lost the device.
    return false;
  }
}

// prepares DX9 for rendering the frame. Returns true on success.
bool RendererD3D9::BeginDraw() {
  // Only perform ops with the device if we have it.
  if (have_device_) {
    if (!HR(d3d_device_->GetRenderTarget(0, &back_buffer_surface_)))
      return false;
    if (!HR(d3d_device_->GetDepthStencilSurface(&back_buffer_depth_surface_)))
      return false;
    if (!HR(d3d_device_->BeginScene()))
      return false;
    // Reset the viewport.
    SetViewport(Float4(0.0f, 0.0f, 1.0f, 1.0f), Float2(0.0f, 1.0f));
    return true;
  } else {
    back_buffer_surface_ = NULL;
    back_buffer_depth_surface_ = NULL;

    // Return false if we have lost the device.
    return false;
  }
}

void RendererD3D9::ShowFullscreenMessage(float elapsed_time,
    float display_duration) {
  RECT rect;
  const float line_thickness = 60.0f;
  const float line_height = line_thickness - 1;  // Prevent a gap at the top.
  const float line_width = 340.0f;
  const D3DXCOLOR background_color(0.0f, 0.0f, 0.0f, 0.5f);
  const float curve_radius = 9.0f;
  const float curve_radius_squared = curve_radius * curve_radius;
  const float line_base_thickness = line_thickness - curve_radius;
  const float line_base_height = line_height - curve_radius;
  const float line_x = width() - line_width;

  float y_offset = 0;
  const float animation_length = 0.25f;
  if (elapsed_time < animation_length) {
    y_offset = (elapsed_time / animation_length - 1) * line_height;
  } else if (display_duration - elapsed_time < animation_length) {
    y_offset = ((display_duration - elapsed_time) / animation_length - 1) *
        line_height;
  }

  SetRect(&rect, static_cast<int>(line_x), static_cast<int>(y_offset),
      width(), static_cast<int>(y_offset + line_height));

  D3DXVECTOR2 line_vertices[2];
  HR(fullscreen_message_line_->SetWidth(line_base_thickness));
  line_vertices[0].x = line_x;
  line_vertices[0].y = y_offset + line_base_height / 2;
  line_vertices[1].x = static_cast<float>(width());
  line_vertices[1].y = y_offset + line_base_height / 2;
  HR(fullscreen_message_line_->Draw(line_vertices, 2, background_color));

  HR(fullscreen_message_line_->SetWidth(1));
  HR(fullscreen_message_line_->Begin());
  for (int i = 0; i < curve_radius; ++i) {
    const float x = line_x + curve_radius - sqrt(curve_radius_squared - i * i);
    const float y = y_offset + i + line_base_height;
    line_vertices[0].x = x;
    line_vertices[0].y = y;
    line_vertices[1].x = static_cast<float>(width());
    line_vertices[1].y = y;
    HR(fullscreen_message_line_->Draw(line_vertices, 2, background_color));
  }
  HR(fullscreen_message_line_->End());

  DWORD z_enable;  // Back up this setting and restore it afterward.
  d3d_device_->GetRenderState(D3DRS_ZENABLE, &z_enable);
  d3d_device_->SetRenderState(D3DRS_ZENABLE, FALSE);

  HR(fullscreen_message_font_->DrawText(NULL,
      L"Press ESC to exit fullscreen", -1, &rect,
      DT_CENTER | DT_VCENTER, D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f)));

  d3d_device_->SetRenderState(D3DRS_ZENABLE, z_enable);
}

// Notifies DX9 that rendering of the frame is complete and swaps the buffers.
void RendererD3D9::EndDraw() {
  if (have_device_) {
    if (showing_fullscreen_message_) {
      // Message should display for 3 seconds after transition to fullscreen.
      float elapsed_time =
          fullscreen_message_timer_.GetElapsedTimeWithoutClearing();
      const float display_duration = 3.5f;
      if (elapsed_time > display_duration) {
        showing_fullscreen_message_ = false;
      } else {
        ShowFullscreenMessage(elapsed_time, display_duration);
      }
    }
    HR(d3d_device_->EndScene());

    // Release the back-buffer references.
    back_buffer_surface_ = NULL;
    back_buffer_depth_surface_ = NULL;
  }
}

void RendererD3D9::FinishRendering() {
  // No need to call Present(...) if we are rendering to an off-screen
  // target.
  if (!off_screen_surface_) {
    HRESULT hr = d3d_device_->Present(NULL, NULL, NULL, NULL);
    // Test for lost device if Present fails.
    if (hr != D3D_OK) {
      TestLostDevice();
      // TODO: This should only be called if some resources were
      //    actually lost. In other words if there are no RenderSurfaces
      //    then there is no reason to call this.
      lost_resources_callback_manager_.Run();
    }
  }
}

// Asks the primitive to draw itself.
void RendererD3D9::RenderElement(Element* element,
                                 DrawElement* draw_element,
                                 Material* material,
                                 ParamObject* override,
                                 ParamCache* param_cache) {
  ++draw_elements_rendered_;
  // If this a new state then reset the old state.
  State *current_state = material ? material->state() : NULL;
  PushRenderStates(current_state);
  element->Render(this, draw_element, material, override, param_cache);
  PopRenderStates();
}

void RendererD3D9::SetRenderSurfacesPlatformSpecific(
    RenderSurface* surface,
    RenderDepthStencilSurface* surface_depth) {
  RenderSurfaceD3D9 *d3d_render_surface =
      down_cast<RenderSurfaceD3D9*>(surface);
  RenderDepthStencilSurfaceD3D9 *d3d_render_depth_surface =
      down_cast<RenderDepthStencilSurfaceD3D9*>(surface_depth);

  IDirect3DSurface9 *d3d_surface =
      d3d_render_surface ? d3d_render_surface->GetSurfaceHandle() : NULL;
  IDirect3DSurface9 *d3d_depth_surface = d3d_render_depth_surface ?
      d3d_render_depth_surface->GetSurfaceHandle() : NULL;

  // At least one of the surfaces must be non-null.
  DCHECK(d3d_surface || d3d_depth_surface);

  HR(d3d_device()->SetRenderTarget(0, d3d_surface));
  HR(d3d_device()->SetDepthStencilSurface(d3d_depth_surface));
}

void RendererD3D9::SetBackBufferPlatformSpecific() {
  HR(d3d_device()->SetRenderTarget(0, back_buffer_surface_));
  HR(d3d_device()->SetDepthStencilSurface(back_buffer_depth_surface_));
}

StreamBank::Ref RendererD3D9::CreateStreamBank() {
  return StreamBank::Ref(new StreamBankD3D9(service_locator(), d3d_device()));
}

Primitive::Ref RendererD3D9::CreatePrimitive() {
  return Primitive::Ref(new PrimitiveD3D9(service_locator(), d3d_device()));
}

DrawElement::Ref RendererD3D9::CreateDrawElement() {
  return DrawElement::Ref(new DrawElementD3D9(service_locator()));
}

VertexBuffer::Ref RendererD3D9::CreateVertexBuffer() {
  return VertexBuffer::Ref(new VertexBufferD3D9(service_locator(),
                                                d3d_device()));
}

// Creates and returns a D3D9 specific integer buffer.
IndexBuffer::Ref RendererD3D9::CreateIndexBuffer() {
  return IndexBuffer::Ref(new IndexBufferD3D9(service_locator(),
                                              d3d_device(),
                                              use_small_index_buffers_));
}

Effect::Ref RendererD3D9::CreateEffect() {
  return Effect::Ref(new EffectD3D9(service_locator(), d3d_device()));
}

Sampler::Ref RendererD3D9::CreateSampler() {
  return Sampler::Ref(new SamplerD3D9(service_locator(), d3d_device()));
}

ParamCache* RendererD3D9::CreatePlatformSpecificParamCache() {
  return new ParamCacheD3D9(service_locator());
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
Texture::Ref RendererD3D9::CreatePlatformSpecificTextureFromBitmap(
    Bitmap* bitmap) {
  if (bitmap->is_cubemap()) {
    return Texture::Ref(TextureCUBED3D9::Create(service_locator(),
                                                bitmap,
                                                this,
                                                false));
  } else {
    return Texture::Ref(Texture2DD3D9::Create(service_locator(),
                                              bitmap,
                                              this,
                                              false));
  }
}

// Attempts to create a Texture2D with the given specs.  If creation fails
// then the method returns NULL.
Texture2D::Ref RendererD3D9::CreatePlatformSpecificTexture2D(
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
  return Texture2D::Ref(Texture2DD3D9::Create(service_locator(),
                                              &bitmap,
                                              this,
                                              enable_render_surfaces));
}

// Attempts to create a Texture2D with the given specs.  If creation fails
// then the method returns NULL.
TextureCUBE::Ref RendererD3D9::CreatePlatformSpecificTextureCUBE(
    int edge_length,
    Texture::Format format,
    int levels,
    bool enable_render_surfaces) {
  Bitmap bitmap;
  bitmap.set_format(format);
  bitmap.set_width(edge_length);
  bitmap.set_height(edge_length);
  bitmap.set_num_mipmaps(levels);
  bitmap.set_is_cubemap(true);
  return TextureCUBE::Ref(TextureCUBED3D9::Create(service_locator(),
                                                  &bitmap,
                                                  this,
                                                  enable_render_surfaces));
}

RenderDepthStencilSurface::Ref RendererD3D9::CreateDepthStencilSurface(
    int width,
    int height) {
  DepthStencilSurfaceConstructor *depth_constructor =
      new DepthStencilSurfaceConstructor(service_locator(), width, height);

  // Note that since the returned surface is not associated with a Texture
  // mip-level, NULL is passed for the texture argument.
  return RenderDepthStencilSurface::Ref(
      new RenderDepthStencilSurfaceD3D9(service_locator(),
                                        width,
                                        height,
                                        depth_constructor));
}

// Saves a png screenshot 'filename.png'.
// Returns true on success and false on failure.
bool RendererD3D9::SaveScreen(const String& file_name) {
#ifdef TESTING
  LPDIRECT3DDEVICE9 device = d3d_device();
  CComPtr<IDirect3DSurface9> system_surface;
  CComPtr<IDirect3DSurface9> current_surface;

  if (!HR(device->GetRenderTarget(0, &current_surface)))
    return false;

  D3DSURFACE_DESC surface_description;
  if (!HR(current_surface->GetDesc(&surface_description)))
    return false;

  // Construct an intermediate surface with multi-sampling disabled.
  // This surface is required because GetRenderTargetData(...) will fail
  // for multi-sampled targets.  One must first down-sample to a
  // non-multi-sample buffer, and then copy from that intermediate buffer
  // to a main memory surface.
  CComPtr<IDirect3DSurface9> intermediate_target;
  if (!HR(device->CreateRenderTarget(surface_description.Width,
                                     surface_description.Height,
                                     surface_description.Format,
                                     D3DMULTISAMPLE_NONE,
                                     0,
                                     FALSE,
                                     &intermediate_target,
                                     NULL))) {
    return false;
  }

  if (!HR(device->StretchRect(current_surface,
                              NULL,
                              intermediate_target,
                              NULL,
                              D3DTEXF_NONE))) {
    return false;
  }

  if (!HR(device->CreateOffscreenPlainSurface(surface_description.Width,
                                              surface_description.Height,
                                              surface_description.Format,
                                              D3DPOOL_SYSTEMMEM,
                                              &system_surface,
                                              NULL))) {
    return false;
  }

  if (!HR(device->GetRenderTargetData(intermediate_target, system_surface)))
    return false;

  // append .png to the end of file_name
  String png_file_name = file_name;
  png_file_name.append(String(".png"));
  // convert file name to utf16
  std::wstring file_name_utf16 = UTF8ToWide(png_file_name);

  return HR(o3d::D3DXSaveSurfaceToFile(file_name_utf16.c_str(),
                                       D3DXIFF_PNG,
                                       system_surface,
                                       NULL,
                                       NULL));
#else
  // Not a test build, always return false.
  return false;
#endif
}

const int* RendererD3D9::GetRGBAUByteNSwizzleTable() {
  static int swizzle_table[] = { 2, 1, 0, 3, };
  return swizzle_table;
}

// This is a factory function for creating Renderer objects.  Since
// we're implementing D3D9, we only ever return a D3D9 renderer.
Renderer* Renderer::CreateDefaultRenderer(ServiceLocator* service_locator) {
  return RendererD3D9::CreateDefault(service_locator);
}
}  // namespace o3d
