// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/convert_dict/hunspell_reader.h"

#include "base/string_util.h"

namespace convert_dict {

// This silly 64K buffer is just copied from Hunspell's way of parsing.
const int kLineBufferLen = 65535;
char line_buffer[kLineBufferLen];

// Shortcut for trimming whitespace from both ends of the line.
void TrimLine(std::string* line) {
  if (line->size() > 3 &&
      static_cast<unsigned char>((*line)[0]) == 0xef &&
      static_cast<unsigned char>((*line)[1]) == 0xbb &&
      static_cast<unsigned char>((*line)[2]) == 0xbf)
    *line = line->substr(3);

  TrimWhitespace(*line, TRIM_ALL, line);
}

std::string ReadLine(FILE* file) {
  const char* line = fgets(line_buffer, kLineBufferLen - 1, file);
  if (!line)
    return std::string();

  std::string str = line;
  TrimLine(&str);
  return str;
}

void StripComment(std::string* line) {
  for (size_t i = 0; i < line->size(); i++) {
    if ((*line)[i] == '#') {
      line->resize(i);
      TrimLine(line);
      return;
    }
  }
}

}  // namespace convert_dict
