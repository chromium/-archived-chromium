// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioRendererAlgorithmBase provides an interface for algorithms that
// modify playback speed. ARAB owns its own deque of Buffers because
// subclasses may need more than one to produce output. Subclasses
// must implement the following method:
//
//   Process() - fills the buffer passed to it & returns how many bytes copied.
//
// Subclasses must also decrement |expected_queue_size_| after a dequeue.
//
// The general assumption is that the owner of this class will provide us with
// Buffers and a playback speed, and we will fill an output buffer when our
// owner requests it. If we needs more Buffers, we will query our owner via
// a callback passed during construction. This should be a nonblocking call.
// When the owner has a Buffer ready for us, it calls EnqueueBuffer().
//
// Exectution of ARAB is thread-unsafe. This class should be used as
// the guts of AudioRendererBase, which should lock calls into us so
// enqueues and processes do not cause an unpredictable |queue_| size.
//
// Most of ARAB is nonvirtual as the only subclasses we expect are those to
// define Process(). |queue_| management and common property management
// should not have to change for subclasses, so both are implemented here
// non-virtually.

// TODO(kylep): Clean this whole class up. Refactor out the |queue_| into a
// QueueManager or something like that.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_BASE_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_BASE_H_

#include <deque>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "media/base/data_buffer.h"
#include "media/base/buffers.h"

namespace media {

class AudioRendererAlgorithmBase {
 public:
  // Used to simplify callback declarations.
  typedef Callback0::Type RequestReadCallback;

  AudioRendererAlgorithmBase();
  virtual ~AudioRendererAlgorithmBase();

  // Checks validity of audio parameters and takes ownership of |callback|.
  virtual void Initialize(int channels,
                          int sample_rate,
                          int sample_bits,
                          float initial_playback_rate,
                          RequestReadCallback* callback);

  // Implement this strategy method in derived classes.
  // Fills |buffer_out| with possibly scaled data from our |queue_|.
  // |buffer_out| must be initialized and have a datasize.
  // Returns the number of bytes copied into |buffer_out|.
  // TODO(kylep): Think of a better name.
  virtual size_t Process(DataBuffer* buffer_out) = 0;

  // Clears |queue_|.
  void FlushBuffers();

  // Enqueues a buffer. It is called from the owner
  // of the algorithm after a read completes.
  void EnqueueBuffer(Buffer* buffer_in);

  // Sets the playback rate.
  void SetPlaybackRate(float new_rate);

 protected:
  // Fills the |queue_|. This is a helper function called from
  // Initialize() to initially fill |queue_| and from subclasses'
  // Process() method to refill |queue_| after buffers have been used.
  void FillQueue();

  // Audio properties.
  int channels_;
  int sample_rate_;
  int sample_bits_;

  // Used by algorithm to scale output.
  float playback_rate_;

  // Used to request more data.
  scoped_ptr<RequestReadCallback> request_read_callback_;

  // Keeps track of the number of buffers we will have in |queue_|
  // after all reads complete. Subclasses MUST decrement this after
  // dequeuing.
  size_t expected_queue_size_;

  // Queued audio data.
  typedef std::deque< scoped_refptr<Buffer> > BufferQueue;
  BufferQueue queue_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioRendererAlgorithmBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_BASE_H_
