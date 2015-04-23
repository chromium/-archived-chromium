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

#include <sys/wait.h>
#include <vector>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "core/cross/error.h"
#include "core/cross/service_locator.h"
#include "import/cross/collada.h"
#include "import/cross/collada_conditioner.h"

namespace o3d {

bool ColladaConditioner::CompileHLSL(const String& shader_source,
                                     const String& vs_entry,
                                     const String& ps_entry) {
  // The HLSL compiler isn't available on Linux.
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

  std::vector<const char*> args;
  args.push_back(executable_path.value().c_str());
  args.push_back("-E");
  args.push_back(in_filename.value().c_str());
  args.push_back("-o");
  args.push_back(out_filename.value().c_str());
  args.push_back(NULL);
  char** args_array = const_cast<char**>(&*args.begin());

  pid_t pid = ::fork();
  if (pid < 0) {
    return false;
  }
  if (pid == 0) {
    DLOG(INFO) << "Now invoking '"
               << args_array[0] << " "
               << args_array[1] << " "
               << args_array[2] << " "
               << args_array[3] << " "
               << args_array[4] << "'";
    char *environ[] = {NULL};
    ::execve(args[0], args_array, environ);
    NOTREACHED() << "Problems invoking "
                 << args_array[0] << " "
                 << args_array[1] << " "
                 << args_array[2] << " "
                 << args_array[3] << " "
                 << args_array[4];
  }

  int status;
  ::wait(&status);
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    return true;
  } else {
    if (WIFEXITED(status)) {
      DLOG(ERROR) << "Cgc terminated with status: " << WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
      DLOG(ERROR) << "Cgc received a signal: " << WTERMSIG(status);
      if (WCOREDUMP(status)) {
        DLOG(ERROR) << "and Cgc dumped a core file.";
      }
    }
    if (WIFSTOPPED(status)) {
      DLOG(ERROR) << "Cgc is stopped on a signal: " << WSTOPSIG(status);
    }
  }
  return false;
}
}  // end namespace o3d
