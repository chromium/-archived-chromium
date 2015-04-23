// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/multipart_response_delegate.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/net_util.h"
#include "webkit/api/public/WebHTTPHeaderVisitor.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLLoaderClient.h"
#include "webkit/glue/glue_util.h"

using WebKit::WebHTTPHeaderVisitor;
using WebKit::WebString;
using WebKit::WebURLLoader;
using WebKit::WebURLLoaderClient;
using WebKit::WebURLResponse;

namespace webkit_glue {

namespace {

// The list of response headers that we do not copy from the original
// response when generating a WebURLResponse for a MIME payload.
const char* kReplaceHeaders[] = {
  "content-type",
  "content-length",
  "content-disposition",
  "content-range",
  "range",
  "set-cookie"
};

class HeaderCopier : public WebHTTPHeaderVisitor {
 public:
  HeaderCopier(WebURLResponse* response)
      : response_(response) {
  }
  virtual void visitHeader(const WebString& name, const WebString& value) {
    const std::string& name_utf8 = WebStringToStdString(name);
    for (size_t i = 0; i < arraysize(kReplaceHeaders); ++i) {
      if (LowerCaseEqualsASCII(name_utf8, kReplaceHeaders[i]))
        return;
    }
    response_->setHTTPHeaderField(name, value);
  }
 private:
  WebURLResponse* response_;
};

}  // namespace

MultipartResponseDelegate::MultipartResponseDelegate(
    WebURLLoaderClient* client,
    WebURLLoader* loader,
    const WebURLResponse& response,
    const std::string& boundary)
    : client_(client),
      loader_(loader),
      original_response_(response),
      boundary_("--"),
      first_received_data_(true),
      processing_headers_(false),
      stop_sending_(false) {
  // Some servers report a boundary prefixed with "--".  See bug 5786.
  if (StartsWithASCII(boundary, "--", true)) {
    boundary_.assign(boundary);
  } else {
    boundary_.append(boundary);
  }
}

void MultipartResponseDelegate::OnReceivedData(const char* data,
                                               int data_len) {
  // stop_sending_ means that we've already received the final boundary token.
  // The server should stop sending us data at this point, but if it does, we
  // just throw it away.
  if (stop_sending_)
    return;

  // TODO(tc): Figure out what to use for length_received.  Maybe we can just
  // pass the value on from our caller.
  int length_received = -1;

  data_.append(data, data_len);
  if (first_received_data_) {
    // Some servers don't send a boundary token before the first chunk of
    // data.  We handle this case anyway (Gecko does too).
    first_received_data_ = false;

    // Eat leading \r\n
    int pos = PushOverLine(data_, 0);
    if (pos)
      data_ = data_.substr(pos);

    if (data_.length() < boundary_.length() + 2) {
      // We don't have enough data yet to make a boundary token.  Just wait
      // until the next chunk of data arrives.
      first_received_data_ = true;
      return;
    }

    if (data_.substr(0, boundary_.length()) != boundary_) {
      data_ = boundary_ + "\n" + data_;
    }
  }
  DCHECK(!first_received_data_);

  // Headers
  if (processing_headers_) {
    // Eat leading \r\n
    int pos = PushOverLine(data_, 0);
    if (pos)
      data_ = data_.substr(pos);

    if (ParseHeaders()) {
      // Successfully parsed headers.
      processing_headers_ = false;
    } else {
      // Get more data before trying again.
      return;
    }
  }
  DCHECK(!processing_headers_);

  size_t boundary_pos;
  while ((boundary_pos = FindBoundary()) != std::string::npos) {
    if (boundary_pos > 0) {
      // Send the last data chunk.
      client_->didReceiveData(loader_,
                              data_.substr(0, boundary_pos).data(),
                              static_cast<int>(boundary_pos),
                              length_received);
    }
    size_t boundary_end_pos = boundary_pos + boundary_.length();
    if (boundary_end_pos < data_.length() && '-' == data_[boundary_end_pos]) {
      // This was the last boundary so we can stop processing.
      stop_sending_ = true;
      data_.clear();
      return;
    }

    // We can now throw out data up through the boundary
    int offset = PushOverLine(data_, boundary_end_pos);
    data_ = data_.substr(boundary_end_pos + offset);

    // Ok, back to parsing headers
    if (!ParseHeaders()) {
      processing_headers_ = true;
      break;
    }
  }
}

void MultipartResponseDelegate::OnCompletedRequest() {
  // If we have any pending data and we're not in a header, go ahead and send
  // it to WebCore.
  if (!processing_headers_ && !data_.empty()) {
    // TODO(tc): Figure out what to use for length_received.  Maybe we can just
    // pass the value on from our caller.
    int length_received = -1;
    client_->didReceiveData(loader_,
                            data_.data(),
                            static_cast<int>(data_.length()),
                            length_received);
  }
}

int MultipartResponseDelegate::PushOverLine(const std::string& data,
                                            size_t pos) {
  int offset = 0;
  if (pos < data.length() && (data[pos] == '\r' || data[pos] == '\n')) {
    ++offset;
    if (pos + 1 < data.length() && data[pos + 1] == '\n')
      ++offset;
  }
  return offset;
}

bool MultipartResponseDelegate::ParseHeaders() {
  int line_feed_increment = 1;

  // Grab the headers being liberal about line endings.
  size_t line_start_pos = 0;
  size_t line_end_pos = data_.find('\n');
  while (line_end_pos != std::string::npos) {
    // Handle CRLF
    if (line_end_pos > line_start_pos && data_[line_end_pos - 1] == '\r') {
      line_feed_increment = 2;
      --line_end_pos;
    } else {
      line_feed_increment = 1;
    }
    if (line_start_pos == line_end_pos) {
      // A blank line, end of headers
      line_end_pos += line_feed_increment;
      break;
    }
    // Find the next header line.
    line_start_pos = line_end_pos + line_feed_increment;
    line_end_pos = data_.find('\n', line_start_pos);
  }
  // Truncated in the middle of a header, stop parsing.
  if (line_end_pos == std::string::npos)
    return false;

  // Eat headers
  std::string headers("\n");
  headers.append(data_.substr(0, line_end_pos));
  data_ = data_.substr(line_end_pos);

  // Create a WebURLResponse based on the original set of headers + the
  // replacement headers.  We only replace the same few headers that gecko
  // does.  See netwerk/streamconv/converters/nsMultiMixedConv.cpp.
  std::string mime_type = net::GetSpecificHeader(headers, "content-type");
  std::string charset = net::GetHeaderParamValue(mime_type, "charset");
  WebURLResponse response(original_response_.url());
  response.setMIMEType(StdStringToWebString(mime_type));
  response.setTextEncodingName(StdStringToWebString(charset));

  HeaderCopier copier(&response);
  original_response_.visitHTTPHeaderFields(&copier);

  for (size_t i = 0; i < arraysize(kReplaceHeaders); ++i) {
    std::string name(kReplaceHeaders[i]);
    std::string value = net::GetSpecificHeader(headers, name);
    if (!value.empty()) {
      response.setHTTPHeaderField(StdStringToWebString(name),
                                  StdStringToWebString(value));
    }
  }
  // Send the response!
  client_->didReceiveResponse(loader_, response);

  return true;
}

// Boundaries are supposed to be preceeded with --, but it looks like gecko
// doesn't require the dashes to exist.  See nsMultiMixedConv::FindToken.
size_t MultipartResponseDelegate::FindBoundary() {
  size_t boundary_pos = data_.find(boundary_);
  if (boundary_pos != std::string::npos) {
    // Back up over -- for backwards compat
    // TODO(tc): Don't we only want to do this once?  Gecko code doesn't seem
    // to care.
    if (boundary_pos >= 2) {
      if ('-' == data_[boundary_pos - 1] && '-' == data_[boundary_pos - 2]) {
        boundary_pos -= 2;
        boundary_ = "--" + boundary_;
      }
    }
  }
  return boundary_pos;
}

bool MultipartResponseDelegate::ReadMultipartBoundary(
    const WebURLResponse& response,
    std::string* multipart_boundary) {
  std::string content_type = WebStringToStdString(
      response.httpHeaderField(WebString::fromUTF8("Content-Type")));

  size_t boundary_start_offset = content_type.find("boundary=");
  if (boundary_start_offset == std::wstring::npos) {
    return false;
  }

  boundary_start_offset += strlen("boundary=");

  size_t boundary_end_offset = content_type.find(';', boundary_start_offset);

  if (boundary_end_offset == std::string::npos)
    boundary_end_offset = content_type.length();

  size_t boundary_length = boundary_end_offset - boundary_start_offset;

  *multipart_boundary =
      content_type.substr(boundary_start_offset, boundary_length);
  // The byte range response can have quoted boundary strings. This is legal
  // as per MIME specifications. Individual data fragements however don't
  // contain quoted boundary strings.
  TrimString(*multipart_boundary, "\"", multipart_boundary);
  return true;
}

bool MultipartResponseDelegate::ReadContentRanges(
    const WebURLResponse& response,
    int* content_range_lower_bound,
    int* content_range_upper_bound) {

  std::string content_range = WebStringToStdString(
      response.httpHeaderField(WebString::fromUTF8("Content-Range")));

  size_t byte_range_lower_bound_start_offset = content_range.find(" ");
  if (byte_range_lower_bound_start_offset == std::string::npos) {
    return false;
  }

  // Skip over the initial space.
  byte_range_lower_bound_start_offset++;

  size_t byte_range_lower_bound_end_offset =
      content_range.find("-", byte_range_lower_bound_start_offset);
  if (byte_range_lower_bound_end_offset == std::string::npos) {
    return false;
  }

  size_t byte_range_lower_bound_characters =
      byte_range_lower_bound_end_offset - byte_range_lower_bound_start_offset;
  std::string byte_range_lower_bound =
      content_range.substr(byte_range_lower_bound_start_offset,
                           byte_range_lower_bound_characters);

  size_t byte_range_upper_bound_start_offset =
      byte_range_lower_bound_end_offset + 1;

  size_t byte_range_upper_bound_end_offset =
      content_range.find("/", byte_range_upper_bound_start_offset);
  if (byte_range_upper_bound_end_offset == std::string::npos) {
    return false;
  }

  size_t byte_range_upper_bound_characters =
      byte_range_upper_bound_end_offset - byte_range_upper_bound_start_offset;

  std::string byte_range_upper_bound =
      content_range.substr(byte_range_upper_bound_start_offset,
                           byte_range_upper_bound_characters);

  if (!StringToInt(byte_range_lower_bound, content_range_lower_bound))
    return false;
  if (!StringToInt(byte_range_upper_bound, content_range_upper_bound))
    return false;
  return true;
}

}  // namespace webkit_glue
