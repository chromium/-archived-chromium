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


// Declares the system metrics used by the plugin.

#ifndef O3D_PLUGIN_CROSS_PLUGIN_METRICS_H_
#define O3D_PLUGIN_CROSS_PLUGIN_METRICS_H_

#include "statsreport/metrics.h"

namespace o3d {
// NOTE: DO NOT REORDER THESE ENUMS!
// This enum will be used by stats analysis code as well and they must stay in
// sync. When this stat is reported, it only reports an integer that we compare
// against this enum so the order is very important.
// We start at 1, as 0 is reserved for "no value", ie we didn't check.
enum BrowserTypeName {
  BROWSER_NAME_UNKNOWN = 1,
  BROWSER_NAME_CHROME,
  BROWSER_NAME_SAFARI,
  BROWSER_NAME_FIREFOX,
  BROWSER_NAME_MSIE,
  BROWSER_NAME_OPERA,
  BROWSER_NAME_CAMINO,
  BROWSER_NAME_OMNIWEB
};

// This enum is simply for easy viewing of data
enum SystemType {
  SYSTEM_NAME_WIN = 1,
  SYSTEM_NAME_MAC,
  SYSTEM_NAME_LINUX
};

// User operating system
DECLARE_METRIC_integer(system_type);

DECLARE_METRIC_integer(windows_major_version);
DECLARE_METRIC_integer(windows_minor_version);
DECLARE_METRIC_integer(windows_sp_major_version);
DECLARE_METRIC_integer(windows_sp_minor_version);

DECLARE_METRIC_integer(mac_major_version);
DECLARE_METRIC_integer(mac_minor_version);
DECLARE_METRIC_integer(mac_bugfix_version);

DECLARE_METRIC_integer(gl_major_version);
DECLARE_METRIC_integer(gl_minor_version);
DECLARE_METRIC_integer(gl_hlsl_major_version);
DECLARE_METRIC_integer(gl_hlsl_minor_version);


DECLARE_METRIC_bool(POW2_texture_caps);
DECLARE_METRIC_bool(NONPOW2CONDITIONAL_texture_caps);

// User GPU
DECLARE_METRIC_integer(gpu_vendor_id);
DECLARE_METRIC_integer(gpu_device_id);
DECLARE_METRIC_integer(gpu_driver_major_version);
DECLARE_METRIC_integer(gpu_driver_minor_version);
DECLARE_METRIC_integer(gpu_driver_bugfix_version);
DECLARE_METRIC_integer(gpu_vram_size);
DECLARE_METRIC_bool(direct3d_available);


// Shader versions
DECLARE_METRIC_integer(pixel_shader_main_version);
DECLARE_METRIC_integer(pixel_shader_sub_version);
DECLARE_METRIC_integer(vertex_shader_main_version);
DECLARE_METRIC_integer(vertex_shader_sub_version);

// Browser
DECLARE_METRIC_integer(browser_type);
DECLARE_METRIC_integer(browser_major_version);
DECLARE_METRIC_integer(browser_minor_version);
DECLARE_METRIC_integer(browser_bugfix_version);
  
// Running time for instance of plugin
DECLARE_METRIC_count(uptime_seconds);
DECLARE_METRIC_count(cpu_time_seconds);
DECLARE_METRIC_timing(running_time_seconds);

// Crashes
// How many times the plugin has crashed. Not all crashes may be reported.
DECLARE_METRIC_count(crashes_total);
DECLARE_METRIC_count(crashes_uploaded);
DECLARE_METRIC_count(out_of_memory_total);

// Bluescreens
// How many times has the plugin caused a bluescreen of death
DECLARE_METRIC_count(bluescreens_total);

// D3D Caps
DECLARE_METRIC_integer(d3d_devcaps);
DECLARE_METRIC_integer(d3d_misccaps);
DECLARE_METRIC_integer(d3d_rastercaps);
DECLARE_METRIC_integer(d3d_zcmpcaps);
DECLARE_METRIC_integer(d3d_srcblendcaps);
DECLARE_METRIC_integer(d3d_dstblendcaps);
DECLARE_METRIC_integer(d3d_alphacaps);
DECLARE_METRIC_integer(d3d_texcaps);
DECLARE_METRIC_integer(d3d_texfiltercaps);
DECLARE_METRIC_integer(d3d_cubetexfiltercaps);
DECLARE_METRIC_integer(d3d_texaddrcaps);
DECLARE_METRIC_integer(d3d_linecaps);
DECLARE_METRIC_integer(d3d_stencilcaps);
DECLARE_METRIC_integer(d3d_texopcaps);
DECLARE_METRIC_integer(d3d_vs20caps);
DECLARE_METRIC_integer(d3d_vs20_dynflowctrldepth);
DECLARE_METRIC_integer(d3d_vs20_numtemps);
DECLARE_METRIC_integer(d3d_vs20_staticflowctrldepth);
DECLARE_METRIC_integer(d3d_ps20caps);
DECLARE_METRIC_integer(d3d_ps20_dynflowctrldepth);
DECLARE_METRIC_integer(d3d_ps20_numtemps);
DECLARE_METRIC_integer(d3d_ps20_staticflowctrldepth);
DECLARE_METRIC_integer(d3d_ps20_numinstrslots);

// Installs and uninstalls
// TODO: Will likely need to get from Google Update

}  // namespace o3d

#endif  // O3D_PLUGIN_CROSS_PLUGIN_METRICS_H_
