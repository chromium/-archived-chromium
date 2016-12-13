/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the unit tests for the effect utilities.

#include "tests/common/win/testing_common.h"
#include "command_buffer/service/cross/effect_utils.h"

namespace o3d {
namespace command_buffer {

TEST(ParseEffectDataTest, ValidData) {
  // Tests well-formed data.
  const char kEffect[] = "vertex_entry\0fragment_entry\0effect code";
  String vertex_program_entry;
  String fragment_program_entry;
  String effect_code;
  EXPECT_TRUE(ParseEffectData(sizeof(kEffect), kEffect, &vertex_program_entry,
                              &fragment_program_entry, &effect_code));
  EXPECT_EQ(vertex_program_entry, "vertex_entry");
  EXPECT_EQ(fragment_program_entry, "fragment_entry");
  EXPECT_EQ(effect_code, "effect code");

  // The terminal \0 isn't needed, check that we parse correctly without it.
  EXPECT_TRUE(ParseEffectData(sizeof(kEffect)-1, kEffect,
                              &vertex_program_entry, &fragment_program_entry,
                              &effect_code));
  EXPECT_EQ(vertex_program_entry, "vertex_entry");
  EXPECT_EQ(fragment_program_entry, "fragment_entry");
  EXPECT_EQ(effect_code, "effect code");
}

TEST(ParseEffectDataTest, InvalidData) {
  const char kEffect[] = "vertex_entry\0fragment_entry\0effect code";
  String vertex_program_entry;
  String fragment_program_entry;
  String effect_code;
  // 0-size
  EXPECT_FALSE(ParseEffectData(0, kEffect,
                               &vertex_program_entry, &fragment_program_entry,
                               &effect_code));
  // Only vertex_entry, no \0
  EXPECT_FALSE(ParseEffectData(strlen("vertex_entry"), kEffect,
                               &vertex_program_entry, &fragment_program_entry,
                               &effect_code));
  // Only vertex_entry\0
  EXPECT_FALSE(ParseEffectData(strlen("vertex_entry") + 1, kEffect,
                               &vertex_program_entry, &fragment_program_entry,
                               &effect_code));
  // Only vertex_entry\0fragment_entry, no \0
  EXPECT_FALSE(ParseEffectData(strlen("vertex_entry.fragment_entry"), kEffect,
                               &vertex_program_entry, &fragment_program_entry,
                               &effect_code));
}

}  // namespace command_buffer
}  // namespace o3d
