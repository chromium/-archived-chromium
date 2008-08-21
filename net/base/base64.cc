// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "net/base/base64.h"

#include "third_party/modp_b64/modp_b64.h"

namespace net {

bool Base64Encode(const std::string& input, std::string* output) {
  std::string temp;
  temp.resize(modp_b64_encode_len(input.size()));  // makes room for null byte

  // null terminates result since result is base64 text!
  int input_size = static_cast<int>(input.size());
  int output_size= modp_b64_encode(&(temp[0]), input.data(), input_size);
  if (output_size < 0)
    return false;

  temp.resize(output_size);  // strips off null byte
  output->swap(temp);
  return true;
}

bool Base64Decode(const std::string& input, std::string* output) {
  std::string temp;
  temp.resize(modp_b64_decode_len(input.size()));

  // does not null terminate result since result is binary data!
  int input_size = static_cast<int>(input.size());
  int output_size = modp_b64_decode(&(temp[0]), input.data(), input_size);
  if (output_size < 0)
    return false;

  temp.resize(output_size);
  output->swap(temp);
  return true;
}

}  // namespace net
