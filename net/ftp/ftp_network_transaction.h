// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_NETWORK_TRANSACTION_H_
#define NET_FTP_FTP_NETWORK_TRANSACTION_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/ftp/ftp_response_info.h"
#include "net/ftp/ftp_transaction.h"

namespace net {

class ClientSocket;
class ClientSocketFactory;
class FtpNetworkSession;

class FtpNetworkTransaction : public FtpTransaction {
 public:
  FtpNetworkTransaction(
      FtpNetworkSession* session, ClientSocketFactory* socket_factory);
  ~FtpNetworkTransaction();

  // FtpTransactionFactory methods:
  virtual void Destroy();
  virtual int Start(
      const FtpRequestInfo* request_info, CompletionCallback* callback);
  virtual int RestartWithAuth(
      const std::wstring& username, const std::wstring& password,
      CompletionCallback* callback);
  virtual int Read(char* buf, int buf_len, CompletionCallback* callback);
  virtual const FtpResponseInfo* GetResponseInfo() const;
  virtual LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress() const;

 private:
  void DoCallback(int result);
  void OnIOComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  // Each of these methods corresponds to a State value.  Those with an input
  // argument receive the result from the previous state.  If a method returns
  // ERR_IO_PENDING, then the result from OnIOComplete will be passed to the
  // next state method as the result arg.
  int DoCtrlInit();
  int DoCtrlInitComplete(int result);
  int DoCtrlResolveHost();
  int DoCtrlResolveHostComplete(int result);
  int DoCtrlConnect();
  int DoCtrlConnectComplete(int result);
  int DoCtrlWrite();
  int DoCtrlWriteComplete(int result);
  int DoCtrlRead();
  int DoCtrlReadComplete(int result);
  int DoDataConnect();
  int DoDataConnectComplete(int result);
  int DoDataRead();
  int DoDataReadComplete(int result);

  CompletionCallbackImpl<FtpNetworkTransaction> io_callback_;
  CompletionCallback* user_callback_;

  scoped_refptr<FtpNetworkSession> session_;

  const FtpRequestInfo* request_;
  FtpResponseInfo response_;

  ClientSocketFactory* socket_factory_;
  scoped_ptr<ClientSocket> ctrl_socket_;
  scoped_ptr<ClientSocket> data_socket_;

  enum State {
    // Control connection states:
    STATE_CTRL_INIT,
    STATE_CTRL_INIT_COMPLETE,
    STATE_CTRL_RESOLVE_HOST,
    STATE_CTRL_RESOLVE_HOST_COMPLETE,
    STATE_CTRL_CONNECT,
    STATE_CTRL_CONNECT_COMPLETE,
    STATE_CTRL_WRITE,
    STATE_CTRL_WRITE_COMPLETE,
    STATE_CTRL_READ,
    STATE_CTRL_READ_COMPLETE,
    // Data connection states:
    STATE_DATA_CONNECT,
    STATE_DATA_CONNECT_COMPLETE,
    STATE_DATA_READ,
    STATE_DATA_READ_COMPLETE,
    STATE_NONE
  };
  State next_state_;
};

}  // namespace net

#endif  // NET_FTP_FTP_NETWORK_TRANSACTION_H_
