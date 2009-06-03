// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_new_ftp_job.h"

#include "base/compiler_specific.h"
#include "base/file_version_info.h"
#include "base/message_loop.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/ftp/ftp_directory_parser.h"
#include "net/ftp/ftp_response_info.h"
#include "net/ftp/ftp_transaction_factory.h"
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
  DCHECK(!transaction_.get());
  request_info_.url = request_->url();
  StartTransaction();
}

void URLRequestNewFtpJob::Kill() {
  if (!transaction_.get())
    return;
  DestroyTransaction();
  URLRequestJob::Kill();
}

bool URLRequestNewFtpJob::ReadRawData(net::IOBuffer* buf,
                                      int buf_size,
                                      int *bytes_read) {
  DCHECK_NE(buf_size, 0);
  DCHECK(bytes_read);
  DCHECK(!read_in_progress_);
  if (response_info_ == NULL) {
     response_info_ = transaction_->GetResponseInfo();
    if (response_info_->is_directory_listing) {
      // Unescape the URL path and pass the raw 8bit directly to the browser.
      directory_html_ = net::GetDirectoryListingHeader(
          UnescapeURLComponent(request_->url().path(),
          UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS));
      // If this isn't top level directory (i.e. the path isn't "/",)
      // add a link to the parent directory.
      if (request_->url().path().length() > 1)
        directory_html_.append(net::GetDirectoryListingEntry("..",
                                                             false,
                                                             0,
                                                             base::Time()));
    }
  }
  if (!directory_html_.empty()) {
    size_t bytes_to_copy = std::min(static_cast<size_t>(buf_size),
                                    directory_html_.size());
    memcpy(buf->data(), directory_html_.c_str(), bytes_to_copy);
    *bytes_read = bytes_to_copy;
    directory_html_.erase(0, bytes_to_copy);
    return true;
  }
  int rv = transaction_->Read(buf, buf_size, &read_callback_);
  if (response_info_->is_directory_listing) {
    std::string file_entry;
    if (rv > 0) {
      std::string line;
      buf->data()[rv] = 0;
      std::istringstream iss(buf->data());
      while (getline(iss, line)) {
        struct net::ListState state;
        struct net::ListResult result;
        // TODO(ibrar): Use a more descriptive variable name than 'lt', which
        // looks like 'it'.
        net::LineType lt = ParseFTPLine(line.c_str(), &state, &result);
        switch (lt) {
          case net::FTP_TYPE_DIRECTORY:
            file_entry.append(net::GetDirectoryListingEntry(result.fe_fname,
                                                            true,
                                                            rv,
                                                            base::Time()));
            break;
          case net::FTP_TYPE_FILE:
            file_entry.append(net::GetDirectoryListingEntry(result.fe_fname,
                                                            false,
                                                            rv,
                                                            base::Time()));
            break;
          case net::FTP_TYPE_SYMLINK:
          case net::FTP_TYPE_JUNK:
          case net::FTP_TYPE_COMMENT:
            break;
          default:
            break;
        }
      }
      memcpy(buf->data(), file_entry.c_str(), file_entry.length());
      *bytes_read = file_entry.length();
      return true;
    }
  } else {
    rv = transaction_->Read(buf, buf_size, &read_callback_);
  }
  if (rv >= 0) {
    *bytes_read = rv;
    return true;
  }
  if (rv == net::ERR_IO_PENDING) {
    read_in_progress_ = true;
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
  }
  return false;
}

void URLRequestNewFtpJob::OnStartCompleted(int result) {
  // If the request was destroyed, then there is no more work to do.
  if (!request_ || !request_->delegate())
    return;
  // If the transaction was destroyed, then the job was cancelled, and
  // we can just ignore this notification.
  if (!transaction_.get())
    return;
  // Clear the IO_PENDING status
  SetStatus(URLRequestStatus());
  if (result == net::OK) {
    URLRequestJob::NotifyHeadersComplete();
  } else {
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, result));
  }
}

void URLRequestNewFtpJob::OnReadCompleted(int result) {
  read_in_progress_ = false;
  if (result == 0) {
    NotifyDone(URLRequestStatus());
  } else if (result < 0) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, result));
  } else {
    // Clear the IO_PENDING status
    SetStatus(URLRequestStatus());
  }
  NotifyReadComplete(result);
}

void URLRequestNewFtpJob::StartTransaction() {
  // Create a transaction.
  DCHECK(!transaction_.get());
  DCHECK(request_->context());
  DCHECK(request_->context()->ftp_transaction_factory());

  transaction_.reset(
  request_->context()->ftp_transaction_factory()->CreateTransaction());

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  int rv;
  if (transaction_.get()) {
    rv = transaction_->Start(&request_info_, &start_callback_);
    if (rv == net::ERR_IO_PENDING)
      return;
  } else {
    rv = net::ERR_FAILED;
  }
  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestNewFtpJob::OnStartCompleted, rv));
}

void URLRequestNewFtpJob::DestroyTransaction() {
  DCHECK(transaction_.get());

  transaction_.reset();
  response_info_ = NULL;
}
