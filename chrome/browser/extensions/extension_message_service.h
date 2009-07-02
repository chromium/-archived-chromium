// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_

#include <map>
#include <set>
#include <string>

#include "base/lock.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/notification_registrar.h"

class MessageLoop;
class RenderProcessHost;
class ResourceMessageFilter;
class URLRequestContext;

// This class manages message passing between renderer processes.  It maintains
// a list of available extensions and which renderers each lives in, as well as
// a set of open channels.
//
// Terminology:
// channel: connection between two ports (one of which belongs to an extension)
// port: an IPC::Message::Sender interface through which we communicate to a
//   process.  We use MessageFilters for this since that allows us to send our
//   messages on the IO thread.
class ExtensionMessageService : public NotificationObserver {
 public:
  // Javascript function name constants.
  static const char kDispatchOnConnect[];
  static const char kDispatchOnDisconnect[];
  static const char kDispatchOnMessage[];
  static const char kDispatchEvent[];

  // Returns the message service for the given context.  Messages can only
  // be sent within a single context.
  static ExtensionMessageService* GetInstance(URLRequestContext* context);

  ExtensionMessageService();

  // --- UI thread only:

  // Gets the process for the specified extension.
  RenderProcessHost* GetProcessForExtension(const std::string& extension_id);

  // Register an extension and its corresponding renderer process.
  void RegisterExtension(const std::string& extension_id,
                         int render_process_id);

  // Add or remove |render_process_pid| as a listener for |event_name|.
  void AddEventListener(std::string event_name, int render_process_id);
  void RemoveEventListener(std::string event_name, int render_process_id);

  // Closes the message channel associated with the given port, and notifies
  // the other side.
  void CloseChannel(int port_id);

  // Sends a message from a renderer to the given port.
  void PostMessageFromRenderer(int port_id, const std::string& message);

  // Send an event to every registered extension renderer.
  void DispatchEventToRenderers(
      const std::string& event_name, const std::string& event_args);

  // Given an extension's ID, opens a channel between the given automation
  // "port" and that extension.  Returns a channel ID to be used for posting
  // messages between the processes, or -1 if the extension doesn't exist.
  int OpenAutomationChannelToExtension(int source_process_id,
                                       int routing_id,
                                       const std::string& extension_id,
                                       IPC::Message::Sender* source);

  // NotificationObserver interface.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // --- IO thread only:

  // Given an extension's ID, opens a channel between the given renderer "port"
  // and that extension.  Returns a channel ID to be used for posting messages
  // between the processes, or -1 if the extension doesn't exist.
  // This runs on the IO thread so that it can be used in a synchronous IPC
  // message.
  int OpenChannelToExtension(int routing_id, const std::string& extension_id,
                             ResourceMessageFilter* source);

 private:
  // The connection between two ports.  It is possible that both ports
  // refer to the same renderer.
  struct MessageChannel {
    IPC::Message::Sender* port1;
    IPC::Message::Sender* port2;
  };

  // A map of channel ID to its channel object.
  typedef std::map<int, MessageChannel> MessageChannelMap;

  // Allocates a pair of port ids.
  // NOTE: this can be called from any thread.
  void AllocatePortIdPair(int* port1, int* port2);

  // Gets the process ID for the specified extension.
  // NOTE: this can be called from any thread.
  int GetProcessIdForExtension(const std::string& extension_id);

  void CloseChannelImpl(MessageChannelMap::iterator channel_iter, int port_id);

  int OpenChannelToExtensionImpl(const std::string& extension_id,
                                 IPC::Message::Sender* source);

  NotificationRegistrar registrar_;

  // The UI message loop, used for posting tasks.
  MessageLoop* ui_loop_;

  // A map of extension ID to the render_process_id that the extension lives in.
  typedef std::map<std::string, int> ProcessIDMap;
  ProcessIDMap process_ids_;

  // Protects the process_ids map, since it can be accessed on the IO thread
  // or UI thread.  Be careful not to hold this lock when calling external
  // code (especially sending messages) to avoid deadlock.
  Lock process_ids_lock_;

  // A map between an event name and a set of process id's that are listening
  // to that event.
  typedef std::map<std::string, std::set<int> > ListenerMap;
  ListenerMap listeners_;

  // --- UI thread only:

  // UI-thread specific initialization.  Does nothing if called more than once.
  void Init();

  // Handles channel creation and notifies the destination that a channel was
  // opened.
  void OpenChannelOnUIThread(int source_routing_id,
      int source_port_id, int source_process_id,
      int dest_port_id, int dest_process_id,
      const std::string& extension_id);

  // Common between OpenChannelOnUIThread and
  // OpenAutomationChannelToExtension.
  void OpenChannelOnUIThreadImpl(
    int source_routing_id, int source_port_id, int source_process_id,
    IPC::Message::Sender* source, int dest_port_id, int dest_process_id,
    const std::string& extension_id);

  MessageChannelMap channels_;

  // True if Init has been called.
  bool initialized_;

  // For generating unique channel IDs.
  int next_port_id_;

  // Protects the next_port_id_ variable, since it can be
  // used on the IO thread or the UI thread.
  Lock next_port_id_lock_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageService);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_
