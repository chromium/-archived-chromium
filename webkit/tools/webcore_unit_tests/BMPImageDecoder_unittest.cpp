// Copyright (c) 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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

#include "config.h"
#include "webkit/tools/test_shell/image_decoder_unittest.h"

#include "BMPImageDecoder.h"

class BMPImageDecoderTest : public ImageDecoderTest {
 public:
  BMPImageDecoderTest() : ImageDecoderTest(L"bmp") { }

 protected:
  virtual WebCore::ImageDecoder* CreateDecoder() const {
    return new WebCore::BMPImageDecoder();
  }

  // The BMPImageDecoderTest tests are really slow under Valgrind.
  // Thus it is split into fast and slow versions. The threshold is
  // set to 10KB because the fast test can finish under Valgrind in
  // less than 30 seconds.
  static const int64 kThresholdSize = 10240;
};

TEST_F(BMPImageDecoderTest, DecodingFast) {
  TestDecoding(TEST_SMALLER, kThresholdSize);
}

TEST_F(BMPImageDecoderTest, DecodingSlow) {
  TestDecoding(TEST_BIGGER, kThresholdSize);
}

#ifndef CALCULATE_MD5_SUMS
TEST_F(BMPImageDecoderTest, ChunkedDecodingFast) {
  TestChunkedDecoding(TEST_SMALLER, kThresholdSize);
}

TEST_F(BMPImageDecoderTest, ChunkedDecodingSlow) {
  TestChunkedDecoding(TEST_BIGGER, kThresholdSize);
}
#endif
