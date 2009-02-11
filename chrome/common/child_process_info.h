// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_PROCESS_INFO_H_
#define CHROME_COMMON_CHILD_PROCESS_INFO_H_

#include <string>
#include "base/basictypes.h"
#include "base/process.h"

// Holds information about a child process.
class ChildProcessInfo {
 public:
  enum ProcessType {
   BROWSER_PROCESS,
   RENDER_PROCESS,
   PLUGIN_PROCESS,
   WORKER_PROCESS,
   UNKNOWN_PROCESS,
  };

  // Returns the type of the process.
  ProcessType type() const { return type_; }

  // Returns the name of the process.  i.e. for plugins it might be Flash, while
  // for workers it might be the domain that it's from.
  std::wstring name() const { return name_; }

  // Getter to the process.
  base::Process& process() { return process_; }

  // Returns an English name of the process type, should only be used for non
  // user-visible strings, or debugging pages like about:memory.
  static std::wstring GetTypeNameInEnglish(ProcessType type);

  // Returns a localized title for the child process.  For example, a plugin
  // process would be "Plug-in: Flash" when name is "Flash".
  std::wstring GetLocalizedTitle() const;

  // We define the < operator so that the ChildProcessInfo can be used as a key
  // in a std::map.
  bool operator <(const ChildProcessInfo& rhs) const {
    if (process_.handle() != rhs.process_.handle())
      return process_ .handle() < rhs.process_.handle();
    return name_ < rhs.name_;
  }

  bool operator ==(const ChildProcessInfo& rhs) const {
    return (process_.handle() == rhs.process_.handle()) && (name_ == rhs.name_);
  }

 protected:
  void set_type(ProcessType type) { type_ = type; }
  void set_name(const std::wstring& name) { name_ = name; }

 private:
  ProcessType type_;
  std::wstring name_;

  // The handle to the process.
  base::Process process_;
};

#endif  // CHROME_COMMON_CHILD_PROCESS_INFO_H_

