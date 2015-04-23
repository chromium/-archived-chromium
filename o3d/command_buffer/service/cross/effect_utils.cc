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


// This file implements effect related utilities.

#include "command_buffer/service/cross/effect_utils.h"

namespace o3d {
namespace command_buffer {

bool ParseEffectData(unsigned int size,
                     const void *data,
                     String *vertex_program_entry,
                     String *fragment_program_entry,
                     String *effect_code) {
  const char *data_char = static_cast<const char *>(data);
  unsigned int index = 0;

  for (; index < size && data_char[index]; ++index) { }
  if (index >= size) return false;
  *vertex_program_entry = String(data_char, index);
  ++index;  // skip \0
  unsigned int fragment_program_entry_begin = index;

  for (; index < size && data_char[index]; ++index) { }
  if (index >= size) return false;
  *fragment_program_entry = String(data_char + fragment_program_entry_begin,
                                   index - fragment_program_entry_begin);
  ++index;  // skip \0
  unsigned int effect_code_begin = index;

  // text doesn't have to be 0-terminated, but look for one so that we don't
  // construct a std::string with a '\0' in it.
  for (; index < size && data_char[index]; ++index) { }
  *effect_code = String(data_char + effect_code_begin,
                        index - effect_code_begin);
  return true;
}

}  // namespace command_buffer
}  // namespace o3d
