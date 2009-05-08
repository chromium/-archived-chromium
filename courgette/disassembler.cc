// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"

#include "courgette/assembly_program.h"
#include "courgette/courgette.h"
#include "courgette/encoded_program.h"
#include "courgette/image_info.h"

// COURGETTE_HISTOGRAM_TARGETS prints out a histogram of how frequently
// different target addresses are referenced.  Purely for debugging.
#define COURGETTE_HISTOGRAM_TARGETS 0

namespace courgette {

class DisassemblerWin32X86 : public Disassembler {
 public:
  explicit DisassemblerWin32X86(PEInfo* pe_info)
      : pe_info_(pe_info),
        incomplete_disassembly_(false) {
  }

  virtual bool Disassemble(AssemblyProgram* target);

  virtual void Destroy() { delete this; }

 protected:
  PEInfo& pe_info() { return *pe_info_; }

  void ParseFile(AssemblyProgram* target);
  bool ParseAbs32Relocs();
  void ParseRel32RelocsFromSections();
  void ParseRel32RelocsFromSection(const Section* section);

  void ParseNonSectionFileRegion(uint32 start_file_offset,
                                 uint32 end_file_offset,
                                 AssemblyProgram* program);
  void ParseFileRegion(const Section* section,
                       uint32 start_file_offset, uint32 end_file_offset,
                       AssemblyProgram* program);

#if COURGETTE_HISTOGRAM_TARGETS
  void HistogramTargets(const char* kind, const std::map<RVA, int>& map);
#endif

  PEInfo* pe_info_;
  bool incomplete_disassembly_;  // 'true' if can leave out 'uninteresting' bits

