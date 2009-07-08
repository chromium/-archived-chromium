// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_codec.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"

using base::Time;

UniqueIDGenerator::UniqueIDGenerator() {
  Reset();
}

int UniqueIDGenerator::GetUniqueID(int id) {
  // If the given ID is already assigned, generate new ID.
  if (IsIdAssigned(id))
    id = current_max_ + 1;

  // Record the new ID as assigned.
  RecordId(id);

  if (id > current_max_)
    current_max_ = id;

  return id;
}

bool UniqueIDGenerator::IsIdAssigned(int id) const {
  // If the set is already instantiated, then use the set to determine if the
  // given ID is assigned. Otherwise use the current maximum to determine if the
  // given ID is assigned.
  if (assigned_ids_.get())
    return assigned_ids_->find(id) != assigned_ids_->end();
  else
    return id <= current_max_;
}

void UniqueIDGenerator::RecordId(int id) {
  // If the set is instantiated, then use the set.
  if (assigned_ids_.get()) {
    assigned_ids_->insert(id);
    return;
  }

  // The set is not yet instantiated. If the ID is current_max_ + 1, then just
  // update the current_max_. Otherwise, instantiate the set and add all IDs
  // from 0 to current_max_ to it.
  if (id == current_max_ + 1) {
    ++current_max_;
    return;
  }
  assigned_ids_.reset(new std::set<int>);
  for (int i = 0; i <= current_max_; ++i)
    assigned_ids_->insert(i);
  assigned_ids_->insert(id);
}

void UniqueIDGenerator::Reset() {
  current_max_ = 0;
  assigned_ids_.reset(NULL);
}

const wchar_t* BookmarkCodec::kRootsKey = L"roots";
const wchar_t* BookmarkCodec::kRootFolderNameKey = L"bookmark_bar";
const wchar_t* BookmarkCodec::kOtherBookmarkFolderNameKey = L"other";
const wchar_t* BookmarkCodec::kVersionKey = L"version";
const wchar_t* BookmarkCodec::kChecksumKey = L"checksum";
const wchar_t* BookmarkCodec::kIdKey = L"id";
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

BookmarkCodec::BookmarkCodec()
    : persist_ids_(false) {
}

BookmarkCodec::BookmarkCodec(bool persist_ids)
    : persist_ids_(persist_ids) {
}

Value* BookmarkCodec::Encode(BookmarkModel* model) {
  return Encode(model->GetBookmarkBarNode(), model->other_node());
}

Value* BookmarkCodec::Encode(const BookmarkNode* bookmark_bar_node,
                             const BookmarkNode* other_folder_node) {
  InitializeChecksum();
  DictionaryValue* roots = new DictionaryValue();
  roots->Set(kRootFolderNameKey, EncodeNode(bookmark_bar_node));
  roots->Set(kOtherBookmarkFolderNameKey, EncodeNode(other_folder_node));

  DictionaryValue* main = new DictionaryValue();
  main->SetInteger(kVersionKey, kCurrentVersion);
  FinalizeChecksum();
  // We are going to store the computed checksum. So set stored checksum to be
  // the same as computed checksum.
  stored_checksum_ = computed_checksum_;
  main->Set(kChecksumKey, Value::CreateStringValue(computed_checksum_));
  main->Set(kRootsKey, roots);
  return main;
}

bool BookmarkCodec::Decode(BookmarkNode* bb_node,
                           BookmarkNode* other_folder_node,
                           int* max_id,
                           const Value& value) {
  id_generator_.Reset();
  stored_checksum_.clear();
  InitializeChecksum();
  bool success = DecodeHelper(bb_node, other_folder_node, max_id, value);
  FinalizeChecksum();
  *max_id = id_generator_.current_max() + 1;
  return success;
}

Value* BookmarkCodec::EncodeNode(const BookmarkNode* node) {
  DictionaryValue* value = new DictionaryValue();
  std::string id;
  if (persist_ids_) {
    id = IntToString(node->id());
    value->SetString(kIdKey, id);
  }
  const std::wstring& title = node->GetTitle();
  value->SetString(kNameKey, title);
  value->SetString(kDateAddedKey,
                   Int64ToWString(node->date_added().ToInternalValue()));
  if (node->GetType() == BookmarkNode::URL) {
    value->SetString(kTypeKey, kTypeURL);
    std::wstring url = UTF8ToWide(node->GetURL().possibly_invalid_spec());
    value->SetString(kURLKey, url);
    UpdateChecksumWithUrlNode(id, title, url);
  } else {
    value->SetString(kTypeKey, kTypeFolder);
    value->SetString(kDateModifiedKey,
                     Int64ToWString(node->date_group_modified().
                                    ToInternalValue()));
    UpdateChecksumWithFolderNode(id, title);

    ListValue* child_values = new ListValue();
    value->Set(kChildrenKey, child_values);
    for (int i = 0; i < node->GetChildCount(); ++i)
      child_values->Append(EncodeNode(node->GetChild(i)));
  }
  return value;
}

