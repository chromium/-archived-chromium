// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/dom_storage_dispatcher_host.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/browser/in_process_webkit/webkit_thread.h"

DOMStorageDispatcherHost::DOMStorageDispatcherHost(
    IPC::Message::Sender* message_sender,
    WebKitContext* webkit_context,
    WebKitThread* webkit_thread)
    : webkit_context_(webkit_context),
      webkit_thread_(webkit_thread),
      message_sender_(message_sender) {
  DCHECK(webkit_context_.get());
  DCHECK(webkit_thread_);
  DCHECK(message_sender_);
}

DOMStorageDispatcherHost::~DOMStorageDispatcherHost() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  message_sender_ = NULL;
}

bool DOMStorageDispatcherHost::OnMessageReceived(const IPC::Message& msg) {
  // TODO(jorlow): Implement DOM Storage's message handler...and the rest
  //               of DOM Storage.  :-)
  return false;
}

void DOMStorageDispatcherHost::Send(IPC::Message* message) {
  if (!message_sender_) {
    delete message;
    return;
  }

  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    message_sender_->Send(message);
    return;
  }

  // The IO thread can't dissapear while the WebKit thread is still running.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  MessageLoop* io_loop = ChromeThread::GetMessageLoop(ChromeThread::IO);
  CancelableTask* task = NewRunnableMethod(this,
                                           &DOMStorageDispatcherHost::Send,
                                           message);
  io_loop->PostTask(FROM_HERE, task);
}
