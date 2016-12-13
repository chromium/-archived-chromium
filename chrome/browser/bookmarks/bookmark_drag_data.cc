// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_drag_data.h"

// TODO(port): Port this file.
#if defined(TOOLKIT_VIEWS)
#include "app/os_exchange_data.h"
#else
#include "chrome/common/temp_scaffolding_stubs.h"
#endif
#include "base/basictypes.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profile.h"
#include "chrome/common/url_constants.h"

#if defined(OS_WIN)
static CLIPFORMAT clipboard_format = 0;

static void RegisterFormat() {
  if (clipboard_format == 0) {
    clipboard_format = RegisterClipboardFormat(L"chrome/x-bookmark-entries");
    DCHECK(clipboard_format);
  }
}
#endif

BookmarkDragData::Element::Element(const BookmarkNode* node)
    : is_url(node->is_url()),
      url(node->GetURL()),
      title(node->GetTitle()),
      id_(node->id()) {
  for (int i = 0; i < node->GetChildCount(); ++i)
    children.push_back(Element(node->GetChild(i)));
}

void BookmarkDragData::Element::WriteToPickle(Pickle* pickle) const {
  pickle->WriteBool(is_url);
  pickle->WriteString(url.spec());
  pickle->WriteWString(title);
  pickle->WriteInt(id_);
  if (!is_url) {
    pickle->WriteSize(children.size());
    for (std::vector<Element>::const_iterator i = children.begin();
         i != children.end(); ++i) {
      i->WriteToPickle(pickle);
    }
  }
}

bool BookmarkDragData::Element::ReadFromPickle(Pickle* pickle,
                                               void** iterator) {
  std::string url_spec;
  if (!pickle->ReadBool(iterator, &is_url) ||
      !pickle->ReadString(iterator, &url_spec) ||
      !pickle->ReadWString(iterator, &title) ||
      !pickle->ReadInt(iterator, &id_)) {
    return false;
  }
  url = GURL(url_spec);
  children.clear();
  if (!is_url) {
    size_t children_count;
    if (!pickle->ReadSize(iterator, &children_count))
      return false;
    children.resize(children_count);
    for (std::vector<Element>::iterator i = children.begin();
         i != children.end(); ++i) {
      if (!i->ReadFromPickle(pickle, iterator))
        return false;
    }
  }
  return true;
}

BookmarkDragData::BookmarkDragData(const BookmarkNode* node) {
  elements.push_back(Element(node));
}

BookmarkDragData::BookmarkDragData(
    const std::vector<const BookmarkNode*>& nodes) {
  for (size_t i = 0; i < nodes.size(); ++i)
    elements.push_back(Element(nodes[i]));
}

#if defined(OS_WIN)
void BookmarkDragData::Write(Profile* profile, OSExchangeData* data) const {
  RegisterFormat();

  DCHECK(data);

  // If there is only one element and it is a URL, write the URL to the
  // clipboard.
  if (elements.size() == 1 && elements[0].is_url) {
    if (elements[0].url.SchemeIs(chrome::kJavaScriptScheme)) {
      data->SetString(ASCIIToWide(elements[0].url.spec()));
    } else {
      data->SetURL(elements[0].url, elements[0].title);
    }
  }

  Pickle data_pickle;
  WriteToPickle(profile, &data_pickle);

  data->SetPickledData(clipboard_format, data_pickle);
}

bool BookmarkDragData::Read(const OSExchangeData& data) {
  RegisterFormat();

  elements.clear();

  profile_path_.clear();

  if (data.HasFormat(clipboard_format)) {
    Pickle drag_data_pickle;
    if (data.GetPickledData(clipboard_format, &drag_data_pickle)) {
      if (!ReadFromPickle(&drag_data_pickle))
        return false;
    }
  } else {
    // See if there is a URL on the clipboard.
    Element element;
    if (data.GetURLAndTitle(&element.url, &element.title) &&
        element.url.is_valid()) {
      element.is_url = true;
      elements.push_back(element);
    }
  }

  return is_valid();
}
#elif defined(TOOLKIT_VIEWS)
void BookmarkDragData::Write(Profile* profile, OSExchangeData* data) const {
  NOTIMPLEMENTED();
}

bool BookmarkDragData::Read(const OSExchangeData& data) {
  NOTIMPLEMENTED();
  return false;
}
#endif

void BookmarkDragData::WriteToPickle(Profile* profile, Pickle* pickle) const {
#if defined(WCHAR_T_IS_UTF16)
  pickle->WriteWString(
      profile ? profile->GetPath().ToWStringHack() : std::wstring());
#elif defined(WCHAR_T_IS_UTF32)
  pickle->WriteString(
      profile ? profile->GetPath().value() : std::string());
#else
  NOTIMPLEMENTED() << "Impossible encoding situation!";
#endif

  pickle->WriteSize(elements.size());

  for (size_t i = 0; i < elements.size(); ++i)
    elements[i].WriteToPickle(pickle);
}

bool BookmarkDragData::ReadFromPickle(Pickle* pickle) {
  void* data_iterator = NULL;
  size_t element_count;
#if defined(WCHAR_T_IS_UTF16)
  if (pickle->ReadWString(&data_iterator, &profile_path_) &&
#elif defined(WCHAR_T_IS_UTF32)
  if (pickle->ReadString(&data_iterator, &profile_path_) &&
#else
  NOTIMPLEMENTED() << "Impossible encoding situation!";
  if (false &&
#endif
      pickle->ReadSize(&data_iterator, &element_count)) {
    std::vector<Element> tmp_elements;
    tmp_elements.resize(element_count);
    for (size_t i = 0; i < element_count; ++i) {
      if (!tmp_elements[i].ReadFromPickle(pickle, &data_iterator)) {
        return false;
      }
    }
    elements.swap(tmp_elements);
  }

  return true;
}

std::vector<const BookmarkNode*> BookmarkDragData::GetNodes(
    Profile* profile) const {
  std::vector<const BookmarkNode*> nodes;

  if (!IsFromProfile(profile))
    return nodes;

  for (size_t i = 0; i < elements.size(); ++i) {
    const BookmarkNode* node =
        profile->GetBookmarkModel()->GetNodeByID(elements[i].id_);
    if (!node) {
      nodes.clear();
      return nodes;
    }
    nodes.push_back(node);
  }
  return nodes;
}

const BookmarkNode* BookmarkDragData::GetFirstNode(Profile* profile) const {
  std::vector<const BookmarkNode*> nodes = GetNodes(profile);
  return nodes.size() == 1 ? nodes[0] : NULL;
}

bool BookmarkDragData::IsFromProfile(Profile* profile) const {
  // An empty path means the data is not associated with any profile.
  return (!profile_path_.empty() &&
#if defined(WCHAR_T_IS_UTF16)
      profile->GetPath().ToWStringHack() == profile_path_);
#elif defined(WCHAR_T_IS_UTF32)
      profile->GetPath().value() == profile_path_);
#endif
}
