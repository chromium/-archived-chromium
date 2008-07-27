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

#include <vector>

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "Vector.h"
#include "ImageDecoder.h"

// If CALCULATE_MD5_SUMS is not defined, then this test decodes a handful of
// image files and compares their MD5 sums to the stored sums on disk.
//
// To recalculate the MD5 sums, uncommment CALCULATE_MD5_SUMS.
//
// The image files and corresponding MD5 sums live in the directory
// chrome/test/data/*_decoder (where "*" is the format being tested).
//
// Note: The MD5 sums calculated in this test by little- and big-endian systems
// will differ, since no endianness correction is done.  If we start compiling
// for big endian machines this should be fixed.

//#define CALCULATE_MD5_SUMS

// Reads the contents of the specified file into the specified vector.
void ReadFileToVector(const std::wstring& path, Vector<char>* contents);

// Returns the path the decoded data is saved at.
std::wstring GetMD5SumPath(const std::wstring& path);

#ifdef CALCULATE_MD5_SUMS
// Saves the MD5 sum to the specified file.
void SaveMD5Sum(const std::wstring& path, WebCore::RGBA32Buffer* buffer);
#else
// Verifies the image.  |path| identifies the path the image was loaded from.
void VerifyImage(WebCore::ImageDecoder* decoder,
                 const std::wstring& path,
                 const std::wstring& md5_sum_path);
#endif

class ImageDecoderTest : public testing::Test {
 public:
  explicit ImageDecoderTest(const std::wstring& format) : format_(format) { }

 protected:
  virtual void SetUp();

  // Returns the vector of image files for testing.
  std::vector<std::wstring> GetImageFiles() const;

  // Returns true if the image is bogus and should not be successfully decoded.
  bool ShouldImageFail(const std::wstring& path) const;

  // Verifies each of the test image files is decoded correctly and matches the
  // expected state.
  void TestDecoding() const;

#ifndef CALCULATE_MD5_SUMS
  // Verifies that decoding still works correctly when the files are split into
  // pieces at a random point.
  void TestChunkedDecoding() const;
#endif

  // Returns the correct type of image decoder for this test.
  virtual WebCore::ImageDecoder* CreateDecoder() const = 0;

  // The format to be decoded, like "bmp" or "ico".
  std::wstring format_;

 protected:
  // Path to the test files.
  std::wstring data_dir_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ImageDecoderTest);
};
