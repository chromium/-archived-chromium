// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_codec.h"

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/common/l10n_util.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"

using base::Time;

const wchar_t* BookmarkCodec::kRootsKey = L"roots";
const wchar_t* BookmarkCodec::kRootFolderNameKey = L"bookmark_bar";
const wchar_t* BookmarkCodec::kOtherBookmarFolderNameKey = L"other";
const wchar_t* BookmarkCodec::kVersionKey = L"version";
const wchar_t* BookmarkCodec::kTypeKey = L"type";
const wchar_t* BookmarkCodec::kNameKey = L"name";
const wchar_t* BookmarkCodec::kDateAddedKey = L"date_added";
const wchar_t* BookmarkCodec::kURLKey = L"url";
const wchar_t* BookmarkCodec::kDateModifiedKey = L"date_modified";
const wchar_t* BookmarkCodec::kChildrenKey = L"children";
const wchar_t* BookmarkCodec::kTypeURL = L"url";
const wchar_t* BookmarkCodec::kTypeFolder = L"folder";

// Current version of the file.
static const int kCurrentVersion = 1;

Value* BookmarkCodec::Encode(BookmarkModel* model) {
  return Encode(model->GetBookmarkBarNode(), model->other_node());
}

Value* BookmarkCodec::Encode(BookmarkNode* bookmark_bar_node,
                             BookmarkNode* other_folder_node) {
  DictionaryValue* roots = new DictionaryValue();
  roots->Set(kRootFolderNameKey, EncodeNode(bookmark_bar_node));
  roots->Set(kOtherBookmarFolderNameKey, EncodeNode(other_folder_node));

  DictionaryValue* main = new DictionaryValue();
  main->SetInteger(kVersionKey, kCurrentVersion);
  main->Set(kRootsKey, roots);
  return main;
}

bool BookmarkCodec::Decode(BookmarkModel* model, const Value& value) {
  if (value.GetType() != Value::TYPE_DICTIONARY)
    return false;  // Unexpected type.

  const DictionaryValue& d_value = static_cast<const DictionaryValue&>(value);

  int version;
  if (!d_value.GetInteger(kVersionKey, &version) || version != kCurrentVersion)
    return false;  // Unknown version.

  Value* roots;
  if (!d_value.Get(kRootsKey, &roots))
    return false;  // No roots.

  if (roots->GetType() != Value::TYPE_DICTIONARY)
    return false;  // Invalid type for roots.

  DictionaryValue* roots_d_value = static_cast<DictionaryValue*>(roots);
  Value* root_folder_value;
  Value* other_folder_value;
  if (!roots_d_value->Get(kRootFolderNameKey, &root_folder_value) ||
      root_folder_value->GetType() != Value::TYPE_DICTIONARY ||
      !roots_d_value->Get(kOtherBookmarFolderNameKey, &other_folder_value) ||
      other_folder_value->GetType() != Value::TYPE_DICTIONARY)
    return false;  // Invalid type for root folder and/or other folder.

  DecodeNode(model, *static_cast<DictionaryValue*>(root_folder_value),
             NULL, model->GetBookmarkBarNode());
  DecodeNode(model, *static_cast<DictionaryValue*>(other_folder_value),
             NULL, model->other_node());
  // Need to reset the type as decoding resets the type to FOLDER. Similarly
  // we need to reset the title as the title is persisted and restored from
  // the file.
  model->GetBookmarkBarNode()->type_ = history::StarredEntry::BOOKMARK_BAR;
  model->other_node()->type_ = history::StarredEntry::OTHER;
  model->GetBookmarkBarNode()->SetTitle(
      l10n_util::GetString(IDS_BOOMARK_BAR_FOLDER_NAME));
  model->other_node()->SetTitle(
      l10n_util::GetString(IDS_BOOMARK_BAR_OTHER_FOLDER_NAME));
  return true;
}

Value* BookmarkCodec::EncodeNode(BookmarkNode* node) {
  DictionaryValue* value = new DictionaryValue();
  value->SetString(kNameKey, node->GetTitle());
  value->SetString(kDateAddedKey,
                   Int64ToWString(node->date_added().ToInternalValue()));
  if (node->GetType() == history::StarredEntry::URL) {
    value->SetString(kTypeKey, kTypeURL);
    value->SetString(kURLKey,
                     UTF8ToWide(node->GetURL().possibly_invalid_spec()));
  } else {
    value->SetString(kTypeKey, kTypeFolder);
    value->SetString(kDateModifiedKey,
                     Int64ToWString(node->date_group_modified().
                                    ToInternalValue()));

    ListValue* child_values = new ListValue();
    value->Set(kChildrenKey, child_values);
    for (int i = 0; i < node->GetChildCount(); ++i)
      child_values->Append(EncodeNode(node->GetChild(i)));
  }
  return value;
}

bool BookmarkCodec::DecodeChildren(BookmarkModel* model,
                                   const ListValue& child_value_list,
                                   BookmarkNode* parent) {
  for (size_t i = 0; i < child_value_list.GetSize(); ++i) {
    Value* child_value;
    if (!child_value_list.Get(i, &child_value))
      return false;

    if (child_value->GetType() != Value::TYPE_DICTIONARY)
      return false;

    if (!DecodeNode(model, *static_cast<DictionaryValue*>(child_value), parent,
                    NULL)) {
      return false;
    }
  }
  return true;
}

bool BookmarkCodec::DecodeNode(BookmarkModel* model,
                               const DictionaryValue& value,
                               BookmarkNode* parent,
                               BookmarkNode* node) {
  std::wstring title;
  if (!value.GetString(kNameKey, &title))
    return false;

  // TODO(sky): this should be more flexible. Don't hoark if we can't parse it
  // all.
  std::wstring date_added_string;
  if (!value.GetString(kDateAddedKey, &date_added_string))
    return false;

  std::wstring type_string;
  if (!value.GetString(kTypeKey, &type_string))
    return false;

  if (type_string != kTypeURL && type_string != kTypeFolder)
    return false;  // Unknown type.

  if (type_string == kTypeURL) {
    std::wstring url_string;
    if (!value.GetString(kURLKey, &url_string))
      return false;
    // TODO(sky): this should ignore the node if not a valid URL.
    if (!node)
      node = new BookmarkNode(model, GURL(WideToUTF8(url_string)));
    if (parent)
      parent->Add(parent->GetChildCount(), node);
    node->type_ = history::StarredEntry::URL;
  } else {
    std::wstring last_modified_date;
    if (!value.GetString(kDateModifiedKey, &last_modified_date))
      return false;

    Value* child_values;
    if (!value.Get(kChildrenKey, &child_values))
      return false;

    if (child_values->GetType() != Value::TYPE_LIST)
      return false;

    if (!node)
      node = new BookmarkNode(model, GURL());
    node->type_ = history::StarredEntry::USER_GROUP;
    node->date_group_modified_ =
        Time::FromInternalValue(StringToInt64(last_modified_date));

    if (parent)
      parent->Add(parent->GetChildCount(), node);

    if (!DecodeChildren(model, *static_cast<ListValue*>(child_values), node))
      return false;
  }
  
  node->SetTitle(title);
  node->date_added_ =
      Time::FromInternalValue(StringToInt64(date_added_string));
  return true;
}
