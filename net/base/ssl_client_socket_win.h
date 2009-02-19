// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CLIENT_SOCKET_WIN_H_
#define NET_BASE_SSL_CLIENT_SOCKET_WIN_H_

#define SECURITY_WIN32  // Needs to be defined before including security.h

#include <windows.h>
#include <wincrypt.h>
#include <security.h>

#include <string>

#include "base/scoped_ptr.h"
#include "net/base/cert_verifier.h"
#include "net/base/cert_verify_result.h"
#include "net/base/completion_callback.h"
#include "net/base/ssl_client_socket.h"
#include "net/base/ssl_config_service.h"

namespace net {

// An SSL client socket implemented with the Windows Schannel.
class SSLClientSocketWin : public SSLClientSocket {
 public:
  // Takes ownership of the transport_socket, which may already be connected.
  // The given hostname will be compared with the name(s) in the server's
  // certificate during the SSL handshake.  ssl_config specifies the SSL
  // settings.
  SSLClientSocketWin(ClientSocket* transport_socket,
                     const std::string& hostname,
                     const SSLConfig& ssl_config);
  ~SSLClientSocketWin();

  // SSLClientSocket methods:
  virtual void GetSSLInfo(SSLInfo* ssl_info);

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback);
  virtual int ReconnectIgnoringLastError(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;

  // Socket methods:
  virtual int Read(char* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(const char* buf, int buf_len, CompletionCallback* callback);

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
  int DoVerifyCert();
  int DoVerifyCertComplete(int result);
  int DoPayloadRead();
  int DoPayloadReadComplete(int result);
  int DoPayloadEncrypt();
  int DoPayloadWrite();
  int DoPayloadWriteComplete(int result);

  int DidCompleteHandshake();
  void LogConnectionTypeMetrics() const;

  CompletionCallbackImpl<SSLClientSocketWin> io_callback_;
  scoped_ptr<ClientSocket> transport_;
  std::string hostname_;
  SSLConfig ssl_config_;

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
    STATE_VERIFY_CERT,
    STATE_VERIFY_CERT_COMPLETE,
    STATE_PAYLOAD_ENCRYPT,
    STATE_PAYLOAD_WRITE,
    STATE_PAYLOAD_WRITE_COMPLETE,
    STATE_PAYLOAD_READ,
    STATE_PAYLOAD_READ_COMPLETE,
  };
  State next_state_;

  SecPkgContext_StreamSizes stream_sizes_;
  scoped_refptr<X509Certificate> server_cert_;
  CertVerifier verifier_;
  CertVerifyResult server_cert_verify_result_;

  CredHandle* creds_;
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
  bool complete_handshake_on_write_complete_;

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

  // True if the user has no client certificate.
  bool no_client_cert_;
};

}  // namespace net

#endif  // NET_BASE_SSL_CLIENT_SOCKET_WIN_H_

