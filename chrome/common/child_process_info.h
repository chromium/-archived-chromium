// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_PROCESS_INFO_H_
#define CHROME_COMMON_CHILD_PROCESS_INFO_H_

#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/process.h"

class ChildProcessInfo;

// Holds information about a child process.  Plugins/workers and other child
// processes that live on the IO thread derive from this.
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

  ChildProcessInfo(const ChildProcessInfo& original) {
    type_ = original.type_;
    name_ = original.name_;
    process_ = original.process_;
  }

  ChildProcessInfo& operator=(const ChildProcessInfo& original) {
    if (&original != this) {
      type_ = original.type_;
      name_ = original.name_;
      process_ = original.process_;
    }
    return *this;
  }

  ~ChildProcessInfo();

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

  // The Iterator class allows iteration through either all child processes, or
  // ones of a specific type, depending on which constructor is used.  Note that
  // this should be done from the IO thread and that the iterator should not be
  // kept around as it may be invalidated on subsequent event processing in the
  // event loop.
  class Iterator {
   public:
    Iterator();
    Iterator(ProcessType type);
    ChildProcessInfo* operator->() { return *iterator_; }
    ChildProcessInfo* operator*() { return *iterator_; }
    ChildProcessInfo* operator++();
    bool Done();

   private:
    bool all_;
    ProcessType type_;
    std::list<ChildProcessInfo*>::iterator iterator_;
  };

 protected:
  void set_type(ProcessType type) { type_ = type; }
  void set_name(const std::wstring& name) { name_ = name; }

  // Derived objects need to use this constructor so we know what type we are.
  ChildProcessInfo(ProcessType type);

 private:
  // By making the constructor private, we can ensure that ChildProcessInfo
  // objects can only be created by creating objects derived from them (i.e.
  // PluginProcessHost) or by using the copy constructor or assignment operator
  // to create an object from the former.
  ChildProcessInfo() { }

  ProcessType type_;
  std::wstring name_;

  // The handle to the process.
  base::Process process_;
};

#endif  // CHROME_COMMON_CHILD_PROCESS_INFO_H_
