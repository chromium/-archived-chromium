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


#include "statsreport/metrics.h"
#include "plugin/cross/plugin_metrics.h"

namespace o3d {
DEFINE_METRIC_integer(system_type);

DEFINE_METRIC_integer(windows_major_version);
DEFINE_METRIC_integer(windows_minor_version);
DEFINE_METRIC_integer(windows_sp_major_version);
DEFINE_METRIC_integer(windows_sp_minor_version);

// User GPU
DEFINE_METRIC_integer(gpu_vendor_id);
DEFINE_METRIC_integer(gpu_device_id);
DEFINE_METRIC_integer(gpu_driver_major_version);
DEFINE_METRIC_integer(gpu_driver_minor_version);
DEFINE_METRIC_integer(gpu_vram_size);
DEFINE_METRIC_bool(direct3d_available);

// Shader versions
DEFINE_METRIC_integer(pixel_shader_main_version);
DEFINE_METRIC_integer(pixel_shader_sub_version);
DEFINE_METRIC_integer(vertex_shader_main_version);
DEFINE_METRIC_integer(vertex_shader_sub_version);

DEFINE_METRIC_bool(POW2_texture_caps);
DEFINE_METRIC_bool(NONPOW2CONDITIONAL_texture_caps);

DEFINE_METRIC_integer(browser_type);
DEFINE_METRIC_integer(browser_major_version);
DEFINE_METRIC_integer(browser_minor_version);
DEFINE_METRIC_integer(browser_bugfix_version);

DEFINE_METRIC_timing(running_time);

DEFINE_METRIC_count(uptime_seconds);
DEFINE_METRIC_count(cpu_time_seconds);
DEFINE_METRIC_timing(running_time_seconds);

DEFINE_METRIC_count(crashes_total);
DEFINE_METRIC_count(crashes_uploaded);
DEFINE_METRIC_count(out_of_memory_total);

DEFINE_METRIC_count(bluescreens_total);

// D3D Caps
DEFINE_METRIC_integer(d3d_devcaps);
DEFINE_METRIC_integer(d3d_misccaps);
DEFINE_METRIC_integer(d3d_rastercaps);
DEFINE_METRIC_integer(d3d_zcmpcaps);
DEFINE_METRIC_integer(d3d_srcblendcaps);
DEFINE_METRIC_integer(d3d_dstblendcaps);
DEFINE_METRIC_integer(d3d_alphacaps);
DEFINE_METRIC_integer(d3d_texcaps);
DEFINE_METRIC_integer(d3d_texfiltercaps);
DEFINE_METRIC_integer(d3d_cubetexfiltercaps);
DEFINE_METRIC_integer(d3d_texaddrcaps);
DEFINE_METRIC_integer(d3d_linecaps);
DEFINE_METRIC_integer(d3d_stencilcaps);
DEFINE_METRIC_integer(d3d_texopcaps);
DEFINE_METRIC_integer(d3d_vs20caps);
DEFINE_METRIC_integer(d3d_vs20_dynflowctrldepth);
DEFINE_METRIC_integer(d3d_vs20_numtemps);
DEFINE_METRIC_integer(d3d_vs20_staticflowctrldepth);
DEFINE_METRIC_integer(d3d_ps20caps);
DEFINE_METRIC_integer(d3d_ps20_dynflowctrldepth);
DEFINE_METRIC_integer(d3d_ps20_numtemps);
DEFINE_METRIC_integer(d3d_ps20_staticflowctrldepth);
DEFINE_METRIC_integer(d3d_ps20_numinstrslots);

}  // namespace o3d
