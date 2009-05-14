// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_IMAGE_DECODER_UNITTEST_H_
#define WEBKIT_TOOLS_TEST_SHELL_IMAGE_DECODER_UNITTEST_H_

#include <vector>

#include "Vector.h"
#include "ImageDecoder.h"

#undef LOG

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

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

enum ImageDecoderTestFileSelection {
  TEST_ALL,
  TEST_SMALLER,
  TEST_BIGGER,
};

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
  // expected state. |file_selection| and |threshold| can be used to select
  // files to test based on file size.
  void TestDecoding(ImageDecoderTestFileSelection file_selection,
                    const int64 threshold) const;

  void TestDecoding() const {
    TestDecoding(TEST_ALL, 0);
  }

#ifndef CALCULATE_MD5_SUMS
  // Verifies that decoding still works correctly when the files are split into
  // pieces at a random point. |file_selection| and |threshold| can be used to
  // select files to test based on file size.
  void TestChunkedDecoding(ImageDecoderTestFileSelection file_selection,
                           const int64 threshold) const;

  void TestChunkedDecoding() const {
    TestChunkedDecoding(TEST_ALL, 0);
  }

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

#endif  // WEBKIT_TOOLS_TEST_SHELL_IMAGE_DECODER_UNITTEST_H_
