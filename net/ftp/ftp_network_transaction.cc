// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/ftp/ftp_network_transaction.h"

#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/net_errors.h"
#include "net/ftp/ftp_network_session.h"
#include "net/ftp/ftp_request_info.h"
#include "net/socket/client_socket.h"
#include "net/socket/client_socket_factory.h"

// TODO(ibrar): Try to avoid sscanf.
#if !defined(COMPILER_MSVC)
#define sscanf_s sscanf
#endif

const char kCRLF[] = "\r\n";

const int kCtrlBufLen = 1024;

namespace net {

FtpNetworkTransaction::FtpNetworkTransaction(
    FtpNetworkSession* session,
    ClientSocketFactory* socket_factory)
    : command_sent_(COMMAND_NONE),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &FtpNetworkTransaction::OnIOComplete)),
      user_callback_(NULL),
      session_(session),
      request_(NULL),
      resolver_(session->host_resolver()),
      read_ctrl_buf_size_(kCtrlBufLen),
      response_message_buf_len_(0),
      read_data_buf_len_(0),
      file_data_len_(0),
      last_error_(OK),
      is_anonymous_(false),
      retr_failed_(false),
      data_connection_port_(0),
      socket_factory_(socket_factory),
      next_state_(STATE_NONE) {
  read_ctrl_buf_ = new IOBuffer(kCtrlBufLen);
  response_message_buf_ = new IOBuffer(kCtrlBufLen);
}

FtpNetworkTransaction::~FtpNetworkTransaction() {
}

int FtpNetworkTransaction::Start(const FtpRequestInfo* request_info,
                                 CompletionCallback* callback) {
  request_ = request_info;

  next_state_ = STATE_CTRL_INIT;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int FtpNetworkTransaction::Stop(int error) {
  if (command_sent_ == COMMAND_QUIT)
    return error;

  next_state_ = STATE_CTRL_WRITE_QUIT;
  last_error_ = error;
  return OK;
}

int FtpNetworkTransaction::RestartWithAuth(const std::wstring& username,
                                           const std::wstring& password,
                                           CompletionCallback* callback) {
  return ERR_FAILED;
}

int FtpNetworkTransaction::RestartIgnoringLastError(
    CompletionCallback* callback) {
  return ERR_FAILED;
}

int FtpNetworkTransaction::Read(IOBuffer* buf,
                                int buf_len,
                                CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK(buf_len > 0);

  if (data_socket_ == NULL)
    return 0;  // Data socket closed, no more data left.

  if (!data_socket_->IsConnected())
    return 0;  // Data socket disconnected, no more data left.

  read_data_buf_ = buf;
  read_data_buf_len_ = buf_len;

  next_state_ = STATE_DATA_READ;

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  else if (rv == 0)
    data_socket_->Disconnect();
  return rv;
}

const FtpResponseInfo* FtpNetworkTransaction::GetResponseInfo() const {
  return &response_;
}

LoadState FtpNetworkTransaction::GetLoadState() const {
  return LOAD_STATE_IDLE;
}

uint64 FtpNetworkTransaction::GetUploadProgress() const {
  return 0;
}

// Used to prepare and send FTP commad.
int FtpNetworkTransaction::SendFtpCommand(const std::string& command,
                                          Command cmd) {
  response_message_buf_len_ = 0;
  command_sent_ = cmd;
  DLOG(INFO) << " >> " << command;
  const char* buf = command.c_str();
  int buf_len = command.size();
  DCHECK(!write_buf_);
  write_buf_ = new IOBuffer(buf_len + 2);
  memcpy(write_buf_->data(), buf, buf_len);
  memcpy(write_buf_->data() + buf_len, kCRLF, 2);
  buf_len += 2;

  return ctrl_socket_->Write(write_buf_, buf_len, &io_callback_);
}

int FtpNetworkTransaction::GetRespnseCode() {
  std::string str(response_message_buf_->data(), 3);
  return StringToInt(str);
}

int FtpNetworkTransaction::ProcessResponse(int response_code) {
  int rv = OK;
  switch (command_sent_) {
    case COMMAND_NONE:
      next_state_ = STATE_CTRL_WRITE_USER;
      break;
    case COMMAND_USER:
      rv = ProcessResponseUSER(response_code);
      break;
    case COMMAND_PASS:
      rv = ProcessResponsePASS(response_code);
      break;
    case COMMAND_ACCT:
      rv = ProcessResponseACCT(response_code);
      break;
    case COMMAND_SYST:
      rv = ProcessResponseSYST(response_code);
      break;
    case COMMAND_PWD:
      rv = ProcessResponsePWD(response_code);
      break;
    case COMMAND_TYPE:
      rv = ProcessResponseTYPE(response_code);
      break;
    case COMMAND_PASV:
      rv = ProcessResponsePASV(response_code);
      break;
    case COMMAND_SIZE:
      rv = ProcessResponseSIZE(response_code);
      break;
    case COMMAND_RETR:
      rv = ProcessResponseRETR(response_code);
      break;
    case COMMAND_CWD:
      rv = ProcessResponseCWD(response_code);
      break;
    case COMMAND_LIST:
      rv = ProcessResponseLIST(response_code);
      break;
    case COMMAND_MDTM:
      rv = ProcessResponseMDTM(response_code);
      break;
    case COMMAND_QUIT:
      rv = ProcessResponseQUIT(response_code);
      break;
    default:
      DLOG(INFO) << "Missing Command response handling!";
      return ERR_FAILED;
  }
  return rv;
}

void FtpNetworkTransaction::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_callback_);

  // Since Run may result in Read being called, clear callback_ up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
}

