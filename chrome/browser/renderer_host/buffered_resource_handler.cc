// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/buffered_resource_handler.h"

#include "base/histogram.h"
#include "base/string_util.h"
#include "net/base/mime_sniffer.h"
#include "chrome/browser/renderer_host/download_throttling_resource_handler.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "net/base/mime_sniffer.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"

namespace {

const int kMaxBytesToSniff = 512;

void RecordSnifferMetrics(bool sniffing_blocked,
                          bool we_would_like_to_sniff,
                          const std::string& mime_type) {
  static BooleanHistogram nosniff_usage("nosniff.usage");
  nosniff_usage.SetFlags(kUmaTargetedHistogramFlag);
  nosniff_usage.AddBoolean(sniffing_blocked);

  if (sniffing_blocked) {
    static BooleanHistogram nosniff_otherwise("nosniff.otherwise");
    nosniff_otherwise.SetFlags(kUmaTargetedHistogramFlag);
    nosniff_otherwise.AddBoolean(we_would_like_to_sniff);

    static BooleanHistogram nosniff_empty_mime_type("nosniff.empty_mime_type");
    nosniff_empty_mime_type.SetFlags(kUmaTargetedHistogramFlag);
    nosniff_empty_mime_type.AddBoolean(mime_type.empty());
  }
}

}  // namespace

BufferedResourceHandler::BufferedResourceHandler(ResourceHandler* handler,
                                                 ResourceDispatcherHost* host,
                                                 URLRequest* request)
    : real_handler_(handler),
      host_(host),
      request_(request),
      bytes_read_(0),
      sniff_content_(false),
      should_buffer_(false),
      buffering_(false),
      finished_(false) {
}

bool BufferedResourceHandler::OnUploadProgress(int request_id,
                                               uint64 position,
                                               uint64 size) {
  return real_handler_->OnUploadProgress(request_id, position, size);
}

bool BufferedResourceHandler::OnRequestRedirected(int request_id,
                                                  const GURL& new_url) {
  return real_handler_->OnRequestRedirected(request_id, new_url);
}

bool BufferedResourceHandler::OnResponseStarted(int request_id,
                                                ResourceResponse* response) {
  response_ = response;
  if (!DelayResponse())
    return CompleteResponseStarted(request_id, false);
  return true;
}


bool BufferedResourceHandler::OnResponseCompleted(
    int request_id, const URLRequestStatus& status) {
  return real_handler_->OnResponseCompleted(request_id, status);
}

// We'll let the original event handler provide a buffer, and reuse it for
// subsequent reads until we're done buffering.
bool BufferedResourceHandler::OnWillRead(int request_id, net::IOBuffer** buf,
                                         int* buf_size, int min_size) {
  if (buffering_) {
    DCHECK(!my_buffer_.get());
    my_buffer_ = new net::IOBuffer(kMaxBytesToSniff);
    *buf = my_buffer_.get();
    *buf_size = kMaxBytesToSniff;
    return true;
  }

  if (finished_)
    return false;

  bool ret = real_handler_->OnWillRead(request_id, buf, buf_size, min_size);
  read_buffer_ = *buf;
  read_buffer_size_ = *buf_size;
  DCHECK(read_buffer_size_ >= kMaxBytesToSniff * 2); 
  bytes_read_ = 0;
  return ret;
}

bool BufferedResourceHandler::OnReadCompleted(int request_id, int* bytes_read) {
  if (sniff_content_ || should_buffer_) {
    if (KeepBuffering(*bytes_read))
      return true;

    LOG(INFO) << "Finished buffering " << request_->url().spec();
    sniff_content_ = should_buffer_ = false;
    *bytes_read = bytes_read_;

    // Done buffering, send the pending ResponseStarted event.
    if (!CompleteResponseStarted(request_id, true))
      return false;
  }

  // Release the reference that we acquired at OnWillRead.
  read_buffer_ = NULL;
  return real_handler_->OnReadCompleted(request_id, bytes_read);
}

bool BufferedResourceHandler::DelayResponse() {
  std::string mime_type;
  request_->GetMimeType(&mime_type);

  std::string content_type_options;
  request_->GetResponseHeaderByName("x-content-type-options",
                                    &content_type_options);

  const bool sniffing_blocked =
      LowerCaseEqualsASCII(content_type_options, "nosniff");
  const bool we_would_like_to_sniff =
      net::ShouldSniffMimeType(request_->url(), mime_type);

  RecordSnifferMetrics(sniffing_blocked, we_would_like_to_sniff, mime_type);

  if (!sniffing_blocked && we_would_like_to_sniff) {
    // We're going to look at the data before deciding what the content type
    // is.  That means we need to delay sending the ResponseStarted message
    // over the IPC channel.
    sniff_content_ = true;
    LOG(INFO) << "To buffer: " << request_->url().spec();
    return true;
  }

  if (sniffing_blocked && mime_type.empty()) {
    // Ugg.  The server told us not to sniff the content but didn't give us a
    // mime type.  What's a browser to do?  Turns out, we're supposed to treat
    // the response as "text/plain".  This is the most secure option.
    mime_type.assign("text/plain");
    response_->response_head.mime_type.assign(mime_type);
  }

  if (ShouldBuffer(request_->url(), mime_type)) {
    // This is a temporary fix for the fact that webkit expects to have
    // enough data to decode the doctype in order to select the rendering
    // mode.
    should_buffer_ = true;
    LOG(INFO) << "To buffer: " << request_->url().spec();
    return true;
  }
  return false;
}

