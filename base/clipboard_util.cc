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

#include "base/clipboard_util.h"

#include <shellapi.h>
#include <shlwapi.h>
#include <wininet.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_handle.h"
#include "base/string_util.h"

namespace {

bool GetUrlFromHDrop(IDataObject* data_object, std::wstring* url,
                     std::wstring* title) {
  DCHECK(data_object && url && title);

  STGMEDIUM medium;
  if (FAILED(data_object->GetData(ClipboardUtil::GetCFHDropFormat(), &medium)))
    return false;

  HDROP hdrop = static_cast<HDROP>(GlobalLock(medium.hGlobal));

  if (!hdrop)
    return false;

  bool success = false;
  wchar_t filename[MAX_PATH];
  if (DragQueryFileW(hdrop, 0, filename, arraysize(filename))) {
    wchar_t url_buffer[INTERNET_MAX_URL_LENGTH];
    if (0 == _wcsicmp(PathFindExtensionW(filename), L".url") &&
        GetPrivateProfileStringW(L"InternetShortcut", L"url", 0, url_buffer,
                                 arraysize(url_buffer), filename)) {
      *url = url_buffer;
      PathRemoveExtension(filename);
      title->assign(PathFindFileName(filename));
      success = true;
    }
  }

  DragFinish(hdrop);
  GlobalUnlock(medium.hGlobal);
  // We don't need to call ReleaseStgMedium here because as far as I can tell,
  // DragFinish frees the hGlobal for us.
  return success;
}

bool SplitUrlAndTitle(const std::wstring& str, std::wstring* url,
    std::wstring* title) {
  DCHECK(url && title);
  size_t newline_pos = str.find('\n');
  bool success = false;
  if (newline_pos != std::string::npos) {
    *url = str.substr(0, newline_pos);
    title->assign(str.substr(newline_pos + 1));
    success = true;
  } else {
    *url = str;
    title->assign(str);
    success = true;
  }
  return success;
}

}  // namespace


FORMATETC* ClipboardUtil::GetUrlFormat() {
  static UINT cf = RegisterClipboardFormat(CFSTR_INETURLA);
  static FORMATETC format = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  return &format;
}

FORMATETC* ClipboardUtil::GetUrlWFormat() {
  static UINT cf = RegisterClipboardFormat(CFSTR_INETURLW);
  static FORMATETC format = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  return &format;
}

FORMATETC* ClipboardUtil::GetMozUrlFormat() {
  // The format is "URL\nTitle"
  static UINT cf = RegisterClipboardFormat(L"text/x-moz-url");
  static FORMATETC format = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  return &format;
}

FORMATETC* ClipboardUtil::GetPlainTextFormat() {
  // We don't need to register this format since it's a built in format.
  static FORMATETC format = {CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  return &format;
}

FORMATETC* ClipboardUtil::GetPlainTextWFormat() {
  // We don't need to register this format since it's a built in format.
  static FORMATETC format = {CF_UNICODETEXT, 0, DVASPECT_CONTENT, -1,
                             TYMED_HGLOBAL};
  return &format;
}

FORMATETC* ClipboardUtil::GetFilenameWFormat() {
  static UINT cf = RegisterClipboardFormat(CFSTR_FILENAMEW);
  static FORMATETC format = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  return &format;
}

FORMATETC* ClipboardUtil::GetFilenameFormat()
{
  static UINT cf = RegisterClipboardFormat(CFSTR_FILENAMEA);
  static FORMATETC format = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  return &format;
}

FORMATETC* ClipboardUtil::GetHtmlFormat() {
  static UINT cf = RegisterClipboardFormat(L"HTML Format");
  static FORMATETC format = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  return &format;
}

FORMATETC* ClipboardUtil::GetTextHtmlFormat() {
  static UINT cf = RegisterClipboardFormat(L"text/html");
  static FORMATETC format = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  return &format;
}

FORMATETC* ClipboardUtil::GetCFHDropFormat() {
  // We don't need to register this format since it's a built in format.
  static FORMATETC format = {CF_HDROP, 0, DVASPECT_CONTENT, -1,
                             TYMED_HGLOBAL};
  return &format;
}

FORMATETC* ClipboardUtil::GetFileDescriptorFormat() {
  static UINT cf = RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);
  static FORMATETC format = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  return &format;
}

FORMATETC* ClipboardUtil::GetFileContentFormatZero() {
  static UINT cf = RegisterClipboardFormat(CFSTR_FILECONTENTS);
  static FORMATETC format = {cf, 0, DVASPECT_CONTENT, 0, TYMED_HGLOBAL};
  return &format;
}