void FtpNetworkTransaction::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int FtpNetworkTransaction::DoLoop(int result) {
  DCHECK(next_state_ != STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_CTRL_INIT:
        DCHECK(rv == OK);
        rv = DoCtrlInit();
        break;
      case STATE_CTRL_INIT_COMPLETE:
        rv = DoCtrlInitComplete(rv);
        break;
      case STATE_CTRL_RESOLVE_HOST:
        DCHECK(rv == OK);
        rv = DoCtrlResolveHost();
        break;
      case STATE_CTRL_RESOLVE_HOST_COMPLETE:
        rv = DoCtrlResolveHostComplete(rv);
        break;
      case STATE_CTRL_CONNECT:
        DCHECK(rv == OK);
        rv = DoCtrlConnect();
        break;
      case STATE_CTRL_CONNECT_COMPLETE:
        rv = DoCtrlConnectComplete(rv);
        break;
      case STATE_CTRL_READ:
        DCHECK(rv == OK);
        rv = DoCtrlRead();
        break;
      case STATE_CTRL_READ_COMPLETE:
        rv = DoCtrlReadComplete(rv);
        break;
      case STATE_CTRL_WRITE_USER:
        DCHECK(rv == OK);
        rv = DoCtrlWriteUSER();
        break;
      case STATE_CTRL_WRITE_PASS:
        DCHECK(rv == OK);
        rv = DoCtrlWritePASS();
        break;
      case STATE_CTRL_WRITE_SYST:
        DCHECK(rv == OK);
        rv = DoCtrlWriteSYST();
        break;
      case STATE_CTRL_WRITE_ACCT:
        DCHECK(rv == OK);
        rv = DoCtrlWriteACCT();
        break;
      case STATE_CTRL_WRITE_PWD:
        DCHECK(rv == OK);
        rv = DoCtrlWritePWD();
        break;
      case STATE_CTRL_WRITE_TYPE:
        DCHECK(rv == OK);
        rv = DoCtrlWriteTYPE();
        break;
      case STATE_CTRL_WRITE_PASV:
        DCHECK(rv == OK);
        rv = DoCtrlWritePASV();
        break;
      case STATE_CTRL_WRITE_RETR:
        DCHECK(rv == OK);
        rv = DoCtrlWriteRETR();
        break;
      case STATE_CTRL_WRITE_SIZE:
        DCHECK(rv == OK);
        rv = DoCtrlWriteSIZE();
        break;
      case STATE_CTRL_WRITE_CWD:
        DCHECK(rv == OK);
        rv = DoCtrlWriteCWD();
        break;
      case STATE_CTRL_WRITE_LIST:
        DCHECK(rv == OK);
        rv = DoCtrlWriteLIST();
        break;
      case STATE_CTRL_WRITE_MDTM:
        DCHECK(rv == OK);
        rv = DoCtrlWriteMDTM();
        break;
      case STATE_CTRL_WRITE_QUIT:
        DCHECK(rv == OK);
        rv = DoCtrlWriteQUIT();
        break;

      case STATE_DATA_RESOLVE_HOST:
        DCHECK(rv == OK);
        rv = DoDataResolveHost();
        break;
      case STATE_DATA_RESOLVE_HOST_COMPLETE:
        rv = DoDataResolveHostComplete(rv);
        break;
      case STATE_DATA_CONNECT:
        DCHECK(rv == OK);
        rv = DoDataConnect();
        break;
      case STATE_DATA_CONNECT_COMPLETE:
        rv = DoDataConnectComplete(rv);
        break;
      case STATE_DATA_READ:
        DCHECK(rv == OK);
        rv = DoDataRead();
        break;
      case STATE_DATA_READ_COMPLETE:
        rv = DoDataReadComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

// TODO(ibrar): Yet to see if we need any intialization
int FtpNetworkTransaction::DoCtrlInit() {
  next_state_ = STATE_CTRL_INIT_COMPLETE;
  return OK;
}

int FtpNetworkTransaction::DoCtrlInitComplete(int result) {
  next_state_ = STATE_CTRL_RESOLVE_HOST;
  return OK;
}

int FtpNetworkTransaction::DoCtrlResolveHost() {
  next_state_ = STATE_CTRL_RESOLVE_HOST_COMPLETE;

  std::string host;
  int port;

  host = request_->url.host();
  port = request_->url.EffectiveIntPort();

  HostResolver::RequestInfo info(host, port);
  // No known referrer.
  return resolver_.Resolve(info, &addresses_, &io_callback_);
}

int FtpNetworkTransaction::DoCtrlResolveHostComplete(int result) {
  bool ok = (result == OK);
  if (ok) {
    next_state_ = STATE_CTRL_CONNECT;
    return result;
  }
  return ERR_FAILED;
}

int FtpNetworkTransaction::DoCtrlConnect() {
  next_state_ = STATE_CTRL_CONNECT_COMPLETE;
  ctrl_socket_.reset(socket_factory_->CreateTCPClientSocket(addresses_));
  return ctrl_socket_->Connect(&io_callback_);
}

int FtpNetworkTransaction::DoCtrlConnectComplete(int result) {
  if (result == OK)
    next_state_ = STATE_CTRL_READ;
  return result;
}

int FtpNetworkTransaction::DoCtrlRead() {
  if (write_buf_)  // Clear the write buffer
    write_buf_ = NULL;

  next_state_ = STATE_CTRL_READ_COMPLETE;
  read_ctrl_buf_->data()[0] = 0;
  return ctrl_socket_->Read(read_ctrl_buf_, read_ctrl_buf_size_ - 1,
                            &io_callback_);
}

int FtpNetworkTransaction::DoCtrlReadComplete(int result) {
  if (result < 0)
    return Stop(ERR_FAILED);
  int response_code;
  // Null termination added, now we can treat this as string.
  read_ctrl_buf_->data()[result] = 0;
  memcpy(response_message_buf_->data() + response_message_buf_len_,
         read_ctrl_buf_->data(), result);

  response_message_buf_len_ = response_message_buf_len_ + result;
  for (int i = 0; i < response_message_buf_len_; i++) {
    if (response_message_buf_->data()[i] == '\r' &&
        response_message_buf_->data()[i + 1] == '\n') {
      if (response_message_buf_len_ > 3 &&
          response_message_buf_->data()[3] == ' ') {
        response_message_buf_->data()[response_message_buf_len_ - 2] = 0;
        response_code = GetRespnseCode();
        return ProcessResponse(response_code);
      }
      response_message_buf_len_ -= (i + 2);
      memcpy(response_message_buf_->data(),
             response_message_buf_->data() + i + 2,
             response_message_buf_len_);
      i = 0;
    }
  }
  next_state_ = STATE_CTRL_READ;
  return OK;
}

// FTP Commands and responses

// USER Command.
int FtpNetworkTransaction::DoCtrlWriteUSER() {
  std::string command = "USER";
  if (request_->url.has_username()) {
    command.append(" ");
    command.append(request_->url.username());
  } else {
    is_anonymous_ = true;
    command.append(" anonymous");
  }
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, COMMAND_USER);
}

int FtpNetworkTransaction::ProcessResponseUSER(int response_code) {
  switch (GetErrorClass(response_code)) {
    case ERROR_CLASS_OK:
      next_state_ = STATE_CTRL_WRITE_SYST;
      break;
    case ERROR_CLASS_PENDING:
      next_state_ = STATE_CTRL_WRITE_PASS;
      break;
    case ERROR_CLASS_ERROR_RETRY:
      if (response_code == 421)
        return Stop(ERR_FAILED);
      break;
    case ERROR_CLASS_ERROR:
      return Stop(ERR_FAILED);
    default:
      return Stop(ERR_FAILED);
  }
  return OK;
}

// PASS command.
int FtpNetworkTransaction::DoCtrlWritePASS() {
  std::string command = "PASS";
  if (request_->url.has_password()) {
    command.append(" ");
    command.append(request_->url.password());
  } else {
    command.append(" ");
    command.append("chrome@example.com");
  }
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, COMMAND_PASS);
}