bool BookmarkCodec::DecodeHelper(BookmarkNode* bb_node,
                                 BookmarkNode* other_folder_node,
                                 int* max_id,
                                 const Value& value) {
  if (value.GetType() != Value::TYPE_DICTIONARY)
    return false;  // Unexpected type.

  const DictionaryValue& d_value = static_cast<const DictionaryValue&>(value);

  int version;
  if (!d_value.GetInteger(kVersionKey, &version) || version != kCurrentVersion)
    return false;  // Unknown version.

  Value* checksum_value;
  if (d_value.Get(kChecksumKey, &checksum_value)) {
    if (checksum_value->GetType() != Value::TYPE_STRING)
      return false;
    StringValue* checksum_value_str = static_cast<StringValue*>(checksum_value);
    if (!checksum_value_str->GetAsString(&stored_checksum_))
      return false;
  }

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
      !roots_d_value->Get(kOtherBookmarkFolderNameKey, &other_folder_value) ||
      other_folder_value->GetType() != Value::TYPE_DICTIONARY)
    return false;  // Invalid type for root folder and/or other folder.

  DecodeNode(*static_cast<DictionaryValue*>(root_folder_value), NULL,
             bb_node);
  DecodeNode(*static_cast<DictionaryValue*>(other_folder_value), NULL,
             other_folder_node);
  // Need to reset the type as decoding resets the type to FOLDER. Similarly
  // we need to reset the title as the title is persisted and restored from
  // the file.
  bb_node->SetType(BookmarkNode::BOOKMARK_BAR);
  other_folder_node->SetType(BookmarkNode::OTHER_NODE);
  bb_node->SetTitle(l10n_util::GetString(IDS_BOOMARK_BAR_FOLDER_NAME));
  other_folder_node->SetTitle(
      l10n_util::GetString(IDS_BOOMARK_BAR_OTHER_FOLDER_NAME));

  return true;
}

bool BookmarkCodec::DecodeChildren(const ListValue& child_value_list,
                                   BookmarkNode* parent) {
  for (size_t i = 0; i < child_value_list.GetSize(); ++i) {
    Value* child_value;
    if (!child_value_list.Get(i, &child_value))
      return false;

    if (child_value->GetType() != Value::TYPE_DICTIONARY)
      return false;

    DecodeNode(*static_cast<DictionaryValue*>(child_value), parent, NULL);
  }
  return true;
}

bool BookmarkCodec::DecodeNode(const DictionaryValue& value,
                               BookmarkNode* parent,
                               BookmarkNode* node) {
  std::string id_string;
  int id = 0;
  if (persist_ids_) {
    if (value.GetString(kIdKey, &id_string))
      if (!StringToInt(id_string, &id))
        return false;
  }
  id = id_generator_.GetUniqueID(id);

  std::wstring title;
  if (!value.GetString(kNameKey, &title))
    title = std::wstring();

  std::wstring date_added_string;
  if (!value.GetString(kDateAddedKey, &date_added_string))
    date_added_string = Int64ToWString(Time::Now().ToInternalValue());

  std::wstring type_string;
  if (!value.GetString(kTypeKey, &type_string))
    return false;

  if (type_string != kTypeURL && type_string != kTypeFolder)
    return false;  // Unknown type.

  if (type_string == kTypeURL) {
    std::wstring url_string;
    if (!value.GetString(kURLKey, &url_string))
      return false;

    if (!node)
      node = new BookmarkNode(id, GURL(WideToUTF8(url_string)));
    else
      return false; // Node invalid.

    if (parent)
      parent->Add(parent->GetChildCount(), node);
    node->SetType(BookmarkNode::URL);
    UpdateChecksumWithUrlNode(id_string, title, url_string);
  } else {
    std::wstring last_modified_date;
    if (!value.GetString(kDateModifiedKey, &last_modified_date))
      last_modified_date = Int64ToWString(Time::Now().ToInternalValue());

    Value* child_values;
    if (!value.Get(kChildrenKey, &child_values))
      return false;

    if (child_values->GetType() != Value::TYPE_LIST)
      return false;

    if (!node) {
      node = new BookmarkNode(id, GURL());
    } else {
      // If a new node is not created, explicitly assign ID to the existing one.
      DCHECK(id != 0);
      node->set_id(id);
    }

    node->SetType(BookmarkNode::FOLDER);
    node->set_date_group_modified(Time::FromInternalValue(
        StringToInt64(WideToUTF16Hack(last_modified_date))));

    if (parent)
      parent->Add(parent->GetChildCount(), node);

    UpdateChecksumWithFolderNode(id_string, title);

    if (!DecodeChildren(*static_cast<ListValue*>(child_values), node))
      return false;
  }

  node->SetTitle(title);
  node->set_date_added(Time::FromInternalValue(
      StringToInt64(WideToUTF16Hack(date_added_string))));
  return true;
}

void BookmarkCodec::UpdateChecksum(const std::string& str) {
  MD5Update(&md5_context_, str.data(), str.length() * sizeof(char));
}

void BookmarkCodec::UpdateChecksum(const std::wstring& str) {
  MD5Update(&md5_context_, str.data(), str.length() * sizeof(wchar_t));
}

void BookmarkCodec::UpdateChecksumWithUrlNode(const std::string& id,
                                              const std::wstring& title,
                                              const std::wstring& url) {
  UpdateChecksum(id);
  UpdateChecksum(title);
  UpdateChecksum(kTypeURL);
  UpdateChecksum(url);
}

void BookmarkCodec::UpdateChecksumWithFolderNode(const std::string& id,
                                                 const std::wstring& title) {
  UpdateChecksum(id);
  UpdateChecksum(title);
  UpdateChecksum(kTypeFolder);
}

void BookmarkCodec::InitializeChecksum() {
  MD5Init(&md5_context_);
}

void BookmarkCodec::FinalizeChecksum() {
  MD5Digest digest;
  MD5Final(&digest, &md5_context_);
  computed_checksum_ = MD5DigestToBase16(digest);
}
