// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include "media/audio/win/waveout_output_win.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/audio/audio_output.h"
#include "media/audio/win/audio_manager_win.h"

// Some general thoughts about the waveOut API which is badly documented :
// - We use CALLBACK_FUNCTION mode in which XP secretly creates two threads
//   named _MixerCallbackThread and _waveThread which have real-time priority.
//   The callbacks occur in _waveThread.
// - Windows does not provide a way to query if the device is playing or paused
//   thus it forces you to maintain state, which naturally is not exactly
//   synchronized to the actual device state.
// - Some functions, like waveOutReset cannot be called in the callback thread
//   or called in any random state because they deadlock. This results in a
//   non- instantaneous Stop() method. waveOutPrepareHeader seems to be in the
//   same boat.
// - waveOutReset() will forcefully kill the _waveThread so it is important
//   to make sure we are not executing inside the audio source's OnMoreData()
//   or that we take locks inside WaveCallback() or QueueNextPacket().

namespace {

// We settled for a double buffering scheme. It seems to strike a good balance
// between how fast data needs to be provided versus memory usage.
const size_t kNumBuffers = 2;

// Our sound buffers are allocated once and kept in a linked list using the
// the WAVEHDR::dwUser variable. The last buffer points to the first buffer.
WAVEHDR* GetNextBuffer(WAVEHDR* current) {
  return reinterpret_cast<WAVEHDR*>(current->dwUser);
}

}  // namespace

PCMWaveOutAudioOutputStream::PCMWaveOutAudioOutputStream(
    AudioManagerWin* manager, int channels, int sampling_rate,
    char bits_per_sample, UINT device_id)
    : state_(PCMA_BRAND_NEW),
      manager_(manager),
      device_id_(device_id),
      waveout_(NULL),
      callback_(NULL),
      buffer_(NULL),
      buffer_size_(0) {
  format_.wFormatTag = WAVE_FORMAT_PCM;
  format_.nChannels = channels;
  format_.nSamplesPerSec = sampling_rate;
  format_.wBitsPerSample = bits_per_sample;
  format_.cbSize = 0;
  // The next are computed from above.
  format_.nBlockAlign = (format_.nChannels * format_.wBitsPerSample) / 8;
  format_.nAvgBytesPerSec = format_.nBlockAlign * format_.nSamplesPerSec;
  // The event is auto-reset.
  stopped_event_.Set(::CreateEventW(NULL, FALSE, FALSE, NULL));
}

PCMWaveOutAudioOutputStream::~PCMWaveOutAudioOutputStream() {
  DCHECK(NULL == waveout_);
}

bool PCMWaveOutAudioOutputStream::Open(size_t buffer_size) {
  if (state_ != PCMA_BRAND_NEW)
    return false;
  // Open the device. We'll be getting callback in WaveCallback function. They
  // occur in a magic, time-critical thread that windows creates.
  MMRESULT result = ::waveOutOpen(&waveout_, device_id_, &format_,
                                  reinterpret_cast<DWORD_PTR>(WaveCallback),
                                  reinterpret_cast<DWORD_PTR>(this),
                                  CALLBACK_FUNCTION);
  if (result != MMSYSERR_NOERROR)
    return false;
  // If we don't have a packet size we use 100ms.
  if (!buffer_size)
    buffer_size = format_.nAvgBytesPerSec / 10;

  SetupBuffers(buffer_size);
  buffer_size_ = buffer_size;
  state_ = PCMA_READY;
  return true;
}

void PCMWaveOutAudioOutputStream::SetupBuffers(size_t rq_size) {
  WAVEHDR* last = NULL;
  WAVEHDR* first = NULL;
  for (int ix = 0; ix != kNumBuffers; ++ix) {
    size_t sz = sizeof(WAVEHDR) + rq_size;
    buffer_ =  reinterpret_cast<WAVEHDR*>(new char[sz]);
    buffer_->lpData = reinterpret_cast<char*>(buffer_) + sizeof(WAVEHDR);
    buffer_->dwBufferLength = rq_size;
    buffer_->dwBytesRecorded = 0;
    buffer_->dwUser = reinterpret_cast<DWORD_PTR>(last);
    buffer_->dwFlags = WHDR_DONE;
    buffer_->dwLoops = 0;
    if (ix == 0)
      first = buffer_;
    last = buffer_;
    // Tell windows sound drivers about our buffers. Not documented what
    // this does but we can guess that causes the OS to keep a reference to
    // the memory pages so the driver can use them without worries.
    ::waveOutPrepareHeader(waveout_, buffer_, sizeof(WAVEHDR));
  }
  // Fix the first buffer to point to the last one.
  first->dwUser = reinterpret_cast<DWORD_PTR>(last);
}

void PCMWaveOutAudioOutputStream::FreeBuffers() {
  WAVEHDR* current = buffer_;
  for (int ix = 0; ix != kNumBuffers; ++ix) {
    WAVEHDR* next = GetNextBuffer(current);
    ::waveOutUnprepareHeader(waveout_, current, sizeof(WAVEHDR));
    delete[] reinterpret_cast<char*>(current);
    current = next;
  }
  buffer_ = NULL;
}