int FtpNetworkTransaction::ProcessResponsePASS(int response_code) {
  switch (GetErrorClass(response_code)) {
    case ERROR_CLASS_OK:
      next_state_ = STATE_CTRL_WRITE_SYST;
      break;
    case ERROR_CLASS_PENDING:
      next_state_ = STATE_CTRL_WRITE_ACCT;
      break;
    case ERROR_CLASS_ERROR_RETRY:
      if (response_code == 421) {
        // TODO(ibrar): Retry here.
      }
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR:
      if (response_code == 503) {
        next_state_ = STATE_CTRL_WRITE_USER;
      } else {
        // TODO(ibrar): Retry here.
        return Stop(ERR_FAILED);
      }
      break;
    default:
      return Stop(ERR_FAILED);
  }
  return OK;
}

// SYST command.
int FtpNetworkTransaction::DoCtrlWriteSYST() {
  std::string command = "SYST";
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, COMMAND_SYST);
}

int FtpNetworkTransaction::ProcessResponseSYST(int response_code) {
  switch (GetErrorClass(response_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_OK:
      // TODO(ibrar): Process SYST response properly.
      next_state_ = STATE_CTRL_WRITE_PWD;
      break;
    case ERROR_CLASS_PENDING:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR_RETRY:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR:
      // Server does not recognize the SYST command so proceed.
      next_state_ = STATE_CTRL_WRITE_PWD;
      break;
    default:
      return Stop(ERR_FAILED);
  }
  return OK;
}

// PWD command.
int FtpNetworkTransaction::DoCtrlWritePWD() {
  std::string command = "PWD";
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, COMMAND_PWD);
}

