// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_

#include <map>
#include <string>

class ExtensionView;
class ResourceMessageFilter;

// This class manages message passing to and from extension processes.  It
// maintains a list of available extensions, as well as a set of open channels.
// It should only be accessed on the IO thread, with the exception of
// RegisterExtensionView().
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

  // Registers an extension so that it can be referenced by its ID.  This method
  // should only be used by the UI thread.
  void RegisterExtensionView(ExtensionView* view);

  // Given an extension's ID, opens a channel between the given renderer "port"
  // and that extension.  Returns a channel ID to be used for posting messages
  // between the processes, or -1 if the extension doesn't exist.
  int OpenChannelToExtension(const std::string& extension_id,
                             ResourceMessageFilter* renderer_port);

  // Sends a message to the extension via the given channel.
  void PostMessageToExtension(int channel_id, const std::string& message);

  // Called to let us know that a renderer is going away.
  void RendererShutdown(ResourceMessageFilter* renderer_port);
 private:
  class ExtensionFilter;
  friend class ExtensionFilter;

  // Called when our ExtensionFilter is ready/going away.
  void OnExtensionRegistered(ExtensionFilter* extension);
  void OnExtensionUnregistered(ExtensionFilter* extension);

  // A map of extension ID to the extension port to communicate through.
  // TODO(mpcomplete): Handle the case where there's multiple ExtensionViews
  // in a given extension.
  typedef std::map<std::string, ExtensionFilter*> ExtensionMap;
  ExtensionMap extensions_;

  // The connection between the renderer and extension.
  struct MessageChannel {
    ExtensionFilter* extension_port;
    ResourceMessageFilter* renderer_port;
  };

  // A map of channel ID to its channel object.
  typedef std::map<int, MessageChannel> MessageChannelMap;
  MessageChannelMap channels_;

  // For generating unique channel IDs.
  int next_channel_id_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_
