// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_clipboard_writer.h"

#include "base/clipboard.h"
#include "base/string_util.h"

ScopedClipboardWriter::ScopedClipboardWriter(Clipboard* clipboard)
    : clipboard_(clipboard) {
}

ScopedClipboardWriter::~ScopedClipboardWriter() {
  if (!objects_.empty() && clipboard_)
    clipboard_->WriteObjects(objects_);
}

void ScopedClipboardWriter::WriteText(const std::wstring& text) {
  if (text.empty())
    return;

  std::string utf8_text = WideToUTF8(text);

  Clipboard::ObjectMapParams params;
  params.push_back(Clipboard::ObjectMapParam(utf8_text.begin(),
                                             utf8_text.end()));
  objects_[Clipboard::CBF_TEXT] = params;
}

void ScopedClipboardWriter::WriteHTML(const std::wstring& markup,
                                      const std::string& src_url) {
  if (markup.empty())
    return;

  std::string utf8_markup = WideToUTF8(markup);

  Clipboard::ObjectMapParams params;
  params.push_back(
      Clipboard::ObjectMapParam(utf8_markup.begin(),
                                utf8_markup.end()));
  if (!src_url.empty()) {
    params.push_back(Clipboard::ObjectMapParam(src_url.begin(),
                                               src_url.end()));
  }

  objects_[Clipboard::CBF_HTML] = params;
}

void ScopedClipboardWriter::WriteBookmark(const std::wstring& title,
                                          const std::string& url) {
  if (title.empty() || url.empty())
    return;

  std::string utf8_markup = WideToUTF8(title);

  Clipboard::ObjectMapParams params;
  params.push_back(Clipboard::ObjectMapParam(utf8_markup.begin(),
                                             utf8_markup.end()));
  params.push_back(Clipboard::ObjectMapParam(url.begin(), url.end()));
  objects_[Clipboard::CBF_BOOKMARK] = params;
}

void ScopedClipboardWriter::WriteHyperlink(const std::wstring& title,
                                           const std::string& url) {
  if (title.empty() || url.empty())
    return;

  std::string utf8_markup = WideToUTF8(title);

  Clipboard::ObjectMapParams params;
  params.push_back(Clipboard::ObjectMapParam(utf8_markup.begin(),
                                             utf8_markup.end()));
  params.push_back(Clipboard::ObjectMapParam(url.begin(), url.end()));
  objects_[Clipboard::CBF_LINK] = params;
}

void ScopedClipboardWriter::WriteFile(const std::wstring& file) {
  WriteFiles(std::vector<std::wstring>(1, file));
}

// Save the filenames as a string separated by nulls and terminated with an
// extra null.
void ScopedClipboardWriter::WriteFiles(const std::vector<std::wstring>& files) {
  if (files.empty())
    return;

  Clipboard::ObjectMapParam param;

  for (std::vector<std::wstring>::const_iterator iter = files.begin();
       iter != files.end(); ++iter) {
    std::string filename = WideToUTF8(*iter);
    for (std::string::const_iterator filename_iter = filename.begin();
         filename_iter != filename.end(); ++filename_iter) {
      param.push_back(*filename_iter);
    }
    param.push_back('\0');
  }

  param.push_back('\0');

  Clipboard::ObjectMapParams params;
  params.push_back(param);
  objects_[Clipboard::CBF_FILES] = params;
}

void ScopedClipboardWriter::WriteWebSmartPaste() {
  objects_[Clipboard::CBF_WEBKIT] = Clipboard::ObjectMapParams();
}

#if defined(OS_WIN)
void ScopedClipboardWriter::WriteBitmapFromPixels(const void* pixels,
                                                  const gfx::Size& size) {
  Clipboard::ObjectMapParam param1, param2;
  const char* pixels_data = reinterpret_cast<const char*>(pixels);
  size_t pixels_len = 4 * size.width() * size.height();
  for (size_t i = 0; i < pixels_len; i++)
    param1.push_back(pixels_data[i]);

  const char* size_data = reinterpret_cast<const char*>(&size);
  size_t size_len = sizeof(gfx::Size);
  for (size_t i = 0; i < size_len; i++)
    param2.push_back(size_data[i]);

  Clipboard::ObjectMapParams params;
  params.push_back(param1);
  params.push_back(param2);
  objects_[Clipboard::CBF_BITMAP] = params;
}
#endif  // defined(OS_WIN)