int FtpNetworkTransaction::ProcessResponsePWD(int response_code) {
  switch (GetErrorClass(response_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_OK:
      next_state_ = STATE_CTRL_WRITE_TYPE;
      break;
    case ERROR_CLASS_PENDING:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR_RETRY:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR:
      return Stop(ERR_FAILED);
    default:
      return Stop(ERR_FAILED);
  }
  return OK;
}

// TYPE command.
int FtpNetworkTransaction::DoCtrlWriteTYPE() {
  std::string command = "TYPE I";
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, COMMAND_TYPE);
}

int FtpNetworkTransaction::ProcessResponseTYPE(int response_code) {
  switch (GetErrorClass(response_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_OK:
      next_state_ = STATE_CTRL_WRITE_PASV;
      break;
    case ERROR_CLASS_PENDING:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR_RETRY:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR:
      return Stop(ERR_FAILED);
    default:
      return Stop(ERR_FAILED);
  }
  return OK;
}

// ACCT command.
int FtpNetworkTransaction::DoCtrlWriteACCT() {
  std::string command = "ACCT noaccount";
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, COMMAND_ACCT);
}

int FtpNetworkTransaction::ProcessResponseACCT(int response_code) {
  switch (GetErrorClass(response_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_OK:
      next_state_ = STATE_CTRL_WRITE_SYST;
      break;
    case ERROR_CLASS_PENDING:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR_RETRY:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR:
      return Stop(ERR_FAILED);
    default:
      return Stop(ERR_FAILED);
  }
  return OK;
}

// PASV command
int FtpNetworkTransaction::DoCtrlWritePASV() {
  std::string command = "PASV";
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, COMMAND_PASV);
}

