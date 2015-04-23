// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_IMAGE_INFO_H_
#define COURGETTE_IMAGE_INFO_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace courgette {

// A Relative Virtual Address is the address in the image file after it is
// loaded into memory relative to the image load address.
typedef uint32 RVA;

// PE file section header.  This struct has the same layout as the
// IMAGE_SECTION_HEADER structure from WINNT.H
// http://msdn.microsoft.com/en-us/library/ms680341(VS.85).aspx
//
#pragma pack(push, 1)  // Supported by MSVC and GCC. Ensures no gaps in packing.
struct Section {
  char name[8];
  uint32 virtual_size;
  uint32 virtual_address;
  uint32 size_of_raw_data;
  uint32 file_offset_of_raw_data;
  uint32 pointer_to_relocations;   // Always zero in an image.
  uint32 pointer_to_line_numbers;  // Always zero in an image.
  uint16 number_of_relocations;    // Always zero in an image.
  uint16 number_of_line_numbers;   // Always zero in an image.
  uint32 characteristics;
};
#pragma pack(pop)

COMPILE_ASSERT(sizeof(Section) == 40, section_is_40_bytes);

// Returns the name of a section, solving the problem that the name is not
// always properly NUL-terminated.  Used only for debugging.
std::string SectionName(const Section* section);

// ImageDataDirectory has same layout as IMAGE_DATA_DIRECTORY structure from
// WINNT.H
// http://msdn.microsoft.com/en-us/library/ms680305(VS.85).aspx
//
class ImageDataDirectory {
 public:
  ImageDataDirectory() : address_(0), size_(0) {}
  RVA address_;
  uint32 size_;
};

COMPILE_ASSERT(sizeof(ImageDataDirectory) == 8,
               image_data_directory_is_8_bytes);

//
//  PEInfo holds information about a single Windows 'Portable Executable' format
//  file in the on-disk format.
//
//  Imagine you had concatenated a bunch of 'original' files into one 'big'
//  file and read the big file into memory.  You could find the executables
//  from the original files by calling PEInfo::Init with different addresses.
//  If PEInfo::TryParseHeader returns true, then Init was passed the address
//  of the first byte of one of the original executables, and PEIinfo::length
//  will tell how long the file was.
//
class PEInfo {
 public:
  PEInfo();

  // ok() may always be called but returns 'true' only after ParseHeader
  // succeeds.
  bool ok() const { return failure_reason_ == NULL; }

  // Initialize with buffer.  This just sets up the region of memory that
  // potentially contains the bytes from an executable file.  The caller
  // continues to own 'start'.
  void Init(const void* start, size_t length);

  // Returns 'true' if the buffer appears to point to a Windows 32 bit
  // executable, 'false' otherwise.  If ParseHeader() succeeds, other member
  // functions may be called.
  bool ParseHeader();

  // Returns 'true' if the base relocation table can be parsed.
  // Output is a vector of the RVAs corresponding to locations within executable
  // that are listed in the base relocation table.
  bool ParseRelocs(std::vector<RVA> *addresses);

  // Returns the length of the image.  Valid only if ParseHeader succeeded.
  uint32 length() const { return file_length_; }

  bool has_text_section() const { return has_text_section_; }

  uint32 size_of_code() const { return size_of_code_; }

  bool is_32bit() const { return !is_PE32_plus_; }

  // Most addresses are represented as 32-bit RVAs.  The one address we can't
  // do this with is the image base address.  'image_base' is valid only for
  // 32-bit executables. 'image_base_64' is valid for 32- and 64-bit executable.
  uint32 image_base() const { return static_cast<uint32>(image_base_); }
  uint64 image_base_64() const { return image_base_; }

  const ImageDataDirectory& base_relocation_table() const {
    return base_relocation_table_;
  }

  bool IsValidRVA(RVA rva) const { return rva < size_of_image_; }

  // Returns description of the RVA, e.g. ".text+0x1243".  For debugging only.
  std::string DescribeRVA(RVA rva) const;

  // Returns a pointer into the memory copy of the file format.
  // FileOffsetToPointer(0) returns a pointer to the start of the file format.
  const uint8* FileOffsetToPointer(uint32 offset) const {
    return start_ + offset;
  }

  // Finds the first section at file_offset or above.  Does not return sections
  // that have no raw bytes in the file.
  const Section* FindNextSection(uint32 file_offset) const;
  // Returns Section containing the relative virtual address, or NULL if none.
  const Section* RVAToSection(RVA rva) const;

  // There are 2 'coordinate systems' for reasoning about executables.
  //   FileOffset - the the offset within a single .EXE or .DLL *file*.
  //   RVA - relative virtual address (offset within *loaded image*)
  // FileOffsetToRVA and RVAToFileOffset convert between these representations.

  RVA FileOffsetToRVA(uint32 offset) const;

  static const int kNoOffset = -1;
  // Returns kNoOffset if there is no file offset corresponding to 'rva'.
  int RVAToFileOffset(RVA rva) const;

  // Returns same as FileOffsetToPointer(RVAToFileOffset(rva)) except that NULL
  // is returned if there is no file offset corresponding to 'rva'.
  const uint8* RVAToPointer(RVA rva) const;

 protected:
  //
  // Fields that are always valid.
  //
  const char* failure_reason_;

  //
  // Basic information that is always valid after Init.
  //
  const uint8* start_;    // In current memory, base for 'file offsets'.
  const uint8* end_;      // In current memory.
  unsigned int length_;   // In current memory.

  //
  // Information that is valid after successful ParseHeader.
  //
  bool is_PE32_plus_;   // PE32_plus is for 64 bit executables.
  uint32 file_length_;

  // Location and size of IMAGE_OPTIONAL_HEADER in the buffer.
  const uint8 *optional_header_;
  uint16 size_of_optional_header_;
  uint16 offset_of_data_directories_;

  uint16 machine_type_;
  uint16 number_of_sections_;
  const Section *sections_;
  bool has_text_section_;

  uint32 size_of_code_;
  uint32 size_of_initialized_data_;
  uint32 size_of_uninitialized_data_;
  RVA base_of_code_;
  RVA base_of_data_;

  uint64 image_base_;  // range limited to 32 bits for 32 bit executable
  uint32 size_of_image_;
  int number_of_data_directories_;

  ImageDataDirectory export_table_;
  ImageDataDirectory import_table_;
  ImageDataDirectory resource_table_;
  ImageDataDirectory exception_table_;
  ImageDataDirectory base_relocation_table_;
  ImageDataDirectory bound_import_table_;
  ImageDataDirectory import_address_table_;
  ImageDataDirectory delay_import_descriptor_;
  ImageDataDirectory clr_runtime_header_;

 private:
  bool ReadDataDirectory(int index, ImageDataDirectory* dir);
  bool Bad(const char *reason);

  DISALLOW_COPY_AND_ASSIGN(PEInfo);
};

}  // namespace
#endif  // COURGETTE_IMAGE_INFO_H_
