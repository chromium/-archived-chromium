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

#include <iostream>
#include <fstream>

#include "plugin/cross/config.h"
#include "base/logging.h"

namespace o3d {

// Checks the driver GUID against the blacklist file.  Returns true if there's a
// match [this driver is blacklisted] or if an IO error occurred.  Check the
// state of input_file to determine which it was.
//
// Note that this function always returns false if the guid is 0, since it will
// be zero if we had a failure in reading it, and the user will already have
// been warned.
bool IsDriverBlacklisted(std::ifstream *input_file, unsigned int guid) {
  if (!guid) {
    return false;
  }
  *input_file >> std::ws;
  while (input_file->good()) {
    if (input_file->peek() == '#') {
      char comment[256];
      input_file->getline(comment, 256);
      // If the line was too long for this to work, it'll set the failbit.
    } else {
      unsigned int id;
      *input_file >> std::hex >> id;
      if (id == guid) {
        return true;
      }
    }
    *input_file >> std::ws; // Skip whitespace here, to catch EOF cleanly.
  }
  if (input_file->fail()) {
    LOG(ERROR) << "Failed to read the blacklisted driver file completely.";
    return true;
  }
  CHECK(input_file->eof());
  return false;
}
}  // namespace o3d
