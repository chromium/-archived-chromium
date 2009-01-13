// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WAVEOUT_OUTPUT_WIN_H_
#define MEDIA_AUDIO_WAVEOUT_OUTPUT_WIN_H_

#include <windows.h>
#include <mmsystem.h>

#include "base/basictypes.h"
#include "base/scoped_handle_win.h"
#include "media/audio/audio_output.h"

class AudioManagerWin;

// Implements PCM audio output support for Windows using the WaveXXX API.
// While not as nice as the DirectSound-based API, it should work in all target
// operating systems regardless or DirectX version installed. It is known that
// in some machines WaveXXX based audio is better while in others DirectSound
// is better.
//
// Important: the OnXXXX functions in AudioSourceCallback are called by more
// than one thread so it is important to have some form of synchronization if
// you are keeping state in it.
class PCMWaveOutAudioOutputStream : public AudioOutputStream {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object and |device_id| which
  // is provided by the operating system.
  PCMWaveOutAudioOutputStream(AudioManagerWin* manager,
                              int channels, int sampling_rate,
                              char bits_per_sample, UINT device_id);
  virtual ~PCMWaveOutAudioOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open(size_t packet_size);
  virtual void Close();
  virtual void Start(AudioSourceCallback* callback);
  virtual void Stop();
  virtual void SetVolume(double left_level, double right_level);
  virtual void GetVolume(double* left_level, double* right_level);

  // Sends a buffer to the audio driver for playback.
  void QueueNextPacket(WAVEHDR* buffer);

 private:
  enum State {
    PCMA_BRAND_NEW,    // Initial state.
    PCMA_READY,        // Device obtained and ready to play.
    PCMA_PLAYING,      // Playing audio.
    PCMA_STOPPING,     // Trying to stop, waiting for callback to finish.
    PCMA_STOPPED,      // Stopped. Device was reset.
    PCMA_CLOSED        // Device has been released.
  };

  // Windows calls us back to feed more data to the device here. See msdn
  // documentation for 'waveOutProc' for details about the parameters.
  static void CALLBACK WaveCallback(HWAVEOUT hwo, UINT msg, DWORD_PTR instance,
                                    DWORD_PTR param1, DWORD_PTR param2);

  // If windows reports an error this function handles it and passes it to
  // the attached AudioSourceCallback::OnError().
  void HandleError(MMRESULT error);
  // Allocates and prepares the memory that will be used for playback. Only
  // two buffers are created.
  void SetupBuffers(size_t rq_size);
  // Deallocates the memory allocated in SetupBuffers.
  void FreeBuffers();

  // Reader beware. Visual C has stronger guarantees on volatile vars than
  // most people expect. In fact, it has release semantics on write and
  // acquire semantics on reads. See the msdn documentation.
  volatile State state_;

  // The audio manager that created this output stream. We notify it when
  // we close so it can release its own resources.
  AudioManagerWin* manager_;

  // We use the callback mostly to periodically request more audio data.
  AudioSourceCallback* callback_;

  // The size in bytes of each audio buffer, we usually have two of these.
  size_t buffer_size_;

  // The id assigned by the operating system to the selected wave output
  // hardware device. Usually this is just -1 which means 'default device'.
  UINT device_id_;

  // Windows native structure to encode the format parameters.
  WAVEFORMATEX format_;

  // Handle to the instance of the wave device.
  HWAVEOUT waveout_;

  // Pointer to the first allocated audio buffer. This object owns it.
  WAVEHDR*  buffer_;

  // An event that is signaled when the callback thread is ready to stop.
  ScopedHandle stopped_event_;

  DISALLOW_COPY_AND_ASSIGN(PCMWaveOutAudioOutputStream);
};

#endif  // MEDIA_AUDIO_WAVEOUT_OUTPUT_WIN_H_

