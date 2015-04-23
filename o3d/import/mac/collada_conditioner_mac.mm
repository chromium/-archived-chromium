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

//
// This file contains the defintions of the functions in the
// conditioner namespace, which do the bulk of the work of
// conditioning a Collada file for use in O3D.

#include "import/cross/precompile.h"

#include <vector>
#import <Cocoa/Cocoa.h>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "core/cross/error.h"
#include "import/cross/collada.h"
#include "import/cross/collada_conditioner.h"
#include "utils/cross/file_path_utils.h"

namespace o3d {
bool ColladaConditioner::CompileHLSL(const String& shader_source,
                                     const String& vs_entry,
                                     const String& ps_entry) {
  // The HLSL compiler isn't available on Mac.
  return true;
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
  executable_path = executable_path.Append(FILE_PATH_LITERAL("cgc"));

  // Have to convert the file paths to UTF-8
  std::string executable_path_utf8 = FilePathToUTF8(executable_path);
  std::string in_filename_utf8 = FilePathToUTF8(in_filename);
  std::string out_filename_utf8 = FilePathToUTF8(out_filename);

  DLOG(INFO) << "Now invoking '"
             << executable_path_utf8 << " -E "
             << in_filename_utf8 << " -o "
             << out_filename_utf8 << "'";

  // Now we have to convert the UTF-8 file paths to NSStrings.
  NSString *executable_path_ns =
      [[[NSString alloc] initWithUTF8String:executable_path_utf8.c_str()]
        autorelease];
  NSString *in_filename_ns =
      [[[NSString alloc] initWithUTF8String:in_filename_utf8.c_str()]
        autorelease];
  NSString *out_filename_ns =
      [[[NSString alloc] initWithUTF8String:out_filename_utf8.c_str()]
        autorelease];

  NSTask *task = [[[NSTask alloc] init] autorelease];
  [task setLaunchPath:executable_path_ns];
  [task setArguments:[NSArray arrayWithObjects:@"-E", in_filename_ns,
                                @"-o", out_filename_ns, nil]];

  // We send the output to a pipe, even though the tool doesn't really
  // output anything.  This is so that the invocation of the command
  // will be synchronous, and we can be sure that the output file has
  // been written by the time we exit this function.
  NSPipe *pipe = [NSPipe pipe];
  [task setStandardOutput:pipe];
  [task launch];
  NSData *data = [[pipe fileHandleForReading] readDataToEndOfFile];
  return true;
}
}  // end namespace o3d
