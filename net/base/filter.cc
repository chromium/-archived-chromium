// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/filter.h"

#include "base/file_path.h"
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
                        const FilterContext& filter_context) {
  DCHECK(filter_context.GetInputStreamBufferSize() > 0);
  if (filter_types.empty() || filter_context.GetInputStreamBufferSize() <= 0)
    return NULL;


  Filter* filter_list = NULL;  // Linked list of filters.
  for (size_t i = 0; i < filter_types.size(); i++) {
    filter_list = PrependNewFilter(filter_types[i], filter_context,
                                   filter_list);
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
    const FilterContext& filter_context,
    std::vector<FilterType>* encoding_types) {
  std::string mime_type;
  bool success = filter_context.GetMimeType(&mime_type);
  DCHECK(success || mime_type.empty());

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

    GURL url;
    success = filter_context.GetURL(&url);
    DCHECK(success);
    FilePath filename = FilePath().AppendASCII(url.ExtractFileName());
    FilePath::StringType extension = filename.Extension();

    // Firefox does not apply the filter to the following extensions.
    // See Firefox's nsHttpChannel::nsContentEncodings::GetNext() and
    // nonDecodableExtensions in nsExternalHelperAppService.cpp
    if (0 == extension.compare(FILE_PATH_LITERAL(".gz")) ||
        0 == extension.compare(FILE_PATH_LITERAL(".tgz")) ||
        0 == extension.compare(FILE_PATH_LITERAL(".svgz"))) {
      encoding_types->clear();
    }
  }

  if (!filter_context.IsSdchResponse()) {
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
  if (StartsWithASCII(mime_type, kTextHtml, false)) {
    // Suspicious case: Advertised dictionary, but server didn't use sdch, and
    // we're HTML tagged.
    if (encoding_types->empty()) {
      SdchManager::SdchErrorRecovery(SdchManager::ADDED_CONTENT_ENCODING);
    } else if (1 == encoding_types->size()) {
      SdchManager::SdchErrorRecovery(SdchManager::FIXED_CONTENT_ENCODING);
    } else {
      SdchManager::SdchErrorRecovery(SdchManager::FIXED_CONTENT_ENCODINGS);
    }
  } else {
    // Remarkable case!?!  We advertised an SDCH dictionary, content-encoding
    // was not marked for SDCH processing: Why did the server suggest an SDCH
    // dictionary in the first place??.  Also, the content isn't
    // tagged as HTML, despite the fact that SDCH encoding is mostly likely for
    // HTML: Did some anti-virus system strip this tag (sometimes they strip
    // accept-encoding headers on the request)??  Does the content encoding not
    // start with "text/html" for some other reason??  We'll report this as a
    // fixup to a binary file, but it probably really is text/html (some how).
    if (encoding_types->empty()) {
      SdchManager::SdchErrorRecovery(
          SdchManager::BINARY_ADDED_CONTENT_ENCODING);
    } else if (1 == encoding_types->size()) {
      SdchManager::SdchErrorRecovery(
          SdchManager::BINARY_FIXED_CONTENT_ENCODING);
    } else {
      SdchManager::SdchErrorRecovery(
          SdchManager::BINARY_FIXED_CONTENT_ENCODINGS);
    }
  }

  encoding_types->clear();
  encoding_types->push_back(FILTER_TYPE_SDCH_POSSIBLE);
  encoding_types->push_back(FILTER_TYPE_GZIP_HELPING_SDCH);
  return;
}

// static
Filter* Filter::PrependNewFilter(FilterType type_id,
                                 const FilterContext& filter_context,
                                 Filter* filter_list) {
  Filter* first_filter = NULL;  // Soon to be start of chain.
  switch (type_id) {
    case FILTER_TYPE_GZIP_HELPING_SDCH:
    case FILTER_TYPE_DEFLATE:
    case FILTER_TYPE_GZIP: {
      scoped_ptr<GZipFilter> gz_filter(new GZipFilter(filter_context));
      if (gz_filter->InitBuffer()) {
        if (gz_filter->InitDecoding(type_id)) {
          first_filter = gz_filter.release();
        }
      }
      break;
    }
    case FILTER_TYPE_BZIP2: {
      scoped_ptr<BZip2Filter> bzip2_filter(new BZip2Filter(filter_context));
      if (bzip2_filter->InitBuffer()) {
        if (bzip2_filter->InitDecoding(false)) {
          first_filter = bzip2_filter.release();
        }
      }
      break;
    }
    case FILTER_TYPE_SDCH:
    case FILTER_TYPE_SDCH_POSSIBLE: {
      scoped_ptr<SdchFilter> sdch_filter(new SdchFilter(filter_context));
      if (sdch_filter->InitBuffer()) {
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

  if (!first_filter) {
    // Cleanup and exit, since we can't construct this filter list.
    delete filter_list;
    return NULL;
  }

  first_filter->next_filter_.reset(filter_list);
  return first_filter;
}

Filter::Filter(const FilterContext& filter_context)
    : stream_buffer_(NULL),
      stream_buffer_size_(0),
      next_stream_data_(NULL),
      stream_data_len_(0),
      next_filter_(NULL),
      last_status_(FILTER_NEED_MORE_DATA),
      filter_context_(filter_context) {
}

Filter::~Filter() {}

bool Filter::InitBuffer() {
  int buffer_size = filter_context_.GetInputStreamBufferSize();
  DCHECK(buffer_size > 0);
  if (buffer_size <= 0 || stream_buffer())
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
  const int dest_buffer_capacity = *dest_len;
  if (last_status_ == FILTER_ERROR)
    return last_status_;
  if (!next_filter_.get())
    return last_status_ = ReadFilteredData(dest_buffer, dest_len);
  if (last_status_ == FILTER_NEED_MORE_DATA && !stream_data_len())
    return next_filter_->ReadData(dest_buffer, dest_len);

  do {
    if (next_filter_->last_status() == FILTER_NEED_MORE_DATA) {
      PushDataIntoNextFilter();
      if (FILTER_ERROR == last_status_)
        return FILTER_ERROR;
    }
    *dest_len = dest_buffer_capacity;  // Reset the input/output parameter.
    next_filter_->ReadData(dest_buffer, dest_len);
    if (FILTER_NEED_MORE_DATA == last_status_)
        return next_filter_->last_status();

    // In the case where this filter has data internally, and is indicating such
    // with a last_status_ of FILTER_OK, but at the same time the next filter in
    // the chain indicated it FILTER_NEED_MORE_DATA, we have to be cautious
    // about confusing the caller.  The API confusion can appear if we return
    // FILTER_OK (suggesting we have more data in aggregate), but yet we don't
    // populate our output buffer.  When that is the case, we need to
    // alternately call our filter element, and the next_filter element until we
    // get out of this state (by pumping data into the next filter until it
    // outputs data, or it runs out of data and reports that it NEED_MORE_DATA.)
  } while (FILTER_OK == last_status_ &&
           FILTER_NEED_MORE_DATA == next_filter_->last_status() &&
           0 == *dest_len);

  if (next_filter_->last_status() == FILTER_ERROR)
    return FILTER_ERROR;
  return FILTER_OK;
}

void Filter::PushDataIntoNextFilter() {
  net::IOBuffer* next_buffer = next_filter_->stream_buffer();
  int next_size = next_filter_->stream_buffer_size();
  last_status_ = ReadFilteredData(next_buffer->data(), &next_size);
  if (FILTER_ERROR != last_status_)
    next_filter_->FlushStreamBuffer(next_size);
}


bool Filter::FlushStreamBuffer(int stream_data_len) {
  DCHECK(stream_data_len <= stream_buffer_size_);
  if (stream_data_len <= 0 || stream_data_len > stream_buffer_size_)
    return false;

  DCHECK(stream_buffer());
  // Bail out if there is more data in the stream buffer to be filtered.
  if (!stream_buffer() || stream_data_len_)
    return false;

  next_stream_data_ = stream_buffer()->data();
  stream_data_len_ = stream_data_len;
  return true;
}
