// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/encoded_program.h"

#include "courgette/streams.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(EncodedProgramTest, Test) {
  //
  // Create a simple program with a few addresses and references and
  // check that the bits produced are as expected.
  //
  courgette::EncodedProgram* program = new courgette::EncodedProgram();

  uint32 base = 0x00900000;
  program->set_image_base(base);

  program->DefineRel32Label(5, 0);  // REL32 index 5 == base + 0
  program->DefineAbs32Label(7, 4);  // ABS32 index 7 == base + 4
  program->EndLabels();

  program->AddOrigin(0);  // Start at base.
  program->AddAbs32(7);
  program->AddRel32(5);

  // Serialize and deserialize.

  courgette::SinkStreamSet sinks;
  program->WriteTo(&sinks);

  courgette::SinkStream sink;
  bool can_collect = sinks.CopyTo(&sink);
  EXPECT_TRUE(can_collect);

  const void* buffer = sink.Buffer();
  size_t length = sink.Length();

  courgette::SourceStreamSet sources;
  bool can_get_source_streams = sources.Init(buffer, length);
  EXPECT_TRUE(can_get_source_streams);

  courgette::EncodedProgram *encoded2 = new courgette::EncodedProgram();
  bool can_read = encoded2->ReadFrom(&sources);
  EXPECT_TRUE(can_read);

  // Finally, try to assemble.
  courgette::SinkStream assembled;
  bool can_assemble = encoded2->AssembleTo(&assembled);
  EXPECT_TRUE(can_assemble);

  const void* assembled_buffer = assembled.Buffer();
  size_t assembled_length = assembled.Length();

  EXPECT_EQ(8, assembled_length);

  static const uint8 golden[] = {
    0x04, 0x00, 0x90, 0x00,  // ABS32 to base + 4
    0xF8, 0xFF, 0xFF, 0xFF   // REL32 from next line to base + 2
  };

  EXPECT_EQ(0, memcmp(assembled_buffer, golden, 8));
}
