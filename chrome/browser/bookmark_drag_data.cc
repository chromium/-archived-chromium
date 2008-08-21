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

#include "base/pickle.h"
#include "chrome/browser/bookmark_bar_model.h"
#include "chrome/browser/bookmark_drag_data.h"
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

BookmarkDragData::BookmarkDragData(BookmarkBarNode* node)
    : is_url(node->GetType() == history::StarredEntry::URL),
      url(node->GetURL()),
      title(node->GetTitle()),
      is_valid(true),
      id_(node->id()) {
  if (!is_url)
    AddChildren(node);
}

void BookmarkDragData::Write(OSExchangeData* data) const {
  RegisterFormat();

  DCHECK(data);

  if (is_url) {
    data->SetURL(url, title);
  }
  Pickle data_pickle;
  WriteToPickle(&data_pickle);
  data->SetPickledData(clipboard_format, data_pickle);
}

bool BookmarkDragData::Read(const OSExchangeData& data) {
  RegisterFormat();

  is_valid = data.GetURLAndTitle(&url, &title) && url.is_valid();
  is_url = is_valid;
  profile_id.clear();

  if (data.HasFormat(clipboard_format)) {
    Pickle drag_data_pickle;
    if (data.GetPickledData(clipboard_format, &drag_data_pickle)) {
      void* data_iterator = NULL;
      if (ReadFromPickle(&drag_data_pickle, &data_iterator)) {
        is_valid = true;
      }
    }
  }
  return is_valid;
}

BookmarkBarNode* BookmarkDragData::GetNode(BookmarkBarModel* model) const {
  DCHECK(!is_url && id_ && is_valid);
  return model->GetNodeByID(id_);
}

void BookmarkDragData::WriteToPickle(Pickle* pickle) const {
  pickle->WriteBool(is_url);
  pickle->WriteWString(profile_id);
  pickle->WriteString(url.spec());
  pickle->WriteWString(title);
  if (!is_url) {
    pickle->WriteInt(id_);
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
      !pickle->ReadWString(iterator, &profile_id) ||
      !pickle->ReadString(iterator, &url_spec) ||
      !pickle->ReadWString(iterator, &title)) {
    return false;
  }
  url = GURL(url_spec);
  if (!is_url) {
    id_ = 0;
    children.clear();
    if (!pickle->ReadInt(iterator, &id_))
      return false;
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

void BookmarkDragData::AddChildren(BookmarkBarNode* node) {
  for (int i = 0, max = node->GetChildCount(); i < max; ++i)
    children.push_back(BookmarkDragData(node->GetChild(i)));
}
