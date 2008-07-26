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

#include "base/json_writer.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
#include "base/string_escape.h"

const char kPrettyPrintLineEnding[] = "\r\n";

/* static */
void JSONWriter::Write(const Value* const node, bool pretty_print,
                       std::string* json) {
  json->clear();
  // Is there a better way to estimate the size of the output?
  json->reserve(1024);
  JSONWriter writer(pretty_print, json);
  writer.BuildJSONString(node, 0);
  if (pretty_print)
    json->append(kPrettyPrintLineEnding);
}

JSONWriter::JSONWriter(bool pretty_print, std::string* json)
  : pretty_print_(pretty_print),
    json_string_(json) {
  DCHECK(json);
}

void JSONWriter::BuildJSONString(const Value* const node, int depth) {
  switch(node->GetType()) {
    case Value::TYPE_NULL:
      json_string_->append("null");
      break;

    case Value::TYPE_BOOLEAN:
      {
        bool value;
        bool result = node->GetAsBoolean(&value);
        DCHECK(result);
        json_string_->append(value ? "true" : "false");
        break;
      }

    case Value::TYPE_INTEGER:
      {
        int value;
        bool result = node->GetAsInteger(&value);
        DCHECK(result);
        StringAppendF(json_string_, "%d", value);
        break;
      }

    case Value::TYPE_REAL:
      {
        double value;
        bool result = node->GetAsReal(&value);
        DCHECK(result);
        std::string real = StringPrintf("%g", value);
        // Ensure that the number has a .0 if there's no decimal or 'e'.  This
        // makes sure that when we read the JSON back, it's interpreted as a
        // real rather than an int.
        if (real.find('.') == std::string::npos &&
            real.find('e') == std::string::npos &&
            real.find('E') == std::string::npos) {
          real.append(".0");
        }
        json_string_->append(real);
        break;
      }

    case Value::TYPE_STRING:
      {
        std::wstring value;
        bool result = node->GetAsString(&value);
        DCHECK(result);
        AppendQuotedString(value);
        break;
      }

    case Value::TYPE_LIST:
      {
        json_string_->append("[");
        if (pretty_print_)
          json_string_->append(" ");

        const ListValue* list = static_cast<const ListValue*>(node);
        for (size_t i = 0; i < list->GetSize(); ++i) {
          if (i != 0) {
            json_string_->append(",");
            if (pretty_print_)
              json_string_->append(" ");
          }

          Value* value = NULL;
          bool result = list->Get(i, &value);
          DCHECK(result);
          BuildJSONString(value, depth);
        }

        if (pretty_print_)
          json_string_->append(" ");
        json_string_->append("]");
        break;
      }

    case Value::TYPE_DICTIONARY:
      {
        json_string_->append("{");
        if (pretty_print_)
          json_string_->append(kPrettyPrintLineEnding);

        const DictionaryValue* dict =
          static_cast<const DictionaryValue*>(node);
        for (DictionaryValue::key_iterator key_itr = dict->begin_keys();
             key_itr != dict->end_keys();
             ++key_itr) {

          if (key_itr != dict->begin_keys()) {
            json_string_->append(",");
            if (pretty_print_)
              json_string_->append(kPrettyPrintLineEnding);
          }

          Value* value = NULL;
          bool result = dict->Get(*key_itr, &value);
          DCHECK(result);

          if (pretty_print_)
            IndentLine(depth + 1);
          AppendQuotedString(*key_itr);
          if (pretty_print_) {
            json_string_->append(": ");
          } else {
            json_string_->append(":");
          }
          BuildJSONString(value, depth + 1);
        }

        if (pretty_print_) {
          json_string_->append(kPrettyPrintLineEnding);
          IndentLine(depth);
          json_string_->append("}");
        } else {
          json_string_->append("}");
        }
        break;
      }

    default:
      // TODO(jhughes): handle TYPE_BINARY
      NOTREACHED() << "unknown json type";
  }
}

void JSONWriter::AppendQuotedString(const std::wstring& str) {
  string_escape::JavascriptDoubleQuote(str, true, json_string_);
}

void JSONWriter::IndentLine(int depth) {
  // It may be faster to keep an indent string so we don't have to keep
  // reallocating.
  json_string_->append(std::string(depth * 3, ' '));
}
