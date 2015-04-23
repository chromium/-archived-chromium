// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioRendererBase takes care of the tricky queuing work and provides simple
// methods for subclasses to peek and poke at audio data.  In addition to
// AudioRenderer interface methods this classes doesn't implement, subclasses
// must also implement the following methods:
//   OnInitialized
//   OnStop
//
// The general assumption is that subclasses start a callback-based audio thread
// which needs to be filled with decoded audio data.  AudioDecoderBase provides
// FillBuffer which handles filling the provided buffer, dequeuing items,
// scheduling additional reads and updating the clock.  In a sense,
// AudioRendererBase is the producer and the subclass is the consumer.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_BASE_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_BASE_H_

#include <deque>

#include "base/lock.h"
#include "media/base/buffers.h"
#include "media/base/factory.h"
#include "media/base/filters.h"

namespace media {

class AudioRendererBase : public AudioRenderer {
 public:
  // MediaFilter implementation.
  virtual void Stop();

  virtual void Seek(base::TimeDelta time);

  // AudioRenderer implementation.
  virtual bool Initialize(AudioDecoder* decoder);

 protected:
  // The default maximum size of the queue.
  static const size_t kDefaultMaxQueueSize;

  // Only allow a factory to create this class.
  explicit AudioRendererBase(size_t max_queue_size);
  virtual ~AudioRendererBase();

  // Called by Initialize().  |media_format| is the format of the AudioDecoder.
  // Subclasses should return true if they were able to initialize, false
  // otherwise.
  virtual bool OnInitialize(const MediaFormat& media_format) = 0;

  // Called by Stop().  Subclasses should perform any necessary cleanup during
  // this time, such as stopping any running threads.
  virtual void OnStop() = 0;

  // Called when a AudioDecoder::Read() completes.
  virtual void OnReadComplete(Buffer* buffer_in);

  // Fills the given buffer with audio data by dequeuing buffers and copying the
  // data into the |dest|.  FillBuffer() also takes care of updating the clock.
  // Returns the number of bytes copied into |dest|, which may be less than
  // equal to |len|.
  //
  // If this method is returns less bytes than |len| (including zero), it could
  // be a sign that the pipeline is stalled or unable to stream the data fast
  // enough.  In such scenarios, the callee should zero out unused portions
  // of their buffer to playback silence.
  //
  // FillBuffer() updates the pipeline's playback timestamp. If FillBuffer() is
  // not called at the same rate as audio samples are played, then the reported
  // timestamp in the pipeline will be ahead of the actual audio playback. In
  // this case |playback_delay| should be used to indicate when in the future
  // should the filled buffer be played. If FillBuffer() is called as the audio
  // hardware plays the buffer, then |playback_delay| should be zero.
  //
  // Safe to call on any thread.
  size_t FillBuffer(uint8* dest,
                    size_t len,
                    float rate,
                    const base::TimeDelta& playback_delay);

  // Helper to parse a media format and return whether we were successful
  // retrieving all the information we care about.
  static bool ParseMediaFormat(const MediaFormat& media_format,
                               int* channels_out, int* sample_rate_out,
                               int* sample_bits_out);

 private:
  // Helper method that schedules an asynchronous read from the decoder.
  //
  // Safe to call from any thread.
  void ScheduleRead();

  // Audio decoder.
  scoped_refptr<AudioDecoder> decoder_;

  // Maximum queue size, configuration parameter passed in during construction.
  size_t max_queue_size_;

  // Queued audio data.
  typedef std::deque< scoped_refptr<Buffer> > BufferQueue;
  BufferQueue queue_;
  Lock lock_;

  // Remembers the amount of remaining audio data for the front buffer.
  size_t data_offset_;

  // Whether or not we're initialized.
  bool initialized_;

  // Whether or not we've stopped.
  bool stopped_;

  // Audio time at end of last call to FillBuffer().
  // TODO(ralphl): Update this value after seeking.
  base::TimeDelta last_fill_buffer_time_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_BASE_H_
