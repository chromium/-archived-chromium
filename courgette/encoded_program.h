// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_ENCODED_PROGRAM_H_
#define COURGETTE_ENCODED_PROGRAM_H_

#include <vector>

#include "base/basictypes.h"
#include "courgette/image_info.h"

namespace courgette {

class SinkStream;
class SinkStreamSet;
class SourceStreamSet;

// An EncodedProgram is a set of tables that contain a simple 'binary assembly
// language' that can be assembled to produce a sequence of bytes, for example,
// a Windows 32-bit executable.
//
class EncodedProgram {
 public:
  EncodedProgram();
  ~EncodedProgram();

  // Generating an EncodedProgram:
  //
  // (1) The image base can be specified at any time.
  void set_image_base(uint64 base) { image_base_ = base; }

  // (2) Address tables and indexes defined first.
  void DefineRel32Label(int index, RVA address);
  void DefineAbs32Label(int index, RVA address);
  void EndLabels();

  // (3) Add instructions in the order needed to generate bytes of file.
  void AddOrigin(RVA rva);
  void AddCopy(int count, const void* bytes);
  void AddRel32(int label_index);
  void AddAbs32(int label_index);
  void AddMakeRelocs();

  // (3) Serialize binary assembly language tables to a set of streams.
  void WriteTo(SinkStreamSet *streams);

  // Using an EncodedProgram to generate a byte stream:
  //
  // (4) Deserializes a fresh EncodedProgram from a set of streams.
  bool ReadFrom(SourceStreamSet *streams);

  // (5) Assembles the 'binary assembly language' into final file.
  bool AssembleTo(SinkStream *buffer);

 private:
  enum OP;    // Binary assembly language operations.

  void DebuggingSummary();
  void GenerateBaseRelocations(SinkStream *buffer);
  void DefineLabelCommon(std::vector<RVA>*, int, RVA);
  void FinishLabelsCommon(std::vector<RVA>* addresses);

  // Binary assembly language tables.
  uint64 image_base_;
  std::vector<RVA> rel32_rva_;
  std::vector<RVA> abs32_rva_;
  std::vector<OP> ops_;
  std::vector<RVA> origins_;
  std::vector<int> copy_counts_;
  std::vector<uint8> copy_bytes_;
  std::vector<uint32> rel32_ix_;
  std::vector<uint32> abs32_ix_;

  // Table of the addresses containing abs32 relocations; computed during
  // assembly, used to generate base relocation table.
  std::vector<uint32> abs32_relocs_;

  DISALLOW_COPY_AND_ASSIGN(EncodedProgram);
};

}  // namespace courgette
#endif  // COURGETTE_ENCODED_FORMAT_H_
