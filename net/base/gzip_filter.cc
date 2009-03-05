// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/gzip_filter.h"

#include "base/logging.h"
#include "net/base/gzip_header.h"
#include "third_party/zlib/zlib.h"

GZipFilter::GZipFilter()
    : decoding_status_(DECODING_UNINITIALIZED),
      decoding_mode_(DECODE_MODE_UNKNOWN),
      gzip_header_status_(GZIP_CHECK_HEADER_IN_PROGRESS),
      zlib_header_added_(false),
      gzip_footer_bytes_(0),
      possible_sdch_pass_through_(false) {
}

GZipFilter::~GZipFilter() {
  if (decoding_status_ != DECODING_UNINITIALIZED) {
    MOZ_Z_inflateEnd(zlib_stream_.get());
  }
}

bool GZipFilter::InitDecoding(Filter::FilterType filter_type) {
  if (decoding_status_ != DECODING_UNINITIALIZED)
    return false;

  // Initialize zlib control block
  zlib_stream_.reset(new z_stream);
  if (!zlib_stream_.get())
    return false;
  memset(zlib_stream_.get(), 0, sizeof(z_stream));

  // Set decoding mode
  switch (filter_type) {
    case Filter::FILTER_TYPE_DEFLATE: {
      if (inflateInit(zlib_stream_.get()) != Z_OK)
        return false;
      decoding_mode_ = DECODE_MODE_DEFLATE;
      break;
    }
    case Filter::FILTER_TYPE_GZIP_HELPING_SDCH:
      possible_sdch_pass_through_ =  true;  // Needed to optionally help sdch.
      // Fall through to GZIP case.
    case Filter::FILTER_TYPE_GZIP: {
      gzip_header_.reset(new GZipHeader());
      if (!gzip_header_.get())
        return false;
      if (inflateInit2(zlib_stream_.get(), -MAX_WBITS) != Z_OK)
        return false;
      decoding_mode_ = DECODE_MODE_GZIP;
      break;
    }
    default: {
      return false;
    }
  }

  decoding_status_ = DECODING_IN_PROGRESS;
  return true;
}

Filter::FilterStatus GZipFilter::ReadFilteredData(char* dest_buffer,
                                                  int* dest_len) {
  if (!dest_buffer || !dest_len || *dest_len <= 0)
    return Filter::FILTER_ERROR;

  if (decoding_status_ == DECODING_DONE) {
    if (GZIP_GET_INVALID_HEADER != gzip_header_status_)
      SkipGZipFooter();
    // Some server might send extra data after the gzip footer. We just copy
    // them out. Mozilla does this too.
    return CopyOut(dest_buffer, dest_len);
  }

  if (decoding_status_ != DECODING_IN_PROGRESS)
    return Filter::FILTER_ERROR;

  Filter::FilterStatus status;

  if (decoding_mode_ == DECODE_MODE_GZIP &&
      gzip_header_status_ == GZIP_CHECK_HEADER_IN_PROGRESS) {
    // With gzip encoding the content is wrapped with a gzip header.
    // We need to parse and verify the header first.
    status = CheckGZipHeader();
    switch (status) {
      case Filter::FILTER_NEED_MORE_DATA: {
        // We have consumed all input data, either getting a complete header or
        // a partial header. Return now to get more data.
        *dest_len = 0;
        // Partial header means it can't be an SDCH header.
        // Reason: SDCH *always* starts with 8 printable characters [a-zA-Z/_].
        // Gzip always starts with two non-printable characters.  Hence even a
        // single character (partial header) means that this can't be an SDCH
        // encoded body masquerading as a GZIP body.
        possible_sdch_pass_through_ = false;
        return status;
      }
      case Filter::FILTER_OK: {
        // The header checking succeeds, and there are more data in the input.
        // We must have got a complete header here.
        DCHECK_EQ(gzip_header_status_, GZIP_GET_COMPLETE_HEADER);
        break;
      }
      case Filter::FILTER_ERROR: {
        if (possible_sdch_pass_through_ &&
            GZIP_GET_INVALID_HEADER == gzip_header_status_) {
          decoding_status_ = DECODING_DONE;  // Become a pass through filter.
          return CopyOut(dest_buffer, dest_len);
        }
        decoding_status_ = DECODING_ERROR;
        return status;
      }
      default: {
        status = Filter::FILTER_ERROR;    // Unexpected.
        decoding_status_ = DECODING_ERROR;
        return status;
      }
    }
  }

  int dest_orig_size = *dest_len;
  status = DoInflate(dest_buffer, dest_len);

  if (decoding_mode_ == DECODE_MODE_DEFLATE && status == Filter::FILTER_ERROR) {
    // As noted in Mozilla implementation, some servers such as Apache with
    // mod_deflate don't generate zlib headers.
    // See 677409 for instances where this work around is needed.
    // Insert a dummy zlib header and try again.
    if (InsertZlibHeader()) {
      *dest_len = dest_orig_size;
      status = DoInflate(dest_buffer, dest_len);
    }
  }

  if (status == Filter::FILTER_DONE) {
    decoding_status_ = DECODING_DONE;
  } else if (status == Filter::FILTER_ERROR) {
    decoding_status_ = DECODING_ERROR;
  }

  return status;
}