// Initially we ask the source to fill up both audio buffers. If we don't do
// this then we would always get the driver callback when it is about to run
// samples and that would leave too little time to react.
void PCMWaveOutAudioOutputStream::Start(AudioSourceCallback* callback) {
  if (state_ != PCMA_READY)
    return;
  callback_ = callback;
  state_ = PCMA_PLAYING;
  WAVEHDR* buffer = buffer_;
  for (int ix = 0; ix != kNumBuffers; ++ix) {
    QueueNextPacket(buffer);
    buffer = GetNextBuffer(buffer);
  }
}

// Stopping is tricky. First, no buffer should be locked by the audio driver
// or else the waveOutReset will deadlock and secondly, the callback should not
// be inside the AudioSource's OnMoreData because waveOutReset() forcefully
// kills the callback thread.
void PCMWaveOutAudioOutputStream::Stop() {
  if (state_ != PCMA_PLAYING)
    return;
  state_ = PCMA_STOPPING;
  // Wait for the callback to finish, it will signal us when ready to be reset.
  if (WAIT_OBJECT_0 != ::WaitForSingleObject(stopped_event_, INFINITE)) {
    HandleError(::GetLastError());
    return;
  }
  state_ = PCMA_STOPPED;
  MMRESULT res = ::waveOutReset(waveout_);
  if (res != MMSYSERR_NOERROR) {
    state_ = PCMA_PLAYING;
    HandleError(res);
  }
}

// We can Close in any state except that trying to close a stream that is
// playing Windows generates an error, which we propagate to the source.
void PCMWaveOutAudioOutputStream::Close() {
  if (waveout_) {
    // waveOutClose generates a callback with WOM_CLOSE id in the same thread.
    MMRESULT res = ::waveOutClose(waveout_);
    if (res != MMSYSERR_NOERROR) {
      HandleError(res);
      return;
    }
    state_ = PCMA_CLOSED;
    waveout_ = NULL;
    FreeBuffers();
  }
  // Tell the audio manager that we have been released. This can result in
  // the manager destroying us in-place so this needs to be the last thing
  // we do on this function.
  manager_->ReleaseStream(this);
}

// TODO(cpu): Implement SetVolume and GetVolume.
void PCMWaveOutAudioOutputStream::SetVolume(double left_level,
                                            double right_level) {
  if (state_ != PCMA_READY)
    return;
}

void PCMWaveOutAudioOutputStream::GetVolume(double* left_level,
                                            double* right_level) {
  if (state_ != PCMA_READY)
    return;
}

void PCMWaveOutAudioOutputStream::HandleError(MMRESULT error) {
  DLOG(WARNING) << "PCMWaveOutAudio error " << error;
  callback_->OnError(this, error);
}

void PCMWaveOutAudioOutputStream::QueueNextPacket(WAVEHDR *buffer) {
  // Call the source which will fill our buffer with pleasant sounds and
  // return to us how many bytes were used.
  size_t used = callback_->OnMoreData(this, buffer->lpData, buffer_size_);

  if (used <= buffer_size_) {
    buffer->dwBufferLength = used;
  } else {
    HandleError(0);
    return;
  }
  // Time to queue the buffer to the audio driver. Since we are reusing
  // the same buffers we can get away without calling waveOutPrepareHeader.
  buffer->dwFlags = WHDR_PREPARED;
  MMRESULT result = ::waveOutWrite(waveout_, buffer, sizeof(WAVEHDR));
  if (result != MMSYSERR_NOERROR)
    HandleError(result);
}

// Windows call us back in this function when some events happen. Most notably
// when it is done playing a buffer. Since we use double buffering it is
// convenient to think of |buffer| as free and GetNextBuffer(buffer) as in
// use by the driver.
void PCMWaveOutAudioOutputStream::WaveCallback(HWAVEOUT hwo, UINT msg,
                                               DWORD_PTR instance,
                                               DWORD_PTR param1, DWORD_PTR) {
  PCMWaveOutAudioOutputStream* obj =
      reinterpret_cast<PCMWaveOutAudioOutputStream*>(instance);

  if (msg == WOM_DONE) {
    // WOM_DONE indicates that the driver is done with our buffer, we can
    // either ask the source for more data or check if we need to stop playing.
    WAVEHDR* buffer = reinterpret_cast<WAVEHDR*>(param1);
    buffer->dwFlags = WHDR_DONE;

    if (obj->state_ == PCMA_STOPPING) {
      // The main thread has called Stop() and is waiting to issue waveOutReset
      // which will kill this thread. We should not enter AudioSourceCallback
      // code anymore.
      ::SetEvent(obj->stopped_event_);
      return;
    } else if (obj->state_ == PCMA_STOPPED) {
      // Not sure if ever hit this but just in case.
      return;
    }
    obj->QueueNextPacket(buffer);
  } else if (msg == WOM_CLOSE) {
    // We can be closed before calling Start, so it is possible to have a
    // null callback at this point.
    if (obj->callback_)
      obj->callback_->OnClose(obj);
  }
}
