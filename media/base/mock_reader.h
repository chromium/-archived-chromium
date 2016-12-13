// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_BASE_MOCK_READER_H_
#define MEDIA_BASE_MOCK_READER_H_

#include <string>

#include "base/ref_counted.h"
#include "base/waitable_event.h"
#include "media/base/filters.h"

namespace media {

// Ref counted object so we can create callbacks for asynchronous Read()
// methods for any filter type.
template <class FilterType, class BufferType>
class MockReader :
    public base::RefCountedThreadSafe<MockReader<FilterType, BufferType> > {
 public:
  MockReader()
      : called_(false),
        expecting_call_(false) {
  }

  virtual ~MockReader() {
  }

  // Prepares this object for another read.
  void Reset() {
    DCHECK(!expecting_call_);
    expecting_call_ = false;
    called_ = false;
    buffer_ = NULL;
  }

  // Executes an asynchronous read on the given filter.
  void Read(FilterType* filter) {
    DCHECK(!expecting_call_);
    called_ = false;
    expecting_call_ = true;
    filter->Read(NewCallback(this, &MockReader::OnReadComplete));
  }

  // Mock accessors.
  BufferType* buffer() { return buffer_; }
  bool called() { return called_; }
  bool expecting_call() { return expecting_call_; }

 private:
  void OnReadComplete(BufferType* buffer) {
    DCHECK(!called_);
    DCHECK(expecting_call_);
    expecting_call_ = false;
    called_ = true;
    buffer_ = buffer;
  }

  // Reference to the buffer provided in the callback.
  scoped_refptr<BufferType> buffer_;

  // Whether or not the callback was executed.
  bool called_;

  // Whether or not this reader was expecting a callback.
  bool expecting_call_;

  DISALLOW_COPY_AND_ASSIGN(MockReader);
};

// Commonly used reader types.
typedef MockReader<DemuxerStream, Buffer> DemuxerStreamReader;
typedef MockReader<AudioDecoder, Buffer> AudioDecoderReader;
typedef MockReader<VideoDecoder, VideoFrame> VideoDecoderReader;

}  // namespace media

#endif  // MEDIA_BASE_MOCK_READER_H_
