// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_NETWORK_TRANSACTION_H_
#define NET_FTP_FTP_NETWORK_TRANSACTION_H_

#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/ftp/ftp_response_info.h"
#include "net/ftp/ftp_transaction.h"

namespace net {

class ClientSocket;
class ClientSocketFactory;
class FtpNetworkSession;

class FtpNetworkTransaction : public FtpTransaction {
 public:
  FtpNetworkTransaction(FtpNetworkSession* session,
                        ClientSocketFactory* socket_factory);
  virtual ~FtpNetworkTransaction();

  // FtpTransaction methods:
  virtual int Start(const FtpRequestInfo* request_info,
                    CompletionCallback* callback);
  virtual int Stop(int error);
  virtual int RestartWithAuth(const std::wstring& username,
                              const std::wstring& password,
                              CompletionCallback* callback);
  virtual int RestartIgnoringLastError(CompletionCallback* callback);
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual const FtpResponseInfo* GetResponseInfo() const;
  virtual LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress() const;

 private:
  enum Command {
    COMMAND_NONE,
    COMMAND_USER,
    COMMAND_PASS,
    COMMAND_ACCT,
    COMMAND_SYST,
    COMMAND_TYPE,
    COMMAND_PASV,
    COMMAND_PWD,
    COMMAND_SIZE,
    COMMAND_RETR,
    COMMAND_CWD,
    COMMAND_LIST,
    COMMAND_MDTM,
    COMMAND_QUIT
  };

  enum ErrorClass {
    ERROR_CLASS_INITIATED = 1,  // The requested action was initiated.
    ERROR_CLASS_OK,             // The requested action successfully completed.
    ERROR_CLASS_PENDING,        // The command accepted, but the
                                // request on hold.
    ERROR_CLASS_ERROR_RETRY,    // The command was not accepted and the
                                // requested action did not take place,
                                // but the error condition is temporary and the
                                // action may be requested again.
    ERROR_CLASS_ERROR,          // The command was not accepted and
                                // the requested action did not take place.
  };

  void DoCallback(int result);
  void OnIOComplete(int result);
  int GetRespnseCode();
  int ProcessResponse(int response_code);
  int ParsePasvResponse();

  int SendFtpCommand(const std::string& command, Command cmd);

  // TODO(ibrar): Use C++ static_cast.
  ErrorClass GetErrorClass(int response_code) {
    return (ErrorClass)(response_code / 100);
  }

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
  int DoCtrlRead();
  int DoCtrlReadComplete(int result);
  int DoCtrlWriteUSER();
  int ProcessResponseUSER(int response_code);
  int DoCtrlWritePASS();
  int ProcessResponsePASS(int response_code);
  int DoCtrlWriteACCT();
  int ProcessResponseACCT(int response_code);
  int DoCtrlWriteSYST();
  int ProcessResponseSYST(int response_code);
  int DoCtrlWritePWD();
  int ProcessResponsePWD(int response_code);
  int DoCtrlWriteTYPE();
  int ProcessResponseTYPE(int response_code);
  int DoCtrlWritePASV();
  int ProcessResponsePASV(int response_code);
  int DoCtrlWriteRETR();
  int ProcessResponseRETR(int response_code);
  int DoCtrlWriteSIZE();
  int ProcessResponseSIZE(int response_code);
  int DoCtrlWriteCWD();
  int ProcessResponseCWD(int response_code);
  int DoCtrlWriteLIST();
  int ProcessResponseLIST(int response_code);
  int DoCtrlWriteMDTM();
  int ProcessResponseMDTM(int response_code);
  int DoCtrlWriteQUIT();
  int ProcessResponseQUIT(int response_code);

  int DoDataResolveHost();
  int DoDataResolveHostComplete(int result);
  int DoDataConnect();
  int DoDataConnectComplete(int result);
  int DoDataRead();
  int DoDataReadComplete(int result);

  Command command_sent_;

  CompletionCallbackImpl<FtpNetworkTransaction> io_callback_;
  CompletionCallback* user_callback_;

  scoped_refptr<FtpNetworkSession> session_;

  const FtpRequestInfo* request_;
  FtpResponseInfo response_;

  HostResolver resolver_;
  AddressList addresses_;

  // User buffer and length passed to the Read method.
  scoped_refptr<IOBuffer> read_ctrl_buf_;
  int read_ctrl_buf_size_;

  scoped_refptr<IOBuffer> response_message_buf_;
  int response_message_buf_len_;

  scoped_refptr<IOBuffer> read_data_buf_;
  int read_data_buf_len_;
  int file_data_len_;

  scoped_refptr<IOBuffer> write_buf_;

  int last_error_;

  bool is_anonymous_;
  bool retr_failed_;

  std::string data_connection_ip_;
  int data_connection_port_;

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
    STATE_CTRL_READ,
    STATE_CTRL_READ_COMPLETE,
    STATE_CTRL_WRITE_USER,
    STATE_CTRL_WRITE_PASS,
    STATE_CTRL_WRITE_ACCT,
    STATE_CTRL_WRITE_SYST,
    STATE_CTRL_WRITE_TYPE,
    STATE_CTRL_WRITE_PASV,
    STATE_CTRL_WRITE_PWD,
    STATE_CTRL_WRITE_RETR,
    STATE_CTRL_WRITE_SIZE,
    STATE_CTRL_WRITE_CWD,
    STATE_CTRL_WRITE_LIST,
    STATE_CTRL_WRITE_MDTM,
    STATE_CTRL_WRITE_QUIT,
    // Data connection states:
    STATE_DATA_RESOLVE_HOST,
    STATE_DATA_RESOLVE_HOST_COMPLETE,
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
