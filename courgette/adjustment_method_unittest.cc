// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/path_service.h"
#include "base/file_util.h"
#include "base/string_util.h"

#include "courgette/assembly_program.h"
#include "courgette/courgette.h"
#include "courgette/streams.h"

#include "testing/gtest/include/gtest/gtest.h"

class AdjustmentMethodTest : public testing::Test {
 public:
  void Test1() const;

 private:
  void SetUp() {
  }

  void TearDown() {
  }

  // Returns one of two similar a simple programs.  They differ only in the
  // label assignment, so that it is possible to make them look identical.
  courgette::AssemblyProgram* MakeProgram(int kind) const {
    courgette::AssemblyProgram* prog = new courgette::AssemblyProgram();
    prog->set_image_base(0x00400000);

    courgette::Label* labelA = prog->FindOrMakeAbs32Label(0x00410000);
    courgette::Label* labelB = prog->FindOrMakeAbs32Label(0x00410004);

    prog->EmitAbs32(labelA);
    prog->EmitAbs32(labelA);
    prog->EmitAbs32(labelB);
    prog->EmitAbs32(labelA);
    prog->EmitAbs32(labelA);
    prog->EmitAbs32(labelB);

    if (kind == 0) {
      labelA->index_ = 0;
      labelB->index_ = 1;
    } else {
      labelA->index_ = 1;
      labelB->index_ = 0;
    }
    prog->AssignRemainingIndexes();

    return prog;
  }

  courgette::AssemblyProgram* MakeProgramA() const { return MakeProgram(0); }
  courgette::AssemblyProgram* MakeProgramB() const { return MakeProgram(1); }

  // Returns a string that is the serialized version of |program|.
  // Deletes |program|.
  std::string Serialize(courgette::AssemblyProgram *program) const {
    courgette::EncodedProgram* encoded = NULL;

    const courgette::Status encode_status = Encode(program, &encoded);
    EXPECT_EQ(courgette::C_OK, encode_status);

    DeleteAssemblyProgram(program);

    courgette::SinkStreamSet sinks;
    const courgette::Status write_status = WriteEncodedProgram(encoded, &sinks);
    EXPECT_EQ(courgette::C_OK, write_status);

    DeleteEncodedProgram(encoded);

    courgette::SinkStream sink;
    bool can_collect = sinks.CopyTo(&sink);
    EXPECT_TRUE(can_collect);

    return std::string(reinterpret_cast<const char *>(sink.Buffer()),
                       sink.Length());
  }
};


void AdjustmentMethodTest::Test1() const {
  courgette::AssemblyProgram* prog1 = MakeProgramA();
  courgette::AssemblyProgram* prog2 = MakeProgramB();
  std::string s1 = Serialize(prog1);
  std::string s2 = Serialize(prog2);

  // Don't use EXPECT_EQ because strings are unprintable.
  EXPECT_FALSE(s1 == s2);  // Unadjusted A and B differ.

  courgette::AssemblyProgram* prog5 = MakeProgramA();
  courgette::AssemblyProgram* prog6 = MakeProgramB();
  courgette::Status can_adjust = Adjust(*prog5, prog6);
  EXPECT_EQ(courgette::C_OK, can_adjust);
  std::string s5 = Serialize(prog5);
  std::string s6 = Serialize(prog6);

  EXPECT_TRUE(s1 == s5);  // Adjustment did not change A (prog5)
  EXPECT_TRUE(s5 == s6);  // Adjustment did change B into A
}


TEST_F(AdjustmentMethodTest, All) {
  Test1();
}
