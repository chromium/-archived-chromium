// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/clipboard.h"
#include "base/string_util.h"

#include <gtk/gtk.h>
#include <string.h>

namespace {

static const char* kMimeHtml = "text/html";

void GetHtml(GtkClipboard* clipboard,
             GtkSelectionData* selection_data,
             guint info,
             gpointer user_data) {
  if (selection_data->target != gdk_atom_intern(kMimeHtml, false))
    return;

  gtk_selection_data_set(selection_data, gdk_atom_intern(kMimeHtml, false),
                         8,
                         static_cast<guchar*>(user_data),
                         strlen(static_cast<char*>(user_data)));
}

void ClearHtml(GtkClipboard* clipboard,
               gpointer user_data) {
  delete[] static_cast<char*>(user_data);
}

void GetNoOp(GtkClipboard* clipboard,
             GtkSelectionData* selection_data,
             guint info,
             gpointer user_data) {
}

void ClearNoOp(GtkClipboard* clipboard,
               gpointer user_data) {
}

}

Clipboard::Clipboard() {
  clipboard_ = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
}

Clipboard::~Clipboard() {
  // TODO(estade): enable this (must first use gtk_clipboard_set_can_store()?)
  // gtk_clipboard_store(clipboard_);
}

void Clipboard::Clear() {
  // gtk_clipboard_clear() does not appear to be enough. We must first grab
  // ownership of the clipboard by setting data.
  GtkTargetEntry empty_target[] = {};
  gtk_clipboard_set_with_data(clipboard_, empty_target, 0,
                              GetNoOp, ClearNoOp, NULL);
  gtk_clipboard_clear(clipboard_);
}

void Clipboard::WriteText(const std::wstring& text) {
  std::string utf8_text = WideToUTF8(text);

  gtk_clipboard_set_text(clipboard_, utf8_text.c_str(), utf8_text.size());
}

void Clipboard::WriteHTML(const std::wstring& markup,
                          const std::string& src_url) {
  // TODO(estade): might not want to ignore src_url
  std::string html = WideToUTF8(markup);
  char* html_data = new char[html.size() + 1];
  strcpy(html_data, html.c_str());

  char target_format[strlen(kMimeHtml) + 1];
  strcpy(target_format, kMimeHtml);
  GtkTargetEntry target[] = { {target_format, 0} };

  if (!gtk_clipboard_set_with_data(clipboard_, target, 1,
                                   GetHtml, ClearHtml, html_data)) {
    delete[] html_data;
  }
}

bool Clipboard::IsFormatAvailable(Clipboard::FormatType format) const {
  if (format == GetPlainTextFormatType()) {
    return gtk_clipboard_wait_is_text_available(clipboard_);
  }

  return gtk_clipboard_wait_is_target_available(clipboard_, format);
}

void Clipboard::ReadText(std::wstring* result) const {
  if (!result) {
    NOTREACHED();
    return;
  }

  result->clear();
  gchar* text = gtk_clipboard_wait_for_text(clipboard_);

  if (text == NULL)
   return;

  // TODO(estade): do we want to handle the possible error here?
  UTF8ToWide(text, strlen(text), result);
  g_free(text);
}

void Clipboard::ReadAsciiText(std::string* result) const {
  if (!result) {
    NOTREACHED();
    return;
  }

  result->clear();
  gchar* text = gtk_clipboard_wait_for_text(clipboard_);

  if (text == NULL)
   return;

  result->assign(text);
  g_free(text);
}

void Clipboard::ReadHTML(std::wstring* markup, std::string* src_url) const {
  if (!markup) {
    NOTREACHED();
    return;
  }

  markup->clear();

  GtkSelectionData* data = gtk_clipboard_wait_for_contents(clipboard_,
                                                           GetHtmlFormatType());

  if (!data)
    return;

  std::string html(reinterpret_cast<char*>(data->data));
  markup->assign(UTF8ToWide(html));
}

// static
Clipboard::FormatType Clipboard::GetPlainTextFormatType() {
  return GDK_TARGET_STRING;
}

// static
Clipboard::FormatType Clipboard::GetPlainTextWFormatType() {
  // all gtk clipboard strings are UTF8
  return GetPlainTextFormatType();
}

// static
Clipboard::FormatType Clipboard::GetHtmlFormatType() {
  return gdk_atom_intern(kMimeHtml, false);
}

