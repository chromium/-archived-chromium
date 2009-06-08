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


// This file defines platform-specific implementations of certain
// methods of ColladaConditioner.

#include "import/cross/precompile.h"

#include <d3dx9effect.h>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "core/cross/error.h"
#include "core/cross/service_locator.h"
#include "import/cross/collada.h"
#include "import/cross/collada_conditioner.h"

namespace o3d {

const int kTimeoutInSeconds = 30;

bool ColladaConditioner::CompileHLSL(const String& shader_source,
                                     const String& vs_entry,
                                     const String& ps_entry) {
  bool retval = false;
  String shader_source_hlsl = shader_source;
  shader_source_hlsl +=
      "technique t {\n"
      "  pass p {\n"
      "    VertexShader = compile vs_2_0 " + vs_entry + "();\n"
      "    PixelShader = compile ps_2_0 " + ps_entry + "();\n"
      "  }\n"
      "};\n";

  // Create an effect compiler from the FX file.
  ID3DXEffectCompiler* compiler = NULL;
  ID3DXBuffer* parse_errors = NULL;
  HRESULT hr = ::D3DXCreateEffectCompiler(shader_source_hlsl.c_str(),
                                          static_cast<UINT>(
                                              shader_source_hlsl.size()),
                                          NULL,
                                          NULL,
                                          0,
                                          &compiler,
                                          &parse_errors);

  if (hr != D3D_OK) {
    const char* error_str = reinterpret_cast<const char*>(
        parse_errors->GetBufferPointer());
    O3D_ERROR(service_locator_) << error_str;
  } else {
    ID3DXBuffer* effect_buffer;
    ID3DXBuffer* error_buffer;
    hr = compiler->CompileEffect(0, &effect_buffer, &error_buffer);
    if (hr != S_OK) {
      const char* compile_errors = reinterpret_cast<const char*>(
          error_buffer->GetBufferPointer());
      O3D_ERROR(service_locator_) << compile_errors;
    } else {
      retval = true;
      if (effect_buffer) effect_buffer->Release();
      if (error_buffer) error_buffer->Release();
    }
    if (compiler) compiler->Release();
    if (parse_errors) parse_errors->Release();
  }
  return retval;
}

// Find cgc executable, and run it on the input so that we can get a
// preprocessed version out.
bool ColladaConditioner::PreprocessShaderFile(const FilePath& in_filename,
                                              const FilePath& out_filename) {
  FilePath executable_path;
  if (!PathService::Get(base::DIR_EXE, &executable_path)) {
    O3D_ERROR(service_locator_) << "Couldn't get executable path.";
    return false;
  }
  executable_path = executable_path.Append(FILE_PATH_LITERAL("cgc.exe"));
  std::wstring cmd;
  cmd = L"\"" + executable_path.value() +  L"\" ";
  cmd += L"-E \"" + in_filename.value() + L"\" ";
  cmd += L"-o \"" + out_filename.value() + L"\"";

  // Have to dup the string because CreateProcessW might modify the
  // contents.
  wchar_t* buf = ::_wcsdup(cmd.c_str());

  // Initialize structure to zero with default array initializer.
  PROCESS_INFORMATION process_info = {0};
  // Initialize structure to it's size, followed by zeros.
  STARTUPINFO startup_info = {sizeof(STARTUPINFO)};

  BOOL success = ::CreateProcessW(NULL,
                                  buf,
                                  NULL,
                                  NULL,
                                  TRUE,
                                  0,
                                  NULL,
                                  NULL,
                                  &startup_info,
                                  &process_info);

  if (success) {
    success = FALSE;
    int exit_code = ::WaitForSingleObject(process_info.hProcess,
                                          kTimeoutInSeconds * 1000);
    if (exit_code == WAIT_OBJECT_0) {
      success = TRUE;
    } else if (exit_code == WAIT_TIMEOUT) {
      O3D_ERROR(service_locator_) << "Timed out waiting for cg compiler!";
    } else {
      O3D_ERROR(service_locator_) << "Couldn't start cg compiler (cgc.exe).";
    }
  } else {
    O3D_ERROR(service_locator_) << "Couldn't start cg compiler (cgc.exe).";
  }
  ::CloseHandle(process_info.hProcess);
  ::CloseHandle(process_info.hThread);
  ::free(buf);
  return success == TRUE;
}
}  // end namespace o3d
