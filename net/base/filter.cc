// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/filter.h"

#include "base/string_util.h"
#include "net/base/gzip_filter.h"
#include "net/base/bzip2_filter.h"
#include "net/base/sdch_filter.h"

namespace {

// Filter types (using canonical lower case only):
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
const char kTextHtml[]             = "text/html";

}  // namespace

Filter* Filter::Factory(const std::vector<FilterType>& filter_types,
                        int buffer_size) {
  if (filter_types.empty() || buffer_size < 0)
    return NULL;

  Filter* filter_list = NULL;  // Linked list of filters.
  for (size_t i = 0; i < filter_types.size(); i++) {
    filter_list = PrependNewFilter(filter_types[i], buffer_size, filter_list);
    if (!filter_list)
      return NULL;
  }

  return filter_list;
}

// static
Filter::FilterType Filter::ConvertEncodingToType(
    const std::string& filter_type) {
  FilterType type_id;
  if (LowerCaseEqualsASCII(filter_type, kDeflate)) {
    type_id = FILTER_TYPE_DEFLATE;
  } else if (LowerCaseEqualsASCII(filter_type, kGZip) ||
             LowerCaseEqualsASCII(filter_type, kXGZip)) {
    type_id = FILTER_TYPE_GZIP;
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
  return type_id;
}

// static
void Filter::FixupEncodingTypes(
    bool is_sdch_response,
    const std::string& mime_type,
    std::vector<FilterType>* encoding_types) {

  if ((1 == encoding_types->size()) &&
      (FILTER_TYPE_GZIP == encoding_types->front())) {
    if (LowerCaseEqualsASCII(mime_type, kApplicationXGzip) ||
        LowerCaseEqualsASCII(mime_type, kApplicationGzip) ||
        LowerCaseEqualsASCII(mime_type, kApplicationXGunzip))
      // The server has told us that it sent us gziped content with a gzip
      // content encoding.  Sadly, Apache mistakenly sets these headers for all
      // .gz files.  We match Firefox's nsHttpChannel::ProcessNormal and ignore
      // the Content-Encoding here.
      encoding_types->clear();
  }

  if (!is_sdch_response) {
    if (1 < encoding_types->size()) {
      // Multiple filters were intended to only be used for SDCH (thus far!)
      SdchManager::SdchErrorRecovery(
          SdchManager::MULTIENCODING_FOR_NON_SDCH_REQUEST);
    }
    if ((1 == encoding_types->size()) &&
        (FILTER_TYPE_SDCH == encoding_types->front())) {
        SdchManager::SdchErrorRecovery(
            SdchManager::SDCH_CONTENT_ENCODE_FOR_NON_SDCH_REQUEST);
    }
    return;
  }

  // If content encoding included SDCH, then everything is fine.
  if (!encoding_types->empty() &&
      (FILTER_TYPE_SDCH == encoding_types->front())) {
    // Some proxies (found currently in Argentina) strip the Content-Encoding
    // text from "sdch,gzip" to a mere "sdch" without modifying the compressed
    // payload.   To handle this gracefully, we simulate the "probably" deleted
    // ",gzip" by appending a tentative gzip decode, which will default to a
    // no-op pass through filter if it doesn't get gzip headers where expected.
    if (1 == encoding_types->size()) {
      encoding_types->push_back(FILTER_TYPE_GZIP_HELPING_SDCH);
      SdchManager::SdchErrorRecovery(
          SdchManager::OPTIONAL_GUNZIP_ENCODING_ADDED);
    }
    return;
  }

  // SDCH "search results" protective hack: To make sure we don't break the only
  // currently deployed SDCH enabled server! Be VERY cautious about proxies that
  // strip all content-encoding to not include sdch.  IF we don't see content
  // encodings that seem to match what we'd expect from a server that asked us
  // to use a dictionary (and we advertised said dictionary in the GET), then
  // we set the encoding to (try to) use SDCH to decode.  Note that SDCH will
  // degrade into a pass-through filter if it doesn't have a viable dictionary
  // hash in its header.  Also note that a solo "sdch" will implicitly create
  // a "sdch,gzip" decoding filter, where the gzip portion will degrade to a
  // pass through if a gzip header is not encountered.  Hence we can replace
  // "gzip" with "sdch" and "everything will work."
  // The one failure mode comes when we advertise a dictionary, and the server
  // tries to *send* a gzipped file (not gzip encode content), and then we could
  // do a gzip decode :-(.  Since current server support does not ever see such
  // a transfer, we are safe (for now).
  if (LowerCaseEqualsASCII(mime_type, kTextHtml)) {
    // Suspicious case: Advertised dictionary, but server didn't use sdch, even
    // though it is text_html content.
    if (encoding_types->empty())
      SdchManager::SdchErrorRecovery(SdchManager::ADDED_CONTENT_ENCODING);
    else if (1 == encoding_types->size())
      SdchManager::SdchErrorRecovery(SdchManager::FIXED_CONTENT_ENCODING);
    else
      SdchManager::SdchErrorRecovery(SdchManager::FIXED_CONTENT_ENCODINGS);
    encoding_types->clear();
    encoding_types->push_back(FILTER_TYPE_SDCH_POSSIBLE);
    encoding_types->push_back(FILTER_TYPE_GZIP_HELPING_SDCH);
    return;
  }

  // It didn't have SDCH encoding... but it wasn't HTML... so maybe it really
  // wasn't SDCH encoded.  It would be nice if we knew this, and didn't bother
  // to propose a dictionary etc., but current SDCH spec does not provide a nice
  // way for us to conclude that.  Perhaps in the future, this case will be much
  // more rare.
  return;
}

// static
Filter* Filter::PrependNewFilter(FilterType type_id, int buffer_size,
                                 Filter* filter_list) {
  Filter* first_filter = NULL;  // Soon to be start of chain.
  switch (type_id) {
    case FILTER_TYPE_GZIP_HELPING_SDCH:
    case FILTER_TYPE_DEFLATE:
    case FILTER_TYPE_GZIP: {
      scoped_ptr<GZipFilter> gz_filter(new GZipFilter());
      if (gz_filter->InitBuffer(buffer_size)) {
        if (gz_filter->InitDecoding(type_id)) {
          first_filter = gz_filter.release();
        }
      }
      break;
    }
    case FILTER_TYPE_BZIP2: {
      scoped_ptr<BZip2Filter> bzip2_filter(new BZip2Filter());
      if (bzip2_filter->InitBuffer(buffer_size)) {
        if (bzip2_filter->InitDecoding(false)) {
          first_filter = bzip2_filter.release();
        }
      }
      break;
    }
    case FILTER_TYPE_SDCH:
    case FILTER_TYPE_SDCH_POSSIBLE: {
      scoped_ptr<SdchFilter> sdch_filter(new SdchFilter());
      if (sdch_filter->InitBuffer(buffer_size)) {
        if (sdch_filter->InitDecoding(type_id)) {
          first_filter = sdch_filter.release();
        }
      }
      break;
    }
    default: {
      break;
    }
  }

  if (first_filter) {
    first_filter->next_filter_.reset(filter_list);
  } else {
    // Cleanup and exit, since we can't construct this filter list.
    delete filter_list;
    filter_list = NULL;
  }
  return first_filter;
}

Filter::Filter()
    : stream_buffer_(NULL),
      stream_buffer_size_(0),
      next_stream_data_(NULL),
      stream_data_len_(0),
      url_(),
      connect_time_(),
      was_cached_(false),
      mime_type_(),
      next_filter_(NULL),
      last_status_(FILTER_NEED_MORE_DATA) {
}

Filter::~Filter() {}

bool Filter::InitBuffer(int buffer_size) {
  if (buffer_size < 0 || stream_buffer())
    return false;

  stream_buffer_ = new net::IOBuffer(buffer_size);

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
    net::IOBuffer* next_buffer = next_filter_->stream_buffer();
    int next_size = next_filter_->stream_buffer_size();
    last_status_ = ReadFilteredData(next_buffer->data(), &next_size);
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

  next_stream_data_ = stream_buffer()->data();
  stream_data_len_ = stream_data_len;
  return true;
}

void Filter::SetURL(const GURL& url) {
  url_ = url;
  if (next_filter_.get())
    next_filter_->SetURL(url);
}

void Filter::SetMimeType(const std::string& mime_type) {
  mime_type_ = mime_type;
  if (next_filter_.get())
    next_filter_->SetMimeType(mime_type);
}

void Filter::SetConnectTime(const base::Time& time, bool was_cached) {
  connect_time_ = time;
  was_cached_ = was_cached;
  if (next_filter_.get())
    next_filter_->SetConnectTime(time, was_cached_);
}
