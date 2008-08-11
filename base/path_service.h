// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BASE_PATH_SERVICE_H__
#define BASE_PATH_SERVICE_H__

#include "build/build_config.h"
#ifdef OS_WIN
// TODO(erikkay): this should be removable, but because SetCurrentDirectory
// is the name of a Windows function, it gets macro-ized to SetCurrentDirectoryW
// by windows.h, which leads to a different name in the header vs. the impl.
// Even if we could fix that, it would still hose all callers of the function.
// The right thing is likely to rename.
#include <windows.h>
#endif

#include <string>

#include "base/base_paths.h"

// The path service is a global table mapping keys to file system paths.  It is
// OK to use this service from multiple threads.
//
class PathService {
 public:
  // Retrieves a path to a special directory or file and places it into the
  // string pointed to by 'path'. If you ask for a directory it is guaranteed
  // to NOT have a path separator at the end. For example, "c:\windows\temp"
  // Directories are also guaranteed to exist when this function succeeds.
  //
  // Returns true if the directory or file was successfully retrieved. On
  // failure, 'path' will not be changed.
  static bool Get(int key, std::wstring* path);

  // Overrides the path to a special directory or file.  This cannot be used to
  // change the value of DIR_CURRENT, but that should be obvious.  Also, if the
  // path specifies a directory that does not exist, the directory will be
  // created by this method.  This method returns true if successful.
  //
  // If the given path is relative, then it will be resolved against DIR_CURRENT.
  //
  // WARNING: Consumers of PathService::Get may expect paths to be constant
  // over the lifetime of the app, so this method should be used with caution.
  static bool Override(int key, const std::wstring& path);

  // Return whether a path was overridden.
  static bool IsOverridden(int key);

  // Sets the current directory.
  static bool SetCurrentDirectory(const std::wstring& current_directory);

  // To extend the set of supported keys, you can register a path provider,
  // which is just a function mirroring PathService::Get.  The ProviderFunc
  // returns false if it cannot provide a non-empty path for the given key.
  // Otherwise, true is returned.
  //
  // WARNING: This function could be called on any thread from which the
  // PathService is used, so a the ProviderFunc MUST BE THREADSAFE.
  //
  typedef bool (*ProviderFunc)(int, std::wstring*);

  // Call to register a path provider.  You must specify the range "[key_start,
  // key_end)" of supported path keys.
  static void RegisterProvider(ProviderFunc provider,
                               int key_start,
                               int key_end);
private:
  static bool GetFromCache(int key, std::wstring* path);
  static void AddToCache(int key, const std::wstring& path);
  
};

#endif // BASE_PATH_SERVICE_H__
