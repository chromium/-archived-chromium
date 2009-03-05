// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CHROME_COMMON_RESOURCE_DISPATCHER_H__
#define CHROME_COMMON_RESOURCE_DISPATCHER_H__

#include <deque>
#include <queue>
#include <string>

#include "base/hash_tables.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "chrome/common/filter_policy.h"
#include "chrome/common/ipc_channel.h"
#include "webkit/glue/resource_loader_bridge.h"

struct ResourceResponseHead;

// Uncomment this to disable loading resources via the parent process.  This
// may be useful for debugging purposes.
//#define USING_SIMPLE_RESOURCE_LOADER_BRIDGE

// This class serves as a communication interface between the
// ResourceDispatcherHost in the browser process and the ResourceLoaderBridge in
// the child process.  It can be used from either the renderer or plugin
// processes.
class ResourceDispatcher : public base::RefCounted<ResourceDispatcher> {
 public:
  explicit ResourceDispatcher(IPC::Message::Sender* sender);
  ~ResourceDispatcher();

  // Called to possibly handle the incoming IPC message.  Returns true if
  // handled, else false.
  bool OnMessageReceived(const IPC::Message& message);

  // creates a ResourceLoaderBridge for this type of dispatcher, this is so
  // this can be tested regardless of the ResourceLoaderBridge::Create
  // implementation.
  webkit_glue::ResourceLoaderBridge* CreateBridge(const std::string& method,
    const GURL& url,
    const GURL& policy_url,
    const GURL& referrer,
    const std::string& headers,
    int load_flags,
    int origin_pid,
    ResourceType::Type resource_type,
    bool mixed_content,
    uint32 request_context /* used for plugin->browser requests */);

  // Adds a request from the pending_requests_ list, returning the new
  // requests' ID
  int AddPendingRequest(webkit_glue::ResourceLoaderBridge::Peer* callback,
                        ResourceType::Type resource_type,
                        bool mixed_content);

  // Removes a request from the pending_requests_ list, returning true if the
  // request was found and removed.
  bool RemovePendingRequest(int request_id);

  IPC::Message::Sender* message_sender() const {
    return message_sender_;
  }

  // Toggles the is_deferred attribute for the specified request.
  void SetDefersLoading(int request_id, bool value);

  // We can no longer use message sender
  void ClearMessageSender() {
    message_sender_ = NULL;
  }

  // Returns true if the message passed in is a resource related
  // message.
  bool IsResourceMessage(const IPC::Message& message) const;

 private:
  friend class ResourceDispatcherTest;

  typedef std::deque<IPC::Message*> MessageQueue;
  struct PendingRequestInfo {
    PendingRequestInfo() { }
    PendingRequestInfo(webkit_glue::ResourceLoaderBridge::Peer* peer,
                       ResourceType::Type resource_type,
                       bool mixed_content)
        : peer(peer),
          resource_type(resource_type),
          filter_policy(FilterPolicy::DONT_FILTER),
          mixed_content(mixed_content),
          is_deferred(false) {
    }
    ~PendingRequestInfo() { }
    webkit_glue::ResourceLoaderBridge::Peer* peer;
    ResourceType::Type resource_type;
    FilterPolicy::Type filter_policy;
    MessageQueue deferred_message_queue;
    bool mixed_content;
    bool is_deferred;
  };
  typedef base::hash_map<int, PendingRequestInfo> PendingRequestList;

  // Message response handlers, called by the message handler for this process.
  void OnUploadProgress(int request_id, int64 position, int64 size);
  void OnReceivedResponse(int request_id, const ResourceResponseHead&);
  void OnReceivedRedirect(int request_id, const GURL& new_url);
  void OnReceivedData(int request_id, base::SharedMemoryHandle data,
                      int data_len);
  void OnRequestComplete(int request_id, const URLRequestStatus& status);

  // Dispatch the message to one of the message response handlers.
  void DispatchMessage(const IPC::Message& message);

  // Dispatch any deferred messages for the given request, provided it is not
  // again in the deferred state.
  void FlushDeferredMessages(int request_id);

  IPC::Message::Sender* message_sender_;

  // All pending requests issued to the host
  PendingRequestList pending_requests_;

  ScopedRunnableMethodFactory<ResourceDispatcher> method_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(ResourceDispatcher);
};

#endif  // CHROME_COMMON_RESOURCE_DISPATCHER_H__
