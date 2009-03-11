// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_H_

#include "base/basictypes.h"

// Low-level audio output support. To make sound there are 3 objects involved:
// - AudioSource : produces audio samples on a pull model. Implements
//   the AudioSourceCallback interface.
// - AudioOutputStream : uses the AudioSource to render audio on a given
//   channel, format and sample frequency configuration. Data from the
//   AudioSource is delivered in a 'pull' model.
// - AudioManager : factory for the AudioOutputStream objects, manager
//   of the hardware resources and mixer control.
//
// The number and configuration of AudioOutputStream does not need to match the
// physically available hardware resources. For example you can have:
//
//  MonoPCMSource1 --> MonoPCMStream1 --> |       | --> audio left channel
//  StereoPCMSource -> StereoPCMStream -> | mixer |
//  MonoPCMSource2 --> MonoPCMStream2 --> |       | --> audio right channel
//
// This facility's objective is mix and render audio with low overhead using
// the OS basic audio support, abstracting as much as possible the
// idiosyncrasies of each platform. Non-goals:
// - Positional, 3d audio
// - Dependence on non-default libraries such as DirectX 9, 10, XAudio
// - Digital signal processing or effects
// - Extra features if a specific hardware is installed (EAX, X-fi)
//
// The primary client of this facility is audio coming from several tabs.
// Specifically for this case we avoid supporting complex formats such as MP3
// or WMA. Complex format decoding should be done by the renderers.


// Models an audio stream that gets rendered to the audio hardware output.
// Because we support more audio streams than physically available channels
// a given AudioOutputStream might or might not talk directly to hardware.
class AudioOutputStream {
 public:
  enum State {
    STATE_STARTED = 0,  // The output stream is started.
    STATE_PAUSED,       // The output stream is paused.
    STATE_ERROR,        // The output stream is in error state.
  };

  // Audio sources must implement AudioSourceCallback. This interface will be
  // called in a random thread which very likely is a high priority thread. Do
  // not rely on using this thread TLS or make calls that alter the thread
  // itself such as creating Windows or initializing COM.
  class AudioSourceCallback {
   public:
    virtual ~AudioSourceCallback() {}

    // Provide more data by filling |dest| up to |max_size| bytes. The provided
    // buffer size is usually what is specified in Open(). The source
    // will return the number of bytes it filled. The expected structure of
    // |dest| is platform and format specific.
    virtual size_t OnMoreData(AudioOutputStream* stream,
                              void* dest, size_t max_size) = 0;

    // The stream is done with this callback. After this call the audio source
    // can go away or be destroyed.
    virtual void OnClose(AudioOutputStream* stream) = 0;

    // There was an error while playing a buffer. Audio source cannot be
    // destroyed yet. No direct action needed by the AudioStream, but it is
    // a good place to stop accumulating sound data since is is likely that
    // playback will not continue. |code| is an error code that is platform
    // specific.
    virtual void OnError(AudioOutputStream* stream, int code) = 0;
  };

  // Open the stream. |packet_size| is the requested buffer allocation which
  // the audio source thinks it can usually fill without blocking. Internally
  // two buffers of |packet_size| size are created, one will be locked for
  // playback and one will be ready to be filled in the call to
  // AudioSourceCallback::OnMoreData().
  virtual bool Open(size_t packet_size) = 0;

  // Starts playing audio and generating AudioSourceCallback::OnMoreData().
  virtual void Start(AudioSourceCallback* callback) = 0;

  // Stops playing audio. Effect might no be instantaneous as the hardware
  // might have locked audio data that is processing.
  virtual void Stop() = 0;

  // Sets the relative volume, with range [0.0, 1.0] inclusive. For mono audio
  // sources the volume must be the same in both channels.
  virtual void SetVolume(double left_level, double right_level) = 0;

  // Gets the relative volume, with range [0.0, 1.0] inclusive. For mono audio
  // sources the level is returned in both channels.
  virtual void GetVolume(double* left_level, double* right_level) = 0;

  // Close the stream. This also generates AudioSourceCallback::OnClose().
  // After calling this method, the object should not be used anymore.
  virtual void Close() = 0;

 protected:
  virtual ~AudioOutputStream() {}
};

// Manages all audio resources. In particular it owns the AudioOutputStream
// objects. Provides some convenience functions that avoid the need to provide
// iterators over the existing streams.
class AudioManager {
 public:
  enum Format {
    AUDIO_PCM_LINEAR = 0, // Pulse code modulation means 'raw' amplitude
                          // samples.
    AUDIO_PCM_DELTA,      // Delta-encoded pulse code modulation.
    AUDIO_MOCK,           // Creates a dummy AudioOutputStream object.
    AUDIO_LAST_FORMAT     // Only used for validation of format.
  };

  // Telephone quality sample rate, mostly for speech-only audio.
  static const int kTelephoneSampleRate = 8000;
  // CD sampling rate is 44.1 KHz or conveniently 2x2x3x3x5x5x7x7.
  static const int kAudioCDSampleRate = 44100;
  // Digital Audio Tape sample rate.
  static const int kAudioDATSampleRate = 48000;

  // Returns true if the OS reports existence of audio devices. This does not
  // guarantee that the existing devices support all formats and sample rates.
  virtual bool HasAudioDevices() = 0;

  // Factory for all the supported stream formats. At this moment |channels|
  // can be 1 (mono) or 2 (stereo). The |sample_rate| is in hertz and can be
  // any value supported by the underlying platform. For some future formats
  // the |sample_rate| and |bits_per_sample| can take special values.
  // Returns NULL if the combination of the parameters is not supported, or if
  // we have reached some other platform specific limit.
  //
  // Do not free the returned AudioOutputStream. It is owned by AudioManager.
  virtual AudioOutputStream* MakeAudioStream(Format format, int channels,
                                             int sample_rate,
                                             char bits_per_sample) = 0;

  // Muting continues playback but effectively the volume is set to zero.
  // Un-muting returns the volume to the previous level.
  virtual void MuteAll() = 0;
  virtual void UnMuteAll() = 0;

  // For testing purposes only. Returns the internal buffer of the last
  // AUDIO_MOCK AudioOutputStream closed. Returns NULL if none closed yet.
  // The buffer size is the same as passed to AudioOutputStream::Open().
  virtual const void* GetLastMockBuffer() = 0;

  // Get AudioManager singleton.
  // TODO(cpu): Define threading requirements for interacting with AudioManager.
  static AudioManager* GetAudioManager();

 protected:
  virtual ~AudioManager() {}
};


#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_H_
