// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/encoded_program.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/sys_info.h"

#include "courgette/courgette.h"
#include "courgette/streams.h"

namespace courgette {

// Stream indexes.
const int kStreamMisc    = 0;
const int kStreamOps     = 1;
const int kStreamBytes   = 2;
const int kStreamAbs32Indexes = 3;
const int kStreamRel32Indexes = 4;
const int kStreamAbs32Addresses = 5;
const int kStreamRel32Addresses = 6;
const int kStreamCopyCounts = 7;
const int kStreamOriginAddresses = kStreamMisc;

const int kStreamLimit = 9;

// Binary assembly language operations.
enum EncodedProgram::OP {
  ORIGIN,    // ORIGIN <rva> - set address for subsequent assembly.
  COPY,      // COPY <count> <bytes> - copy bytes to output.
  COPY1,     // COPY1 <byte> - same as COPY 1 <byte>.
  REL32,     // REL32 <index> - emit rel32 encoded reference to address at
             // address table offset <index>
  ABS32,     // ABS32 <index> - emit abs32 encoded reference to address at
             // address table offset <index>
  MAKE_BASE_RELOCATION_TABLE,  // Emit base relocation table blocks.
  OP_LAST
};


// Constructor is here rather than in the header.  Although the constructor
// appears to do nothing it is fact quite large because of the implict calls to
// field constructors.  Ditto for the destructor.
EncodedProgram::EncodedProgram() {}
EncodedProgram::~EncodedProgram() {}

// Serializes a vector of integral values using Varint32 coding.
template<typename T>
void WriteVector(const std::vector<T>& items, SinkStream* buffer) {
  size_t count = items.size();
  buffer->WriteVarint32(count);
  for (size_t i = 0;  i < count;  ++i) {
    COMPILE_ASSERT(sizeof(T) <= sizeof(uint32), T_must_fit_in_uint32);
    buffer->WriteVarint32(static_cast<uint32>(items[i]));
  }
}

template<typename T>
bool ReadVector(std::vector<T>* items, SourceStream* buffer) {
  uint32 count;
  if (!buffer->ReadVarint32(&count))
    return false;

  items->clear();
  items->reserve(count);
  for (size_t i = 0;  i < count;  ++i) {
    uint32 item;
    if (!buffer->ReadVarint32(&item))
      return false;
    items->push_back(static_cast<T>(item));
  }

  return true;
}

// Serializes a vector, using delta coding followed by Varint32 coding.
void WriteU32Delta(const std::vector<uint32>& set, SinkStream* buffer) {
  size_t count = set.size();
  buffer->WriteVarint32(count);
  uint32 prev = 0;
  for (size_t i = 0;  i < count;  ++i) {
    uint32 current = set[i];
    uint32 delta = current - prev;
    buffer->WriteVarint32(delta);
    prev = current;
  }
}

static bool ReadU32Delta(std::vector<uint32>* set, SourceStream* buffer) {
  uint32 count;

  if (!buffer->ReadVarint32(&count))
    return false;

  set->clear();
  set->reserve(count);
  uint32 prev = 0;

  for (size_t i = 0;  i < count;  ++i) {
    uint32 delta;
    if (!buffer->ReadVarint32(&delta))
      return false;
    uint32 current = prev + delta;
    set->push_back(current);
    prev = current;
  }

  return true;
}

// Write a vector as the byte representation of the contents.
//
// (This only really makes sense for a type T that has sizeof(T)==1, otherwise
// serilized representation is not endian-agnositic.  But it is useful to keep
// the possibility of a greater size for experiments comparing Varint32 encoding
// of a vector of larger integrals vs a plain form.)
//
template<typename T>
void WriteVectorU8(const std::vector<T>& items, SinkStream* buffer) {
  uint32 count = items.size();
  buffer->WriteVarint32(count);
  if (count != 0) {
    size_t byte_count = count * sizeof(T);
    buffer->Write(static_cast<const void*>(&items[0]), byte_count);
  }
}

template<typename T>
bool ReadVectorU8(std::vector<T>* items, SourceStream* buffer) {
  uint32 count;
  if (!buffer->ReadVarint32(&count))
    return false;

  items->clear();
  items->resize(count);
  if (count != 0) {
    size_t byte_count = count * sizeof(T);
    return buffer->Read(static_cast<void*>(&((*items)[0])), byte_count);
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////

void EncodedProgram::DefineRel32Label(int index, RVA value) {
  DefineLabelCommon(&rel32_rva_, index, value);
}

void EncodedProgram::DefineAbs32Label(int index, RVA value) {
  DefineLabelCommon(&abs32_rva_, index, value);
}

static const RVA kUnassignedRVA = static_cast<RVA>(-1);

void EncodedProgram::DefineLabelCommon(std::vector<RVA>* rvas,
                                       int index,
                                       RVA rva) {
  if (static_cast<int>(rvas->size()) <= index) {
    rvas->resize(index + 1, kUnassignedRVA);
  }
  if ((*rvas)[index] != kUnassignedRVA) {
    NOTREACHED() << "DefineLabel double assigned " << index;
  }
  (*rvas)[index] = rva;
}

void EncodedProgram::EndLabels() {
  FinishLabelsCommon(&abs32_rva_);
  FinishLabelsCommon(&rel32_rva_);
}

void EncodedProgram::FinishLabelsCommon(std::vector<RVA>* rvas) {
  // Replace all unassigned slots with the value at the previous index so they
  // delta-encode to zero.  (There might be better values than zero.  The way to
  // get that is have the higher level assembly program assign the unassigned
  // slots.)
  RVA previous = 0;
  size_t size = rvas->size();
  for (size_t i = 0;  i < size;  ++i) {
    if ((*rvas)[i] == kUnassignedRVA)
      (*rvas)[i] = previous;
    else
      previous = (*rvas)[i];
  }
}

void EncodedProgram::AddOrigin(RVA origin) {
  ops_.push_back(ORIGIN);
  origins_.push_back(origin);
}

void EncodedProgram::AddCopy(int count, const void* bytes) {
  const uint8* source = static_cast<const uint8*>(bytes);

  // Fold adjacent COPY instructions into one.  This nearly halves the size of
  // an EncodedProgram with only COPY1 instructions since there are approx plain
  // 16 bytes per reloc.  This has a working-set benefit during decompression.
  // For compression of files with large differences this makes a small (4%)
  // improvement in size.  For files with small differences this degrades the
  // compressed size by 1.3%
  if (ops_.size() > 0) {
    if (ops_.back() == COPY1) {
      ops_.back() = COPY;
      copy_counts_.push_back(1);
    }
    if (ops_.back() == COPY) {
      copy_counts_.back() += count;
      for (int i = 0;  i < count;  ++i) {
        copy_bytes_.push_back(source[i]);
      }
      return;
    }
  }

  if (count == 1) {
    ops_.push_back(COPY1);
    copy_bytes_.push_back(source[0]);
  } else {
    ops_.push_back(COPY);
    copy_counts_.push_back(count);
    for (int i = 0;  i < count;  ++i) {
      copy_bytes_.push_back(source[i]);
    }
  }
}

void EncodedProgram::AddAbs32(int label_index) {
  ops_.push_back(ABS32);
  abs32_ix_.push_back(label_index);
}

void EncodedProgram::AddRel32(int label_index) {
  ops_.push_back(REL32);
  rel32_ix_.push_back(label_index);
}

void EncodedProgram::AddMakeRelocs() {
  ops_.push_back(MAKE_BASE_RELOCATION_TABLE);
}

void EncodedProgram::DebuggingSummary() {
  LOG(INFO) << "EncodedProgram Summary";
  LOG(INFO) << "  image base  " << image_base_;
  LOG(INFO) << "  abs32 rvas  " << abs32_rva_.size();
  LOG(INFO) << "  rel32 rvas  " << rel32_rva_.size();
  LOG(INFO) << "  ops         " << ops_.size();
  LOG(INFO) << "  origins     " << origins_.size();
  LOG(INFO) << "  copy_counts " << copy_counts_.size();
  LOG(INFO) << "  copy_bytes  " << copy_bytes_.size();
  LOG(INFO) << "  abs32_ix    " << abs32_ix_.size();
  LOG(INFO) << "  rel32_ix    " << rel32_ix_.size();
}

////////////////////////////////////////////////////////////////////////////////

// For algorithm refinement purposes it is useful to write subsets of the file
// format.  This gives us the ability to estimate the entropy of the
// differential compression of the individual streams, which can provide
// invaluable insights.  The default, of course, is to include all the streams.
//
enum FieldSelect {
  INCLUDE_ABS32_ADDRESSES = 0x0001,
  INCLUDE_REL32_ADDRESSES = 0x0002,
  INCLUDE_ABS32_INDEXES   = 0x0010,
  INCLUDE_REL32_INDEXES   = 0x0020,
  INCLUDE_OPS             = 0x0100,
  INCLUDE_BYTES           = 0x0200,
  INCLUDE_COPY_COUNTS     = 0x0400,
  INCLUDE_MISC            = 0x1000
};

static FieldSelect GetFieldSelect() {
#if 1
  // TODO(sra): Use better configuration.
  std::wstring s = base::SysInfo::GetEnvVar(L"A_FIELDS");
  if (!s.empty()) {
    return static_cast<FieldSelect>(wcstoul(s.c_str(), 0, 0));
  }
#endif
  return  static_cast<FieldSelect>(~0);
}

void EncodedProgram::WriteTo(SinkStreamSet* streams) {
  FieldSelect select = GetFieldSelect();

  // The order of fields must be consistent in WriteTo and ReadFrom, regardless
  // of the streams used.  The code can be configured with all kStreamXXX
  // constants the same.
  //
  // If we change the code to pipeline reading with assembly (to avoid temporary
  // storage vectors by consuming operands directly from the stream) then we
  // need to read the base address and the random access address tables first,
  // the rest can be interleaved.

  if (select & INCLUDE_MISC) {
    // TODO(sra): write 64 bits.
    streams->stream(kStreamMisc)->WriteVarint32(
      static_cast<uint32>(image_base_));
  }

  if (select & INCLUDE_ABS32_ADDRESSES)
    WriteU32Delta(abs32_rva_, streams->stream(kStreamAbs32Addresses));
  if (select & INCLUDE_REL32_ADDRESSES)
    WriteU32Delta(rel32_rva_, streams->stream(kStreamRel32Addresses));
  if (select & INCLUDE_MISC)
    WriteVector(origins_, streams->stream(kStreamOriginAddresses));
  if (select & INCLUDE_OPS) {
    streams->stream(kStreamOps)->Reserve(ops_.size() + 5);  // 5 for length.
    WriteVector(ops_, streams->stream(kStreamOps));
  }
  if (select & INCLUDE_COPY_COUNTS)
    WriteVector(copy_counts_, streams->stream(kStreamCopyCounts));
  if (select & INCLUDE_BYTES)
    WriteVectorU8(copy_bytes_, streams->stream(kStreamBytes));
  if (select & INCLUDE_ABS32_INDEXES)
    WriteVector(abs32_ix_, streams->stream(kStreamAbs32Indexes));
  if (select & INCLUDE_REL32_INDEXES)
    WriteVector(rel32_ix_, streams->stream(kStreamRel32Indexes));
}

bool EncodedProgram::ReadFrom(SourceStreamSet* streams) {
  // TODO(sra): read 64 bits.
  uint32 temp;
  if (!streams->stream(kStreamMisc)->ReadVarint32(&temp))
    return false;
  image_base_ = temp;

  if (!ReadU32Delta(&abs32_rva_, streams->stream(kStreamAbs32Addresses)))
    return false;
  if (!ReadU32Delta(&rel32_rva_, streams->stream(kStreamRel32Addresses)))
    return false;
  if (!ReadVector(&origins_, streams->stream(kStreamOriginAddresses)))
    return false;
  if (!ReadVector(&ops_, streams->stream(kStreamOps)))
    return false;
  if (!ReadVector(&copy_counts_, streams->stream(kStreamCopyCounts)))
    return false;
  if (!ReadVectorU8(&copy_bytes_, streams->stream(kStreamBytes)))
    return false;
  if (!ReadVector(&abs32_ix_, streams->stream(kStreamAbs32Indexes)))
    return false;
  if (!ReadVector(&rel32_ix_, streams->stream(kStreamRel32Indexes)))
    return false;

  // Check that streams have been completely consumed.
  for (int i = 0;  i < kStreamLimit;  ++i) {
    if (streams->stream(i)->Remaining() > 0)
      return false;
  }

  return true;
}

// Safe, non-throwing version of std::vector::at().  Returns 'true' for success,
// 'false' for out-of-bounds index error.
template<typename T>
bool VectorAt(const std::vector<T>& v, size_t index, T* output) {
  if (index >= v.size())
    return false;
  *output = v[index];
  return true;
}

bool EncodedProgram::AssembleTo(SinkStream* final_buffer) {
  // For the most part, the assembly process walks the various tables.
  // ix_mumble is the index into the mumble table.
  size_t ix_origins = 0;
  size_t ix_copy_counts = 0;
  size_t ix_copy_bytes = 0;
  size_t ix_abs32_ix = 0;
  size_t ix_rel32_ix = 0;

  RVA current_rva = 0;

  bool pending_base_relocation_table = false;
  SinkStream bytes_following_base_relocation_table;

  SinkStream* output = final_buffer;

  for (size_t ix_ops = 0;  ix_ops < ops_.size();  ++ix_ops) {
    OP op = ops_[ix_ops];

    switch (op) {
      default:
        return false;

      case ORIGIN: {
        RVA section_rva;
        if (!VectorAt(origins_, ix_origins, &section_rva))
          return false;
        ++ix_origins;
        current_rva = section_rva;
        break;
      }

      case COPY: {
        int count;
        if (!VectorAt(copy_counts_, ix_copy_counts, &count))
          return false;
        ++ix_copy_counts;
        for (int i = 0;  i < count;  ++i) {
          uint8 b;
          if (!VectorAt(copy_bytes_, ix_copy_bytes, &b))
            return false;
          ++ix_copy_bytes;
          output->Write(&b, 1);
        }
        current_rva += count;
        break;
      }

      case COPY1: {
        uint8 b;
        if (!VectorAt(copy_bytes_, ix_copy_bytes, &b))
          return false;
        ++ix_copy_bytes;
        output->Write(&b, 1);
        current_rva += 1;
        break;
      }

      case REL32: {
        uint32 index;
        if (!VectorAt(rel32_ix_, ix_rel32_ix, &index))
          return false;
        ++ix_rel32_ix;
        RVA rva;
        if (!VectorAt(rel32_rva_, index, &rva))
          return false;
        uint32 offset = (rva - (current_rva + 4));
        output->Write(&offset, 4);
        current_rva += 4;
        break;
      }

      case ABS32: {
        uint32 index;
        if (!VectorAt(abs32_ix_, ix_abs32_ix, &index))
          return false;
        ++ix_abs32_ix;
        RVA rva;
        if (!VectorAt(abs32_rva_, index, &rva))
          return false;
        uint32 abs32 = static_cast<uint32>(rva + image_base_);
        abs32_relocs_.push_back(current_rva);
        output->Write(&abs32, 4);
        current_rva += 4;
        break;
      }

      case MAKE_BASE_RELOCATION_TABLE: {
        // We can see the base relocation anywhere, but we only have the
        // information to generate it at the very end.  So we divert the bytes
        // we are generating to a temporary stream.
        if (pending_base_relocation_table)  // Can't have two base relocation
                                            // tables.
          return false;

        pending_base_relocation_table = true;
        output = &bytes_following_base_relocation_table;
        break;
        // There is a potential problem *if* the instruction stream contains
        // some REL32 relocations following the base relocation and in the same
        // section.  We don't know the size of the table, so 'current_rva' will
        // be wrong, causing REL32 offsets to be miscalculated.  This never
        // happens; the base relocation table is usually in a section of its
        // own, a data-only section, and following everything else in the
        // executable except some padding zero bytes.  We could fix this by
        // emitting an ORIGIN after the MAKE_BASE_RELOCATION_TABLE.
      }
    }
  }

  if (pending_base_relocation_table) {
    GenerateBaseRelocations(final_buffer);
    final_buffer->Append(&bytes_following_base_relocation_table);
  }

  // Final verification check: did we consume all lists?
  if (ix_copy_counts != copy_counts_.size())
    return false;
  if (ix_copy_bytes != copy_bytes_.size())
    return false;
  if (ix_abs32_ix != abs32_ix_.size())
    return false;
  if (ix_rel32_ix != rel32_ix_.size())
    return false;

  return true;
}


// RelocBlock has the layout of a block of relocations in the base relocation
// table file format.
//
class RelocBlock {
 public:
  uint32 page_rva;
  uint32 block_size;
  uint16 relocs[4096];  // Allow up to one relocation per byte of a 4k page.

  RelocBlock() : page_rva(~0), block_size(8) {}

  void Add(uint16 item) {
    relocs[(block_size-8)/2] = item;
    block_size += 2;
  }

  void Flush(SinkStream* buffer) {
    if (block_size != 8) {
      if (block_size % 4 != 0) {  // Pad to make size multiple of 4 bytes.
        Add(0);
      }
      buffer->Write(this, block_size);
      block_size = 8;
    }
  }
};

COMPILE_ASSERT(offsetof(RelocBlock, relocs) == 8, reloc_block_header_size);

void EncodedProgram::GenerateBaseRelocations(SinkStream* buffer) {
  std::sort(abs32_relocs_.begin(), abs32_relocs_.end());

  RelocBlock block;

  for (size_t i = 0;  i < abs32_relocs_.size();  ++i) {
    uint32 rva = abs32_relocs_[i];
    uint32 page_rva = rva & ~0xFFF;
    if (page_rva != block.page_rva) {
      block.Flush(buffer);
      block.page_rva = page_rva;
    }
    block.Add(0x3000 | (rva & 0xFFF));
  }
  block.Flush(buffer);
}

////////////////////////////////////////////////////////////////////////////////

Status WriteEncodedProgram(EncodedProgram* encoded, SinkStreamSet* sink) {
  encoded->WriteTo(sink);
  return C_OK;
}

Status ReadEncodedProgram(SourceStreamSet* streams, EncodedProgram** output) {
  EncodedProgram* encoded = new EncodedProgram();
  if (encoded->ReadFrom(streams)) {
    *output = encoded;
    return C_OK;
  }
  delete encoded;
  return C_DESERIALIZATION_FAILED;
}

Status Assemble(EncodedProgram* encoded, SinkStream* buffer) {
  bool assembled = encoded->AssembleTo(buffer);
  if (assembled)
    return C_OK;
  return C_ASSEMBLY_FAILED;
}

void DeleteEncodedProgram(EncodedProgram* encoded) {
  delete encoded;
}

}  // end namespace
