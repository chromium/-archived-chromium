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
// To filters a data stream, the caller first gets filter's stream_buffer_
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

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

class Filter {
 public:
  // Creates a Filter object.
  // Parameters: Filter_type specifies the type of filter created; Buffer_size
  // specifies the size (in number of chars) of the buffer the filter should
  // allocate to hold pre-filter data.
  // If success, the function returns the pointer to the Filter object created.
  // If failed or a filter is not needed, the function returns NULL.
  static Filter* Factory(const std::string& filter_type,
                         const std::string& mime_type,
                         int buffer_size);

  virtual ~Filter();

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

  // Returns a pointer to the beginning of stream_buffer_.
  char* stream_buffer() const { return stream_buffer_.get(); }

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

 protected:
  Filter();

  // Copy pre-filter data directly to destination buffer without decoding.
  FilterStatus CopyOut(char* dest_buffer, int* dest_len);

  // Specifies type of filters that can be created.
  enum FilterType {
    FILTER_TYPE_DEFLATE,
    FILTER_TYPE_GZIP,
    FILTER_TYPE_BZIP2,
    FILTER_TYPE_UNSUPPORTED
  };

  // Allocates and initializes stream_buffer_.
  // Buffer_size is the maximum size of stream_buffer_ in number of chars.
  bool InitBuffer(int buffer_size);

  // Buffer to hold the data to be filtered.
  scoped_array<char> stream_buffer_;

  // Maximum size of stream_buffer_ in number of chars.
  int stream_buffer_size_;

  // Pointer to the next data in stream_buffer_ to be filtered.
  char* next_stream_data_;

  // Total number of remaining chars in stream_buffer_ to be filtered.
  int stream_data_len_;

  // Filter can be chained
  // TODO (huanr)
  // Filter* next_filter_;

  DISALLOW_EVIL_CONSTRUCTORS(Filter);
};

#endif  // NET_BASE_FILTER_H__

