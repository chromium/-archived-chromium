// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_SIMPLE_SOURCES_H_
#define MEDIA_AUDIO_SIMPLE_SOURCES_H_

#include <list>

#include "base/lock.h"
#include "media/audio/audio_output.h"

// An audio source that produces a pure sinusoidal tone.
class SineWaveAudioSource : public AudioOutputStream::AudioSourceCallback {
 public:
  enum Format {
    FORMAT_8BIT_LINEAR_PCM,
    FORMAT_16BIT_LINEAR_PCM,
  };
  // |channels| is the number of audio channels, |freq| is the frequency in
  // hertz and it has to be less than half of the sampling frequency
  // |sample_freq| or else you will get aliasing.
  SineWaveAudioSource(Format format, int channels,
                      double freq, double sample_freq);
  virtual ~SineWaveAudioSource() {}

  // Implementation of AudioSourceCallback.
  virtual size_t OnMoreData(AudioOutputStream* stream,
                            void* dest, size_t max_size);
  virtual void OnClose(AudioOutputStream* stream);
  virtual void OnError(AudioOutputStream* stream, int code);

 protected:
  Format format_;
  int channels_;
  double freq_;
  double sample_freq_;
};

// Defines an interface for pushing audio output. In contrast, the interfaces
// defined by AudioSourceCallback are pull model only.
class PushAudioOutput {
 public:
  virtual ~PushAudioOutput(){}

  // Write audio data to the audio device. It will be played eventually.
  // Returns false on failure.
  virtual bool Write(const void* data, size_t len) = 0;

  // Returns the number of bytes that have been buffered but not yet given
  // to the audio device.
  virtual size_t UnProcessedBytes() = 0;
};

// A fairly basic class to connect a push model provider PushAudioOutput to
// a pull model provider AudioSourceCallback. Fundamentally it manages a series
// of audio buffers and is unaware of the actual audio format.
class PushSource : public AudioOutputStream::AudioSourceCallback,
                   public PushAudioOutput {
 public:
  // Construct the audio source. Pass the same |packet_size| specified in the
  // AudioOutputStream::Open() here.
  // TODO(hclam): |packet_size| is not used anymore, remove it.
  explicit PushSource(size_t packet_size);
  virtual ~PushSource();

  // Write one buffer. The ideal size is |packet_size| but smaller sizes are
  // accepted.
  virtual bool Write(const void* data, size_t len);

  // Return the total number of bytes not given to the audio device yet.
  virtual size_t UnProcessedBytes();

  // Implementation of AudioSourceCallback.
  virtual size_t OnMoreData(AudioOutputStream* stream,
                            void* dest, size_t max_size);
  virtual void OnClose(AudioOutputStream* stream);
  virtual void OnError(AudioOutputStream* stream, int code);

 private:
  // Defines the unit of playback. We own the memory pointed by |buffer|. 
  struct Packet {
    char* buffer;
    size_t size;
  };

  // Free acquired resources.
  void CleanUp();

  const size_t packet_size_;
  typedef std::list<Packet> PacketList; 
  PacketList packets_;
  size_t buffered_bytes_;
  size_t front_buffer_consumed_;
  // Serialize access to packets_ and buffered_bytes_ using this lock.
  Lock lock_;
};

#endif  // MEDIA_AUDIO_SIMPLE_SOURCES_H_
