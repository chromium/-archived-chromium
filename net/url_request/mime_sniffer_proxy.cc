// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/mime_sniffer_proxy.h"

#include "net/base/mime_sniffer.h"

MimeSnifferProxy::MimeSnifferProxy(URLRequest* request,
                                   URLRequest::Delegate* delegate)
    : request_(request), delegate_(delegate),
      sniff_content_(false), error_(false) {
  request->set_delegate(this);
}

void MimeSnifferProxy::OnResponseStarted(URLRequest* request) {
  if (request->status().is_success()) {
    request->GetMimeType(&mime_type_);
    if (net::ShouldSniffMimeType(request->url(), mime_type_)) {
      // We need to read content before we know the mime type,
      // so we don't call OnResponseStarted.
      sniff_content_ = true;
      if (request_->Read(buf_, sizeof(buf_), &bytes_read_) && bytes_read_) {
        OnReadCompleted(request, bytes_read_);
      } else if (!request_->status().is_io_pending()) {
        error_ = true;
        delegate_->OnResponseStarted(request);
      } // Otherwise, IO pending.  Wait for OnReadCompleted.
      return;
    }
  }
  delegate_->OnResponseStarted(request);
}

bool MimeSnifferProxy::Read(char* buf, int max_bytes, int *bytes_read) {
  if (sniff_content_) {
    // This is the first call to Read() after we've sniffed content.
    // Return our local buffer or the error we ran into.
    sniff_content_ = false;  // We're done with sniffing for this request.

    if (error_) {
      *bytes_read = 0;
      return false;
    }

    memcpy(buf, buf_, bytes_read_);
    *bytes_read = bytes_read_;
    return true;
  }
  return request_->Read(buf, max_bytes, bytes_read);
}

void MimeSnifferProxy::OnReadCompleted(URLRequest* request, int bytes_read) {
  if (sniff_content_) {
    // Our initial content-sniffing Read() has completed.
    if (request->status().is_success() && bytes_read) {
      std::string type_hint;
      request_->GetMimeType(&type_hint);
      bytes_read_ = bytes_read;
      net::SniffMimeType(
          buf_, bytes_read_, request_->url(), type_hint, &mime_type_);
    } else {
      error_ = true;
    }
    delegate_->OnResponseStarted(request_);
    return;
  }
  delegate_->OnReadCompleted(request, bytes_read);
}

