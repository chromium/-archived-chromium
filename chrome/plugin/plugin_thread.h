// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_PLUGIN_THREAD_H_
#define CHROME_PLUGIN_PLUGIN_THREAD_H_

#include "base/file_path.h"
#include "base/native_library.h"
#include "build/build_config.h"
#include "chrome/common/child_thread.h"
#include "chrome/plugin/plugin_channel.h"
#include "webkit/glue/plugins/plugin_lib.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

class NotificationService;

// The PluginThread class represents a background thread where plugin instances
// live.  Communication occurs between WebPluginDelegateProxy in the renderer
// process and WebPluginDelegateStub in this thread through IPC messages.
class PluginThread : public ChildThread {
 public:
  PluginThread();
  ~PluginThread();

  // Returns the one plugin thread.
  static PluginThread* current();

 private:
  virtual void OnControlMessageReceived(const IPC::Message& msg);

  // Thread implementation:
  virtual void Init();
  virtual void CleanUp();

  // Callback for when a channel has been created.
  void OnCreateChannel(
      int process_id,
      bool off_the_record);
  void OnPluginMessage(const std::vector<uint8> &data);

  scoped_ptr<NotificationService> notification_service_;

  // The plugin module which is preloaded in Init
  base::NativeLibrary preloaded_plugin_module_;

  // Points to the plugin file that this process hosts.
  FilePath plugin_path_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginThread);
};

#endif  // CHROME_PLUGIN_PLUGIN_THREAD_H_
