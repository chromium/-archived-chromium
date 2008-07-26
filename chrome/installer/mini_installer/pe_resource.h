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

#ifndef CHROME_INSTALLER_MINI_INSTALLER_PE_RESOURCE__
#define CHROME_INSTALLER_MINI_INSTALLER_PE_RESOURCE__

#include <windows.h>

// This class models a windows PE resource. It does not pretend to be a full
// API wrapper and it is just concerned with loading it to memory and writing
// it to disk. Each resource is unique only in the context of a loaded module,
// that is why you need to specify one on each constructor.
class PEResource {
 public:
  // This ctor takes the handle to the resource and the module where it was
  // found. Ownership of the resource is transfered to this object.
  PEResource(HRSRC resource, HMODULE module);

  // This ctor takes the resource name, the resource type and the module where
  // to look for the resource. If the resource is found IsValid() returns true.
  PEResource(const wchar_t* name, const wchar_t* type, HMODULE module);

  // Returns true if the resource is valid.
  bool IsValid();

  // Returns the size in bytes of the resource. Returns zero if the resource is
  // not valid.
  size_t Size();

  // Creates a file in 'path' with a copy of the resource. If the resource can
  // not be loaded into memory or if it cannot be written to disk it returns
  // false.
  bool WriteToDisk(const wchar_t* path);

 private:
  HRSRC resource_;
  HMODULE module_;
};

#endif  // CHROME_INSTALLER_MINI_INSTALLER_PE_RESOURCE__
