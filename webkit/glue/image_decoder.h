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

#include "base/basictypes.h"
#include "base/gfx/size.h"
#include "base/scoped_ptr.h"

class SkBitmap;

namespace webkit_glue {

// Provides an interface to WebKit's image decoders.
//
// Note to future: This class should be deleted. We should have our own nice
// image decoders in base/gfx, and our port should use those. Currently, it's
// the other way around.
class ImageDecoder {
 public:
  // Use the constructor with desired_size when you think you may have an .ico
  // format and care about which size you get back. Otherwise, use the 0-arg
  // constructor.
  ImageDecoder();
  ImageDecoder(const gfx::Size& desired_icon_size);
  ~ImageDecoder();

  // Call this function to decode the image. If successful, the decoded image
  // will be returned. Otherwise, an empty bitmap will be returned.
  SkBitmap Decode(const unsigned char* data, size_t size);

 private:
  // Size will be empty to get the largest possible size.
  gfx::Size desired_icon_size_;

  DISALLOW_EVIL_CONSTRUCTORS(ImageDecoder);
};

}  // namespace webkit_glue