bool BufferedResourceHandler::ShouldBuffer(const GURL& url,
                                           const std::string& mime_type) {
  // We are willing to buffer for HTTP and HTTPS.
  bool sniffable_scheme = url.is_empty() ||
                          url.SchemeIs("http") ||
                          url.SchemeIs("https");
  if (!sniffable_scheme)
    return false;

  // Today, the only reason to buffer the request is to fix the doctype decoding
  // performed by webkit: if there is not enough data it will go to quirks mode.
  // We only expect the doctype check to apply to html documents.
  return mime_type == "text/html";
}

bool BufferedResourceHandler::KeepBuffering(int bytes_read) {
  DCHECK(read_buffer_);
  if (my_buffer_) {
    // We are using our own buffer to read, update the main buffer.
    CHECK(bytes_read + bytes_read_ < read_buffer_size_);
    memcpy(read_buffer_->data() + bytes_read_, my_buffer_->data(), bytes_read);
    my_buffer_ = NULL;
  }
  bytes_read_ += bytes_read;
  finished_ = (bytes_read == 0);

  if (sniff_content_) {
    std::string type_hint, new_type;
    request_->GetMimeType(&type_hint);

    if (!net::SniffMimeType(read_buffer_->data(), bytes_read_,
                            request_->url(), type_hint, &new_type)) {
      // SniffMimeType() returns false if there is not enough data to determine
      // the mime type. However, even if it returns false, it returns a new type
      // that is probably better than the current one.
      DCHECK(bytes_read_ < kMaxBytesToSniff);
      if (!finished_) {
        buffering_ = true;
        return true;
      }
    }
    sniff_content_ = false;
    response_->response_head.mime_type.assign(new_type);

    // We just sniffed the mime type, maybe there is a doctype to process.
    if (ShouldBuffer(request_->url(), new_type))
      should_buffer_ = true;
  }

  if (!finished_ && should_buffer_) {
    if (!DidBufferEnough(bytes_read_)) {
      buffering_ = true;
      return true;
    }
  }
  buffering_ = false;
  return false;
}

bool BufferedResourceHandler::CompleteResponseStarted(int request_id,
                                                      bool in_complete) {
  // Check to see if we should forward the data from this request to the
  // download thread.
  // TODO(paulg): Only download if the context from the renderer allows it.
  std::string content_disposition;
  request_->GetResponseHeaderByName("content-disposition",
                                    &content_disposition);

  ResourceDispatcherHost::ExtraRequestInfo* info =
      ResourceDispatcherHost::ExtraInfoForRequest(request_);

  if (info->allow_download &&
      host_->ShouldDownload(response_->response_head.mime_type,
                            content_disposition)) {
    if (response_->response_head.headers &&  // Can be NULL if FTP.
        response_->response_head.headers->response_code() / 100 != 2) {
      // The response code indicates that this is an error page, but we don't
      // know how to display the content.  We follow Firefox here and show our
      // own error page instead of triggering a download.
      // TODO(abarth): We should abstract the response_code test, but this kind
      //               of check is scattered throughout our codebase.
      request_->CancelWithError(net::ERR_FILE_NOT_FOUND);
      return false;
    }

    info->is_download = true;

    scoped_refptr<DownloadThrottlingResourceHandler> download_handler =
        new DownloadThrottlingResourceHandler(host_,
                                           request_,
                                           request_->url(),
                                           info->render_process_host_id,
                                           info->render_view_id,
                                           request_id,
                                           in_complete);
    if (bytes_read_) {
      // a Read has already occurred and we need to copy the data into the
      // EventHandler.
      net::IOBuffer* buf = NULL;
      int buf_len = 0;
      download_handler->OnWillRead(request_id, &buf, &buf_len, bytes_read_);
      CHECK((buf_len >= bytes_read_) && (bytes_read_ >= 0));
      memcpy(buf->data(), read_buffer_->data(), bytes_read_);
    }

    // Send the renderer a response that indicates that the request will be
    // handled by an external source (the browser's DownloadManager).
    real_handler_->OnResponseStarted(info->request_id, response_);
    URLRequestStatus status(URLRequestStatus::HANDLED_EXTERNALLY, 0);
    real_handler_->OnResponseCompleted(info->request_id, status);

    // Ditch the old async handler that talks to the renderer for the new
    // download handler that talks to the DownloadManager.
    real_handler_ = download_handler;
  }
  return real_handler_->OnResponseStarted(request_id, response_);
}

bool BufferedResourceHandler::DidBufferEnough(int bytes_read) {
  const int kRequiredLength = 256;

  return bytes_read >= kRequiredLength;
}
