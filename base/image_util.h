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

// This file/namespace contains utility functions for gathering
// information about PE (Portable Executable) headers within
// images (dll's / exe's )

#ifndef BASE_IMAGE_UTIL_H__
#define BASE_IMAGE_UTIL_H__

#include <vector>
#include "base/basictypes.h"
#include <windows.h>

namespace image_util {

// Contains both the PE section name (.text, .reloc etc) and its size.
struct ImageSectionData {
  std::string name;
  size_t size_in_bytes;
  ImageSectionData (const std::string& section_name, size_t section_size) :
      name (section_name), size_in_bytes(section_size) {
  }
};

typedef std::vector<ImageSectionData> ImageSectionsData;

// Provides image statistics for modules of a specified process, or for the
// specified process' own executable file. To use, invoke CreateImageMetrics()
// to get an instance for a specified process, then access the information via
// methods.
class ImageMetrics {
  public:
    // Creates an ImageMetrics instance for given process owned by
    // the caller.
    explicit ImageMetrics(HANDLE process);
    ~ImageMetrics();

    // Fills a vector of ImageSectionsData containing name/size info
    // for every section found in the specified dll's PE section table.
    // The DLL must be loaded by the process associated with this ImageMetrics
    // instance.
    bool GetDllImageSectionData(const std::string& loaded_dll_name,
        ImageSectionsData* section_sizes);

    // Fills a vector if ImageSectionsData containing name/size info
    // for every section found in the executable file of the process
    // associated with this ImageMetrics instance.
    bool GetProcessImageSectionData(ImageSectionsData* section_sizes);

  private:
    // Helper for GetDllImageSectionData and GetProcessImageSectionData
    bool GetImageSectionSizes(char* qualified_path, ImageSectionsData* result);

    HANDLE process_;

    DISALLOW_EVIL_CONSTRUCTORS(ImageMetrics);
};

} // namespace image_util

#endif
