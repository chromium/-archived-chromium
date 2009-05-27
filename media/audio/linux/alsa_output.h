// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Creates an output stream based on the ALSA PCM interface.  The current
// implementation creates one thread per ALSA playback handle that is
// responsible for synchronously pulling data from the audio data source.
//
// This output stream buffers in two places:
//   (1) In the ALSA device
//   (2) In an in-memory buffer.
//
// The ALSA device buffer is kept as full as possible.  The in-memory buffer
// attempts to keep enough extra data so that |min_buffer_ms| worth of data
// is available between the in-memory buffer and the device buffer.  Requests
// to the audio data source are made if the total amount buffered falls below
// |min_buffer_ms|.
//
// On device write failure, the stream will move into an invalid state.  No
// more data will be pulled from the data source, and the playback thread will
// be stopped.
//
// If the stream is successfully opened, Close() must be called before the
// stream is deleted.

#ifndef MEDIA_AUDIO_LINUX_ALSA_OUTPUT_H_
#define MEDIA_AUDIO_LINUX_ALSA_OUTPUT_H_

#include <alsa/asoundlib.h>

#include <deque>
#include <string>

#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "media/audio/audio_output.h"

class Thread;

class AlsaPCMOutputStream :
    public AudioOutputStream,
    public base::RefCountedThreadSafe<AlsaPCMOutputStream> {
 public:
  // Set to "default" which should avoid locking the sound device and allow
  // ALSA to multiplex sound from different processes that want to write PCM
  // data.
  static const char* kDefaultDevice;

  // Create a PCM Output stream for the ALSA device identified by
  // |device_name|.  If unsure of hte device_name, use kDefaultDevice.
  AlsaPCMOutputStream(const std::string& device_name,
                      int min_buffer_ms,
                      AudioManager::Format format,
                      int channels,
                      int sample_rate,
                      char bits_per_sample);
  virtual ~AlsaPCMOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open(size_t packet_size);
  virtual void Close();
  virtual void Start(AudioSourceCallback* callback);
  virtual void Stop();
  virtual void SetVolume(double left_level, double right_level);
  virtual void GetVolume(double* left_level, double* right_level);
  virtual size_t GetNumBuffers();

 private:
  // Closes the playback handle, reporting errors if any occur.  Returns true
  // if the device was successfully closed.
  bool CloseDevice_Locked();

  // Stops playback, ignoring state checks.
  void StopInternal_Locked();

  // Moves the stream into the error state, setting the correct internal flags.
  // Ensure that all resources are cleaned up before executing this function.
  void EnterStateError_Locked();

  // Releases all the resources in the audio stream.  This method will also
  // terminate the playback thread itself.
  //
  // This method must be run in the |playback_thead_|.
  void ReleaseResources();

  // Retrieve the total number of frames buffered in both memory and in the
  // audio device.  Use this to determine if more data should be requested from
  // the audio source.
  snd_pcm_sframes_t GetFramesOfDelay_Locked();

  // Buffer more packets from data source if necessary.
  //
  // This function must be run in the |playback_thread_|.
  void BufferPackets();

  // Returns true if our buffer is underfull.
  bool ShouldBufferMore_NoLock();

  // Write as many buffered packets into the device as there is room in the
  // device buffer.
  //
  // This function must be run in the |playback_thread_|.
  void FillAlsaDeviceBuffer();

  // State of the stream.
  State state_;

  // The ALSA device name to use.
  std::string device_name_;

  // Handle to the actual PCM playback device.
  snd_pcm_t* playback_handle_;

  // Period size for ALSA ring-buffer.  Basically, how long to wait between
  // writes.
  snd_pcm_sframes_t period_size_;

  // Callback used to request more data from the data source.
  AudioSourceCallback* source_callback_;

  // Playback thread.
  base::Thread playback_thread_;

  // Lock for field access to this object.
  Lock lock_;

  // Sample format configuration.
  snd_pcm_format_t pcm_format_;
  const int channels_;
  const int sample_rate_;
  const char bits_per_sample_;
  char bytes_per_frame_;

  // In-memory buffer to hold sound samples before pushing to the sound device.
  // TODO(ajwong): There are now about 3 buffer queue implementations. Factor
  // them out.
  struct Packet {
    explicit Packet(int new_capacity)
        : capacity(new_capacity),
          size(0),
          used(0),
          buffer(new char[capacity]) {
    }
    size_t capacity;
    size_t size;
    size_t used;
    scoped_array<char> buffer;
  };
  int min_buffer_frames_;
  std::deque<Packet*> buffered_packets_;
  size_t packet_size_;

  // Flag indiciating the device write tasks have stopped scheduling
  // themselves.  This should only be modified by the BufferPackets() and
  // FillAlsaDeviceBuffer() methods.
  bool device_write_suspended_;

  // Flag indicating that the resources are already cleaned.
  bool resources_released_;

  DISALLOW_COPY_AND_ASSIGN(AlsaPCMOutputStream);
};

#endif  // MEDIA_AUDIO_LINUX_ALSA_OUTPUT_H_
