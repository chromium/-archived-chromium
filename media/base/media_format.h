// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_FORMAT_H_
#define MEDIA_BASE_MEDIA_FORMAT_H_

#include "base/values.h"

namespace media {

// Common MIME types.
namespace mime_type {
extern const std::wstring kURI;
extern const std::wstring kApplicationOctetStream;
extern const std::wstring kMPEGAudio;
extern const std::wstring kAACAudio;
extern const std::wstring kH264AnnexB;
extern const std::wstring kUncompressedAudio;
extern const std::wstring kUncompressedVideo;
extern const std::wstring kFFmpegAudio;
extern const std::wstring kFFmpegVideo;
}  // namespace mime_type

// MediaFormat is used to describe the output of a MediaFilterInterface to
// determine whether a downstream filter can accept the output from an upstream
// filter.  In general, every MediaFormat contains a MIME type describing
// its output as well as additional key-values describing additional details.
//
// For example, an audio decoder could output audio/x-uncompressed and include
// additional key-values such as the number of channels and the sample rate.
// An audio renderer would use this information to properly initialize the
// audio hardware before playback started.
//
// It's also perfectly acceptable to create your own MIME types and standards
// to allow communication between two filters that goes beyond the basics
// described here.  For example, you could write a video decoder that outputs
// a MIME type video/x-special for which your video renderer knows it's some
// special form of decompressed video output that regular video renderers
// couldn't handle.
class MediaFormat {
 public:
  // Common keys.
  static const std::wstring kMimeType;
  static const std::wstring kURI;
  static const std::wstring kSurfaceFormat;
  static const std::wstring kSampleRate;
  static const std::wstring kSampleBits;
  static const std::wstring kChannels;
  static const std::wstring kWidth;
  static const std::wstring kHeight;
  static const std::wstring kFfmpegCodecId;

  MediaFormat();
  ~MediaFormat();

  // Basic map operations.
  bool empty() const { return value_map_.empty(); }

  bool Contains(const std::wstring& key) const;

  void Clear();

  // Value accessors.
  void SetAsBoolean(const std::wstring& key, bool in_value);
  void SetAsInteger(const std::wstring& key, int in_value);
  void SetAsReal(const std::wstring& key, double in_value);
  void SetAsString(const std::wstring& key, const std::wstring& in_value);

  bool GetAsBoolean(const std::wstring& key, bool* out_value) const;
  bool GetAsInteger(const std::wstring& key, int* out_value) const;
  bool GetAsReal(const std::wstring& key, double* out_value) const;
  bool GetAsString(const std::wstring& key, std::wstring* out_value) const;

 private:
  // Helper to return a value.
  Value* GetValue(const std::wstring& key) const;

  ValueMap value_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaFormat);
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_FORMAT_H_
