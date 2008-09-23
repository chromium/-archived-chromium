// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/filter.h"

#include "base/string_util.h"
#include "net/base/gzip_filter.h"
#include "net/base/bzip2_filter.h"
#include "net/base/sdch_filter.h"

namespace {

// Filter types:
const char kDeflate[]      = "deflate";
const char kGZip[]         = "gzip";
const char kXGZip[]        = "x-gzip";
const char kBZip2[]        = "bzip2";
const char kXBZip2[]       = "x-bzip2";
const char kSdch[]         = "sdch";
// compress and x-compress are currently not supported.  If we decide to support
// them, we'll need the same mime type compatibility hack we have for gzip.  For
// more information, see Firefox's nsHttpChannel::ProcessNormal.
const char kCompress[]     = "compress";
const char kXCompress[]    = "x-compress";
const char kIdentity[]     = "identity";
const char kUncompressed[] = "uncompressed";

// Mime types:
const char kApplicationXGzip[]     = "application/x-gzip";
const char kApplicationGzip[]      = "application/gzip";
const char kApplicationXGunzip[]   = "application/x-gunzip";
const char kApplicationXCompress[] = "application/x-compress";
const char kApplicationCompress[]  = "application/compress";

}  // namespace

Filter* Filter::Factory(const std::vector<std::string>& filter_types,
                        const std::string& mime_type,
                        int buffer_size) {
  if (filter_types.empty() || buffer_size < 0)
    return NULL;

  std::string safe_mime_type =  (filter_types.size() > 1) ? "" : mime_type;
  Filter* filter_list = NULL;  // Linked list of filters.
  for (size_t i = 0; i < filter_types.size(); ++i) {
    Filter* first_filter;
    first_filter = SingleFilter(filter_types[i], safe_mime_type, buffer_size);
    if (!first_filter) {
      // Cleanup and exit, since we can't construct this filter list.
      if (filter_list)
        delete filter_list;
      filter_list = NULL;
      break;
    }
    first_filter->next_filter_.reset(filter_list);
    filter_list = first_filter;
  }
  return filter_list;
}

// static
Filter* Filter::SingleFilter(const std::string& filter_type,
                             const std::string& mime_type,
                             int buffer_size) {
  FilterType type_id;
  if (LowerCaseEqualsASCII(filter_type, kDeflate)) {
    type_id = FILTER_TYPE_DEFLATE;
  } else if (LowerCaseEqualsASCII(filter_type, kGZip) ||
             LowerCaseEqualsASCII(filter_type, kXGZip)) {
    if (LowerCaseEqualsASCII(mime_type, kApplicationXGzip) ||
        LowerCaseEqualsASCII(mime_type, kApplicationGzip) ||
        LowerCaseEqualsASCII(mime_type, kApplicationXGunzip)) {
      // The server has told us that it sent us gziped content with a gzip
      // content encoding.  Sadly, Apache mistakenly sets these headers for all
      // .gz files.  We match Firefox's nsHttpChannel::ProcessNormal and ignore
      // the Content-Encoding here.
      type_id = FILTER_TYPE_UNSUPPORTED;
    } else {
      type_id = FILTER_TYPE_GZIP;
    }
  } else if (LowerCaseEqualsASCII(filter_type, kBZip2) ||
             LowerCaseEqualsASCII(filter_type, kXBZip2)) {
    type_id = FILTER_TYPE_BZIP2;
  } else if (LowerCaseEqualsASCII(filter_type, kSdch)) {
    type_id = FILTER_TYPE_SDCH;
  } else {
    // Note we also consider "identity" and "uncompressed" UNSUPPORTED as
    // filter should be disabled in such cases.
    type_id = FILTER_TYPE_UNSUPPORTED;
  }

  switch (type_id) {
    case FILTER_TYPE_DEFLATE:
    case FILTER_TYPE_GZIP: {
      scoped_ptr<GZipFilter> gz_filter(new GZipFilter());
      if (gz_filter->InitBuffer(buffer_size)) {
        if (gz_filter->InitDecoding(type_id)) {
          return gz_filter.release();
        }
      }
      break;
    }
    case FILTER_TYPE_BZIP2: {
      scoped_ptr<BZip2Filter> bzip2_filter(new BZip2Filter());
      if (bzip2_filter->InitBuffer(buffer_size)) {
        if (bzip2_filter->InitDecoding(false)) {
          return bzip2_filter.release();
        }
      }
      break;
    }
    case FILTER_TYPE_SDCH: {
      scoped_ptr<SdchFilter> sdch_filter(new SdchFilter());
      if (sdch_filter->InitBuffer(buffer_size)) {
        if (sdch_filter->InitDecoding()) {
          return sdch_filter.release();
        }
      }
      break;
    }
    default: {
      break;
    }
  }

  return NULL;
}

