// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_algorithm_default.h"

namespace media {

AudioRendererAlgorithmDefault::AudioRendererAlgorithmDefault()
    : data_offset_(0) {
}

AudioRendererAlgorithmDefault::~AudioRendererAlgorithmDefault() {
}

size_t AudioRendererAlgorithmDefault::FillBuffer(DataBuffer* buffer_out) {
  size_t dest_written = 0;
  if (playback_rate() == 0.0f) {
    return 0;
  }
  if (playback_rate() == 1.0f) {
    size_t dest_length = buffer_out->GetDataSize();
    uint8* dest = buffer_out->GetWritableData(dest_length);
    while (dest_length > 0 && !IsQueueEmpty()) {
      scoped_refptr<Buffer> buffer = FrontQueue();
      size_t data_length = buffer->GetDataSize() - data_offset_;

      // Prevent writing past end of the buffer.
      if (data_length > dest_length)
        data_length = dest_length;
      memcpy(dest, buffer->GetData() + data_offset_, data_length);
      dest += data_length;
      dest_length -= data_length;
      dest_written += data_length;

      data_offset_ += data_length;

      // We should not have run over the end of the buffer.
      DCHECK_GE(buffer->GetDataSize(), data_offset_);
      // but if we've reached the end, dequeue it.
      if (buffer->GetDataSize() - data_offset_ == 0) {
        PopFrontQueue();
        data_offset_ = 0;
      }
    }
  } else {
    // Mute (we will write to the whole buffer, so set |dest_written|
    // to the requested size).
    dest_written = buffer_out->GetDataSize();
    memset(buffer_out->GetWritableData(dest_written), 0, dest_written);

    // Discard any buffers that should be "used".
    size_t scaled_dest_length_remaining =
        static_cast<size_t>(dest_written * playback_rate());
    while (scaled_dest_length_remaining > 0 && !IsQueueEmpty()) {
      size_t data_length = FrontQueue()->GetDataSize() - data_offset_;

      // Last buffer we need.
      if (data_length > scaled_dest_length_remaining) {
        data_offset_ += scaled_dest_length_remaining;
      } else {
        scaled_dest_length_remaining -= data_length;
        PopFrontQueue();
        data_offset_ = 0;
      }
    }
    // If we ran out, don't report we used more than we did.
    if (IsQueueEmpty()) {
      dest_written -=
          static_cast<size_t>(scaled_dest_length_remaining / playback_rate());
    }
  }
  return dest_written;
}

}  // namespace media
