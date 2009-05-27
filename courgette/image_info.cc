// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/image_info.h"

#include <memory.h>
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "base/logging.h"

namespace courgette {

std::string SectionName(const Section* section) {
  if (section == NULL)
    return "<none>";
  char name[9];
  memcpy(name, section->name, 8);
  name[8] = '\0';  // Ensure termination.
  return name;
}

PEInfo::PEInfo()
  : failure_reason_("uninitialized"),
    start_(0), end_(0), length_(0),
    is_PE32_plus_(0), file_length_(0), has_text_section_(false) {
}

void PEInfo::Init(const void* start, size_t length) {
  start_ = reinterpret_cast<const uint8*>(start);
  length_ = length;
  end_ = start_ + length_;
  failure_reason_ = "unparsed";
}

// DescribeRVA is for debugging only.  I would put it under #ifdef DEBUG except
// that during development I'm finding I need to call it when compiled in
// Release mode.  Hence:
// TODO(sra): make this compile only for debug mode.
std::string PEInfo::DescribeRVA(RVA rva) const {
  const Section* section = RVAToSection(rva);
  std::ostringstream s;
  s << std::hex << rva;
  if (section) {
    s << " (";
    s << SectionName(section) << "+"
      << std::hex << (rva - section->virtual_address)
      << ")";
  }
  return s.str();
}

const Section* PEInfo::FindNextSection(uint32 fileOffset) const {
  const Section* best = 0;
  for (int i = 0; i < number_of_sections_; i++) {
    const Section* section = &sections_[i];
    if (section->size_of_raw_data > 0) {  // i.e. has data in file.
      if (fileOffset <= section->file_offset_of_raw_data) {
        if (best == 0 ||
            section->file_offset_of_raw_data < best->file_offset_of_raw_data) {
          best = section;
        }
      }
    }
  }
  return best;
}

const Section* PEInfo::RVAToSection(RVA rva) const {
  for (int i = 0; i < number_of_sections_; i++) {
    const Section* section = &sections_[i];
    uint32 offset = rva - section->virtual_address;
    if (offset < section->virtual_size) {
      return section;
    }
  }
  return NULL;
}

int PEInfo::RVAToFileOffset(RVA rva) const {
  const Section* section = RVAToSection(rva);
  if (section) {
    uint32 offset = rva - section->virtual_address;
    if (offset < section->size_of_raw_data) {
      return section->file_offset_of_raw_data + offset;
    } else {
      return kNoOffset;  // In section but not in file (e.g. uninit data).
    }
  }

  // Small RVA values point into the file header in the loaded image.
  // RVA 0 is the module load address which Windows uses as the module handle.
  // RVA 2 sometimes occurs, I'm not sure what it is, but it would map into the
  // DOS header.
  if (rva == 0 || rva == 2)
    return rva;

  NOTREACHED();
  return kNoOffset;
}

const uint8* PEInfo::RVAToPointer(RVA rva) const {
  int file_offset = RVAToFileOffset(rva);
  if (file_offset == kNoOffset)
    return NULL;
  else
    return start_ + file_offset;
}

RVA PEInfo::FileOffsetToRVA(uint32 file_offset) const {
  for (int i = 0; i < number_of_sections_; i++) {
    const Section* section = &sections_[i];
    uint32 offset = file_offset - section->file_offset_of_raw_data;
    if (offset < section->size_of_raw_data) {
      return section->virtual_address + offset;
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

namespace {

// Constants and offsets gleaned from WINNT.H and various articles on the
// format of Windows PE executables.

// This is FIELD_OFFSET(IMAGE_DOS_HEADER, e_lfanew):
const size_t kOffsetOfFileAddressOfNewExeHeader = 0x3c;

const uint16 kImageNtOptionalHdr32Magic = 0x10b;
const uint16 kImageNtOptionalHdr64Magic = 0x20b;

const size_t kSizeOfCoffHeader = 20;
const size_t kOffsetOfDataDirectoryFromImageOptionalHeader32 = 96;
const size_t kOffsetOfDataDirectoryFromImageOptionalHeader64 = 112;

// These helper functions avoid the need for casts in the main code.
inline uint16 ReadU16(const uint8* address, size_t offset) {
  return *reinterpret_cast<const uint16*>(address + offset);
}

inline uint32 ReadU32(const uint8* address, size_t offset) {
  return *reinterpret_cast<const uint32*>(address + offset);
}

inline uint64 ReadU64(const uint8* address, size_t offset) {
  return *reinterpret_cast<const uint64*>(address + offset);
}

}  // namespace

// ParseHeader attempts to match up the buffer with the Windows data
// structures that exist within a Windows 'Portable Executable' format file.
// Returns 'true' if the buffer matches, and 'false' if the data looks
// suspicious.  Rather than try to 'map' the buffer to the numerous windows
// structures, we extract the information we need into the courgette::PEInfo
// structure.
//
bool PEInfo::ParseHeader() {
  if (length_ < kOffsetOfFileAddressOfNewExeHeader + 4 /*size*/)
    return Bad("Too small");

  // Have 'MZ' magic for a DOS header?
  if (start_[0] != 'M' || start_[1] != 'Z')
    return Bad("Not MZ");

  // offset from DOS header to PE header is stored in DOS header.
  uint32 offset = ReadU32(start_, kOffsetOfFileAddressOfNewExeHeader);

  const uint8* const pe_header = start_ + offset;
  const size_t kMinPEHeaderSize = 4 /*signature*/ + kSizeOfCoffHeader;
  if (pe_header <= start_  ||  pe_header >= end_ - kMinPEHeaderSize)
    return Bad("Bad offset to PE header");

  if (offset % 8 != 0)
    return Bad("Misaligned PE header");

  // The 'PE' header is an IMAGE_NT_HEADERS structure as defined in WINNT.H.
  // See http://msdn.microsoft.com/en-us/library/ms680336(VS.85).aspx
  //
  // The first field of the IMAGE_NT_HEADERS is the signature.
  if (!(pe_header[0] == 'P' &&
        pe_header[1] == 'E' &&
        pe_header[2] == 0 &&
        pe_header[3] == 0))
    return Bad("no PE signature");

  // The second field of the IMAGE_NT_HEADERS is the COFF header.
  // The COFF header is also called an IMAGE_FILE_HEADER
  //   http://msdn.microsoft.com/en-us/library/ms680313(VS.85).aspx
  const uint8* const coff_header = pe_header + 4;
  machine_type_       = ReadU16(coff_header, 0);
  number_of_sections_ = ReadU16(coff_header, 2);
  size_of_optional_header_ = ReadU16(coff_header, 16);

  // The rest of the IMAGE_NT_HEADERS is the IMAGE_OPTIONAL_HEADER(32|64)
  const uint8* const optional_header = coff_header + kSizeOfCoffHeader;
  optional_header_ = optional_header;

  if (optional_header + size_of_optional_header_ >= end_)
    return Bad("optional header past end of file");

  // Check we can read the magic.
  if (size_of_optional_header_ < 2)
    return Bad("optional header no magic");

  uint16 magic = ReadU16(optional_header, 0);

  if (magic == kImageNtOptionalHdr32Magic) {
    is_PE32_plus_ = false;
    offset_of_data_directories_ =
      kOffsetOfDataDirectoryFromImageOptionalHeader32;
  } else if (magic == kImageNtOptionalHdr64Magic) {
    is_PE32_plus_ = true;
    offset_of_data_directories_ =
      kOffsetOfDataDirectoryFromImageOptionalHeader64;
  } else {
    return Bad("unrecognized magic");
  }

  // Check that we can read the rest of the the fixed fields.  Data directories
  // directly follow the fixed fields of the IMAGE_OPTIONAL_HEADER.
  if (size_of_optional_header_ < offset_of_data_directories_)
    return Bad("optional header too short");

  // The optional header is either an IMAGE_OPTIONAL_HEADER32 or
  // IMAGE_OPTIONAL_HEADER64
  // http://msdn.microsoft.com/en-us/library/ms680339(VS.85).aspx
  //
  // Copy the fields we care about.
  size_of_code_               = ReadU32(optional_header, 4);
  size_of_initialized_data_   = ReadU32(optional_header, 8);
  size_of_uninitialized_data_ = ReadU32(optional_header, 12);
  base_of_code_               = ReadU32(optional_header, 20);
  if (is_PE32_plus_) {
    base_of_data_ = 0;
    image_base_  = ReadU64(optional_header, 24);
  } else {
    base_of_data_ = ReadU32(optional_header, 24);
    image_base_   = ReadU32(optional_header, 28);
  }
  size_of_image_ = ReadU32(optional_header, 56);
  number_of_data_directories_ =
    ReadU32(optional_header, (is_PE32_plus_ ? 108 : 92));

  if (size_of_code_ >= length_ ||
      size_of_initialized_data_ >= length_ ||
      size_of_code_ + size_of_initialized_data_ >= length_) {
    // This validation fires on some perfectly fine executables.
    //  return Bad("code or initialized data too big");
  }

  // TODO(sra): we can probably get rid of most of the data directories.
  bool b = true;
  // 'b &= ...' could be short circuit 'b = b && ...' but it is not necessary
  // for correctness and it compiles smaller this way.
  b &= ReadDataDirectory(0, &export_table_);
  b &= ReadDataDirectory(1, &import_table_);
  b &= ReadDataDirectory(2, &resource_table_);
  b &= ReadDataDirectory(3, &exception_table_);
  b &= ReadDataDirectory(5, &base_relocation_table_);
  b &= ReadDataDirectory(11, &bound_import_table_);
  b &= ReadDataDirectory(12, &import_address_table_);
  b &= ReadDataDirectory(13, &delay_import_descriptor_);
  b &= ReadDataDirectory(14, &clr_runtime_header_);
  if (!b) {
    return Bad("malformed data directory");
  }

  // Sections follow the optional header.
  sections_ =
      reinterpret_cast<const Section*>(optional_header +
                                       size_of_optional_header_);
  file_length_ = 0;

  for (int i = 0;  i < number_of_sections_;  ++i) {
    const Section* section = &sections_[i];

    // TODO(sra): consider using the 'characteristics' field of the section
    // header to see if the section contains instructions.
    if (memcmp(section->name, ".text", 6) == 0)
      has_text_section_ = true;

    uint32 section_end =
        section->file_offset_of_raw_data + section->size_of_raw_data;
    if (section_end > file_length_)
      file_length_ = section_end;
  }

  failure_reason_ = NULL;
  return true;
}

bool PEInfo::ReadDataDirectory(int index, ImageDataDirectory* directory) {
  if (index < number_of_data_directories_) {
    size_t offset = index * 8 + offset_of_data_directories_;
    if (offset >= size_of_optional_header_)
      return Bad("number of data directories inconsistent");
    const uint8* data_directory = optional_header_ + offset;
    if (data_directory < start_ || data_directory + 8 >= end_)
      return Bad("data directory outside image");
    RVA rva = ReadU32(data_directory, 0);
    size_t size  = ReadU32(data_directory, 4);
    if (size > size_of_image_)
      return Bad("data directory size too big");

    // TODO(sra): validate RVA.
    directory->address_ = rva;
    directory->size_ = size;
    return true;
  } else {
    directory->address_ = 0;
    directory->size_ = 0;
    return true;
  }
}

bool PEInfo::Bad(const char* reason) {
  failure_reason_ = reason;
  return false;
}

////////////////////////////////////////////////////////////////////////////////

bool PEInfo::ParseRelocs(std::vector<RVA> *relocs) {
  relocs->clear();

  size_t relocs_size = base_relocation_table_.size_;
  if (relocs_size == 0)
    return true;

  // The format of the base relocation table is a sequence of variable sized
  // IMAGE_BASE_RELOCATION blocks.  Search for
  //   "The format of the base relocation data is somewhat quirky"
  // at http://msdn.microsoft.com/en-us/library/ms809762.aspx

  const uint8* start = RVAToPointer(base_relocation_table_.address_);
  const uint8* end = start + relocs_size;

  // Make sure entire base relocation table is within the buffer.
  if (start < start_ ||
      start >= end_ ||
      end <= start_ ||
      end > end_) {
    return Bad(".relocs outside image");
  }

  const uint8* block = start;

  // Walk the variable sized blocks.
  while (block + 8 < end) {
    RVA page_rva = ReadU32(block, 0);
    uint32 size = ReadU32(block, 4);
    if (size < 8 ||        // Size includes header ...
        size % 4  !=  0)   // ... and is word aligned.
      return Bad("unreasonable relocs block");

    const uint8* end_entries = block + size;

    if (end_entries <= block || end_entries <= start_ || end_entries > end_)
      return Bad(".relocs block outside image");

    // Walk through the two-byte entries.
    for (const uint8* p = block + 8;  p < end_entries;  p += 2) {
      uint16 entry = ReadU16(p, 0);
      int type = entry >> 12;
      int offset = entry & 0xFFF;

      RVA rva = page_rva + offset;
      if (type == 3) {         // IMAGE_REL_BASED_HIGHLOW
        relocs->push_back(rva);
      } else if (type == 0) {  // IMAGE_REL_BASED_ABSOLUTE
        // Ignore, used as padding.
      } else {
        // Does not occur in Windows x86 executables.
        return Bad("unknown type of reloc");
      }
    }

    block += size;
  }

  std::sort(relocs->begin(), relocs->end());

  return true;
}

}  // namespace courgette
