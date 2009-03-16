// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_new_ftp_job.h"

#include "base/file_version_info.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"


URLRequestNewFtpJob::URLRequestNewFtpJob(URLRequest* request)
    : URLRequestJob(request),
      server_auth_state_(net::AUTH_STATE_DONT_NEED_AUTH),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          start_callback_(this, &URLRequestNewFtpJob::OnStartCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &URLRequestNewFtpJob::OnReadCompleted)),
      read_in_progress_(false),
      context_(request->context()) {
}

URLRequestNewFtpJob::~URLRequestNewFtpJob() {
}

// static
URLRequestJob* URLRequestNewFtpJob::Factory(URLRequest* request,
                                            const std::string& scheme) {
  DCHECK(scheme == "ftp");

  if (request->url().has_port() &&
      !net::IsPortAllowedByFtp(request->url().IntPort()))
    return new URLRequestErrorJob(request, net::ERR_UNSAFE_PORT);

  DCHECK(request->context());
  DCHECK(request->context()->ftp_transaction_factory());
  return new URLRequestNewFtpJob(request);
}

void URLRequestNewFtpJob::Start() {
  NOTIMPLEMENTED();
}

void URLRequestNewFtpJob::Kill() {
  NOTIMPLEMENTED();
}

uint64 URLRequestNewFtpJob::GetUploadProgress() const {
  NOTIMPLEMENTED();
  return 0;
}

void URLRequestNewFtpJob::GetResponseInfo() {
  NOTIMPLEMENTED();
}

int URLRequestNewFtpJob::GetResponseCode() {
  NOTIMPLEMENTED();
  return -1;
}

bool URLRequestNewFtpJob::GetMoreData() {
  NOTIMPLEMENTED();
  return false;
}

bool URLRequestNewFtpJob::ReadRawData(net::IOBuffer* buf,
                                      int buf_size,
                                      int *bytes_read) {
  NOTIMPLEMENTED();
  return false;
}

void URLRequestNewFtpJob::OnStartCompleted(int result) {
  NOTIMPLEMENTED();
}

void URLRequestNewFtpJob::OnReadCompleted(int result) {
  NOTIMPLEMENTED();
}

void URLRequestNewFtpJob::DestroyTransaction() {
  NOTIMPLEMENTED();
}

void URLRequestNewFtpJob::StartTransaction() {
  NOTIMPLEMENTED();
}
