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

// This file input format is based loosely on
// WebKitTools/DumpRenderTree/ImageDiff.m

// The exact format of this tool's output to stdout is important, to match
// what the run-webkit-tests script expects.

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/gfx/png_decoder.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"

// Causes the app to remain open, waiting for pairs of filenames on stdin.
// The caller is then responsible for terminating this app.
static const wchar_t kOptionPollStdin[] = L"use-stdin";

// Return codes used by this utility.
static const int kStatusSame = 0;
static const int kStatusDifferent = 1;
static const int kStatusError = 2;

class Image {
 public:
  Image() : w_(0), h_(0) {
  }

  bool has_image() const {
    return w_ > 0 && h_ > 0;
  }

  int w() const {
    return w_;
  }

  int h() const {
    return h_;
  }

  // Creates the image from stdin with the given data length. On success, it
  // will return true. On failure, no other methods should be accessed.
  bool CreateFromStdin(int byte_length) {
    if (byte_length <= 0)
      return false;

    scoped_array<unsigned char> source(new unsigned char[byte_length]);
    if (fread(source.get(), 1, byte_length, stdin) != byte_length)
      return false;

    if (!PNGDecoder::Decode(source.get(), byte_length, PNGDecoder::FORMAT_RGBA,
                          &data_, &w_, &h_)) {
      Clear();
      return false;
    }
    return true;
  }

  // Creates the image from the given filename on disk, and returns true on
  // success.
  bool CreateFromFilename(const char* filename) {
    FILE* f;
    if (fopen_s(&f, filename, "rb") != 0)
      return false;

    std::vector<unsigned char> compressed;
    const int buf_size = 1024;
    unsigned char buf[buf_size];
    size_t num_read = 0;
    while ((num_read = fread(buf, 1, buf_size, f)) > 0) {
      std::copy(buf, &buf[num_read], std::back_inserter(compressed));
    }

    fclose(f);

    if (!PNGDecoder::Decode(&compressed[0], compressed.size(),
                          PNGDecoder::FORMAT_RGBA, &data_, &w_, &h_)) {
      Clear();
      return false;
    }
    return true;
  }

  void Clear() {
    w_ = h_ = 0;
    data_.clear();
  }

  // Returns the RGBA value of the pixel at the given location
  uint32 pixel_at(int x, int y) const {
    DCHECK(x >= 0 && x < w_);
    DCHECK(y >= 0 && y < h_);
    return data_[(y * w_ + x) * 4];
  }

 private:
  // pixel dimensions of the image
  int w_, h_;

  std::vector<unsigned char> data_;
};

float PercentageDifferent(const Image& baseline, const Image& actual) {
  int w = std::min(baseline.w(), actual.w());
  int h = std::min(baseline.h(), actual.h());

  // compute pixels different in the overlap
  int pixels_different = 0;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      if (baseline.pixel_at(x, y) != actual.pixel_at(x, y))
        pixels_different++;
    }
  }

  // count pixels that are a difference in size as also being different
  int max_w = std::max(baseline.w(), actual.w());
  int max_h = std::max(baseline.h(), actual.h());

  // ...pixels off the right side, but not including the lower right corner
  pixels_different += (max_w - w) * h;

  // ...pixels along the bottom, including the lower right corner
  pixels_different += (max_h - h) * max_w;

  // Like the WebKit ImageDiff tool, we define percentage different in terms
  // of the size of the 'actual' bitmap.
  float total_pixels = static_cast<float>(actual.w()) *
                       static_cast<float>(actual.h());
  if (total_pixels == 0)
    return 100.0f;  // when the bitmap is empty, they are 100% different
  return static_cast<float>(pixels_different) / total_pixels * 100;
}

