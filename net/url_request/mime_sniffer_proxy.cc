// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
