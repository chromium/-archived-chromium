// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKS_CLIENT_SOCKET_H_
#define NET_SOCKET_SOCKS_CLIENT_SOCKET_H_

#include <string>

#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {

// The SOCKS client socket implementation
class SOCKSClientSocket : public ClientSocket {
 public:
  // Takes ownership of the |transport_socket|, which should already be
  // connected by the time Connect() is called.
  //
  // |req_info| contains the hostname and port to which the socket above will
  // communicate to via the socks layer. For testing the referrer is optional.
  SOCKSClientSocket(ClientSocket* transport_socket,
                    const HostResolver::RequestInfo& req_info,
                    HostResolver* host_resolver);

  // On destruction Disconnect() is called.
  virtual ~SOCKSClientSocket();

  // ClientSocket methods:

  // Does the SOCKS handshake and completes the protocol.
  virtual int Connect(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len, CompletionCallback* callback);

#if defined(OS_LINUX)
  // Needed by ssl_client_socket_nss.
  virtual int GetPeerName(struct sockaddr* name, socklen_t* namelen);
#endif

 private:
  FRIEND_TEST(SOCKSClientSocketTest, CompleteHandshake);
  FRIEND_TEST(SOCKSClientSocketTest, SOCKS4AFailedDNS);
  FRIEND_TEST(SOCKSClientSocketTest, SOCKS4AIfDomainInIPv6);

  enum State {
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_HANDSHAKE_WRITE,
    STATE_HANDSHAKE_WRITE_COMPLETE,
    STATE_HANDSHAKE_READ,
    STATE_HANDSHAKE_READ_COMPLETE,
    STATE_NONE,
  };

  // The SOCKS proxy connection either has the hostname resolved via the
  // client or via the server. This enum stores the state of the SOCKS
  // connection. If the client can resolve the hostname, the connection is
  // SOCKS4, otherwise it is SOCKS4A.
  enum SocksVersion {
    kSOCKS4Unresolved,
    kSOCKS4,
    kSOCKS4a,
  };

  void DoCallback(int result);
  void OnIOComplete(int result);

  int DoLoop(int last_io_result);
  int DoResolveHost();
  int DoResolveHostComplete(int result);
  int DoHandshakeRead();
  int DoHandshakeReadComplete(int result);
  int DoHandshakeWrite();
  int DoHandshakeWriteComplete(int result);

  const std::string BuildHandshakeWriteBuffer() const;

  CompletionCallbackImpl<SOCKSClientSocket> io_callback_;

  // Stores the underlying socket.
  scoped_ptr<ClientSocket> transport_;

  State next_state_;
  SocksVersion socks_version_;

  // Stores the callback to the layer above, called on completing Connect().
  CompletionCallback* user_callback_;

  // This IOBuffer is used by the class to read and write
  // SOCKS handshake data. The length contains the expected size to
  // read or write.
  scoped_refptr<IOBuffer> handshake_buf_;

  // While writing, this buffer stores the complete write handshake data.
  // While reading, it stores the handshake information received so far.
  std::string buffer_;

  // This becomes true when the SOCKS handshake has completed and the
  // overlying connection is free to communicate.
  bool completed_handshake_;

  // These contain the bytes sent / received by the SOCKS handshake.
  size_t bytes_sent_;
  size_t bytes_received_;

  // Used to resolve the hostname to which the SOCKS proxy will connect.
  SingleRequestHostResolver host_resolver_;
  AddressList addresses_;
  HostResolver::RequestInfo host_request_info_;

  DISALLOW_COPY_AND_ASSIGN(SOCKSClientSocket);
};

}  // namespace net

#endif  // NET_SOCKET_SOCKS_CLIENT_SOCKET_H_