Filter::Filter()
    : stream_buffer_(NULL),
      stream_buffer_size_(0),
      next_stream_data_(NULL),
      stream_data_len_(0),
      next_filter_(NULL),
      last_status_(FILTER_OK) {
}

Filter::~Filter() {}

bool Filter::InitBuffer(int buffer_size) {
  if (buffer_size < 0 || stream_buffer())
    return false;

  stream_buffer_.reset(new char[buffer_size]);

  if (stream_buffer()) {
    stream_buffer_size_ = buffer_size;
    return true;
  }

  return false;
}


Filter::FilterStatus Filter::CopyOut(char* dest_buffer, int* dest_len) {
  int out_len;
  int input_len = *dest_len;
  *dest_len = 0;

  if (0 == stream_data_len_)
    return Filter::FILTER_NEED_MORE_DATA;

  out_len = std::min(input_len, stream_data_len_);
  memcpy(dest_buffer, next_stream_data_, out_len);
  *dest_len += out_len;
  stream_data_len_ -= out_len;
  if (0 == stream_data_len_) {
    next_stream_data_ = NULL;
    return Filter::FILTER_NEED_MORE_DATA;
  } else {
    next_stream_data_ += out_len;
    return Filter::FILTER_OK;
  }
}


Filter::FilterStatus Filter::ReadFilteredData(char* dest_buffer,
                                              int* dest_len) {
  return Filter::FILTER_ERROR;
}

Filter::FilterStatus Filter::ReadData(char* dest_buffer, int* dest_len) {
  if (last_status_ == FILTER_ERROR)
    return last_status_;
  if (!next_filter_.get())
    return last_status_ = ReadFilteredData(dest_buffer, dest_len);
  if (last_status_ == FILTER_NEED_MORE_DATA && !stream_data_len())
    return next_filter_->ReadData(dest_buffer, dest_len);
  if (next_filter_->last_status() == FILTER_NEED_MORE_DATA) {
    // Push data into next filter's input.
    char* next_buffer = next_filter_->stream_buffer();
    int next_size = next_filter_->stream_buffer_size();
    last_status_ = ReadFilteredData(next_buffer, &next_size);
    next_filter_->FlushStreamBuffer(next_size);
    switch (last_status_) {
      case FILTER_ERROR:
        return last_status_;

      case FILTER_NEED_MORE_DATA:
        return next_filter_->ReadData(dest_buffer, dest_len);

      case FILTER_OK:
      case FILTER_DONE:
        break;
    }
  }
  FilterStatus status = next_filter_->ReadData(dest_buffer, dest_len);
  // We could loop to fill next_filter_ if it needs data, but we have to be
  // careful about output buffer.  Simpler is to just wait until we are called
  // again, and return FILTER_OK.
  return (status == FILTER_ERROR) ? FILTER_ERROR : FILTER_OK;
}

bool Filter::FlushStreamBuffer(int stream_data_len) {
  if (stream_data_len <= 0 || stream_data_len > stream_buffer_size_)
    return false;

  // bail out if there are more data in the stream buffer to be filtered.
  if (!stream_buffer() || stream_data_len_)
    return false;

  next_stream_data_ = stream_buffer();
  stream_data_len_ = stream_data_len;
  return true;
}

void Filter::SetURL(const GURL& url) {
  url_ = url;
  if (next_filter_.get())
    next_filter_->SetURL(url);
}
