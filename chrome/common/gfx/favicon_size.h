// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_COMMON_GFX_FAVICON_SIZE_H__
#define CHROME_COMMON_GFX_FAVICON_SIZE_H__

// Size (along each axis) of the favicon.
const int kFavIconSize = 16;

// If the width or height is bigger than the favicon size, a new width/height
// is calculated and returned in width/height that maintains the aspect
// ratio of the supplied values.
static void calc_favicon_target_size(int* width, int* height) {
  if (*width > kFavIconSize || *height > kFavIconSize) {
    // Too big, resize it maintaining the aspect ratio.
    float aspect_ratio = static_cast<float>(*width) /
                         static_cast<float>(*height);
    *height = kFavIconSize;
    *width = static_cast<int>(aspect_ratio * *height);
    if (*width > kFavIconSize) {
      *width = kFavIconSize;
      *height = static_cast<int>(*width / aspect_ratio);
    }
  }
}

#endif // CHROME_COMMON_GFX_FAVICON_SIZE_H__
