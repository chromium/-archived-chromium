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


// This file contains the definition of class JsonWriter.

#include "utils/cross/json_writer.h"
#include "base/logging.h"
#include "base/string_util.h"

using std::string;

namespace o3d {
JsonWriter::JsonWriter(TextWriter* writer, int indent_spaces)
    : writer_(writer),
      indent_spaces_(indent_spaces),
      compacting_level_(0),
      current_indentation_(0),
      new_line_pending_(false),
      comma_pending_(false) {
  DCHECK(writer_);
}

JsonWriter::~JsonWriter() {
  Close();
}

void JsonWriter::OpenObject() {
  DCHECK(writer_);
  WritePending();
  writer_->WriteChar('{');
  IncreaseIndentation();
  ScheduleNewLine();
}

void JsonWriter::CloseObject() {
  DCHECK(writer_);
  CancelComma();
  DecreaseIndentation();
  WritePending();
  writer_->WriteChar('}');
  ScheduleComma();
  ScheduleNewLine();
}

void JsonWriter::OpenArray() {
  DCHECK(writer_);
  WritePending();
  writer_->WriteChar('[');
  IncreaseIndentation();
  ScheduleNewLine();
}

void JsonWriter::CloseArray() {
  DCHECK(writer_);
  CancelComma();
  DecreaseIndentation();
  WritePending();
  writer_->WriteChar(']');
  ScheduleComma();
  ScheduleNewLine();
}

void JsonWriter::BeginCompacting() {
  WritePending();
  ++compacting_level_;
}

void JsonWriter::EndCompacting() {
  DCHECK_GT(compacting_level_, 0);
  --compacting_level_;
}

void JsonWriter::WritePropertyName(const string& name) {
  DCHECK(writer_);
  WritePending();
  writer_->WriteChar('\"');
  WriteEscapedString(name);
  writer_->WriteChar('\"');
  if (compacting_level_ > 0) {
    writer_->WriteChar(':');
  } else {
    writer_->WriteString(string(": "));
  }
}

void JsonWriter::WriteBool(bool value) {
  DCHECK(writer_);
  WritePending();
  writer_->WriteBool(value);
  ScheduleComma();
  ScheduleNewLine();
}

void JsonWriter::WriteInt(int value) {
  DCHECK(writer_);
  WritePending();
  writer_->WriteInt(value);
  ScheduleComma();
  ScheduleNewLine();
}

void JsonWriter::WriteUnsignedInt(unsigned int value) {
  DCHECK(writer_);
  WritePending();
  writer_->WriteUnsignedInt(value);
  ScheduleComma();
  ScheduleNewLine();
}

void JsonWriter::WriteFloat(float value) {
  DCHECK(writer_);
  WritePending();
  writer_->WriteFloat(value);
  ScheduleComma();
  ScheduleNewLine();
}

void JsonWriter::WriteString(const string& value) {
  DCHECK(writer_);
  WritePending();
  writer_->WriteChar('\"');
  WriteEscapedString(value);
  writer_->WriteChar('\"');
  ScheduleComma();
  ScheduleNewLine();
}

void JsonWriter::WriteNull() {
  DCHECK(writer_);
  WritePending();
  writer_->WriteString(string("null"));
  ScheduleComma();
  ScheduleNewLine();
}

void JsonWriter::Close() {
  if (writer_) {
    CancelComma();
    WritePending();
    writer_->Close();
    writer_ = NULL;
  }
}

void JsonWriter::IncreaseIndentation() {
  ++current_indentation_;
}

void JsonWriter::DecreaseIndentation() {
  DCHECK_GT(current_indentation_, 0);
  --current_indentation_;
}

void JsonWriter::ScheduleNewLine() {
  new_line_pending_ = true;
}

void JsonWriter::ScheduleComma() {
  comma_pending_ = true;
}

void JsonWriter::CancelComma() {
  comma_pending_ = false;
}

void JsonWriter::WritePending() {
  if (comma_pending_) {
    writer_->WriteChar(',');
    comma_pending_ = false;
  }

  if (new_line_pending_) {
    if (compacting_level_ == 0) {
      writer_->WriteNewLine();
      for (int i = 0; i < current_indentation_ * indent_spaces_; ++i) {
        writer_->WriteChar(' ');
      }
    }
    new_line_pending_ = false;
  }
}

void JsonWriter::WriteEscapedString(const string& unescaped) {
  for (string::size_type i = 0; i < unescaped.length(); ++i) {
    char c = unescaped[i];
    switch (c) {
      case '\"':
        writer_->WriteString("\\\"");
        break;
      case '\\':
        writer_->WriteString("\\\\");
        break;
      case '\b':
        writer_->WriteString("\\b");
        break;
      case '\f':
        writer_->WriteString("\\f");
        break;
      case '\n':
        writer_->WriteString("\\n");
        break;
      case '\r':
        writer_->WriteString("\\r");
        break;
      case '\t':
        writer_->WriteString("\\t");
        break;
      default:
        if (c < ' ') {
          writer_->WriteString(StringPrintf("\\u%04x", static_cast<int>(c)));
        } else {
          writer_->WriteChar(c);
        }
    }
  }
}
}  // namespace o3d