void PrintHelp() {
  fprintf(stderr,
    "Usage:\n"
    "  image_diff <compare file> <reference file>\n"
    "    Compares two files on disk, returning 0 when they are the same\n"
    "  image_diff --use-stdin\n"
    "    Stays open reading pairs of filenames from stdin, comparing them,\n"
    "    and sending 0 to stdout when they are the same\n");
  /* For unfinished webkit-like-mode (see below)
    "\n"
    "  image_diff -s\n"
    "    Reads stream input from stdin, should be EXACTLY of the format\n"
    "    \"Content-length: <byte length> <data>Content-length: ...\n"
    "    it will take as many file pairs as given, and will compare them as\n"
    "    (cmp_file, reference_file) pairs\n");
  */
}

int CompareImages(const char* file1, const char* file2) {
  Image actual_image;
  Image baseline_image;

  if (!actual_image.CreateFromFilename(file1)) {
    fprintf(stderr, "image_diff: Unable to open file \"%s\"\n", file1);
    return kStatusError;
  }
  if (!baseline_image.CreateFromFilename(file2)) {
    fprintf(stderr, "image_diff: Unable to open file \"%s\"\n", file2);
    return kStatusError;
  }

  float percent = PercentageDifferent(actual_image, baseline_image);
  if (percent > 0.0) {
    // failure: The WebKit version also writes the difference image to
    // stdout, which seems excessive for our needs.
    printf("diff: %01.2f%% failed\n", percent);
    return kStatusDifferent;
  }

  // success
  printf("diff: %01.2f%% passed\n", percent);
  return kStatusSame;

/* Untested mode that acts like WebKit's image comparator. I wrote this but
   decided it's too complicated. We may use it in the future if it looks useful

  char buffer[2048];
  while (fgets(buffer, sizeof(buffer), stdin)) {

    if (strncmp("Content-length: ", buffer, 16) == 0) {
      char* context;
      strtok_s(buffer, " ", &context);
      int image_size = strtol(strtok_s(NULL, " ", &context), NULL, 10);

      bool success = false;
      if (image_size > 0 && actual_image.has_image() == 0) {
        if (!actual_image.CreateFromStdin(image_size)) {
          fputs("Error, input image can't be decoded.\n", stderr);
          return 1;
        }
      } else if (image_size > 0 && baseline_image.has_image() == 0) {
        if (!baseline_image.CreateFromStdin(image_size)) {
          fputs("Error, baseline image can't be decoded.\n", stderr);
          return 1;
        }
      } else {
        fputs("Error, image size must be specified.\n", stderr);
        return 1;
      }
    }

    if (actual_image.has_image() && baseline_image.has_image()) {
      float percent = PercentageDifferent(actual_image, baseline_image);
      if (percent > 0.0) {
        // failure: The WebKit version also writes the difference image to
        // stdout, which seems excessive for our needs.
        printf("diff: %01.2f%% failed\n", percent);
      } else {
        // success
        printf("diff: %01.2f%% passed\n", percent);
      }
      actual_image.Clear();
      baseline_image.Clear();
    }

    fflush(stdout);
  }
*/
}

int main(int argc, const char* argv[]) {
  CommandLine parsed_command_line;
  if (parsed_command_line.HasSwitch(kOptionPollStdin)) {
    // Watch stdin for filenames.
    char stdin_buffer[2048];
    char filename1_buffer[2048];
    bool have_filename1 = false;
    while (fgets(stdin_buffer, sizeof(stdin_buffer), stdin)) {
      char *newLine = strchr(stdin_buffer, '\n');
      if (newLine)
        *newLine = '\0';
      if (!*stdin_buffer)
        continue;

      if (have_filename1) {
        // CompareImages writes results to stdout unless an error occurred.
        if (CompareImages(filename1_buffer, stdin_buffer) == kStatusError)
          printf("error\n");
        fflush(stdout);
        have_filename1 = false;
      } else {
        // Save the first filename in another buffer and wait for the second
        // filename to arrive via stdin.
        strcpy_s(filename1_buffer, sizeof(filename1_buffer), stdin_buffer);
        have_filename1 = true;
      }
    }
    return 0;
  }

  if (argc == 3) {
    return CompareImages(argv[1], argv[2]);
  }

  PrintHelp();
  return kStatusError;
}