  std::vector<RVA> abs32_locations_;
  std::vector<RVA> rel32_locations_;

#if COURGETTE_HISTOGRAM_TARGETS
  std::map<RVA, int> abs32_target_rvas_;
  std::map<RVA, int> rel32_target_rvas_;
#endif
};

bool DisassemblerWin32X86::Disassemble(AssemblyProgram* target) {
  if (!pe_info().ok())
    return false;

  target->set_image_base(pe_info().image_base());

  if (!ParseAbs32Relocs())
    return false;

  ParseRel32RelocsFromSections();

  ParseFile(target);

  target->DefaultAssignIndexes();
  return true;
}

static uint32 Read32LittleEndian(const void* address) {
  return *reinterpret_cast<const uint32*>(address);
}

bool DisassemblerWin32X86::ParseAbs32Relocs() {
  abs32_locations_.clear();
  if (!pe_info().ParseRelocs(&abs32_locations_))
    return false;

  std::sort(abs32_locations_.begin(), abs32_locations_.end());

#if COURGETTE_HISTOGRAM_TARGETS
  for (size_t i = 0;  i < abs32_locations_.size(); ++i) {
    RVA rva = abs32_locations_[i];
    // The 4 bytes at the relocation are a reference to some address.
    uint32 target_address = Read32LittleEndian(pe_info().RVAToPointer(rva));
    ++abs32_target_rvas_[target_address - pe_info().image_base()];
  }
#endif
  return true;
}

void DisassemblerWin32X86::ParseRel32RelocsFromSections() {
  uint32 file_offset = 0;
  while (file_offset < pe_info().length()) {
    const Section* section = pe_info().FindNextSection(file_offset);
    if (section == NULL)
      break;
    if (file_offset < section->file_offset_of_raw_data)
      file_offset = section->file_offset_of_raw_data;
    ParseRel32RelocsFromSection(section);
    file_offset += section->size_of_raw_data;
  }
  std::sort(rel32_locations_.begin(), rel32_locations_.end());

#if COURGETTE_HISTOGRAM_TARGETS
  LOG(INFO) << "abs32_locations_ " << abs32_locations_.size();
  LOG(INFO) << "rel32_locations_ " << rel32_locations_.size();
  LOG(INFO) << "abs32_target_rvas_ " << abs32_target_rvas_.size();
  LOG(INFO) << "rel32_target_rvas_ " << rel32_target_rvas_.size();

  int common = 0;
  std::map<RVA, int>::iterator abs32_iter = abs32_target_rvas_.begin();
  std::map<RVA, int>::iterator rel32_iter = rel32_target_rvas_.begin();
  while (abs32_iter != abs32_target_rvas_.end() &&
         rel32_iter != rel32_target_rvas_.end()) {
    if (abs32_iter->first < rel32_iter->first)
      ++abs32_iter;
    else if (rel32_iter->first < abs32_iter->first)
      ++rel32_iter;
    else {
      ++common;
      ++abs32_iter;
      ++rel32_iter;
    }
  }
  LOG(INFO) << "common " << common;
#endif
}

void DisassemblerWin32X86::ParseRel32RelocsFromSection(const Section* section) {
  // TODO(sra): use characteristic.
  bool isCode = strcmp(section->name, ".text") == 0;
  if (!isCode)
    return;

  uint32 start_file_offset = section->file_offset_of_raw_data;
  uint32 end_file_offset = start_file_offset + section->size_of_raw_data;
  RVA relocs_start_rva = pe_info().base_relocation_table().address_;

  const uint8* start_pointer = pe_info().FileOffsetToPointer(start_file_offset);
  const uint8* end_pointer = pe_info().FileOffsetToPointer(end_file_offset);

  RVA start_rva = pe_info().FileOffsetToRVA(start_file_offset);
  RVA end_rva = start_rva + section->virtual_size;

  // Quick way to convert from Pointer to RVA within a single Section is to
  // subtract 'pointer_to_rva'.
  const uint8* const adjust_pointer_to_rva = start_pointer - start_rva;

  std::vector<RVA>::iterator abs32_pos = abs32_locations_.begin();

  // Find the rel32 relocations.
  const uint8* p = start_pointer;
  while (p < end_pointer) {
    RVA current_rva = p - adjust_pointer_to_rva;
    if (current_rva == relocs_start_rva) {
      uint32 relocs_size = pe_info().base_relocation_table().size_;
      if (relocs_size) {
        p += relocs_size;
        continue;
      }
    }

    //while (abs32_pos != abs32_locations_.end() && *abs32_pos < current_rva)
    //  ++abs32_pos;

    // Heuristic discovery of rel32 locations in instruction stream: are the
    // next few bytes the start of an instruction containing a rel32
    // addressing mode?
    const uint8* rel32 = NULL;

    if (p + 5 < end_pointer) {
      if (*p == 0xE8 || *p == 0xE9) {  // jmp rel32 and call rel32
        rel32 = p + 1;
      }
    }
    if (p + 6 < end_pointer) {
      if (*p == 0x0F  &&  (*(p+1) & 0xF0) == 0x80) {  // Jcc long form
        if (p[1] != 0x8A && p[1] != 0x8B)  // JPE/JPO unlikely
          rel32 = p + 2;
      }
    }
    if (rel32) {
      RVA rel32_rva = rel32 - adjust_pointer_to_rva;

      // Is there an abs32 reloc overlapping the candidate?
      while (abs32_pos != abs32_locations_.end() && *abs32_pos < rel32_rva - 3)
        ++abs32_pos;
      // Now: (*abs32_pos > rel32_rva - 4) i.e. the lowest addressed 4-byte
      // region that could overlap rel32_rva.
      if (abs32_pos != abs32_locations_.end()) {
        if (*abs32_pos < rel32_rva + 4) {
          // Beginning of abs32 reloc is before end of rel32 reloc so they
          // overlap.  Skip four bytes past the abs32 reloc.
          p += (*abs32_pos + 4) - current_rva;
          continue;
        }
      }

      RVA target_rva = rel32_rva + 4 + Read32LittleEndian(rel32);
      // To be valid, rel32 target must be within image, and within this
      // section.
      if (pe_info().IsValidRVA(target_rva) &&
          start_rva <= target_rva && target_rva < end_rva) {
        rel32_locations_.push_back(rel32_rva);
#if COURGETTE_HISTOGRAM_TARGETS
        ++rel32_target_rvas_[target_rva];
#endif
        p += 4;
        continue;
      }
    }
    p += 1;
  }
}

void DisassemblerWin32X86::ParseFile(AssemblyProgram* program) {
  // Walk all the bytes in the file, whether or not in a section.
  uint32 file_offset = 0;
  while (file_offset < pe_info().length()) {
    const Section* section = pe_info().FindNextSection(file_offset);
    if (section == NULL) {
      // No more sections.  There should not be extra stuff following last
      // section.
      //   ParseNonSectionFileRegion(file_offset, pe_info().length(), program);
      break;
    }
    if (file_offset < section->file_offset_of_raw_data) {
      uint32 section_start_offset = section->file_offset_of_raw_data;
      ParseNonSectionFileRegion(file_offset, section_start_offset, program);
      file_offset = section_start_offset;
    }
    uint32 end = file_offset + section->size_of_raw_data;
    ParseFileRegion(section, file_offset, end, program);
    file_offset = end;
  }

#if COURGETTE_HISTOGRAM_TARGETS
  HistogramTargets("abs32 relocs", abs32_target_rvas_);
  HistogramTargets("rel32 relocs", rel32_target_rvas_);
#endif
}

void DisassemblerWin32X86::ParseNonSectionFileRegion(
    uint32 start_file_offset,
    uint32 end_file_offset,
    AssemblyProgram* program) {
  if (incomplete_disassembly_)
    return;

  const uint8* start = pe_info().FileOffsetToPointer(start_file_offset);
  const uint8* end = pe_info().FileOffsetToPointer(end_file_offset);

  const uint8* p = start;

  while (p < end) {
    program->EmitByteInstruction(*p);
    ++p;
  }
}

void DisassemblerWin32X86::ParseFileRegion(
    const Section* section,
    uint32 start_file_offset, uint32 end_file_offset,
    AssemblyProgram* program) {
  RVA relocs_start_rva = pe_info().base_relocation_table().address_;

  const uint8* start_pointer = pe_info().FileOffsetToPointer(start_file_offset);
  const uint8* end_pointer = pe_info().FileOffsetToPointer(end_file_offset);

  RVA start_rva = pe_info().FileOffsetToRVA(start_file_offset);
  RVA end_rva = start_rva + section->virtual_size;

  // Quick way to convert from Pointer to RVA within a single Section is to
  // subtract 'pointer_to_rva'.
  const uint8* const adjust_pointer_to_rva = start_pointer - start_rva;

  std::vector<RVA>::iterator rel32_pos = rel32_locations_.begin();
  std::vector<RVA>::iterator abs32_pos = abs32_locations_.begin();

  program->EmitOriginInstruction(start_rva);

  const uint8* p = start_pointer;

  while (p < end_pointer) {
    RVA current_rva = p - adjust_pointer_to_rva;

    // The base relocation table is usually in the .relocs section, but it could
    // actually be anywhere.  Make sure we skip it because we will regenerate it
    // during assembly.
    if (current_rva == relocs_start_rva) {
      program->EmitMakeRelocsInstruction();
      uint32 relocs_size = pe_info().base_relocation_table().size_;
      if (relocs_size) {
        p += relocs_size;
        continue;
      }
    }

    while (abs32_pos != abs32_locations_.end() && *abs32_pos < current_rva)
      ++abs32_pos;

    if (abs32_pos != abs32_locations_.end() && *abs32_pos == current_rva) {
      uint32 target_address = Read32LittleEndian(p);
      RVA target_rva = target_address - pe_info().image_base();
      // TODO(sra): target could be Label+offset.  It is not clear how to guess
      // which it might be.  We assume offset==0.
      program->EmitAbs32(program->FindOrMakeAbs32Label(target_rva));
      p += 4;
      continue;
    }

    while (rel32_pos != rel32_locations_.end() && *rel32_pos < current_rva)
      ++rel32_pos;

    if (rel32_pos != rel32_locations_.end() && *rel32_pos == current_rva) {
      RVA target_rva = current_rva + 4 + Read32LittleEndian(p);
      program->EmitRel32(program->FindOrMakeRel32Label(target_rva));
      p += 4;
      continue;
    }

    if (incomplete_disassembly_) {
      if ((abs32_pos == abs32_locations_.end() || end_rva <= *abs32_pos) &&
          (rel32_pos == rel32_locations_.end() || end_rva <= *rel32_pos) &&
          (end_rva <= relocs_start_rva || current_rva >= relocs_start_rva)) {
        // No more relocs in this section, don't bother encoding bytes.
        break;
      }
    }

    program->EmitByteInstruction(*p);
    p += 1;
  }
}

#if COURGETTE_HISTOGRAM_TARGETS
// Histogram is printed to std::cout.  It is purely for debugging the algorithm
// and is only enabled manually in 'exploration' builds.  I don't want to add
// command-line configuration for this feature because this code has to be
// small, which means compiled-out.
void DisassemblerWin32X86::HistogramTargets(const char* kind,
                                            const std::map<RVA, int>& map) {
  int total = 0;
  std::map<int, std::vector<RVA> > h;
  for (std::map<RVA, int>::const_iterator p = map.begin();
       p != map.end();
       ++p) {
    h[p->second].push_back(p->first);
    total += p->second;
  }

  std::cout << total << " " << kind << " to "
            << map.size() << " unique targets" << std::endl;

  std::cout << "indegree: #targets-with-indegree (example)" << std::endl;
  const int kFirstN = 15;
  bool someSkipped = false;
  int index = 0;
  for (std::map<int, std::vector<RVA> >::reverse_iterator p = h.rbegin();
       p != h.rend();
       ++p) {
    ++index;
    if (index <= kFirstN || p->first <= 3) {
      if (someSkipped) {
        std::cout << "..." << std::endl;
      }
      size_t count = p->second.size();
      std::cout << std::dec << p->first << ": " << count;
      if (count <= 2) {
        for (size_t i = 0;  i < count;  ++i)
          std::cout << "  " << pe_info().DescribeRVA(p->second[i]);
      }
      std::cout << std::endl;
      someSkipped = false;
    } else {
      someSkipped = true;
    }
  }
}
#endif  // COURGETTE_HISTOGRAM_TARGETS

Disassembler* Disassembler::MakeDisassemberWin32X86(PEInfo* pe_info) {
  return new DisassemblerWin32X86(pe_info);
}

////////////////////////////////////////////////////////////////////////////////

Status ParseWin32X86PE(const void* buffer, size_t length,
                       AssemblyProgram** output) {
  *output = NULL;

  PEInfo* pe_info = new PEInfo();
  pe_info->Init(buffer, length);

  if (!pe_info->ParseHeader()) {
    delete pe_info;
    return C_INPUT_NOT_RECOGNIZED;
  }

  Disassembler* disassembler = Disassembler::MakeDisassemberWin32X86(pe_info);
  AssemblyProgram* program = new AssemblyProgram();

  if (!disassembler->Disassemble(program)) {
    delete program;
    disassembler->Destroy();
    delete pe_info;
    return C_DISASSEMBLY_FAILED;
  }

  disassembler->Destroy();
  delete pe_info;
  *output = program;
  return C_OK;
}

void DeleteAssemblyProgram(AssemblyProgram* program) {
  delete program;
}

}  // namespace courgette
