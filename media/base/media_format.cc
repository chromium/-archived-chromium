// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_format.h"

namespace media {

namespace mime_type {

// Represents a URI, typically used to create a DataSourceInterface.
// Expected keys:
//   kUri             String          The URI
const std::wstring kURI(L"text/x-uri");

// Represents a generic byte stream, typically from a DataSourceInterface.
// Expected keys:
//   None
const std::wstring kApplicationOctetStream(L"application/octet-stream");

// Represents encoded audio data, typically from an DemuxerStreamInterface.
// Expected keys:
//   None
const std::wstring kMPEGAudio(L"audio/mpeg");
const std::wstring kAACAudio(L"audio/aac");

// Represents encoded video data, typically from a DemuxerStreamInterface.
// Expected keys:
//   None
const std::wstring kH264AnnexB(L"video/x-h264-annex-b");

// Represents decoded audio data, typically from an AudioDecoderInterface.
// Expected keys:
//   kChannels        Integer         Number of audio channels
//   kSampleRate      Integer         Audio sample rate (i.e., 44100)
//   kSampleBits      Integer         Audio bits-per-sample (i.e., 16)
const std::wstring kRawAudio(L"audio/x-uncompressed");

// Represents decoded video data, typically from a VideoDecoderInterface.
// Other information, such as surface format (i.e., YV12), stride and planes are
// included with the buffer itself and is not part of the MediaFormat.
// Expected keys:
//   kWidth           Integer         Display width of the surface
//   kHeight          Integer         Display height of the surface
const std::wstring kRawVideo(L"video/x-uncompressed");

// Represents FFmpeg encoded packets, typically from an DemuxerStreamInterface.
// Expected keys:
//   kFfmpegCodecId   Integer         The FFmpeg CodecID identifying the decoder
const std::wstring kFFmpegAudio(L"audio/x-ffmpeg");
const std::wstring kFFmpegVideo(L"video/x-ffmpeg");

}  // namespace mime_type

// Common keys.
const std::wstring MediaFormat::kMimeType(L"MimeType");
const std::wstring MediaFormat::kURI(L"Uri");
const std::wstring MediaFormat::kSurfaceFormat(L"SurfaceFormat");
const std::wstring MediaFormat::kSampleRate(L"SampleRate");
const std::wstring MediaFormat::kSampleBits(L"SampleBits");
const std::wstring MediaFormat::kChannels(L"Channels");
const std::wstring MediaFormat::kWidth(L"Width");
const std::wstring MediaFormat::kHeight(L"Height");
const std::wstring MediaFormat::kFfmpegCodecId(L"FfmpegCodecId");

MediaFormat::MediaFormat() {
}

MediaFormat::~MediaFormat() {
  Clear();
}

bool MediaFormat::Contains(const std::wstring& key) const {
  return value_map_.find(key) != value_map_.end();
}

void MediaFormat::Clear() {
  for (ValueMap::iterator iter(value_map_.begin());
       iter != value_map_.end(); iter = value_map_.erase(iter))
    delete iter->second;
}

void MediaFormat::SetAsBoolean(const std::wstring& key, bool in_value) {
  value_map_[key] = Value::CreateBooleanValue(in_value);
}

void MediaFormat::SetAsInteger(const std::wstring& key, int in_value) {
  value_map_[key] = Value::CreateIntegerValue(in_value);
}

void MediaFormat::SetAsReal(const std::wstring& key, double in_value) {
  value_map_[key] = Value::CreateRealValue(in_value);
}

void MediaFormat::SetAsString(const std::wstring& key,
                              const std::wstring& in_value) {
  value_map_[key] = Value::CreateStringValue(in_value);
}

bool MediaFormat::GetAsBoolean(const std::wstring& key, bool* out_value) const {
  Value* value = GetValue(key);
  return value != NULL && value->GetAsBoolean(out_value);
}

bool MediaFormat::GetAsInteger(const std::wstring& key, int* out_value) const {
  Value* value = GetValue(key);
  return value != NULL && value->GetAsInteger(out_value);
}

bool MediaFormat::GetAsReal(const std::wstring& key, double* out_value) const {
  Value* value = GetValue(key);
  return value != NULL && value->GetAsReal(out_value);
}

bool MediaFormat::GetAsString(const std::wstring& key,
                              std::wstring* out_value) const {
  Value* value = GetValue(key);
  return value != NULL && value->GetAsString(out_value);
}

Value* MediaFormat::GetValue(const std::wstring& key) const {
  ValueMap::const_iterator value_iter = value_map_.find(key);
  return (value_iter == value_map_.end()) ? NULL : value_iter->second;
}

}  // namespace media
