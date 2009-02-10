// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_

// This is the default implementation of AudioRenderer, which uses the audio
// interfaces to open an audio device.  Although it cannot be used in the
// sandbox, it serves as a reference implementation and can be used in other
// applications such as the test player.

#include <deque>

#include "base/lock.h"
#include "media/audio/audio_output.h"
#include "media/base/buffers.h"
#include "media/base/factory.h"
#include "media/base/filters.h"

namespace media {

class AudioRendererImpl : public AudioRenderer,
                          public AudioOutputStream::AudioSourceCallback {
 public:
  static FilterFactory* CreateFilterFactory() {
    return new FilterFactoryImpl0<AudioRendererImpl>();
  }

  static bool IsMediaFormatSupported(const MediaFormat* media_format);

  // MediaFilter implementation.
  virtual void Stop();
  virtual void SetPlaybackRate(float playback_rate);

  // AudioRenderer implementation.
  virtual bool Initialize(AudioDecoder* decoder);
  virtual void SetVolume(float volume);

  // AssignableBuffer<AudioRendererImpl, BufferInterface> implementation.
  void OnAssignment(Buffer* buffer_in);

  // AudioSourceCallback implementation.
  virtual size_t OnMoreData(AudioOutputStream* stream, void* dest, size_t len);
  virtual void OnClose(AudioOutputStream* stream);
  virtual void OnError(AudioOutputStream* stream, int code);

 private:
  friend class FilterFactoryImpl0<AudioRendererImpl>;
  AudioRendererImpl();
  virtual ~AudioRendererImpl();

  // Helper to parse a media format and return whether we were successful
  // retrieving all the information we care about.
  static bool ParseMediaFormat(const MediaFormat* media_format,
                               int* channels_out, int* sample_rate_out,
                               int* sample_bits_out);

  // Posts a task on the pipeline thread to read a sample from the decoder.
  // Safe to call on any thread.
  void ScheduleRead();

  // Audio decoder.
  AudioDecoder* decoder_;

  // Audio output stream device.
  AudioOutputStream* stream_;

  // Whether or not we have initialized.
  bool initialized_;

  // Queued audio data.
  typedef std::deque<Buffer*> BufferQueue;
  BufferQueue input_queue_;
  Lock input_lock_;

  // Holds any remaining audio data that couldn't fit into the callback buffer.
  size_t data_offset_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_
