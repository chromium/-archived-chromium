// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Filter performs filtering on data streams. Sample usage:
//
//   IStream* pre_filter_source;
//   ...
//   Filter* filter = Filter::Factory(filter_type, size);
//   int pre_filter_data_len = filter->stream_buffer_size();
//   pre_filter_source->read(filter->stream_buffer(), pre_filter_data_len);
//
//   filter->FlushStreamBuffer(pre_filter_data_len);
//
//   char post_filter_buf[kBufferSize];
//   int post_filter_data_len = kBufferSize;
//   filter->ReadFilteredData(post_filter_buf, &post_filter_data_len);
//
// To filter a data stream, the caller first gets filter's stream_buffer_
// through its accessor and fills in stream_buffer_ with pre-filter data, next
// calls FlushStreamBuffer to notify Filter, then calls ReadFilteredData
// repeatedly to get all the filtered data. After all data have been fitlered
// and read out, the caller may fill in stream_buffer_ again. This
// WriteBuffer-Flush-Read cycle is repeated until reaching the end of data
// stream.
//
// The lifetime of a Filter instance is completely controlled by its caller.

#ifndef NET_BASE_FILTER_H__
#define NET_BASE_FILTER_H__

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "net/base/io_buffer.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class Filter {
 public:
  // Return values of function ReadFilteredData.
  enum FilterStatus {
    // Read filtered data successfully
    FILTER_OK,
    // Read filtered data successfully, and the data in the buffer has been
    // consumed by the filter, but more data is needed in order to continue
    // filtering.  At this point, the caller is free to reuse the filter
    // buffer to provide more data.
    FILTER_NEED_MORE_DATA,
    // Read filtered data successfully, and filter reaches the end of the data
    // stream.
    FILTER_DONE,
    // There is an error during filtering.
    FILTER_ERROR
  };

  // Specifies type of filters that can be created.
  enum FilterType {
    FILTER_TYPE_DEFLATE,
    FILTER_TYPE_GZIP,
    FILTER_TYPE_BZIP2,
    FILTER_TYPE_GZIP_HELPING_SDCH,  // Gzip possible, but pass through allowed.
    FILTER_TYPE_SDCH,
    FILTER_TYPE_SDCH_POSSIBLE,  // Sdch possible, but pass through allowed.
    FILTER_TYPE_UNSUPPORTED,
  };


  virtual ~Filter();

  // Creates a Filter object.
  // Parameters: Filter_types specifies the type of filter created; Buffer_size
  // specifies the size (in number of chars) of the buffer the filter should
  // allocate to hold pre-filter data.
  // If success, the function returns the pointer to the Filter object created.
  // If failed or a filter is not needed, the function returns NULL.
  //
  // Note: filter_types is an array of filter names (content encoding types as
  // provided in an HTTP header), which will be chained together serially do
  // successive filtering of data.  The names in the vector are ordered based on
  // encoding order, and the filters are chained to operate in the reverse
  // (decoding) order. For example, types[0] = "sdch", types[1] = "gzip" will
  // cause data to first be gunizip filtered, and the resulting output from that
  // filter will be sdch decoded.
  static Filter* Factory(const std::vector<FilterType>& filter_types,
                         int buffer_size);

  // External call to obtain data from this filter chain.  If ther is no
  // next_filter_, then it obtains data from this specific filter.
  FilterStatus ReadData(char* dest_buffer, int* dest_len);

  // Returns a pointer to the stream_buffer_.
  net::IOBuffer* stream_buffer() const { return stream_buffer_.get(); }

  // Returns the maximum size of stream_buffer_ in number of chars.
  int stream_buffer_size() const { return stream_buffer_size_; }

  // Returns the total number of chars remaining in stream_buffer_ to be
  // filtered.
  //
  // If the function returns 0 then all data have been filtered and the caller
  // is safe to copy new data into stream_buffer_.
  int stream_data_len() const { return stream_data_len_; }

  // Flushes stream_buffer_ for next round of filtering. After copying data to
  // stream_buffer_, the caller should call this function to notify Filter to
  // start filtering. Then after this function is called, the caller can get
  // post-filtered data using ReadFilteredData. The caller must not write to
  // stream_buffer_ and call this function again before stream_buffer_ is empty
  // out by ReadFilteredData.
  //
  // The input stream_data_len is the length (in number of chars) of valid
  // data in stream_buffer_. It can not be greater than stream_buffer_size_.
  // The function returns true if success, and false otherwise.
  bool FlushStreamBuffer(int stream_data_len);

  void SetURL(const GURL& url);
  const GURL& url() const { return url_; }

  void SetMimeType(const std::string& mime_type);
  const std::string& mime_type() const { return mime_type_; }

  void SetConnectTime(const base::Time& time, bool was_cached);

  // Translate the text of a filter name (from Content-Encoding header) into a
  // FilterType.
  static FilterType ConvertEncodingToType(const std::string& filter_type);

  // Given a array of encoding_types, try to do some error recovery adjustment
  // to the list.  This includes handling known bugs in the Apache server (where
  // redundant gzip encoding is specified), as well as issues regarding SDCH
  // encoding, where various proxies and anti-virus products modify or strip the
  // encodings.  These fixups require context, which includes whether this
  // response was made to an SDCH request (i.e., an available dictionary was
  // advertised in the GET), as well as the mime type of the content.
  static void FixupEncodingTypes(bool is_sdch_response,
                                 const std::string& mime_type,
                                 std::vector<FilterType>* encoding_types);
 protected:
  Filter();

  FRIEND_TEST(SdchFilterTest, ContentTypeId);
  // Filters the data stored in stream_buffer_ and writes the output into the
  // dest_buffer passed in.
  //
  // Upon entry, *dest_len is the total size (in number of chars) of the
  // destination buffer. Upon exit, *dest_len is the actual number of chars
  // written into the destination buffer.
  //
  // This function will fail if there is no pre-filter data in the
  // stream_buffer_. On the other hand, *dest_len can be 0 upon successful
  // return. For example, a decoding filter may process some pre-filter data
  // but not produce output yet.
  virtual FilterStatus ReadFilteredData(char* dest_buffer, int* dest_len);

  // Copy pre-filter data directly to destination buffer without decoding.
  FilterStatus CopyOut(char* dest_buffer, int* dest_len);

  // Allocates and initializes stream_buffer_.
  // Buffer_size is the maximum size of stream_buffer_ in number of chars.
  bool InitBuffer(int buffer_size);

  // A factory helper for creating filters for within a chain of potentially
  // multiple encodings.  If a chain of filters is created, then this may be
  // called multiple times during the filter creation process.  In most simple
  // cases, this is only called once. Returns NULL and cleans up (deleting
  // filter_list) if a new filter can't be constructed.
  static Filter* PrependNewFilter(FilterType type_id, int buffer_size,
                                  Filter* filter_list);

  FilterStatus last_status() const { return last_status_; }

  base::Time connect_time() const { return connect_time_; }

  bool was_cached() const { return was_cached_; }

  // Buffer to hold the data to be filtered.
  scoped_refptr<net::IOBuffer> stream_buffer_;

  // Maximum size of stream_buffer_ in number of chars.
  int stream_buffer_size_;

  // Pointer to the next data in stream_buffer_ to be filtered.
  char* next_stream_data_;

  // Total number of remaining chars in stream_buffer_ to be filtered.
  int stream_data_len_;

  // The URL that is currently being filtered.
  // This is used by SDCH filters which need to restrict use of a dictionary to
  // a specific URL or path.
  GURL url_;

  // To facilitate histogramming by individual filters, we store the connect
  // time for the corresponding HTTP transaction, as well as whether this time
  // was recalled from a cached entry.
  base::Time connect_time_;
  bool was_cached_;

  // To facilitate error recovery in SDCH filters, allow filter to know if
  // content is text/html by checking within this mime type (SDCH filter may
  // do a meta-refresh via html).
  std::string mime_type_;

  // An optional filter to process output from this filter.
  scoped_ptr<Filter> next_filter_;
  // Remember what status or local filter last returned so we can better handle
  // chained filters.
  FilterStatus last_status_;

  DISALLOW_COPY_AND_ASSIGN(Filter);
};

#endif  // NET_BASE_FILTER_H__

