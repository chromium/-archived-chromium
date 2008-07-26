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

// This class contains some information about a plugin process.
// It is used as the source to notifications about plugin process connections/
// disconnections.
// It has a copy constructor and assignment operator so that it can be copied.
// It also defines the < operator so that it can be used as a key in a std::map.

#ifndef CHROME_BROWSER_PLUGIN_PROCESS_INFO_H__
#define CHROME_BROWSER_PLUGIN_PROCESS_INFO_H__

#include <string>
#include "windows.h"

class PluginProcessInfo {
 public:
  PluginProcessInfo(std::wstring dll_path, HANDLE process)
    : dll_path_(dll_path),
      process_(process) {
  }

  PluginProcessInfo(const PluginProcessInfo& ppi) {
    dll_path_ = ppi.dll_path_;
    process_ = ppi.process_;
  }

  ~PluginProcessInfo() { };

  PluginProcessInfo&
  PluginProcessInfo::operator=(const PluginProcessInfo& ppi) {
    if (this != &ppi) {
      dll_path_ = ppi.dll_path_;
      process_ = ppi.process_;
    }
    return *this;
  }

  // We define the < operator so that the PluginProcessInfo can be used as a key
  // in a std::map.
  bool operator <(const PluginProcessInfo& rhs) const {
    if (process_ != rhs.process_)
      return process_ < rhs.process_;
    return dll_path_ < rhs.dll_path_;
  }

  bool operator ==(const PluginProcessInfo& rhs) const {
    return (process_ == rhs.process_) && (dll_path_ == rhs.dll_path_);
  }

  std::wstring dll_path() const { return dll_path_; }

  HANDLE process() const { return process_; }

 private:
  std::wstring dll_path_;
  HANDLE process_;
};

#endif  // #ifndef CHROME_BROWSER_PLUGIN_PROCESS_INFO_H__