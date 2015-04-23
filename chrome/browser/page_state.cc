// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  scoped_ptr<Value> root(serializer.Deserialize(NULL));

  if (!root.get()) {
    NOTREACHED();
    return;
  }

  if (root->GetType() == Value::TYPE_DICTIONARY)
    state_.reset(static_cast<DictionaryValue*>(root.release()));
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

bool PageState::GetProperty(const std::wstring& key,
                            std::wstring* value) const {
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
  SetProperty(key, Int64ToWString(value));
}

bool PageState::GetInt64Property(const std::wstring& key, int64* value) const {
  std::wstring v;
  if (GetProperty(key, &v)) {
    return StringToInt64(WideToUTF16Hack(v), value);
  }
  return false;
}

void PageState::SetIntProperty(const std::wstring& key, int value) {
  SetProperty(key, IntToWString(value));
}

bool PageState::GetIntProperty(const std::wstring& key, int* value) const {
  std::wstring v;
  if (GetProperty(key, &v)) {
    return StringToInt(WideToUTF16Hack(v), value);
  }
  return false;
}

PageState* PageState::Copy() const {
  PageState* copy = new PageState();
  if (state_.get())
    copy->state_.reset(static_cast<DictionaryValue*>(state_->DeepCopy()));
  return copy;
}
