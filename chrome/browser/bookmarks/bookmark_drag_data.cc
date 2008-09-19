// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_drag_data.h"

#include "base/pickle.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profile.h"
#include "chrome/common/os_exchange_data.h"

static CLIPFORMAT clipboard_format = 0;

static void RegisterFormat() {
  if (clipboard_format == 0) {
    clipboard_format = RegisterClipboardFormat(L"chrome/x-bookmark-entry");
    DCHECK(clipboard_format);
  }
}

BookmarkDragData::BookmarkDragData() : is_url(false), is_valid(false) {
}

BookmarkDragData::BookmarkDragData(BookmarkNode* node)
    : is_url(node->GetType() == history::StarredEntry::URL),
      url(node->GetURL()),
      title(node->GetTitle()),
      is_valid(true),
      id_(node->id()) {
  if (!is_url)
    AddChildren(node);
}

void BookmarkDragData::Write(Profile* profile, OSExchangeData* data) const {
  RegisterFormat();

  DCHECK(data);

  if (is_url)
    data->SetURL(url, title);
  Pickle data_pickle;
  data_pickle.WriteWString(profile->GetPath());
  data_pickle.WriteInt(id_);
  WriteToPickle(&data_pickle);
  data->SetPickledData(clipboard_format, data_pickle);
}

bool BookmarkDragData::Read(const OSExchangeData& data) {
  RegisterFormat();

  is_valid = data.GetURLAndTitle(&url, &title) && url.is_valid();
  is_url = is_valid;
  profile_path_.clear();

  if (data.HasFormat(clipboard_format)) {
    Pickle drag_data_pickle;
    if (data.GetPickledData(clipboard_format, &drag_data_pickle)) {
      void* data_iterator = NULL;
      if (drag_data_pickle.ReadWString(&data_iterator, &profile_path_) &&
          drag_data_pickle.ReadInt(&data_iterator, &id_) &&
          ReadFromPickle(&drag_data_pickle, &data_iterator)) {
        is_valid = true;
      }
    }
  }
  return is_valid;
}

BookmarkNode* BookmarkDragData::GetNode(Profile* profile) const {
  DCHECK(is_valid);
  return (profile->GetPath() == profile_path_) ?
      profile->GetBookmarkModel()->GetNodeByID(id_) : NULL;
}

void BookmarkDragData::WriteToPickle(Pickle* pickle) const {
  pickle->WriteBool(is_url);
  pickle->WriteString(url.spec());
  pickle->WriteWString(title);
  if (!is_url) {
    pickle->WriteInt(static_cast<int>(children.size()));
    for (std::vector<BookmarkDragData>::const_iterator i = children.begin();
         i != children.end(); ++i) {
      i->WriteToPickle(pickle);
    }
  }
}

bool BookmarkDragData::ReadFromPickle(Pickle* pickle, void** iterator) {
  std::string url_spec;
  is_valid = false;
  if (!pickle->ReadBool(iterator, &is_url) ||
      !pickle->ReadString(iterator, &url_spec) ||
      !pickle->ReadWString(iterator, &title)) {
    return false;
  }
  url = GURL(url_spec);
  if (!is_url) {
    children.clear();
    int children_count;
    if (!pickle->ReadInt(iterator, &children_count))
      return false;
    children.resize(children_count);
    for (std::vector<BookmarkDragData>::iterator i = children.begin();
         i != children.end(); ++i) {
      if (!i->ReadFromPickle(pickle, iterator))
        return false;
    }
  }
  is_valid = true;
  return true;
}

void BookmarkDragData::AddChildren(BookmarkNode* node) {
  for (int i = 0, max = node->GetChildCount(); i < max; ++i)
    children.push_back(BookmarkDragData(node->GetChild(i)));
}
