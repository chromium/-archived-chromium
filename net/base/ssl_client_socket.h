// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CLIENT_SOCKET_H_
#define NET_BASE_SSL_CLIENT_SOCKET_H_

#define SECURITY_WIN32  // Needs to be defined before including security.h

#include <windows.h>
#include <security.h>

#include <string>

#include "base/scoped_ptr.h"
#include "net/base/client_socket.h"
#include "net/base/completion_callback.h"

namespace net {

class SSLInfo;

// A client socket that uses SSL as the transport layer.
//
// NOTE: The SSL handshake occurs within the Connect method after a TCP
// connection is established.  If a SSL error occurs during the handshake,
// Connect will fail.  The consumer may choose to ignore certain SSL errors,
// such as a name mismatch, by calling ReconnectIgnoringLastError.
//
class SSLClientSocket : public ClientSocket {
 public:
  // Takes ownership of the transport_socket, which may already be connected.
  // The given hostname will be compared with the name(s) in the server's
  // certificate during the SSL handshake.
  SSLClientSocket(ClientSocket* transport_socket, const std::string& hostname);
  ~SSLClientSocket();

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback);
  virtual int ReconnectIgnoringLastError(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;

  // Socket methods:
  virtual int Read(char* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(const char* buf, int buf_len, CompletionCallback* callback);

  // Gets the SSL connection information of the socket.
  void GetSSLInfo(SSLInfo* ssl_info);

 private:
  void DoCallback(int result);
  void OnIOComplete(int result);

  int DoLoop(int last_io_result);
  int DoConnect();
  int DoConnectComplete(int result);
  int DoHandshakeRead();
  int DoHandshakeReadComplete(int result);
  int DoHandshakeWrite();
  int DoHandshakeWriteComplete(int result);
  int DoPayloadRead();
  int DoPayloadReadComplete(int result);
  int DoPayloadEncrypt();
  int DoPayloadWrite();
  int DoPayloadWriteComplete(int result);

  int DidCompleteHandshake();
  int VerifyServerCert();

  CompletionCallbackImpl<SSLClientSocket> io_callback_;
  scoped_ptr<ClientSocket> transport_;
  std::string hostname_;

  CompletionCallback* user_callback_;

  // Used by both Read and Write functions.
  char* user_buf_;
  int user_buf_len_;

  enum State {
    STATE_NONE,
    STATE_CONNECT,
    STATE_CONNECT_COMPLETE,
    STATE_HANDSHAKE_READ,
    STATE_HANDSHAKE_READ_COMPLETE,
    STATE_HANDSHAKE_WRITE,
    STATE_HANDSHAKE_WRITE_COMPLETE,
    STATE_PAYLOAD_ENCRYPT,
    STATE_PAYLOAD_WRITE,
    STATE_PAYLOAD_WRITE_COMPLETE,
    STATE_PAYLOAD_READ,
    STATE_PAYLOAD_READ_COMPLETE,
  };
  State next_state_;

  SecPkgContext_StreamSizes stream_sizes_;
  PCCERT_CONTEXT server_cert_;
  int server_cert_status_;

  CredHandle creds_;
  CtxtHandle ctxt_;
  SecBuffer send_buffer_;
  scoped_array<char> payload_send_buffer_;
  int payload_send_buffer_len_;
  int bytes_sent_;

  // recv_buffer_ holds the received ciphertext.  Since Schannel decrypts
  // data in place, sometimes recv_buffer_ may contain decrypted plaintext and
  // any undecrypted ciphertext.  (Ciphertext is decrypted one full SSL record
  // at a time.)
  //
  // If bytes_decrypted_ is 0, the received ciphertext is at the beginning of
  // recv_buffer_, ready to be passed to DecryptMessage.
  scoped_array<char> recv_buffer_;
  char* decrypted_ptr_;  // Points to the decrypted plaintext in recv_buffer_
  int bytes_decrypted_;  // The number of bytes of decrypted plaintext.
  char* received_ptr_;  // Points to the received ciphertext in recv_buffer_
  int bytes_received_;  // The number of bytes of received ciphertext.

  bool completed_handshake_;

  // Only used in the STATE_HANDSHAKE_READ_COMPLETE and
  // STATE_PAYLOAD_READ_COMPLETE states.  True if a 'result' argument of OK
  // should be ignored, to prevent it from being interpreted as EOF.
  //
  // The reason we need this flag is that OK means not only "0 bytes of data
  // were read" but also EOF.  We set ignore_ok_result_ to true when we need
  // to continue processing previously read data without reading more data.
  // We have to pass a 'result' of OK to the DoLoop method, and don't want it
  // to be interpreted as EOF.
  bool ignore_ok_result_;
};

}  // namespace net

#endif  // NET_BASE_SSL_CLIENT_SOCKET_H_

