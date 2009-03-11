// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TCP_PINGER_H_
#define NET_BASE_TCP_PINGER_H_

#include "base/compiler_specific.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/base/tcp_client_socket.h"

namespace net {

// Simple class to wait until a TCP server is accepting connections.
class TCPPinger {
 public:
  explicit TCPPinger(const net::AddressList& addr)
    : io_thread_("TCPPinger"),
      worker_(new Worker(addr)) {
    worker_->AddRef();
    // Start up a throwaway IO thread just for this.
    // TODO(dkegel): use some existing thread pool instead?
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);
  }

  ~TCPPinger() {
    io_thread_.message_loop()->ReleaseSoon(FROM_HERE, worker_);
  }

  int Ping() {
    // Default is 10 tries, each with a timeout of 1000ms,
    // for a total max timeout of 10 seconds.
    return Ping(base::TimeDelta::FromMilliseconds(1000), 10);
  }

  int Ping(base::TimeDelta tryTimeout, int nTries) {
    int err = ERR_IO_PENDING;
    // Post a request to do the connect on that thread.
    for (int i = 0; i < nTries; i++) {
      io_thread_.message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(worker_,
        &net::TCPPinger::Worker::DoConnect));
      // Timeout here in case remote host offline
      err = worker_->TimedWaitForResult(tryTimeout);
      if (err == net::OK)
        break;
      PlatformThread::Sleep(static_cast<int>(tryTimeout.InMilliseconds()));

      // Cancel leftover activity, if any
      io_thread_.message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(worker_,
        &net::TCPPinger::Worker::DoDisconnect));
      worker_->WaitForResult();
    }
    return err;
  }

 private:

  // Inner class to handle all actual socket calls.
  // This makes the outer interface simpler,
  // and helps us obey the "all socket calls
  // must be on same thread" restriction.
  class Worker : public base::RefCountedThreadSafe<Worker> {
   public:
    explicit Worker(const net::AddressList& addr)
      : event_(false, false),
        net_error_(ERR_IO_PENDING),
        addr_(addr),
        ALLOW_THIS_IN_INITIALIZER_LIST(connect_callback_(this,
            &net::TCPPinger::Worker::ConnectDone)) {
    }

    void DoConnect() {
      sock_.reset(new TCPClientSocket(addr_));
      int rv = sock_->Connect(&connect_callback_);
      // Regardless of success or failure, if we're done now,
      // signal the customer.
      if (rv != ERR_IO_PENDING)
        ConnectDone(rv);
    }

    void DoDisconnect() {
      sock_.reset();
      event_.Signal();
    }

    void ConnectDone(int rv) {
      sock_.reset();
      net_error_ = rv;
      event_.Signal();
    }

    int TimedWaitForResult(base::TimeDelta tryTimeout) {
      event_.TimedWait(tryTimeout);
      return net_error_;
    }

    int WaitForResult() {
      event_.Wait();
      return net_error_;
    }

   private:
    base::WaitableEvent event_;
    int net_error_;
    net::AddressList addr_;
    scoped_ptr<TCPClientSocket> sock_;
    net::CompletionCallbackImpl<Worker> connect_callback_;
  };

  base::Thread io_thread_;
  Worker* worker_;
  DISALLOW_COPY_AND_ASSIGN(TCPPinger);
};

}  // namespace net

#endif  // NET_BASE_TCP_PINGER_H_
