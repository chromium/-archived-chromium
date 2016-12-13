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


// File containing implementation routines for the DirectX testing framework
// framebuffer and command stream capture routines.

#include "tests/common/win/dxcapture.h"

#include <d3dx9.h>

#include "core/cross/types.h"
#include "core/win/d3d9/d3d_entry_points.h"
#include "core/win/d3d9/renderer_d3d9.h"
#include "core/win/d3d9/utils_d3d9.h"
#include "tests/common/win/testing_common.h"

namespace directx_capture {

void StartCommandCapture(const wchar_t* stream_name) {
#if defined(RENDERER_D3D9)
  if (D3DPERF_GetStatus()) {
    D3DPERF_SetMarker(0, stream_name);
    D3DPERF_SetMarker(0, L"BeginCommandStreamCapture");
  }
#endif
}

void EndCommandCapture() {
#if defined(RENDERER_D3D9)
  if (D3DPERF_GetStatus()) {
    D3DPERF_SetMarker(0, L"EndCommandStreamCapture");
  }
#endif
}

void CaptureFramebuffer(const wchar_t* buffer_metadata) {
#if defined(RENDERER_D3D9)
  // Keep track of the invocation count for output file-naming purposes.
  static int g_call_count = 0;
  ++g_call_count;

  // If PIX is present, then send a message to PIX requesting a framebuffer
  // capture.
  if (D3DPERF_GetStatus()) {
    ::Sleep(500);
    D3DPERF_SetMarker(0, buffer_metadata);
    D3DPERF_SetMarker(0, L"CaptureScreenContents");
  } else {
    // Otherwise, explicitly read the contents of the buffer into system memory
    // and story a .png file.
    o3d::RendererD3D9* d3d9_renderer =
        down_cast<o3d::RendererD3D9*>(g_renderer);
    LPDIRECT3DDEVICE9 device = d3d9_renderer->d3d_device();
    IDirect3DSurface9* system_surface = NULL;
    IDirect3DSurface9* current_surface = NULL;

    HR(device->GetRenderTarget(0, &current_surface));
    D3DSURFACE_DESC surface_description;
    HR(current_surface->GetDesc(&surface_description));

    // Construct an intermediate surface with multi-sampling disabled.
    // This surface is required because GetRenderTargetData(...) will fail
    // for multi-sampled targets.  One must first down-sample to a
    // non-multi-sample buffer, and then copy from that intermediate buffer
    // to a main memory surface.
    IDirect3DSurface9* intermediate_target;
    HR(device->CreateRenderTarget(surface_description.Width,
                                  surface_description.Height,
                                  surface_description.Format,
                                  D3DMULTISAMPLE_NONE,
                                  0,
                                  FALSE,
                                  &intermediate_target,
                                  NULL));

    HR(device->StretchRect(current_surface,
                           NULL,
                           intermediate_target,
                           NULL,
                           D3DTEXF_NONE));

    HR(device->CreateOffscreenPlainSurface(surface_description.Width,
                                           surface_description.Height,
                                           surface_description.Format,
                                           D3DPOOL_SYSTEMMEM,
                                           &system_surface,
                                           NULL));

    HR(device->GetRenderTargetData(intermediate_target, system_surface));

    const o3d::String file_name(*g_program_name +
                                o3d::String("_") +
                                UintToString(g_call_count) +
                                o3d::String(".png"));

    std::wstring file_name_utf16 = UTF8ToWide(file_name);

    HR(o3d::D3DXSaveSurfaceToFile(file_name_utf16.c_str(),
                                  D3DXIFF_PNG,
                                  system_surface,
                                  NULL,
                                  NULL));

    system_surface->Release();
    intermediate_target->Release();
    current_surface->Release();
  }
#endif
}

}  // end namespace directx_capture
