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

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/page_state.h"
#include "chrome/common/json_value_serializer.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"

void PageState::InitWithURL(const GURL& url) {
  // Reset our state
  state_.reset(new DictionaryValue);

  std::string query_string = url.query();
  if (query_string.empty())
    return;

  url_parse::Component queryComp, keyComp, valueComp;
  queryComp.len = static_cast<int>(query_string.size());
  while (url_parse::ExtractQueryKeyValue(query_string.c_str(), &queryComp,
                                         &keyComp, &valueComp)) {
    if (keyComp.is_nonempty()) {
      std::string escaped = query_string.substr(valueComp.begin,
                                                valueComp.len);
      // We know that the query string is UTF-8 since it's an internal URL.
      std::wstring value = UTF8ToWide(
          UnescapeURLComponent(escaped, UnescapeRule::REPLACE_PLUS_WITH_SPACE));
      state_->Set(UTF8ToWide(query_string.substr(keyComp.begin, keyComp.len)),
                  new StringValue(value));
    }
  }
}

void PageState::InitWithBytes(const std::string& bytes) {
  // Reset our state. We create a new empty one just in case
  // deserialization fails
  state_.reset(new DictionaryValue);

  JSONStringValueSerializer serializer(bytes);
  Value* root = NULL;

  if (!serializer.Deserialize(&root))
    NOTREACHED();

  if (root != NULL && root->GetType() == Value::TYPE_DICTIONARY) {
    state_.reset(static_cast<DictionaryValue*>(root));
  } else if (root) {
    delete root;
  }
}

void PageState::GetByteRepresentation(std::string* out) const {
  JSONStringValueSerializer serializer(out);
  if (!serializer.Serialize(*state_))
    NOTREACHED();
}

void PageState::SetProperty(const std::wstring& key,
                                   const std::wstring& value) {
  state_->Set(key, new StringValue(value));
}

bool PageState::GetProperty(const std::wstring& key, std::wstring* value) const {
  if (state_->HasKey(key)) {
    Value* v;
    state_->Get(key, &v);
    if (v->GetType() == Value::TYPE_STRING) {
      StringValue* sv = reinterpret_cast<StringValue*>(v);
      sv->GetAsString(value);
      return true;
    }
  }
  return false;
}

void PageState::SetInt64Property(const std::wstring& key, int64 value) {
  wchar_t buff[64];
  _i64tow_s(value, buff, arraysize(buff), 10);
  SetProperty(key, buff);
}

bool PageState::GetInt64Property(const std::wstring& key, int64* value) const {
  std::wstring v;
  if (GetProperty(key, &v)) {
    *value = _wtoi64(v.c_str());
    return true;
  }
  return false;
}

void PageState::SetIntProperty(const std::wstring& key, int value) {
  wchar_t buff[64];
  _itow_s(value, buff, arraysize(buff), 10);
  SetProperty(key, buff);
}

bool PageState::GetIntProperty(const std::wstring& key, int* value) const {
  std::wstring v;
  if (GetProperty(key, &v)) {
    *value = _wtoi(v.c_str());
    return true;
  }
  return false;
}

PageState* PageState::Copy() const {
  PageState* copy = new PageState();
  if (state_.get())
    copy->state_.reset(static_cast<DictionaryValue*>(state_->DeepCopy()));
  return copy;
}