FORMATETC* ClipboardUtil::GetWebKitSmartPasteFormat() {
  static UINT cf = RegisterClipboardFormat(L"WebKit Smart Paste Format");
  static FORMATETC format = {cf, 0, DVASPECT_CONTENT, 0, TYMED_HGLOBAL};
  return &format;
}


bool ClipboardUtil::HasUrl(IDataObject* data_object) {
  DCHECK(data_object);
  return SUCCEEDED(data_object->QueryGetData(GetMozUrlFormat())) ||
      SUCCEEDED(data_object->QueryGetData(GetUrlWFormat())) ||
      SUCCEEDED(data_object->QueryGetData(GetUrlFormat())) ||
      SUCCEEDED(data_object->QueryGetData(GetFilenameWFormat())) ||
      SUCCEEDED(data_object->QueryGetData(GetFilenameFormat()));
}

bool ClipboardUtil::HasFilenames(IDataObject* data_object) {
  DCHECK(data_object);
  return SUCCEEDED(data_object->QueryGetData(GetCFHDropFormat()));
}

bool ClipboardUtil::HasPlainText(IDataObject* data_object) {
  DCHECK(data_object);
  return SUCCEEDED(data_object->QueryGetData(GetPlainTextWFormat())) ||
      SUCCEEDED(data_object->QueryGetData(GetPlainTextFormat()));
}


bool ClipboardUtil::GetUrl(IDataObject* data_object,
    std::wstring* url, std::wstring* title) {
  DCHECK(data_object && url && title);
  if (!HasUrl(data_object))
    return false;

  // Try to extract a URL from |data_object| in a variety of formats.
  STGMEDIUM store;
  if (GetUrlFromHDrop(data_object, url, title)) {
    return true;
  }

  if (SUCCEEDED(data_object->GetData(GetMozUrlFormat(), &store)) ||
      SUCCEEDED(data_object->GetData(GetUrlWFormat(), &store))) {
    // Mozilla URL format or unicode URL
    ScopedHGlobal<wchar_t> data(store.hGlobal);
    bool success = SplitUrlAndTitle(data.get(), url, title);
    ReleaseStgMedium(&store);
    if (success)
      return true;
  }

  if (SUCCEEDED(data_object->GetData(GetUrlFormat(), &store))) {
    // URL using ascii
    ScopedHGlobal<char> data(store.hGlobal);
    bool success = SplitUrlAndTitle(UTF8ToWide(data.get()), url, title);
    ReleaseStgMedium(&store);
    if (success)
      return true;
  }

  if (SUCCEEDED(data_object->GetData(GetFilenameWFormat(), &store))) {
    // filename using unicode
    ScopedHGlobal<wchar_t> data(store.hGlobal);
    bool success = false;
    if (data.get() && data.get()[0] && (PathFileExists(data.get()) ||
                                        PathIsUNC(data.get()))) {
      wchar_t file_url[INTERNET_MAX_URL_LENGTH];
      DWORD file_url_len = sizeof(file_url) / sizeof(file_url[0]);
      if (SUCCEEDED(::UrlCreateFromPathW(data.get(), file_url, &file_url_len,
                                         0))) {
        *url = file_url;
        title->assign(file_url);
        success = true;
      }
    }
    ReleaseStgMedium(&store);
    if (success)
      return true;
  }

  if (SUCCEEDED(data_object->GetData(GetFilenameFormat(), &store))) {
    // filename using ascii
    ScopedHGlobal<char> data(store.hGlobal);
    bool success = false;
    if (data.get() && data.get()[0] && (PathFileExistsA(data.get()) ||
                                        PathIsUNCA(data.get()))) {
      char file_url[INTERNET_MAX_URL_LENGTH];
      DWORD file_url_len = sizeof(file_url) / sizeof(file_url[0]);
      if (SUCCEEDED(::UrlCreateFromPathA(data.get(), file_url, &file_url_len, 0))) {
        *url = UTF8ToWide(file_url);
        title->assign(*url);
        success = true;
      }
    }
    ReleaseStgMedium(&store);
    if (success)
      return true;
  }

  return false;
}

