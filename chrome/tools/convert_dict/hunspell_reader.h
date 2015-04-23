// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TOOLS_DIC_CONVERTER_HUNSPELL_READER_H__
#define CHROME_TOOLS_DIC_CONVERTER_HUNSPELL_READER_H__

#include <string>

// Common routines for reading hunspell files.
namespace convert_dict {

// Reads one line and returns it. Whitespace will be trimmed.
std::string ReadLine(FILE* file);

// Trims whitespace from the beginning and end of the given string. Also trims
// UTF-8 byte order markers from the beginning.
void TrimLine(std::string* line);

// Strips any comments for the given line.
void StripComment(std::string* line);

}  // namespace convert_dict

#endif  // CHROME_TOOLS_DIC_CONVERTER_HUNSPELL_READER_H__
