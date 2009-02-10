// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filter_host.h"
#include "media/filters/audio_renderer_impl.h"

namespace media {

// The number of initial reads to perform for buffering.  There should never
// be more than |kInitialReads| in the queue since we read only after we're
// finished with a buffer in the callback thread.
//
// This is sort of a magic number, but for 44.1kHz stereo audio this will give
// us enough data to fill approximately 4 complete callback buffers.
static const size_t kInitialReads = 16;

// We'll try to fill 4096 samples per buffer, which is roughly ~92ms of audio
// data for a 44.1kHz audio source.
static const size_t kSamplesPerBuffer = 4096;

AudioRendererImpl::AudioRendererImpl()
    : decoder_(NULL),
      stream_(NULL),
      initialized_(false),
      data_offset_(0) {
}

AudioRendererImpl::~AudioRendererImpl() {
  // Empty buffer queue (callback will simply keep exiting).
  input_lock_.Acquire();
  while (!input_queue_.empty()) {
    Buffer* buffer = input_queue_.front();
    input_queue_.pop_front();
    buffer->Release();
  }
  input_lock_.Release();

  // Close down the audio device.
  if (stream_) {
    stream_->Stop();
    stream_->Close();
  }
}

bool AudioRendererImpl::IsMediaFormatSupported(
    const MediaFormat* media_format) {
  int channels;
  int sample_rate;
  int sample_bits;
  return ParseMediaFormat(media_format, &channels, &sample_rate, &sample_bits);
}

void AudioRendererImpl::Stop() {
  DCHECK(stream_);
  stream_->Stop();
}

void AudioRendererImpl::SetPlaybackRate(float playback_rate) {
  DCHECK(stream_);
  // TODO(scherkus): handle playback rates not equal to 1.0.
  if (playback_rate == 1.0f) {
    stream_->Start(this);
  } else {
    NOTIMPLEMENTED();
  }
}

bool AudioRendererImpl::Initialize(AudioDecoder* decoder) {
  decoder_ = decoder;
  const MediaFormat* media_format = decoder_->GetMediaFormat();
  std::string mime_type;
  int channels;
  int sample_rate;
  int sample_bits;
  if (!ParseMediaFormat(media_format, &channels, &sample_rate, &sample_bits)) {
    return false;
  }

  // Create our audio stream.
  stream_ = AudioManager::GetAudioManager()->MakeAudioStream(
      AudioManager::AUDIO_PCM_LINEAR, channels, sample_rate, sample_bits);
  DCHECK(stream_);

  // Calculate buffer size and open the stream.
  size_t size = kSamplesPerBuffer * channels * sample_bits / 8;
  if (!stream_->Open(size)) {
    stream_->Close();
    stream_ = NULL;
    return false;
  }

  // Schedule our initial reads.
  for (size_t i = 0; i < kInitialReads; ++i) {
    ScheduleRead();
  }

  // Defer initialization until all scheduled reads have completed.
  return true;
}

void AudioRendererImpl::SetVolume(float volume) {
  stream_->SetVolume(volume, volume);
}

void AudioRendererImpl::OnAssignment(Buffer* buffer_in) {
  AutoLock auto_lock(input_lock_);
  buffer_in->AddRef();
  input_queue_.push_back(buffer_in);
  DCHECK(input_queue_.size() <= kInitialReads);

  // See if we're finally initialized.
  // TODO(scherkus): handle end of stream.
  if (!initialized_ && input_queue_.size() == kInitialReads) {
    initialized_ = true;
    host_->InitializationComplete();
  }
}

size_t AudioRendererImpl::OnMoreData(AudioOutputStream* stream, void* dest_void,
                                     size_t len) {
  // TODO(scherkus): handle end of stream.
  DCHECK(stream_ == stream);

  // TODO(scherkus): maybe change interface to pass in char/uint8 or similar.
  char* dest = reinterpret_cast<char*>(dest_void);
  size_t result = 0;

  // Loop until the buffer has been filled.
  while (len > 0) {
    Buffer* buffer = NULL;
    {
      AutoLock auto_lock(input_lock_);
      if (input_queue_.empty()) {
        // TODO(scherkus): consider blocking here until more data arrives.
        return result;
      }

      // Access the front buffer.
      buffer = input_queue_.front();
    }

    // Determine how much to copy.
    const char* data = buffer->GetData() + data_offset_;
    size_t data_len = buffer->GetDataSize() - data_offset_;
    data_len = std::min(len, data_len);

    // Copy into buffer.
    memcpy(dest, data, data_len);
    len -= data_len;
    dest += data_len;
    data_offset_ += data_len;
    result += data_len;

    // Check to see if we're finished with the front buffer.
    if (data_offset_ == buffer->GetDataSize()) {
      // Update the clock.
      host_->SetTime(buffer->GetTimestamp());

      // Dequeue the buffer.
      {
        AutoLock auto_lock(input_lock_);
        input_queue_.pop_front();
      }

      // Release and request another buffer from the decoder.
      buffer->Release();
      ScheduleRead();

      // Reset our offset into the front buffer.
      data_offset_ = 0;
    }
  }
  return result;
}

void AudioRendererImpl::OnClose(AudioOutputStream* stream) {
  // TODO(scherkus): implement OnClose.
  NOTIMPLEMENTED();
}

void AudioRendererImpl::OnError(AudioOutputStream* stream, int code) {
  // TODO(scherkus): implement OnError.
  NOTIMPLEMENTED();
}

// static
bool AudioRendererImpl::ParseMediaFormat(const MediaFormat* media_format,
                                         int* channels_out,
                                         int* sample_rate_out,
                                         int* sample_bits_out) {
  std::string mime_type;
  return media_format->GetAsString(MediaFormat::kMimeType, &mime_type) &&
      media_format->GetAsInteger(MediaFormat::kChannels, channels_out) &&
      media_format->GetAsInteger(MediaFormat::kSampleRate, sample_rate_out) &&
      media_format->GetAsInteger(MediaFormat::kSampleBits, sample_bits_out) &&
      mime_type.compare(mime_type::kUncompressedAudio) == 0;
}

void AudioRendererImpl::ScheduleRead() {
  host_->PostTask(NewRunnableMethod(decoder_, &AudioDecoder::Read,
      new AssignableBuffer<AudioRendererImpl, Buffer>(this)));
}

}  // namespace media
