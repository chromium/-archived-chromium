// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
