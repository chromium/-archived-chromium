// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_algorithm_default.h"

#include "media/base/buffers.h"
#include "media/base/data_buffer.h"

namespace media {

AudioRendererAlgorithmDefault::AudioRendererAlgorithmDefault() {
}

AudioRendererAlgorithmDefault::~AudioRendererAlgorithmDefault() {
}

size_t AudioRendererAlgorithmDefault::FillBuffer(DataBuffer* buffer_out) {
  if (playback_rate() == 0.0f) {
    return 0;
  }

  size_t dest_written = 0;

  if (playback_rate() == 1.0f) {
    size_t dest_length = buffer_out->GetDataSize();
    uint8* dest = buffer_out->GetWritableData(dest_length);

    // If we don't have enough data, copy what we have.
    if (QueueSize() < dest_length)
      dest_written = CopyFromInput(dest, QueueSize());
    else
      dest_written = CopyFromInput(dest, dest_length);
    AdvanceInputPosition(dest_written);
  } else {
    // Mute (we will write to the whole buffer, so set |dest_written| to the
    // requested size).
    dest_written = buffer_out->GetDataSize();
    memset(buffer_out->GetWritableData(dest_written), 0, dest_written);

    // Calculate the number of bytes we "used".
    size_t scaled_dest_length =
        static_cast<size_t>(dest_written * playback_rate());

    // If this is more than we have, report the correct amount consumed.
    if (QueueSize() < scaled_dest_length) {
      scaled_dest_length = QueueSize();
      dest_written = static_cast<size_t>(scaled_dest_length / playback_rate());
    }
    AdvanceInputPosition(scaled_dest_length);
  }
  return dest_written;
}

}  // namespace media
