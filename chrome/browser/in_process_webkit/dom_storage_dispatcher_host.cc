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
  DCHECK(webkit_thread_.get());
  DCHECK(message_sender_);
}

DOMStorageDispatcherHost::~DOMStorageDispatcherHost() {
  DCHECK(!message_sender_);
}

void DOMStorageDispatcherHost::Shutdown() {
  DCHECK(IsOnIOThread());
  AutoLock lock(message_sender_lock_);
  message_sender_ = NULL;
}

bool DOMStorageDispatcherHost::OnMessageReceived(const IPC::Message& msg) {
  // TODO(jorlow): Implement DOM Storage's message handler...and the rest
  //               of DOM Storage.  :-)
  return false;
}

void DOMStorageDispatcherHost::Send(IPC::Message* message) {
  if (IsOnIOThread()) {
    if (message_sender_)
      message_sender_->Send(message);
    else
      delete message;
  }

  // If message_sender_ is NULL, the IO thread has either gone away
  // or will do so soon.  By holding this lock until we finish posting to the
  // thread, we block the IO thread from completely shutting down benieth us.
  AutoLock lock(message_sender_lock_);
  if (!message_sender_) {
    delete message;
    return;
  }

  MessageLoop* io_loop = ChromeThread::GetMessageLoop(ChromeThread::IO);
  CancelableTask* task = NewRunnableMethod(this,
                                           &DOMStorageDispatcherHost::Send,
                                           message);
  io_loop->PostTask(FROM_HERE, task);
}

bool DOMStorageDispatcherHost::IsOnIOThread() const {
  MessageLoop* io_loop = ChromeThread::GetMessageLoop(ChromeThread::IO);
  return MessageLoop::current() == io_loop;
}

bool DOMStorageDispatcherHost::IsOnWebKitThread() const {
  return MessageLoop::current() == webkit_thread_->GetMessageLoop();
}