bool ClipboardUtil::GetFilenames(IDataObject* data_object,
                                 std::vector<std::wstring>* filenames) {
  DCHECK(data_object && filenames);
  if (!HasFilenames(data_object))
    return false;

  STGMEDIUM medium;
  if (FAILED(data_object->GetData(GetCFHDropFormat(), &medium)))
    return false;

  HDROP hdrop = static_cast<HDROP>(GlobalLock(medium.hGlobal));
  if (!hdrop)
    return false;

  const int kMaxFilenameLen = 4096;
  const unsigned num_files = DragQueryFileW(hdrop, 0xffffffff, 0, 0);
  for (unsigned int i = 0; i < num_files; ++i) {
    wchar_t filename[kMaxFilenameLen];
    if (!DragQueryFileW(hdrop, i, filename, kMaxFilenameLen))
      continue;
    filenames->push_back(filename);
  }

  DragFinish(hdrop);
  GlobalUnlock(medium.hGlobal);
  // We don't need to call ReleaseStgMedium here because as far as I can tell,
  // DragFinish frees the hGlobal for us.
  return true;
}

bool ClipboardUtil::GetPlainText(IDataObject* data_object,
                                 std::wstring* plain_text) {
  DCHECK(data_object && plain_text);
  if (!HasPlainText(data_object))
    return false;

  STGMEDIUM store;
  bool success = false;
  if (SUCCEEDED(data_object->GetData(GetPlainTextWFormat(), &store))) {
    // Unicode text
    ScopedHGlobal<wchar_t> data(store.hGlobal);
    plain_text->assign(data.get());
    ReleaseStgMedium(&store);
    success = true;
  } else if (SUCCEEDED(data_object->GetData(GetPlainTextFormat(), &store))) {
    // ascii text
    ScopedHGlobal<char> data(store.hGlobal);
    plain_text->assign(UTF8ToWide(data.get()));
    ReleaseStgMedium(&store);
    success = true;
  } else {
    //If a file is dropped on the window, it does not provide either of the
    //plain text formats, so here we try to forcibly get a url.
    std::wstring title;
    success = GetUrl(data_object, plain_text, &title);
  }

  return success;
}

bool ClipboardUtil::GetCFHtml(IDataObject* data_object,
                              std::wstring* cf_html) {
  DCHECK(data_object && cf_html);
  if (FAILED(data_object->QueryGetData(GetHtmlFormat())))
    return false;

  STGMEDIUM store;
  if (FAILED(data_object->GetData(GetHtmlFormat(), &store)))
    return false;

  // MS CF html
  ScopedHGlobal<char> data(store.hGlobal);
  cf_html->assign(UTF8ToWide(std::string(data.get(), data.Size())));
  ReleaseStgMedium(&store);
  return true;
}

bool ClipboardUtil::GetTextHtml(IDataObject* data_object,
                                std::wstring* text_html) {
  DCHECK(data_object && text_html);
  if (FAILED(data_object->QueryGetData(GetTextHtmlFormat())))
    return false;

  STGMEDIUM store;
  if (FAILED(data_object->GetData(GetTextHtmlFormat(), &store)))
    return false;

  // raw html
  ScopedHGlobal<wchar_t> data(store.hGlobal);
  text_html->assign(data.get());
  ReleaseStgMedium(&store);
  return true;
}

bool ClipboardUtil::GetFileContents(IDataObject* data_object,
    std::wstring* filename, std::string* file_contents) {
  DCHECK(data_object && filename && file_contents);
  bool has_data =
      SUCCEEDED(data_object->QueryGetData(GetFileContentFormatZero())) ||
      SUCCEEDED(data_object->QueryGetData(GetFileDescriptorFormat()));

  if (!has_data)
    return false;

  STGMEDIUM content;
  // The call to GetData can be very slow depending on what is in
  // |data_object|.
  if (SUCCEEDED(data_object->GetData(GetFileContentFormatZero(), &content))) {
    if (TYMED_HGLOBAL == content.tymed) {
      ScopedHGlobal<char> data(content.hGlobal);
      // The size includes the trailing NULL byte.  We don't want it.
      file_contents->assign(data.get(), data.Size() - 1);
    }
    ReleaseStgMedium(&content);
  }

  STGMEDIUM description;
  if (SUCCEEDED(data_object->GetData(GetFileDescriptorFormat(),
                                     &description))) {
    ScopedHGlobal<FILEGROUPDESCRIPTOR> fgd(description.hGlobal);
    // We expect there to be at least one file in here.
    DCHECK(fgd->cItems >= 1);
    filename->assign(fgd->fgd[0].cFileName);
    ReleaseStgMedium(&description);
  }
  return true;
}
