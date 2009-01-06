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

#include "base/file_path.h"

class PluginProcessInfo {
 public:
  PluginProcessInfo(FilePath plugin_path, HANDLE process)
    : plugin_path_(plugin_path),
      process_(process) {
  }

  PluginProcessInfo(const PluginProcessInfo& ppi) {
    plugin_path_ = ppi.plugin_path_;
    process_ = ppi.process_;
  }

  ~PluginProcessInfo() { };

  PluginProcessInfo&
  PluginProcessInfo::operator=(const PluginProcessInfo& ppi) {
    if (this != &ppi) {
      plugin_path_ = ppi.plugin_path_;
      process_ = ppi.process_;
    }
    return *this;
  }

  // We define the < operator so that the PluginProcessInfo can be used as a key
  // in a std::map.
  bool operator <(const PluginProcessInfo& rhs) const {
    if (process_ != rhs.process_)
      return process_ < rhs.process_;
    return plugin_path_ < rhs.plugin_path_;
  }

  bool operator ==(const PluginProcessInfo& rhs) const {
    return (process_ == rhs.process_) && (plugin_path_ == rhs.plugin_path_);
  }

  FilePath plugin_path() const { return plugin_path_; }

  HANDLE process() const { return process_; }

 private:
  FilePath plugin_path_;
  HANDLE process_;
};

#endif  // #ifndef CHROME_BROWSER_PLUGIN_PROCESS_INFO_H__
