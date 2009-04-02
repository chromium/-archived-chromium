// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_

#include <map>
#include <string>

#include "base/lock.h"

class ExtensionView;
class ResourceMessageFilter;

// This class manages message passing between renderer processes.  It maintains
// a list of available extensions and which renderers each lives in, as well as
// a set of open channels.
//
// Terminology:
// channel: connection between two ports (one of which belongs to an extension)
// port: an IPC::Message::Sender interface through which we communicate to a
//   process.  We use MessageFilters for this since that allows us to send our
//   messages on the IO thread.
class ExtensionMessageService {
 public:
  static ExtensionMessageService* GetInstance();

  ExtensionMessageService();

  // --- UI thread only:

  // Register an extension and its corresponding renderer process.
  void RegisterExtension(const std::string& extension_id,
                         int render_process_id);

  // --- IO thread only:

  // Given an extension's ID, opens a channel between the given renderer "port"
  // and that extension.  Returns a channel ID to be used for posting messages
  // between the processes, or -1 if the extension doesn't exist.
  int OpenChannelToExtension(const std::string& extension_id,
                             ResourceMessageFilter* source);

  // Sends a message from a renderer to the given port.
  void PostMessageFromRenderer(int port_id, const std::string& message,
                               ResourceMessageFilter* source);

  // --- UI or IO thread:

  // Called to let us know that a renderer has been started.
  void RendererReady(ResourceMessageFilter* port);

  // Called to let us know that a renderer is going away.
  void RendererShutdown(ResourceMessageFilter* port);

 private:
  // A map of extension ID to the render_process_id that the extension lives in.
  typedef std::map<std::string, int> ProcessIDMap;
  ProcessIDMap process_ids_;

  // A map of render_process_id to its corresponding message filter, which we
  // use for sending messages.
  typedef std::map<int, ResourceMessageFilter*> RendererMap;
  RendererMap renderers_;

  // Protects the two maps above, since each can be accessed on the IO thread
  // or UI thread.  Be careful not to hold this lock when calling external
  // code (especially sending messages) to avoid deadlock.
  Lock renderers_lock_;

  // --- IO thread only:

  // The connection between two renderers.
  struct MessageChannel {
    ResourceMessageFilter* port1;
    ResourceMessageFilter* port2;
  };

  // A map of channel ID to its channel object.
  typedef std::map<int, MessageChannel> MessageChannelMap;
  MessageChannelMap channels_;

  // For generating unique channel IDs.
  int next_port_id_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_
