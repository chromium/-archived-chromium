// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_DISPATCHER_HOST_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_DISPATCHER_HOST_H_

#include "base/ref_counted.h"
#include "base/thread.h"
#include "chrome/common/ipc_message.h"

class WebKitContext;
class WebKitThread;

// This class handles the logistics of DOM Storage within the browser process.
// It mostly ferries information between IPCs and the WebKit implementations,
// but it also handles some special cases like when renderer processes die.
// THIS CLASS MUST NOT BE DESTROYED ON THE WEBKIT THREAD (for now).
class DOMStorageDispatcherHost :
    public base::RefCountedThreadSafe<DOMStorageDispatcherHost> {
 public:
  // Only call the constructor from the UI thread.
  DOMStorageDispatcherHost(IPC::Message::Sender* message_sender,
                           WebKitContext*, WebKitThread*);

  // Only call Shutdown from the IO thread.  Shutdown warns us that we're going
  // to go away soon and tells us not to send anything else to the IO thread.
  void Shutdown();

  // Only call from IO thread.
  bool OnMessageReceived(const IPC::Message& message);

  // Send a message to the renderer process associated with our
  // message_sender_ via the IO thread.  May be called from any thread.
  void Send(IPC::Message* message);

 private:
  friend class base::RefCountedThreadSafe<DOMStorageDispatcherHost>;
  ~DOMStorageDispatcherHost();

  // Obviously can be called from any thread.
  bool IsOnIOThread() const;
  bool IsOnWebKitThread() const;

  // Are immutable and are always valid throughout the lifetime of the object.
  scoped_refptr<WebKitContext> webkit_context_;
  scoped_refptr<WebKitThread> webkit_thread_;

  // We keep the message_sender_ pointer for sending messages.  All access
  // to the message_sender_ (and the IO thread in general) should be done under
  // this lock and only if message_sender_ is non-NULL.
  Lock message_sender_lock_;
  IPC::Message::Sender* message_sender_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageDispatcherHost);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_DISPATCHER_HOST_H_
