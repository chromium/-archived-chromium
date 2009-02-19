// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FFmpeg is written in C99 and requires <stdint.h> and <inttypes.h>.
// Since MSVC doesn't include these headers, we have to write our own version
// to provide a compatibility layer between MSVC and the FFmpeg headers.

#ifndef THIRD_PARTY_FFMPEG_INCLUDE_WIN_STDINT_H_
#define THIRD_PARTY_FFMPEG_INCLUDE_WIN_STDINT_H_

#if !defined(_MSC_VER)
#error This file should only be included when compiling with MSVC.
#endif

// Define C99 equivalent types.
typedef signed char           int8_t;
typedef signed short          int16_t;
typedef signed int            int32_t;
typedef signed long long      int64_t;
typedef unsigned char         uint8_t;
typedef unsigned short        uint16_t;
typedef unsigned int          uint32_t;
typedef unsigned long long    uint64_t;

#endif  // THIRD_PARTY_FFMPEG_INCLUDE_WIN_STDINT_H_
