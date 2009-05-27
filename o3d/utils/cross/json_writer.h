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


// This file contains the declaration of class JsonWriter.

#ifndef O3D_UTILS_CROSS_JSON_WRITER_H_
#define O3D_UTILS_CROSS_JSON_WRITER_H_

#include <string>

#include "base/basictypes.h"
#include "utils/cross/text_writer.h"
#include "utils/cross/structured_writer.h"

namespace o3d {

// A JsonWriter is used to write data to a TextWriter using
// the JSON format. See http://json.org.
class JsonWriter : public StructuredWriter {
 public:
  // Construct a JsonWriter that writes to the specified TextWriter.
  // Specify the number of spaces for each indentation level.
  JsonWriter(TextWriter* writer, int indent_spaces);

  // Flushes the remaining output and closes the underlying TextWriter.
  virtual ~JsonWriter();

  // Writes the opening brace of an JSON object literal.
  virtual void OpenObject();

  // Writes the closing brace of an JSON object literal.
  virtual void CloseObject();

  // Writes the opening bracket of a JSON array literal.
  virtual void OpenArray();

  // Writes the closing bracket of a JSON array literal.
  virtual void CloseArray();

  // Disables redundant white space until compacting is closed.
  // Can be nested.
  virtual void BeginCompacting();

  // Disables previously opened compacting.
  virtual void EndCompacting();

  // Writes the name of a property surrounded by quotes. Follow
  // with a call to one of the functions that writes a value.
  virtual void WritePropertyName(const std::string& name);

  // Writes a boolean literal.
  virtual void WriteBool(bool value);

  // Writes a number literal.
  virtual void WriteInt(int value);

  // Writes a number literal.
  virtual void WriteUnsignedInt(unsigned int value);

  // Writes a number literal.
  virtual void WriteFloat(float value);

  // Writes a string literal with JSON escape sequences for
  // special characters.
  virtual void WriteString(const std::string& value);

  // Writes a null literal.
  virtual void WriteNull();

  // Flushes the remaining output and closes the underlying TextWriter.
  virtual void Close();

 private:
  void IncreaseIndentation();
  void DecreaseIndentation();
  void ScheduleNewLine();
  void ScheduleComma();
  void CancelComma();
  void WritePending();
  void WriteEscapedString(const std::string& unescaped);

  TextWriter* writer_;
  int indent_spaces_;
  int compacting_level_;
  int current_indentation_;
  bool new_line_pending_;
  bool comma_pending_;
  DISALLOW_COPY_AND_ASSIGN(JsonWriter);
};
}  // namespace o3d

#endif  // O3D_UTILS_CROSS_JSON_WRITER_H_
