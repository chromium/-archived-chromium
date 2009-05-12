// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TCP_CLIENT_SOCKET_WIN_H_
#define NET_BASE_TCP_CLIENT_SOCKET_WIN_H_

#include <ws2tcpip.h>

#include "base/object_watcher.h"
#include "net/base/address_list.h"
#include "net/base/client_socket.h"
#include "net/base/completion_callback.h"

namespace net {

class TCPClientSocketWin : public ClientSocket {
 public:
  // The IP address(es) and port number to connect to.  The TCP socket will try
  // each IP address in the list until it succeeds in establishing a
  // connection.
  explicit TCPClientSocketWin(const AddressList& addresses);

  ~TCPClientSocketWin();

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;

  // Socket methods:
  // Multiple outstanding requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len, CompletionCallback* callback);

 private:
  class ReadDelegate : public base::ObjectWatcher::Delegate {
   public:
    explicit ReadDelegate(TCPClientSocketWin* tcp_socket)
        : tcp_socket_(tcp_socket) { }
    virtual ~ReadDelegate() { }

    // base::ObjectWatcher::Delegate methods:
    virtual void OnObjectSignaled(HANDLE object);

   private:
    TCPClientSocketWin* const tcp_socket_;
  };

  class WriteDelegate : public base::ObjectWatcher::Delegate {
   public:
    explicit WriteDelegate(TCPClientSocketWin* tcp_socket)
        : tcp_socket_(tcp_socket) { }
    virtual ~WriteDelegate() { }

    // base::ObjectWatcher::Delegate methods:
    virtual void OnObjectSignaled(HANDLE object);

   private:
    TCPClientSocketWin* const tcp_socket_;
  };

  int CreateSocket(const struct addrinfo* ai);
  void DoReadCallback(int rv);
  void DoWriteCallback(int rv);
  void DidCompleteConnect();

  SOCKET socket_;

  // The list of addresses we should try in order to establish a connection.
  AddressList addresses_;

  // Where we are in above list, or NULL if all addrinfos have been tried.
  const struct addrinfo* current_ai_;

  // The various states that the socket could be in.
  bool waiting_connect_;
  bool waiting_read_;
  bool waiting_write_;

  // The separate OVERLAPPED variables for asynchronous operation.
  // |read_overlapped_| is used for both Connect() and Read().
  // |write_overlapped_| is only used for Write();
  OVERLAPPED read_overlapped_;
  OVERLAPPED write_overlapped_;

  // The buffers used in Read() and Write().
  WSABUF read_buffer_;
  WSABUF write_buffer_;
  scoped_refptr<IOBuffer> read_iobuffer_;
  scoped_refptr<IOBuffer> write_iobuffer_;

  // |reader_| handles the signals from |read_watcher_|.
  ReadDelegate reader_;
  // |writer_| handles the signals from |write_watcher_|.
  WriteDelegate writer_;

  // |read_watcher_| watches for events from Connect() and Read().
  base::ObjectWatcher read_watcher_;
  // |write_watcher_| watches for events from Write();
  base::ObjectWatcher write_watcher_;

  // External callback; called when connect or read is complete.
  CompletionCallback* read_callback_;

  // External callback; called when write is complete.
  CompletionCallback* write_callback_;

  DISALLOW_COPY_AND_ASSIGN(TCPClientSocketWin);
};

}  // namespace net

#endif  // NET_BASE_TCP_CLIENT_SOCKET_WIN_H_
