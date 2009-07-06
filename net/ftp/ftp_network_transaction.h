// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_NETWORK_TRANSACTION_H_
#define NET_FTP_FTP_NETWORK_TRANSACTION_H_

#include <string>
#include <queue>
#include <utility>

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

  struct ResponseLine {
    ResponseLine(int code, const std::string& text) : code(code), text(text) {
    }

    int code;  // Three-digit status code.
    std::string text;  // Text after the code, without ending CRLF.
  };

  void DoCallback(int result);
  void OnIOComplete(int result);

  // Executes correct ProcessResponse + command_name function based on last
  // issued command. Returns error code.
  int ProcessCtrlResponses();

  // Parses as much as possible from response_message_buf_. Puts index of the
  // first unparsed character in cut_pos. Returns error code.
  int ParseCtrlResponse(int* cut_pos);

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
  int ProcessResponseUSER(const ResponseLine& response);
  int DoCtrlWritePASS();
  int ProcessResponsePASS(const ResponseLine& response);
  int DoCtrlWriteACCT();
  int ProcessResponseACCT(const ResponseLine& response);
  int DoCtrlWriteSYST();
  int ProcessResponseSYST(const ResponseLine& response);
  int DoCtrlWritePWD();
  int ProcessResponsePWD(const ResponseLine& response);
  int DoCtrlWriteTYPE();
  int ProcessResponseTYPE(const ResponseLine& response);
  int DoCtrlWritePASV();
  int ProcessResponsePASV(const ResponseLine& response);
  int DoCtrlWriteRETR();
  int ProcessResponseRETR(const ResponseLine& response);
  int DoCtrlWriteSIZE();
  int ProcessResponseSIZE(const ResponseLine& response);
  int DoCtrlWriteCWD();
  int ProcessResponseCWD(const ResponseLine& response);
  int DoCtrlWriteLIST();
  int ProcessResponseLIST(const ResponseLine& response);
  int DoCtrlWriteMDTM();
  int ProcessResponseMDTM(const ResponseLine& response);
  int DoCtrlWriteQUIT();
  int ProcessResponseQUIT(const ResponseLine& response);

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

  // Cancels the outstanding request on destruction.
  SingleRequestHostResolver resolver_;
  AddressList addresses_;

  // As we read full response lines, we parse them and add to the queue.
  std::queue<ResponseLine> ctrl_responses_;

  // Buffer holding not-yet-parsed control socket responses.
  scoped_refptr<IOBufferWithSize> response_message_buf_;
  int response_message_buf_len_;

  // User buffer passed to the Read method. It actually writes to the
  // response_message_buf_ at correct offset.
  scoped_refptr<ReusedIOBuffer> read_ctrl_buf_;

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
