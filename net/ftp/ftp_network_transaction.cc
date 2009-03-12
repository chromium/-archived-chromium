// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/ftp/ftp_network_transaction.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "net/base/client_socket.h"
#include "net/base/net_errors.h"
#include "net/ftp/ftp_network_session.h"

namespace net {

FtpNetworkTransaction::FtpNetworkTransaction(
    FtpNetworkSession* session, ClientSocketFactory* socket_factory)
  :  ALLOW_THIS_IN_INITIALIZER_LIST(io_callback_(this,
                                         &FtpNetworkTransaction::OnIOComplete)),
      user_callback_(NULL),
      session_(session),
      socket_factory_(socket_factory),
      next_state_(STATE_NONE) {
}

FtpNetworkTransaction::~FtpNetworkTransaction() {
}

void FtpNetworkTransaction::Destroy() {
  delete this;
}

int FtpNetworkTransaction::Start(
    const FtpRequestInfo* request_info, CompletionCallback* callback) {
  request_ = request_info;

  next_state_ = STATE_CTRL_INIT;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int FtpNetworkTransaction::RestartWithAuth(
    const std::wstring& username, const std::wstring& password,
    CompletionCallback* callback) {
  return ERR_FAILED;
}

int FtpNetworkTransaction::Read(
    char* buf, int buf_len, CompletionCallback* callback) {
  return ERR_FAILED;
}

const FtpResponseInfo* FtpNetworkTransaction::GetResponseInfo() const {
  return NULL;
}

LoadState FtpNetworkTransaction::GetLoadState() const {
  return LOAD_STATE_IDLE;
}

uint64 FtpNetworkTransaction::GetUploadProgress() const {
  return 0;
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
      case STATE_CTRL_WRITE:
        DCHECK(rv == OK);
        rv = DoCtrlWrite();
        break;
      case STATE_CTRL_WRITE_COMPLETE:
        rv = DoCtrlWriteComplete(rv);
        break;
      case STATE_CTRL_READ:
        DCHECK(rv == OK);
        rv = DoCtrlRead();
        break;
      case STATE_CTRL_READ_COMPLETE:
        rv = DoCtrlReadComplete(rv);
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

int FtpNetworkTransaction::DoCtrlInit() {
  return ERR_FAILED;  // TODO(darin): implement me
}

int FtpNetworkTransaction::DoCtrlInitComplete(int result) {
  return ERR_FAILED;  // TODO(darin): implement me
}

int FtpNetworkTransaction::DoCtrlResolveHost() {
  return ERR_FAILED;  // TODO(darin): implement me
}

int FtpNetworkTransaction::DoCtrlResolveHostComplete(int result) {
  return ERR_FAILED;  // TODO(darin): implement me
}

int FtpNetworkTransaction::DoCtrlConnect() {
  return ERR_FAILED;  // TODO(darin): implement me
}

int FtpNetworkTransaction::DoCtrlConnectComplete(int result) {
  return ERR_FAILED;  // TODO(darin): implement me
}

int FtpNetworkTransaction::DoCtrlWrite() {
  return ERR_FAILED;  // TODO(darin): implement me
}

int FtpNetworkTransaction::DoCtrlWriteComplete(int result) {
  return ERR_FAILED;  // TODO(darin): implement me
}

int FtpNetworkTransaction::DoCtrlRead() {
  return ERR_FAILED;  // TODO(darin): implement me
}

int FtpNetworkTransaction::DoCtrlReadComplete(int result) {
  return ERR_FAILED;  // TODO(darin): implement me
}

int FtpNetworkTransaction::DoDataConnect() {
  return ERR_FAILED;  // TODO(darin): implement me
}

int FtpNetworkTransaction::DoDataConnectComplete(int result) {
  return ERR_FAILED;  // TODO(darin): implement me
}

int FtpNetworkTransaction::DoDataRead() {
  return ERR_FAILED;  // TODO(darin): implement me
}

int FtpNetworkTransaction::DoDataReadComplete(int result) {
  return ERR_FAILED;  // TODO(darin): implement me
}

}  // namespace net
