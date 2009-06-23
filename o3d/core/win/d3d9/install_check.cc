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


// The precompiled header must appear before anything else.
#include "core/cross/precompile.h"

#include "core/cross/install_check.h"

#include <Windows.h>
#include <Shlwapi.h>
#include <tchar.h>

#include <sstream>

#include "base/scoped_ptr.h"
#include "core/win/d3d9/d3d_entry_points.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
namespace o3d {

D3DXCreateEffect_Ptr D3DXCreateEffect = NULL;
D3DXGetShaderInputSemantics_Ptr D3DXGetShaderInputSemantics = NULL;
D3DXCreateEffectCompilerFromFileW_Ptr D3DXCreateEffectCompilerFromFileW = NULL;
D3DXCreateEffectCompilerFromFileA_Ptr D3DXCreateEffectCompilerFromFileA = NULL;
D3DXSaveSurfaceToFileW_Ptr D3DXSaveSurfaceToFileW = NULL;
D3DXSaveSurfaceToFileA_Ptr D3DXSaveSurfaceToFileA = NULL;
D3DXGetShaderConstantTable_Ptr D3DXGetShaderConstantTable = NULL;
D3DXCreateFontA_Ptr D3DXCreateFontA = NULL;
D3DXCreateFontW_Ptr D3DXCreateFontW = NULL;
D3DXCreateLine_Ptr D3DXCreateLine = NULL;
Direct3DCreate9_Ptr Direct3DCreate9Software = NULL;

static HINSTANCE g_d3dx = NULL;
static HINSTANCE g_d3d9_software = NULL;

namespace {

void RendererCleanupAllVariables() {
  D3DXCreateEffect = NULL;
  D3DXGetShaderInputSemantics = NULL;
  D3DXCreateEffectCompilerFromFileW = NULL;
  D3DXCreateEffectCompilerFromFileA = NULL;
  D3DXSaveSurfaceToFileW = NULL;
  D3DXSaveSurfaceToFileA = NULL;
  D3DXGetShaderConstantTable = NULL;
  D3DXCreateFontW = NULL;
  D3DXCreateFontA = NULL;
  D3DXCreateLine = NULL;

  if (g_d3dx) {
    FreeLibrary(g_d3dx);
    g_d3dx = NULL;
  }

  if (g_d3d9_software) {
    FreeLibrary(g_d3d9_software);
    g_d3d9_software = NULL;
  }
}

bool GetModuleDirectory(TCHAR* path, std::string* error) {
  ::GetModuleFileName((HINSTANCE)&__ImageBase, path, _MAX_PATH);
  // Trim off the module filename, leaving the path to the directory containing
  // the module.
  if (!PathRemoveFileSpec(path)) {
    *error = "Failed to compute plugin directory base.";
    return false;
  }

  return true;
}

bool D3DX9InstallCheck(std::string* error) {
  scoped_array<TCHAR> dll_path(new TCHAR[_MAX_PATH]);
  if (!GetModuleDirectory(dll_path.get(), error)) {
    return false;
  }

  if (!PathAppend(dll_path.get(), _T("O3DExtras\\d3dx9_36.dll"))) {
    *error = "Failed to compute location of d3dx9_36.dll.";
    return false;
  }
  g_d3dx = LoadLibrary(dll_path.get());
  if (!g_d3dx) {  // Last-ditch "Is this anywhere?" check.
    g_d3dx = LoadLibrary(_T("d3dx9_36.dll"));
  }
  if (!g_d3dx) {
    std::stringstream error_code_str;
    error_code_str << GetLastError();
    *error = "Got error " + error_code_str.str() + " loading d3dx9 library.";
    return false;
  }

  D3DXCreateEffect = reinterpret_cast<D3DXCreateEffect_Ptr>(
      GetProcAddress(g_d3dx, "D3DXCreateEffect"));
  if (D3DXCreateEffect == NULL) {
    RendererCleanupAllVariables();
    *error = "Failed to load D3DXCreateEffect.";
    return false;
  }

  D3DXGetShaderInputSemantics =
      reinterpret_cast<D3DXGetShaderInputSemantics_Ptr>(
          GetProcAddress(g_d3dx, "D3DXGetShaderInputSemantics"));
  if (D3DXGetShaderInputSemantics == NULL) {
    RendererCleanupAllVariables();
    *error = "Failed to load D3DXGetShaderInputSemantics.";
    return false;
  }

  D3DXCreateEffectCompilerFromFileW =
      reinterpret_cast<D3DXCreateEffectCompilerFromFileW_Ptr>(
          GetProcAddress(g_d3dx, "D3DXCreateEffectCompilerFromFileW"));
  if (D3DXCreateEffectCompilerFromFileW == NULL) {
    RendererCleanupAllVariables();
    *error = "Failed to load D3DXCreateEffectCompilerFromFileW.";
    return false;
  }

  D3DXCreateEffectCompilerFromFileA =
      reinterpret_cast<D3DXCreateEffectCompilerFromFileA_Ptr>(
          GetProcAddress(g_d3dx, "D3DXCreateEffectCompilerFromFileA"));
  if (D3DXCreateEffectCompilerFromFileA == NULL) {
    RendererCleanupAllVariables();
    *error = "Failed to load D3DXCreateEffectCompilerFromFileA.";
    return false;
  }

  D3DXSaveSurfaceToFileW =
      reinterpret_cast<D3DXSaveSurfaceToFileW_Ptr>(
          GetProcAddress(g_d3dx, "D3DXSaveSurfaceToFileW"));
  if (D3DXSaveSurfaceToFileW == NULL) {
    RendererCleanupAllVariables();
    *error = "Failed to load D3DXSaveSurfaceToFileW.";
    return false;
  }

  D3DXSaveSurfaceToFileA =
      reinterpret_cast<D3DXSaveSurfaceToFileA_Ptr>(
          GetProcAddress(g_d3dx, "D3DXSaveSurfaceToFileA"));
  if (D3DXSaveSurfaceToFileA == NULL) {
    RendererCleanupAllVariables();
    *error = "Failed to load D3DXSaveSurfaceToFileA.";
    return false;
  }

  D3DXGetShaderConstantTable =
      reinterpret_cast<D3DXGetShaderConstantTable_Ptr>(
          GetProcAddress(g_d3dx, "D3DXGetShaderConstantTable"));
  if (D3DXGetShaderConstantTable == NULL) {
    RendererCleanupAllVariables();
    *error = "Failed to load D3DXGetShaderConstantTable.";
    return false;
  }

  D3DXCreateFontW = reinterpret_cast<D3DXCreateFontW_Ptr>(
      GetProcAddress(g_d3dx, "D3DXCreateFontW"));
  if (D3DXCreateFontW == NULL) {
    RendererCleanupAllVariables();
    *error = "Failed to load D3DXCreateFontW.";
    return false;
  }

  D3DXCreateFontA = reinterpret_cast<D3DXCreateFontW_Ptr>(
      GetProcAddress(g_d3dx, "D3DXCreateFontA"));
  if (D3DXCreateFontA == NULL) {
    RendererCleanupAllVariables();
    *error = "Failed to load D3DXCreateFontA.";
    return false;
  }

  D3DXCreateLine = reinterpret_cast<D3DXCreateLine_Ptr>(
      GetProcAddress(g_d3dx, "D3DXCreateLine"));
  if (D3DXCreateLine == NULL) {
    RendererCleanupAllVariables();
    *error = "Failed to load D3DXCreateLine.";
    return false;
  }

  return true;
}

bool D3D9SoftwareInstallCheck(std::string* error) {
  scoped_array<TCHAR> dll_path(new TCHAR[_MAX_PATH]);
  if (!GetModuleDirectory(dll_path.get(), error)) {
    return false;
  }

  if (!PathAppend(dll_path.get(), _T("O3DExtras\\swiftshader_d3d9.dll"))) {
    *error = "Failed to compute new software renderer location.";
    return false;
  }

  g_d3d9_software = LoadLibrary(dll_path.get());
  if (!g_d3d9_software) {
    *error = "Failed to load software renderer.";
    return false;
  }

  Direct3DCreate9Software = reinterpret_cast<Direct3DCreate9_Ptr>(
      GetProcAddress(g_d3d9_software, "Direct3DCreate9"));
  if (Direct3DCreate9Software == NULL) {
    *error = "Failed to locate Direct3DCreate9 in software renderer.";
    return false;
  }

  return true;
}

}  // namespace anonymous

bool RendererInstallCheck(std::string *error) {
  if (g_d3dx) {
    return true;  // Already done.
  }
  if (!D3DX9InstallCheck(error)) {
    return false;
  }

  // Failure to find the software renderer is not an error at this point.
  D3D9SoftwareInstallCheck(error);

  return true;
}

}  // namespace o3d
