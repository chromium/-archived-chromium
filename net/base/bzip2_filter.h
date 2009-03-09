// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BZip2Filter applies bzip2 content encoding/decoding to a datastream.
// Since it is a new feature, and no specification said what 's bzip2 content
// composed of  in http protocol.  So I assume with bzip2 encoding the content
// is full format, which means the content should carry complete bzip2 head,
// such as inlcude magic number 1(BZh), block size bit, magic number 2(0x31,
// 0x41, 0x59, 0x26, 0xx53, 0x59)
// Maybe need to inserts a bzlib2 header to the data stream before calling
// decompression functionality, but at now I do not meet this sort of real
// scenarios. So let's see the further requests.
//
// This BZip2Filter internally uses third_party/bzip2 library to do decoding.
//
// BZip2Filter is also a subclass of Filter. See the latter's header file
// filter.h for sample usage.

#ifndef NET_BASE_BZIP2_FILTER_H_
#define NET_BASE_BZIP2_FILTER_H_

#include "base/scoped_ptr.h"
#include "net/base/filter.h"
#include "third_party/bzip2/bzlib.h"

class BZip2Filter : public Filter {
 public:
  explicit BZip2Filter(const FilterContext& filter_context);

  virtual ~BZip2Filter();

  // Initializes filter decoding mode and internal control blocks.
  // Parameter use_small_memory specifies whether use small memory
  // to decompresss data. If small is nonzero, the bzip2 library will
  // use an alternative decompression algorithm which uses less memory
  // but at the cost of decompressing more slowly (roughly speaking,
  // half the speed, but the maximum memory requirement drops to
  // around 2300k). For more information, see doc in http://www.bzip.org.
  // The function returns true if success and false otherwise.
  // The filter can only be initialized once.
  bool InitDecoding(bool use_small_memory);

  // Decodes the pre-filter data and writes the output into the dest_buffer
  // passed in.
  // The function returns FilterStatus. See filter.h for its description.
  //
  // Since BZ2_bzDecompress need a full BZip header for decompression, so
  // the incoming data should have the full BZip header, otherwise this
  // function will give you nothing with FILTER_ERROR.
  //
  // Upon entry, *dest_len is the total size (in number of chars) of the
  // destination buffer. Upon exit, *dest_len is the actual number of chars
  // written into the destination buffer.
  //
  // This function will fail if there is no pre-filter data in the
  // stream_buffer_. On the other hand, *dest_len can be 0 upon successful
  // return. For example, the internal zlib may process some pre-filter data
  // but not produce output yet.
  virtual FilterStatus ReadFilteredData(char* dest_buffer, int* dest_len);

 private:
  enum DecodingStatus {
    DECODING_UNINITIALIZED,
    DECODING_IN_PROGRESS,
    DECODING_DONE,
    DECODING_ERROR
  };

  // Tracks the status of decoding.
  // This variable is initialized by InitDecoding and updated only by
  // ReadFilteredData.
  DecodingStatus decoding_status_;

  // The control block of bzip which actually does the decoding.
  // This data structure is initialized by InitDecoding and updated in
  // ReadFilteredData.
  scoped_ptr<bz_stream> bzip2_data_stream_;

  DISALLOW_COPY_AND_ASSIGN(BZip2Filter);
};

#endif  // NET_BASE_BZIP2_FILTER_H_