Filter::FilterStatus GZipFilter::CheckGZipHeader() {
  DCHECK_EQ(gzip_header_status_, GZIP_CHECK_HEADER_IN_PROGRESS);

  // Check input data in pre-filter buffer.
  if (!next_stream_data_ || stream_data_len_ <= 0)
    return Filter::FILTER_ERROR;

  const char* header_end = NULL;
  GZipHeader::Status header_status;
  header_status = gzip_header_->ReadMore(next_stream_data_, stream_data_len_,
                                         &header_end);

  switch (header_status) {
    case GZipHeader::INCOMPLETE_HEADER: {
      // We read all the data but only got a partial header.
      next_stream_data_ = NULL;
      stream_data_len_ = 0;
      return Filter::FILTER_NEED_MORE_DATA;
    }
    case GZipHeader::COMPLETE_HEADER: {
      // We have a complete header. Check whether there are more data.
      int num_chars_left = static_cast<int>(stream_data_len_ -
                                            (header_end - next_stream_data_));
      gzip_header_status_ = GZIP_GET_COMPLETE_HEADER;

      if (num_chars_left > 0) {
        next_stream_data_ = const_cast<char*>(header_end);
        stream_data_len_ = num_chars_left;
        return Filter::FILTER_OK;
      } else {
        next_stream_data_ = NULL;
        stream_data_len_ = 0;
        return Filter::FILTER_NEED_MORE_DATA;
      }
    }
    case GZipHeader::INVALID_HEADER: {
      gzip_header_status_ = GZIP_GET_INVALID_HEADER;
      return Filter::FILTER_ERROR;
    }
    default: {
      break;
    }
  }

  return Filter::FILTER_ERROR;
}

Filter::FilterStatus GZipFilter::DoInflate(char* dest_buffer, int* dest_len) {
  // Make sure we have both valid input data and output buffer.
  if (!dest_buffer || !dest_len || *dest_len <= 0)  // output
    return Filter::FILTER_ERROR;

  if (!next_stream_data_ || stream_data_len_ <= 0)  // input
    return Filter::FILTER_ERROR;

  // Fill in zlib control block
  zlib_stream_.get()->next_in = bit_cast<Bytef*>(next_stream_data_);
  zlib_stream_.get()->avail_in = stream_data_len_;
  zlib_stream_.get()->next_out = bit_cast<Bytef*>(dest_buffer);
  zlib_stream_.get()->avail_out = *dest_len;

  int inflate_code = MOZ_Z_inflate(zlib_stream_.get(), Z_NO_FLUSH);
  int bytesWritten = *dest_len - zlib_stream_.get()->avail_out;

  Filter::FilterStatus status;

  switch (inflate_code) {
    case Z_STREAM_END: {
      *dest_len = bytesWritten;

      stream_data_len_ = zlib_stream_.get()->avail_in;
      next_stream_data_ = bit_cast<char*>(zlib_stream_.get()->next_in);

      SkipGZipFooter();

      status = Filter::FILTER_DONE;
      break;
    }
    case Z_BUF_ERROR: {
      // According to zlib documentation, when calling inflate with Z_NO_FLUSH,
      // getting Z_BUF_ERROR means no progress is possible. Neither processing
      // more input nor producing more output can be done.
      // Since we have checked both input data and output buffer before calling
      // inflate, this result is unexpected.
      status = Filter::FILTER_ERROR;
      break;
    }
    case Z_OK: {
      // Some progress has been made (more input processed or more output
      // produced).
      *dest_len = bytesWritten;

      // Check whether we have consumed all input data.
      stream_data_len_ = zlib_stream_.get()->avail_in;
      if (stream_data_len_ == 0) {
        next_stream_data_ = NULL;
        status = Filter::FILTER_NEED_MORE_DATA;
      } else {
        next_stream_data_ = bit_cast<char*>(zlib_stream_.get()->next_in);
        status = Filter::FILTER_OK;
      }
      break;
    }
    default: {
      status = Filter::FILTER_ERROR;
      break;
    }
  }

  return status;
}

bool GZipFilter::InsertZlibHeader() {
  static char dummy_head[2] = { 0x78, 0x1 };

  char dummy_output[4];

  // We only try add additional header once.
  if (zlib_header_added_)
    return false;

  MOZ_Z_inflateReset(zlib_stream_.get());
  zlib_stream_.get()->next_in = bit_cast<Bytef*>(&dummy_head[0]);
  zlib_stream_.get()->avail_in = sizeof(dummy_head);
  zlib_stream_.get()->next_out = bit_cast<Bytef*>(&dummy_output[0]);
  zlib_stream_.get()->avail_out = sizeof(dummy_output);

  int code = MOZ_Z_inflate(zlib_stream_.get(), Z_NO_FLUSH);
  zlib_header_added_ = true;

  return (code == Z_OK);
}


void GZipFilter::SkipGZipFooter() {
  int footer_bytes_expected = kGZipFooterSize - gzip_footer_bytes_;
  if (footer_bytes_expected > 0) {
    int footer_byte_avail = std::min(footer_bytes_expected, stream_data_len_);
    stream_data_len_ -= footer_byte_avail;
    next_stream_data_ += footer_byte_avail;
    gzip_footer_bytes_ += footer_byte_avail;

    if (stream_data_len_ == 0)
      next_stream_data_ = NULL;
  }
}

