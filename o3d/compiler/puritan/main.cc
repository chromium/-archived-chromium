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



// This program demonstrates how to get test case information out of Puritan.

#include <iostream>
#include "test_gen.h"
#include "knobs.h"

static std::string name_of_size(Salem::Puritan::OutputInfo::ArgSize x) {
  switch (x) {
    case Salem::Puritan::OutputInfo::Float1:
    return "float";
    case Salem::Puritan::OutputInfo::Float2:
    return "float2";
    case Salem::Puritan::OutputInfo::Float4:
    return "float4";
    default:
    break;
  }
  return "Impossible";
}


static std::string comma(bool * need_comma) {
  if (*need_comma) {
    return ", ";
  } else {
    *need_comma = true;
    return "";
  }
}

int main (int argc, char * const argv[]) {
  int j = 0;
  if (argc > 1) {
    sscanf(argv[1], "%d", &j);
  }
  
  Salem::Puritan::Knobs options;

  // Set up some options just the way we like
  options.block_count.set(2, 3);
  options.for_count.set(2, 3);
  options.for_nesting.set(2, 3);
  options.array_in_for_use.set(false);
  options.seed.set(j);

  Salem::Puritan::OutputInfo info;


  // Build a test case
  std::string test_case =
    Salem::Puritan::generate(&info, options);

  // Dump out the test information
  std::cout << "(Seed " << options.seed.get() << "), "
    << "(Samplers (";

  bool need_comma = false;

  for (unsigned i = 0; i < info.n_samplers; i++) {
    std::cout << comma(&need_comma) << "in" << i;
  }

  std::cout << ")),"
    << "(Uniforms (";

  need_comma = false;

  for (std::list < 
    std::pair <
    Salem::Puritan::OutputInfo::ArgSize,
    std::string > >::const_iterator 
    i = info.uniforms.begin(); 
  i != info.uniforms.end();
  i++) {
    std::cout << comma(&need_comma) 
      << name_of_size(i->first)
      << " "
      << i->second;
  }

  std::cout << ")), "
    << "(return struct {";

  need_comma = false;
  for (std::list <Salem::Puritan::OutputInfo::ArgSize>::const_iterator
    i = info.returns.begin();
  i != info.returns.end();
  i++) {
    std::cout << comma(&need_comma) << name_of_size(*i);
  }
  
  std::cout << "})\n";
  std::cout << test_case;

  return 1;
}