// There are two way we can receive IP address and port.
// (127,0,0,1,23,21) IP address and port encapsulate in ().
// 127,0,0,1,23,21  IP address and port without ().
int FtpNetworkTransaction::ProcessResponsePASV(int response_code) {
  switch (GetErrorClass(response_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_OK:
      char* ptr;
      int i0, i1, i2, i3, p0, p1;
      ptr = read_ctrl_buf_->data();  // Try with bracket.
      while (*ptr && *ptr != '(')
        ++ptr;
      if (*ptr) {
        ++ptr;
      } else {
        ptr = read_ctrl_buf_->data();  // Try without bracket.
        while (*ptr && *ptr != ',')
          ++ptr;
        while (*ptr && *ptr != ' ')
          --ptr;
      }
      if (sscanf_s(ptr, "%d,%d,%d,%d,%d,%d",
                   &i0, &i1, &i2, &i3, &p0, &p1) == 6) {
        data_connection_ip_ = StringPrintf("%d.%d.%d.%d", i0, i1, i2, i3);
        data_connection_port_ = (p0 << 8) + p1;
        next_state_ = STATE_DATA_RESOLVE_HOST;
      } else {
        return Stop(ERR_FAILED);
      }
      break;
    case ERROR_CLASS_PENDING:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR_RETRY:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR:
      return Stop(ERR_FAILED);
    default:
      return Stop(ERR_FAILED);
  }
  return OK;
}

// SIZE command
int FtpNetworkTransaction::DoCtrlWriteSIZE() {
  std::string command = "SIZE";
  if (request_->url.has_path()) {
    command.append(" ");
    command.append(request_->url.path());
  }
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, COMMAND_SIZE);
}

int FtpNetworkTransaction::ProcessResponseSIZE(int response_code) {
  switch (GetErrorClass(response_code)) {
    case ERROR_CLASS_INITIATED:
      break;
    case ERROR_CLASS_OK:
      // Remove CR, CRLF from read_ctrl_buf_.
      for (char* ptr = read_ctrl_buf_->data(); *ptr != '\0'; ptr++) {
        if ((*ptr == '\r') || (*ptr == '\n')) {
          // Stop if '\n' or '\r' detected.
          *ptr = '\0';
          break;
        }
      }
      if (!StringToInt(read_ctrl_buf_->data() + 4, &file_data_len_))
        return Stop(ERR_FAILED);
      break;
    case ERROR_CLASS_PENDING:
      break;
    case ERROR_CLASS_ERROR_RETRY:
      break;
    case ERROR_CLASS_ERROR:
      break;
    default:
      return Stop(ERR_FAILED);
  }
  next_state_ = STATE_CTRL_WRITE_MDTM;
  return OK;
}

// RETR command
int FtpNetworkTransaction::DoCtrlWriteRETR() {
  std::string command = "RETR";
  if (request_->url.has_path()) {
    command.append(" ");
    command.append(request_->url.path());
  } else {
    command.append(" /");
  }
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, COMMAND_RETR);
}

int FtpNetworkTransaction::ProcessResponseRETR(int response_code) {
  switch (GetErrorClass(response_code)) {
    case ERROR_CLASS_INITIATED:
      break;
    case ERROR_CLASS_OK:
      next_state_ = STATE_CTRL_WRITE_QUIT;
      break;
    case ERROR_CLASS_PENDING:
      next_state_ = STATE_CTRL_WRITE_PASV;
      break;
    case ERROR_CLASS_ERROR_RETRY:
      if (response_code == 421 || response_code == 425 || response_code == 426)
        return Stop(ERR_FAILED);
      return ERR_FAILED;  // TODO(ibrar): Retry here.
    case ERROR_CLASS_ERROR:
      if (retr_failed_)
        return Stop(ERR_FAILED);
      retr_failed_ = true;
      next_state_ = STATE_CTRL_WRITE_PASV;
      break;
    default:
      return Stop(ERR_FAILED);
  }
  return OK;
}

// MDMT command
int FtpNetworkTransaction::DoCtrlWriteMDTM() {
  std::string command = "MDTM";
  if (request_->url.has_path()) {
    command.append(" ");
    command.append(request_->url.path());
  } else {
    command.append(" /");
  }
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, COMMAND_MDTM);
}

