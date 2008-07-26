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
#include <windows.h>
#include <ImageHlp.h>
#include <psapi.h>

#include "base/image_util.h"
#include "base/process_util.h"

// imagehlp.dll appears to ship in all win versions after Win95.
// nsylvain verified it is present in win2k.
// Using #pragma comment for dependency, instead of LoadLibrary/GetProcAddress.
#pragma comment(lib, "imagehlp.lib")

namespace image_util {

// ImageMetrics
ImageMetrics::ImageMetrics(HANDLE process) : process_(process) {
}

ImageMetrics::~ImageMetrics() {
}

bool ImageMetrics::GetDllImageSectionData(const std::string& loaded_dll_name,
                                          ImageSectionsData* section_sizes) {
  // Get a handle to the loaded DLL
  HMODULE the_dll = GetModuleHandleA(loaded_dll_name.c_str());
  char full_filename[MAX_PATH];
  // Get image path
  if (GetModuleFileNameExA(process_, the_dll, full_filename, MAX_PATH)) {
    return GetImageSectionSizes(full_filename, section_sizes);
  }
  return false;
}

bool ImageMetrics::GetProcessImageSectionData(ImageSectionsData*
                                              section_sizes) {
  char exe_path[MAX_PATH];
  // Get image path
  if (GetModuleFileNameExA(process_, NULL, exe_path, MAX_PATH)) {
    return GetImageSectionSizes(exe_path, section_sizes);
  }
  return false;
}

// private
bool ImageMetrics::GetImageSectionSizes(char* qualified_path,
                                        ImageSectionsData* result) {
  LOADED_IMAGE li;
  // TODO (timsteele): There is no unicode version for MapAndLoad, hence
  // why ansi functions are used in this class. Should we try and rewrite
  // this call ourselves to be safe?
  if (MapAndLoad(qualified_path, 0, &li, FALSE, TRUE)) {
    IMAGE_SECTION_HEADER* section_header = li.Sections;
    for (unsigned i = 0; i < li.NumberOfSections; i++, section_header++) {
      std::string name(reinterpret_cast<char*>(section_header->Name));
      ImageSectionData data(name, section_header->Misc.VirtualSize ?
          section_header->Misc.VirtualSize :
          section_header->SizeOfRawData);
      // copy into result
      result->push_back(data);
    }
  } else {
    // map and load failed
    return false;
  }
  UnMapAndLoad(&li);
  return true;
}

} // namespace image_util
