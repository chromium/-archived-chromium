// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/clipboard.h"
#include "base/string_util.h"

#include <gtk/gtk.h>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace {

static const char* kMimeHtml = "text/html";
static const char* kMimeText = "text/plain";

// GtkClipboardGetFunc callback.
// GTK will call this when an application wants data we copied to the clipboard.
void GetData(GtkClipboard* clipboard,
             GtkSelectionData* selection_data,
             guint info,
             gpointer user_data) {
  Clipboard::TargetMap* data_map =
      reinterpret_cast<Clipboard::TargetMap*>(user_data);

  Clipboard::TargetMap::iterator iter =
      data_map->find(std::string(gdk_atom_name(selection_data->target)));

  if (iter == data_map->end())
    return;

  gtk_selection_data_set(selection_data, selection_data->target, 8,
                         iter->second.first,
                         iter->second.second);
}

// GtkClipboardClearFunc callback.
// GTK will call this when new data is set on the clipboard (whether or not we
// retain ownership) or when gtk_clipboard_clear() is called. We don't do
// anything because we don't want to clear the clipboard_data_ map if we call
// set data several times in a row. Instead we manually clear the
// clipboard_data_ map on Clear() and on Clipboard destruction.
void ClearData(GtkClipboard* clipboard,
               gpointer user_data) {
}

}  // namespace

Clipboard::Clipboard() {
  clipboard_ = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
}

Clipboard::~Clipboard() {
  // TODO(estade): do we want to save clipboard data after we exit?
  // gtk_clipboard_set_can_store and gtk_clipboard_store work
  // but have strangely awful performance.
  Clear();
}

void Clipboard::Clear() {
  gtk_clipboard_clear(clipboard_);
  FreeTargetMap();
}

void Clipboard::WriteText(const std::wstring& text) {
  std::string utf8_text = WideToUTF8(text);
  size_t text_len = utf8_text.size() + 1;
  uint8* text_data = new uint8[text_len];
  memcpy(text_data, utf8_text.c_str(), text_len);

  uint8* old_data = InsertOrOverwrite(kMimeText, text_data, text_len);
  InsertOrOverwrite("TEXT", text_data, text_len);
  InsertOrOverwrite("STRING", text_data, text_len);
  InsertOrOverwrite("UTF8_STRING", text_data, text_len);
  InsertOrOverwrite("COMPOUND_TEXT", text_data, text_len);

  if (old_data)
    delete[] old_data;
              
  SetGtkClipboard();
}

void Clipboard::WriteHTML(const std::wstring& markup,
                          const std::string& src_url) {
  // TODO(estade): might not want to ignore src_url
  std::string html = WideToUTF8(markup);
  size_t data_len = html.size() + 1;
  uint8* html_data = new uint8[data_len];
  memcpy(html_data, html.c_str(), data_len);

  uint8* old_data = InsertOrOverwrite(kMimeHtml, html_data, data_len);

  if (old_data)
    delete[] old_data;

  SetGtkClipboard();
}

// We do not use gtk_clipboard_wait_is_target_available because of
// a bug with the gtk clipboard. It caches the available targets
// and does not always refresh the cache when it is appropriate.
// TODO(estade): When gnome bug 557315 is resolved, change this function
// to use gtk_clipboard_wait_is_target_available. Also, catch requests
// for plain text and change them to gtk_clipboard_wait_is_text_available
// (which checks for several standard text targets).
bool Clipboard::IsFormatAvailable(Clipboard::FormatType format) const {
  bool retval = false;
  GdkAtom* targets = NULL;
  GtkSelectionData* data =
      gtk_clipboard_wait_for_contents(clipboard_,
                                      gdk_atom_intern("TARGETS", false));

  if (!data)
    return false;

  int num = 0;
  gtk_selection_data_get_targets(data, &targets, &num);

  for (int i = 0; i < num; i++) {
    if (targets[i] == format) {
      retval = true;
      break;
    }
  }

  gtk_selection_data_free(data);
  g_free(targets);

  return retval;
}

void Clipboard::ReadText(std::wstring* result) const {
  result->clear();
  gchar* text = gtk_clipboard_wait_for_text(clipboard_);

  if (text == NULL)
   return;

  // TODO(estade): do we want to handle the possible error here?
  UTF8ToWide(text, strlen(text), result);
  g_free(text);
}

void Clipboard::ReadAsciiText(std::string* result) const {
  result->clear();
  gchar* text = gtk_clipboard_wait_for_text(clipboard_);

  if (text == NULL)
   return;

  result->assign(text);
  g_free(text);
}

// TODO(estade): handle different charsets.
void Clipboard::ReadHTML(std::wstring* markup, std::string* src_url) const {
  markup->clear();

  GtkSelectionData* data = gtk_clipboard_wait_for_contents(clipboard_,
                                                           GetHtmlFormatType());

  if (!data)
    return;

  UTF8ToWide(reinterpret_cast<char*>(data->data),
             strlen(reinterpret_cast<char*>(data->data)),
             markup);
  gtk_selection_data_free(data);
}

// static
Clipboard::FormatType Clipboard::GetPlainTextFormatType() {
  return GDK_TARGET_STRING;
}

// static
Clipboard::FormatType Clipboard::GetPlainTextWFormatType() {
  return GetPlainTextFormatType();
}

// static
Clipboard::FormatType Clipboard::GetHtmlFormatType() {
  return gdk_atom_intern(kMimeHtml, false);
}

// Take ownership of the GTK clipboard and inform it of the targets we support.
void Clipboard::SetGtkClipboard() {
  GtkTargetEntry targets[clipboard_data_.size()];

  int i = 0;
  for (Clipboard::TargetMap::iterator iter = clipboard_data_.begin();
       iter != clipboard_data_.end(); iter++, i++) {
    char* target_string = new char[iter->first.size() + 1];
    strcpy(target_string, iter->first.c_str());
    targets[i].target = target_string;
    targets[i].flags = 0;
    targets[i].info = i;
  }

  gtk_clipboard_set_with_data(clipboard_, targets,
                              clipboard_data_.size(),
                              GetData, ClearData,
                              &clipboard_data_);

  for (size_t i = 0; i < clipboard_data_.size(); i++)
    delete[] targets[i].target;
}

// Free the pointers in the clipboard_data_ map and reset the map.
void Clipboard::FreeTargetMap() {
  std::set<uint8*> ptrs;

  for (Clipboard::TargetMap::iterator iter = clipboard_data_.begin();
       iter != clipboard_data_.end(); iter++)
    ptrs.insert(iter->second.first);

  for (std::set<uint8*>::iterator iter = ptrs.begin();
       iter != ptrs.end(); iter++)
    delete[] *iter;

  clipboard_data_.clear();
}

// Insert the key/value pair in the clipboard_data structure. Overwrite
// and any old value that might have had that key. If we overwrote something,
// return the pointer to that something. Otherwise return null.
uint8* Clipboard::InsertOrOverwrite(std::string key,
                                    uint8* data, size_t data_len) {
  std::pair<std::string, std::pair<uint8*, size_t> > mapping =
      std::make_pair(key, std::make_pair(data, data_len));

  Clipboard::TargetMap::iterator iter = clipboard_data_.find(key);
  uint8* retval = NULL;

  if (iter == clipboard_data_.end()) {
    clipboard_data_.insert(mapping);
  } else {
    retval = iter->second.first;
    iter->second = mapping.second;
  }

  return retval;
}

