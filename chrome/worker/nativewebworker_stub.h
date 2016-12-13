// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WORKER_NATIVEWORKER_STUB_H_
#define CHROME_WORKER_NATIVEWORKER_STUB_H_
// Native WebWorker support stub header.

#include <stddef.h>

// As we are not including the native client header files, several types
// from them need to be named outside of structure/method declarations to
// avoid compiler warnings/errors.

struct NaClApp;
struct NaClDesc;
struct NaClSrpcChannel;

namespace WebKit {
class WebWorkerClient;
}

extern "C" {
// The thunk type used by upcall (postMessage from worker) handling.
typedef void (*NativeWorkerPostMessageFunc)(const char *str,
                                            WebKit::WebWorkerClient* client);

// Start a native webworker from the module in buffer.
int NaClStartNativeWebWorker(char *buffer,
                             size_t buf_len,
                             struct NaClApp **nap,
                             struct NaClSrpcChannel **untrusted_channel);

// Performs a postMessage to the worker.
int NaClPostMessageToNativeWebWorker(char *buffer,
                                     size_t buf_len,
                                     struct NaClApp **nap,
                                     struct NaClSrpcChannel **untrusted_ch);

// Causes a native web worker to be shut down.
int NaClTerminateNativeWebWorker(struct NaClApp **nap,
                                 struct NaClSrpcChannel **untrusted_channel);

// Creates a channel that can be used to receive upcalls (postMessage from
// a worker).
int NaClCreateUpcallChannel(struct NaClDesc* desc[2]);

// Part of the initialization of a worker.  Sends the descriptor to the
// worker library to indicate where to send postMessage RPCs.
int NaClSrpcSendUpcallDesc(struct NaClSrpcChannel *channel,
                           struct NaClDesc *nacl_desc);

// Runs an SRPC server loop on the specified channel.  The post_message_func
// is invoked whenever the "postMessage" RPC is received.
int NaClSrpcListenerLoop(struct NaClDesc *chrome_desc,
                         NativeWorkerPostMessageFunc func,
                         WebKit::WebWorkerClient* client);

// Destroys the upcall channel.
void NaClDestroyUpcallChannel(struct NaClDesc* desc[2]);
}

#endif  // CHROME_WORKER_NATIVEWORKER_STUB_H_
