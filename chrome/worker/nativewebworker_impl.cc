// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/nativewebworker_impl.h"

#include "base/compiler_specific.h"

#undef LOG

#include "base/logging.h"
#include "webkit/glue/glue_util.h"
#include "base/thread.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebKitClient.h"
#include "webkit/api/public/WebWorkerClient.h"

// TODO(sehr): This will be changed to point to the real NaCl headers once
// the builds are integrated.
#include "chrome/worker/nativewebworker_stub.h"

namespace {
// Remember the main thread's message loop, so that the listener thread
// can post messages to it when the worker wants to post to the renderer.
static MessageLoop* g_main_thread_message_loop;

// PostMessageTask encapsulates sending messages from native web workers to
// the renderer by placing them on the main thread's message loop.
class PostMessageTask : public Task {
 public:
  PostMessageTask(const char* bufp, WebKit::WebWorkerClient* client)
      : message_string_(WebKit::WebString::fromUTF8(bufp)),
        client_(client) {
  }
  ~PostMessageTask() { }
  void Run() {
    client_->postMessageToWorkerObject(message_string_);
  }

 private:
  WebKit::WebString message_string_;
  WebKit::WebWorkerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(PostMessageTask);
};

// PostToRenderer places a string in bufp in a message and enqueues
// a task to send the message to the renderer.
static void PostToRenderer(const char* bufp,
                           WebKit::WebWorkerClient* client) {
  g_main_thread_message_loop->PostTask(FROM_HERE,
                                       new PostMessageTask(bufp, client));
}

class ListenerTask : public Task {
 public:
  ListenerTask(WebKit::WebWorkerClient* client,
               struct NaClDesc* chrome_desc)
      : client_(client),
        chrome_desc_(chrome_desc) { }
  ~ListenerTask() { }
  void Run() {
    NaClSrpcListenerLoop(chrome_desc_, PostToRenderer, client_);
  }

 private:
  WebKit::WebWorkerClient* client_;
  struct NaClDesc* chrome_desc_;

  DISALLOW_COPY_AND_ASSIGN(ListenerTask);
};
}

// NativeWebWorkerListenerThread encapsulates a listener for SRPC messages from
// native web workers.
class NativeWebWorkerListenerThread : public base::Thread {
 public:
  explicit NativeWebWorkerListenerThread(const char* str) : Thread(str) { }
  ~NativeWebWorkerListenerThread() {}
  static NativeWebWorkerListenerThread* Create() {
    return new NativeWebWorkerListenerThread("NativeWebWorkerListener");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWebWorkerListenerThread);
};

// Utility function to convert to C strings.
static char* WebStringToCharp(const WebKit::WebString& str, size_t* len) {
  // Convert source from webString data to char*
  *len = str.length();
  char* bufp = new char[*len + 1];
  const WebKit::WebUChar* datap = str.data();
  for (size_t i = 0; i < *len; ++i) {
    bufp[i] = static_cast<char>(datap[i]);
  }
  bufp[*len] = 0;
  return bufp;
}

// Used for debugging purposes for now.
static int retval;

WebKit::WebWorker* NativeWebWorkerImpl::create(
    WebKit::WebWorkerClient* client) {
  return new NativeWebWorkerImpl(client);
}

NativeWebWorkerImpl::NativeWebWorkerImpl(WebKit::WebWorkerClient* client)
    : client_(client) {
}

NativeWebWorkerImpl::~NativeWebWorkerImpl() {
}

void NativeWebWorkerImpl::startWorkerContext(const WebKit::WebURL& script_url,
                              const WebKit::WebString& user_agent,
                              const WebKit::WebString& source) {
  size_t len;
  char* bufp = WebStringToCharp(source, &len);
  // Start NaCl using the nexe.
  retval = NaClStartNativeWebWorker(bufp, len, &nap_, &channel_);
  // Free the string.
  delete[] bufp;

  // Remember the main thread's message loop.
  g_main_thread_message_loop = MessageLoop::current();
  // Start the upcall listener thread.
  upcall_thread_ = NativeWebWorkerListenerThread::Create();
  upcall_thread_->Start();
  // Put an SRPC listener loop on the listener thread.
  NaClCreateUpcallChannel(descs_);
  Task* task = new ListenerTask(client_, descs_[1]);
  upcall_thread_->message_loop()->PostTask(FROM_HERE, task);
  // Send upcall listener channel descriptor to the native worker.
  retval = NaClSrpcSendUpcallDesc(channel_, descs_[0]);
}

void NativeWebWorkerImpl::terminateWorkerContext() {
  // Close the descriptors.
  NaClDestroyUpcallChannel(descs_);
  // Shut down the sel_ldr instance for this native web worker.
  retval = NaClTerminateNativeWebWorker(&nap_, &channel_);
  // Shut down the upcall thread.
  upcall_thread_->Stop();
}

void NativeWebWorkerImpl::postMessageToWorkerContext(
         const WebKit::WebString& message) {
  size_t len;
  char* bufp = WebStringToCharp(message, &len);
  // Send a message to NaCl object
  retval = NaClPostMessageToNativeWebWorker(bufp, len, &nap_, &channel_);
  delete[] bufp;
}

void NativeWebWorkerImpl::workerObjectDestroyed() {
}
