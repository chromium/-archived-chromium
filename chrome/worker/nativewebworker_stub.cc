// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Native WebWorker stubs.
// Please note that this implementation is used only until the native client
// build is invokable from within Chromium.

#include "chrome/worker/nativewebworker_stub.h"

int NaClStartNativeWebWorker(char *buffer,
                             size_t buf_len,
                             NaClApp **nap,
                             NaClSrpcChannel **untrusted_channel) {
  return 0;
}

int NaClPostMessageToNativeWebWorker(char *buffer,
                                     size_t buf_len,
                                     NaClApp **nap,
                                     NaClSrpcChannel **untrusted_ch) {
  return 0;
}

int NaClTerminateNativeWebWorker(NaClApp **nap,
                                 NaClSrpcChannel **untrusted_channel) {
  return 0;
}

int NaClCreateUpcallChannel(NaClDesc* desc[2]) {
  return 0;
}

int NaClSrpcSendUpcallDesc(NaClSrpcChannel *channel,
                           NaClDesc *nacl_desc) {
  return 0;
}

int NaClSrpcListenerLoop(NaClDesc *chrome_desc,
                         NativeWorkerPostMessageFunc func,
                         WebKit::WebWorkerClient* client) {
  return 0;
}

void NaClDestroyUpcallChannel(NaClDesc* desc[2]) {
}