int FtpNetworkTransaction::ProcessResponseMDTM(int response_code) {
  switch (GetErrorClass(response_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_OK:
      next_state_ = STATE_CTRL_WRITE_RETR;
      break;
    case ERROR_CLASS_PENDING:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR_RETRY:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR:
      next_state_ = STATE_CTRL_WRITE_RETR;
      break;
    default:
      return Stop(ERR_FAILED);
  }
  return OK;
}


// CWD command
int FtpNetworkTransaction::DoCtrlWriteCWD() {
  std::string command = "CWD";
  if (request_->url.has_path()) {
    command.append(" ");
    command.append(request_->url.path());
  } else {
    command.append(" /");
  }
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, COMMAND_CWD);
}

int FtpNetworkTransaction::ProcessResponseCWD(int response_code) {
  switch (GetErrorClass(response_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_OK:
      next_state_ = STATE_CTRL_WRITE_LIST;
      break;
    case ERROR_CLASS_PENDING:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR_RETRY:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR:
      return Stop(ERR_FAILED);
    default:
      return Stop(ERR_FAILED);
  }
  return OK;
}

// LIST command
int FtpNetworkTransaction::DoCtrlWriteLIST() {
  std::string command = "LIST";
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, COMMAND_LIST);
}

int FtpNetworkTransaction::ProcessResponseLIST(int response_code) {
  switch (GetErrorClass(response_code)) {
    case ERROR_CLASS_INITIATED:
      response_message_buf_len_ = 0;  // Clear the responce buffer.
      next_state_ = STATE_CTRL_READ;
      break;
    case ERROR_CLASS_OK:
      response_.is_directory_listing = true;
      next_state_ = STATE_CTRL_WRITE_QUIT;
      break;
    case ERROR_CLASS_PENDING:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR_RETRY:
      return Stop(ERR_FAILED);
    case ERROR_CLASS_ERROR:
      return Stop(ERR_FAILED);
    default:
      return Stop(ERR_FAILED);
  }
  return OK;
}

// Quit command
int FtpNetworkTransaction::DoCtrlWriteQUIT() {
  std::string command = "QUIT";
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, COMMAND_QUIT);
}

int FtpNetworkTransaction::ProcessResponseQUIT(int response_code) {
  ctrl_socket_->Disconnect();
  return last_error_;
}

// Data Connection

int FtpNetworkTransaction::DoDataResolveHost() {
  if (data_socket_ != NULL && data_socket_->IsConnected())
    data_socket_->Disconnect();

  next_state_ = STATE_DATA_RESOLVE_HOST_COMPLETE;

  HostResolver::RequestInfo info(data_connection_ip_,
                                 data_connection_port_);
  // No known referrer.
  return resolver_.Resolve(info, &addresses_, &io_callback_);
}

int FtpNetworkTransaction::DoDataResolveHostComplete(int result) {
  bool ok = (result == OK);
  if (ok) {
    next_state_ = STATE_DATA_CONNECT;
    return result;
  }
  return ERR_FAILED;
}

int FtpNetworkTransaction::DoDataConnect() {
  next_state_ = STATE_DATA_CONNECT_COMPLETE;
  data_socket_.reset(socket_factory_->CreateTCPClientSocket(addresses_));
  return data_socket_->Connect(&io_callback_);
}

int FtpNetworkTransaction::DoDataConnectComplete(int result) {
  if (retr_failed_) {
    next_state_ = STATE_CTRL_WRITE_CWD;
  } else {
    next_state_ = STATE_CTRL_WRITE_SIZE;
  }
  return OK;
}

int FtpNetworkTransaction::DoDataRead() {
  DCHECK(read_data_buf_);
  DCHECK(read_data_buf_len_ > 0);

  next_state_ = STATE_DATA_READ_COMPLETE;
  read_data_buf_->data()[0] = 0;
  return data_socket_->Read(read_data_buf_, read_data_buf_len_,
                            &io_callback_);
}

int FtpNetworkTransaction::DoDataReadComplete(int result) {
  DLOG(INFO) << read_data_buf_->data();  // The read_data_buf_ is NULL
                                         // terminated string.
  return result;
}

}  // namespace net
