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


// This file contains the main routine for the converter that writes
// out a scene graph as a JSON file.

#include <string>
#include <iostream>
#include <vector>

#include "base/at_exit.h"
#include "base/file_path.h"
#include "base/command_line.h"
#include "converter/cross/converter.h"
#include "utils/cross/file_path_utils.h"

using std::string;
using std::wstring;

#if defined(OS_WIN)
int wmain(int argc, wchar_t **argv) {
  // On Windows, CommandLine::Init ignores its arguments and uses
  // GetCommandLineW.
  CommandLine::Init(0, NULL);
#endif
#if defined(OS_LINUX)
int main(int argc, char **argv) {
  CommandLine::Init(argc, argv);
#endif
#if defined(OS_MACOSX)
// The "real" main on Mac is in mac/converter_main.mm, so we can get
// memory pool initialization for Cocoa.
int CrossMain(int argc, char**argv) {
  CommandLine::Init(argc, argv);
#endif
  // Create an at_exit_manager so that base singletons will get
  // deleted properly.
  base::AtExitManager at_exit_manager;
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  FilePath in_filename, out_filename;

  std::vector<std::wstring> values = command_line->GetLooseValues();
  if (values.size() == 1) {
    in_filename = o3d::WideToFilePath(values[0]);
  } else if (values.size()== 2) {
    in_filename = o3d::WideToFilePath(values[0]);
    out_filename = o3d::WideToFilePath(values[1]);
  } else {
    std::cerr << "Usage: " << FilePath(argv[0]).BaseName().value()
              << " [--no-condition] <infile.fx> [<outfile.fx>]\n";
    return EXIT_FAILURE;
  }

  o3d::converter::Options options;
  options.condition = !command_line->HasSwitch(L"no-condition");

  if (!options.condition && !out_filename.empty()) {
    std::cerr << "Warning: Ignoring output filename because conditioning "
              << "has been turned off.\n";
    out_filename = FilePath();
  }

  o3d::String errors;
  bool result = o3d::converter::Verify(in_filename, out_filename, options,
                                       &errors);
  if (result) {
    std::cerr << "Shader in '" << o3d::FilePathToUTF8(in_filename).c_str()
              << "' has been validated." << std::endl;
    return EXIT_SUCCESS;
  } else {
    std::cerr << errors.c_str() << std::endl;
    std::cerr << "Shader in '" << o3d::FilePathToUTF8(in_filename).c_str()
              << "' FAILED to be validated." << std::endl;
    return EXIT_FAILURE;
  }
}
