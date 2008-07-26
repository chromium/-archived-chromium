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

// See http://wiki.corp.google.com/twiki/bin/view/Main/ChromeMultiProcessResourceLoading

#ifndef CHROME_COMMON_RESOURCE_DISPATCHER_H__
#define CHROME_COMMON_RESOURCE_DISPATCHER_H__

#include <windows.h>
#include <hash_map>

#include "base/task.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/render_messages.h"
#include "webkit/glue/resource_loader_bridge.h"

// Uncomment this to disable loading resources via the parent process.  This
// may be useful for debugging purposes.
//#define USING_SIMPLE_RESOURCE_LOADER_BRIDGE

// This class serves as a communication interface between the
// ResourceDispatcherHost in the browser process and the ResourceLoaderBridge in
// the child process.  It can be used from either the renderer or plugin
// processes.
class ResourceDispatcher : public base::RefCounted<ResourceDispatcher> {
 public:
  ResourceDispatcher(IPC::Message::Sender* sender);
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
  typedef stdext::hash_map<int,PendingRequestInfo> PendingRequestList;

  // Message response handlers, called by the message handler for this process.
  void OnUploadProgress(int request_id, int64 position, int64 size);
  void OnReceivedResponse(int request_id, const ViewMsg_Resource_ResponseHead&);
  void OnReceivedRedirect(int request_id, const GURL& new_url);
  void OnReceivedData(int request_id, SharedMemoryHandle data, int data_len);
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
