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
#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "webkit/tools/test_shell/image_decoder_unittest.h"

#include "ICOImageDecoder.h"
#include "SharedBuffer.h"

static const int kFavIconSize = 16;

class ICOImageDecoderTest : public ImageDecoderTest {
 public:
  ICOImageDecoderTest() : ImageDecoderTest(L"ico") { }

 protected:
  virtual WebCore::ImageDecoder* CreateDecoder() const {
    return new WebCore::ICOImageDecoder(WebCore::IntSize());
  }
};

TEST_F(ICOImageDecoderTest, Decoding) {
  TestDecoding();
}

#ifndef CALCULATE_MD5_SUMS
TEST_F(ICOImageDecoderTest, ChunkedDecoding) {
  TestChunkedDecoding();
}
#endif

TEST_F(ICOImageDecoderTest, FaviconSize) {
  // Test that the decoder decodes a preferred size when specified.

  // Load an icon that has both favicon-size and larger entries.
  std::wstring multisize_icon_path(data_dir_);
  file_util::AppendToPath(&multisize_icon_path, L"yahoo.ico");
  Vector<char> image_contents;
  ReadFileToVector(multisize_icon_path, &image_contents);

  // Decode, preferring the favicon size.
  scoped_ptr<WebCore::ImageDecoder> decoder(new WebCore::ICOImageDecoder(
      WebCore::IntSize(kFavIconSize, kFavIconSize)));
  RefPtr<WebCore::SharedBuffer> shared_contents(WebCore::SharedBuffer::create());
  shared_contents->append(image_contents.data(),
                          static_cast<int>(image_contents.size()));
  decoder->setData(shared_contents.get(), true);

  // Verify the decoding.
  const std::wstring md5_sum_path(GetMD5SumPath(multisize_icon_path) + L"2");
#ifdef CALCULATE_MD5_SUMS
  SaveMD5Sum(md5_sum_path, decoder->frameBufferAtIndex(0));
#else
  VerifyImage(decoder.get(), multisize_icon_path, md5_sum_path);
#endif
}
