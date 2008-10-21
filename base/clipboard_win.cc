// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Many of these functions are based on those found in
// webkit/port/platform/PasteboardWin.cpp

#include <shlobj.h>
#include <shellapi.h>

#include "base/clipboard.h"

#include "base/clipboard_util.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace {

// A small object to ensure we close the clipboard after opening it.
class ClipboardLock {
 public:
  ClipboardLock() : we_own_the_lock_(false) { }

  ~ClipboardLock() {
    if (we_own_the_lock_)
      Release();
  }

  bool Acquire(HWND owner) {
    // We shouldn't be calling this if we already own the clipboard lock.
    DCHECK(!we_own_the_lock_);

    // We already have the lock.  We don't want to stomp on the other use.
    if (we_own_the_lock_)
      return false;

    const int kMaxAttemptsToOpenClipboard = 5;

    // Attempt to acquire the clipboard lock.  This may fail if another process
    // currently holds the lock.  We're willing to try a few times in the hopes
    // of acquiring it.
    //
    // This turns out to be an issue when using remote desktop because the
    // rdpclip.exe process likes to read what we've written to the clipboard and
    // send it to the RDP client.  If we open and close the clipboard in quick
    // succession, we might be trying to open it while rdpclip.exe has it open,
    // See Bug 815425.
    //
    // In fact, we believe we'll only spin this loop over remote desktop.  In
    // normal situations, the user is initiating clipboard operations and there
    // shouldn't be lock contention.

    for (int attempts = 0; attempts < kMaxAttemptsToOpenClipboard; ++attempts) {
      if (::OpenClipboard(owner)) {
        we_own_the_lock_ = true;
        return we_own_the_lock_;
      }

      // Having failed, we yield our timeslice to other processes. ::Yield seems
      // to be insufficient here, so we sleep for 5 ms.
      if (attempts < (kMaxAttemptsToOpenClipboard - 1))
        ::Sleep(5);
    }

    // We failed to acquire the clipboard.
    return false;
  }

  void Release() {
    // We should only be calling this if we already own the clipboard lock.
    DCHECK(we_own_the_lock_);

    // We we don't have the lock, there is nothing to release.
    if (!we_own_the_lock_)
      return;

    ::CloseClipboard();
    we_own_the_lock_ = false;
  }

 private:
  bool we_own_the_lock_;
};

LRESULT CALLBACK ClipboardOwnerWndProc(HWND hwnd,
                                       UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam) {
  LRESULT lresult = 0;

  switch(message) {
  case WM_RENDERFORMAT:
    // This message comes when SetClipboardData was sent a null data handle
    // and now it's come time to put the data on the clipboard.
    // We always set data, so there isn't a need to actually do anything here.
    break;
  case WM_RENDERALLFORMATS:
    // This message comes when SetClipboardData was sent a null data handle
    // and now this application is about to quit, so it must put data on
    // the clipboard before it exits.
    // We always set data, so there isn't a need to actually do anything here.
    break;
  case WM_DRAWCLIPBOARD:
    break;
  case WM_DESTROY:
    break;
  case WM_CHANGECBCHAIN:
    break;
  default:
    lresult = DefWindowProc(hwnd, message, wparam, lparam);
    break;
  }
  return lresult;
}

template <typename charT>
HGLOBAL CreateGlobalData(const std::basic_string<charT>& str) {
  HGLOBAL data =
    ::GlobalAlloc(GMEM_MOVEABLE, ((str.size() + 1) * sizeof(charT)));
  if (data) {
    charT* raw_data = static_cast<charT*>(::GlobalLock(data));
    memcpy(raw_data, str.data(), str.size() * sizeof(charT));
    raw_data[str.size()] = '\0';
    ::GlobalUnlock(data);
  }
  return data;
};

} // namespace

Clipboard::Clipboard() {
  // make a dummy HWND to be the clipboard's owner
  WNDCLASSEX wcex = {0};
  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.lpfnWndProc = ClipboardOwnerWndProc;
  wcex.hInstance = GetModuleHandle(NULL);
  wcex.lpszClassName = L"ClipboardOwnerWindowClass";
  ::RegisterClassEx(&wcex);

  clipboard_owner_ = ::CreateWindow(L"ClipboardOwnerWindowClass",
                                    L"ClipboardOwnerWindow",
                                    0, 0, 0, 0, 0,
                                    HWND_MESSAGE,
                                    0, 0, 0);
}

Clipboard::~Clipboard() {
  ::DestroyWindow(clipboard_owner_);
  clipboard_owner_ = NULL;
}

void Clipboard::Clear() {
  // Acquire the clipboard.
  ClipboardLock lock;
  if (!lock.Acquire(clipboard_owner_))
    return;

  ::EmptyClipboard();
}

void Clipboard::WriteText(const std::wstring& text) {
  ClipboardLock lock;
  if (!lock.Acquire(clipboard_owner_))
    return;

  HGLOBAL glob = CreateGlobalData(text);
  if (glob && !::SetClipboardData(CF_UNICODETEXT, glob))
    ::GlobalFree(glob);
}

void Clipboard::WriteHTML(const std::wstring& markup,
                          const std::string& url) {
  // Acquire the clipboard.
  ClipboardLock lock;
  if (!lock.Acquire(clipboard_owner_))
    return;

  std::string html_fragment;
  MarkupToHTMLClipboardFormat(markup, url, &html_fragment);
  HGLOBAL glob = CreateGlobalData(html_fragment);
  if (glob && !::SetClipboardData(GetHtmlFormatType(), glob)) {
    ::GlobalFree(glob);
  }
}

void Clipboard::WriteBookmark(const std::wstring& title,
                              const std::string& url) {
  // Acquire the clipboard.
  ClipboardLock lock;
  if (!lock.Acquire(clipboard_owner_))
    return;

  std::wstring bookmark(title);
  bookmark.append(1, L'\n');
  bookmark.append(UTF8ToWide(url));
  HGLOBAL glob = CreateGlobalData(bookmark);
  if (glob && !::SetClipboardData(GetUrlWFormatType(), glob)) {
    ::GlobalFree(glob);
  }
}

void Clipboard::WriteHyperlink(const std::wstring& title,
                               const std::string& url) {
  // Write as a bookmark.
  WriteBookmark(title, url);

  // Build the HTML link.
  std::wstring link(L"<a href=\"");
  link.append(UTF8ToWide(url));
  link.append(L"\">");
  link.append(title);
  link.append(L"</a>");

  // Write as an HTML link.
  WriteHTML(link, std::string());
}

void Clipboard::WriteWebSmartPaste() {
  // Acquire the clipboard.
  ClipboardLock lock;
  if (!lock.Acquire(clipboard_owner_))
    return;

  SetClipboardData(GetWebKitSmartPasteFormatType(), NULL);
}

void Clipboard::WriteBitmap(const void* pixels, const gfx::Size& size) {
  HDC dc = ::GetDC(NULL);

  // This doesn't actually cost us a memcpy when the bitmap comes from the
  // renderer as we load it into the bitmap using setPixels which just sets a
  // pointer.  Someone has to memcpy it into GDI, it might as well be us here.

  // TODO(darin): share data in gfx/bitmap_header.cc somehow
  BITMAPINFO bm_info = {0};
  bm_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bm_info.bmiHeader.biWidth = size.width();
  bm_info.bmiHeader.biHeight = -size.height();  // sets vertical orientation
  bm_info.bmiHeader.biPlanes = 1;
  bm_info.bmiHeader.biBitCount = 32;
  bm_info.bmiHeader.biCompression = BI_RGB;

  // ::CreateDIBSection allocates memory for us to copy our bitmap into.
  // Unfortunately, we can't write the created bitmap to the clipboard,
  // (see http://msdn2.microsoft.com/en-us/library/ms532292.aspx)
  void *bits;
  HBITMAP source_hbitmap =
      ::CreateDIBSection(dc, &bm_info, DIB_RGB_COLORS, &bits, NULL, 0);

  if (bits && source_hbitmap) {
    // Copy the bitmap out of shared memory and into GDI
    memcpy(bits, pixels, 4 * size.width() * size.height());

    // Now we have an HBITMAP, we can write it to the clipboard
    WriteBitmapFromHandle(source_hbitmap, size);
  }

  ::DeleteObject(source_hbitmap);
  ::ReleaseDC(NULL, dc);
}

void Clipboard::WriteBitmapFromSharedMemory(const SharedMemory& bitmap,
                                            const gfx::Size& size) {
  // TODO(darin): share data in gfx/bitmap_header.cc somehow
  BITMAPINFO bm_info = {0};
  bm_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bm_info.bmiHeader.biWidth = size.width();
  bm_info.bmiHeader.biHeight = -size.height();  // Sets the vertical orientation
  bm_info.bmiHeader.biPlanes = 1;
  bm_info.bmiHeader.biBitCount = 32;
  bm_info.bmiHeader.biCompression = BI_RGB;

  HDC dc = ::GetDC(NULL);

  // We can create an HBITMAP directly using the shared memory handle, saving
  // a memcpy.
  HBITMAP source_hbitmap =
      ::CreateDIBSection(dc, &bm_info, DIB_RGB_COLORS, NULL, bitmap.handle(), 0);

  if (source_hbitmap) {
    // Now we can write the HBITMAP to the clipboard
    WriteBitmapFromHandle(source_hbitmap, size);
  }

  ::DeleteObject(source_hbitmap);
  ::ReleaseDC(NULL, dc);
}

void Clipboard::WriteBitmapFromHandle(HBITMAP source_hbitmap,
                                      const gfx::Size& size) {
  // Acquire the clipboard.
  ClipboardLock lock;
  if (!lock.Acquire(clipboard_owner_))
    return;

  // We would like to just call ::SetClipboardData on the source_hbitmap,
  // but that bitmap might not be of a sort we can write to the clipboard.
  // For this reason, we create a new bitmap, copy the bits over, and then
  // write that to the clipboard.

  HDC dc = ::GetDC(NULL);
  HDC compatible_dc = ::CreateCompatibleDC(NULL);
  HDC source_dc = ::CreateCompatibleDC(NULL);

  // This is the HBITMAP we will eventually write to the clipboard
  HBITMAP hbitmap = ::CreateCompatibleBitmap(dc, size.width(), size.height());
  if (!hbitmap) {
    // Failed to create the bitmap
    ::DeleteDC(compatible_dc);
    ::DeleteDC(source_dc);
    ::ReleaseDC(NULL, dc);
    return;
  }

  HBITMAP old_hbitmap = (HBITMAP)SelectObject(compatible_dc, hbitmap);
  HBITMAP old_source = (HBITMAP)SelectObject(source_dc, source_hbitmap);

  // Now we need to blend it into an HBITMAP we can place on the clipboard
  BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  ::GdiAlphaBlend(compatible_dc, 0, 0, size.width(), size.height(),
                  source_dc, 0, 0, size.width(), size.height(), bf);

  // Clean up all the handles we just opened
  ::SelectObject(compatible_dc, old_hbitmap);
  ::SelectObject(source_dc, old_source);
  ::DeleteObject(old_hbitmap);
  ::DeleteObject(old_source);
  ::DeleteDC(compatible_dc);
  ::DeleteDC(source_dc);
  ::ReleaseDC(NULL, dc);

  // Actually write the bitmap to the clipboard
  ::SetClipboardData(CF_BITMAP, hbitmap);
}

// Write a file or set of files to the clipboard in HDROP format. When the user
// invokes a paste command (in a Windows explorer shell, for example), the files
// will be copied to the paste location.
void Clipboard::WriteFile(const std::wstring& file) {
  std::vector<std::wstring> files;
  files.push_back(file);
  WriteFiles(files);
}

void Clipboard::WriteFiles(const std::vector<std::wstring>& files) {
  ClipboardLock lock;
  if (!lock.Acquire(clipboard_owner_))
    return;

  // Calculate the amount of space we'll need store the strings: require
  // NULL terminator between strings, and double null terminator at the end.
  size_t bytes = sizeof(DROPFILES);
  for (size_t i = 0; i < files.size(); ++i)
    bytes += (files[i].length() + 1) * sizeof(wchar_t);
  bytes += sizeof(wchar_t);

  HANDLE hdata = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!hdata)
    return;

  DROPFILES* drop_files = static_cast<DROPFILES*>(::GlobalLock(hdata));
  drop_files->pFiles = sizeof(DROPFILES);
  drop_files->fWide = TRUE;
  BYTE* data = reinterpret_cast<BYTE*>(drop_files) + sizeof(DROPFILES);

  // Copy the strings stored in 'files' with proper NULL separation.
  wchar_t* data_pos = reinterpret_cast<wchar_t*>(data);
  for (size_t i = 0; i < files.size(); ++i) {
    size_t offset = files[i].length() + 1;
    memcpy(data_pos, files[i].c_str(), offset * sizeof(wchar_t));
    data_pos += offset;
  }
  data_pos[0] = L'\0';  // Double NULL termination after the last string.

  ::GlobalUnlock(hdata);
  if (!::SetClipboardData(CF_HDROP, hdata))
    ::GlobalFree(hdata);
}

bool Clipboard::IsFormatAvailable(unsigned int format) const {
  return ::IsClipboardFormatAvailable(format) != FALSE;
}

void Clipboard::ReadText(std::wstring* result) const {
  if (!result) {
    NOTREACHED();
    return;
  }

  result->clear();

  // Acquire the clipboard.
  ClipboardLock lock;
  if (!lock.Acquire(clipboard_owner_))
    return;

  HANDLE data = ::GetClipboardData(CF_UNICODETEXT);
  if (!data)
    return;

  result->assign(static_cast<const wchar_t*>(::GlobalLock(data)));
  ::GlobalUnlock(data);
}

void Clipboard::ReadAsciiText(std::string* result) const {
  if (!result) {
    NOTREACHED();
    return;
  }

  result->clear();

  // Acquire the clipboard.
  ClipboardLock lock;
  if (!lock.Acquire(clipboard_owner_))
    return;

  HANDLE data = ::GetClipboardData(CF_TEXT);
  if (!data)
    return;

  result->assign(static_cast<const char*>(::GlobalLock(data)));
  ::GlobalUnlock(data);
}

void Clipboard::ReadHTML(std::wstring* markup, std::string* src_url) const {
  if (markup)
    markup->clear();

  if (src_url)
    src_url->clear();

  // Acquire the clipboard.
  ClipboardLock lock;
  if (!lock.Acquire(clipboard_owner_))
    return;

  HANDLE data = ::GetClipboardData(GetHtmlFormatType());
  if (!data)
    return;

  std::string html_fragment(static_cast<const char*>(::GlobalLock(data)));
  ::GlobalUnlock(data);

  ParseHTMLClipboardFormat(html_fragment, markup, src_url);
}

void Clipboard::ReadBookmark(std::wstring* title, std::string* url) const {
  if (title)
    title->clear();

  if (url)
    url->clear();

  // Acquire the clipboard.
  ClipboardLock lock;
  if (!lock.Acquire(clipboard_owner_))
    return;

  HANDLE data = ::GetClipboardData(GetUrlWFormatType());
  if (!data)
    return;

  std::wstring bookmark(static_cast<const wchar_t*>(::GlobalLock(data)));
  ::GlobalUnlock(data);

  ParseBookmarkClipboardFormat(bookmark, title, url);
}

// Read a file in HDROP format from the clipboard.
void Clipboard::ReadFile(std::wstring* file) const {
  if (!file) {
    NOTREACHED();
    return;
  }

  file->clear();
  std::vector<std::wstring> files;
  ReadFiles(&files);

  // Take the first file, if available.
  if (!files.empty())
    file->assign(files[0]);
}

// Read a set of files in HDROP format from the clipboard.
void Clipboard::ReadFiles(std::vector<std::wstring>* files) const {
  if (!files) {
    NOTREACHED();
    return;
  }

  files->clear();

  ClipboardLock lock;
  if (!lock.Acquire(clipboard_owner_))
    return;

  HDROP drop = static_cast<HDROP>(::GetClipboardData(CF_HDROP));
  if (!drop)
    return;

  // Count of files in the HDROP.
  int count = ::DragQueryFile(drop, 0xffffffff, NULL, 0);

  if (count) {
    for (int i = 0; i < count; ++i) {
      int size = ::DragQueryFile(drop, i, NULL, 0) + 1;
      std::wstring file;
      ::DragQueryFile(drop, i, WriteInto(&file, size), size);
      files->push_back(file);
    }
  }
}

// static
void Clipboard::MarkupToHTMLClipboardFormat(const std::wstring& markup,
                                            const std::string& src_url,
                                            std::string* html_fragment) {
  DCHECK(html_fragment);
  // Documentation for the CF_HTML format is available at
  // http://msdn.microsoft.com/workshop/networking/clipboard/htmlclipboard.asp

  if (markup.empty()) {
    html_fragment->clear();
    return;
  }

  std::string markup_utf8 = WideToUTF8(markup);

  html_fragment->assign("Version:0.9");

  std::string start_html("\nStartHTML:");
  std::string end_html("\nEndHTML:");
  std::string start_fragment("\nStartFragment:");
  std::string end_fragment("\nEndFragment:");
  std::string source_url("\nSourceURL:");

  bool has_source_url = !src_url.empty() &&
                        !StartsWithASCII(src_url, "about:", false);
  if (has_source_url)
    source_url.append(src_url);

  std::string start_markup("\n<HTML>\n<BODY>\n<!--StartFragment-->\n");
  std::string end_markup("\n<!--EndFragment-->\n</BODY>\n</HTML>");

  // calculate offsets
  const size_t kMaxDigits = 10; // number of digits in UINT_MAX in base 10

  size_t start_html_offset, start_fragment_offset;
  size_t end_fragment_offset, end_html_offset;

  start_html_offset = html_fragment->length() +
                      start_html.length() + end_html.length() +
                      start_fragment.length() + end_fragment.length() +
                      (has_source_url ? source_url.length() : 0) +
                      (4*kMaxDigits);

  start_fragment_offset = start_html_offset + start_markup.length();
  end_fragment_offset = start_fragment_offset + markup_utf8.length();
  end_html_offset = end_fragment_offset + end_markup.length();

  // fill in needed data
  start_html.append(StringPrintf("%010u", start_html_offset));
  end_html.append(StringPrintf("%010u", end_html_offset));
  start_fragment.append(StringPrintf("%010u", start_fragment_offset));
  end_fragment.append(StringPrintf("%010u", end_fragment_offset));
  start_markup.append(markup_utf8);

  // create full html_fragment string from the fragments
  html_fragment->append(start_html);
  html_fragment->append(end_html);
  html_fragment->append(start_fragment);
  html_fragment->append(end_fragment);
  if (has_source_url)
    html_fragment->append(source_url);
  html_fragment->append(start_markup);
  html_fragment->append(end_markup);
}

// static
void Clipboard::ParseHTMLClipboardFormat(const std::string& html_frag,
                                         std::wstring* markup,
                                         std::string* src_url) {
  if (src_url) {
    // Obtain SourceURL, if present
    std::string src_url_str("SourceURL:");
    size_t line_start = html_frag.find(src_url_str, 0);
    if (line_start != std::string::npos) {
      size_t src_start = line_start+src_url_str.length();
      size_t src_end = html_frag.find("\n", line_start);

      if (src_end != std::string::npos)
        *src_url = html_frag.substr(src_start, src_end - src_start);
    }
  }

  if (markup) {
    // Find the markup between "<!--StartFragment -->" and
    // "<!--EndFragment -->", accounting for browser quirks
    size_t markup_start = html_frag.find('<', 0);
    size_t tag_start = html_frag.find("StartFragment", markup_start);
    size_t frag_start = html_frag.find('>', tag_start) + 1;
    // Here we do something slightly differently than WebKit.  Webkit does a
    // forward find for EndFragment, but that seems to be a bug if the html
    // fragment actually includes the string "EndFragment"
    size_t tag_end = html_frag.rfind("EndFragment", std::string::npos);
    size_t frag_end = html_frag.rfind('<', tag_end);

    TrimWhitespace(UTF8ToWide(html_frag.substr(frag_start,
                                               frag_end - frag_start)),
                   TRIM_ALL, markup);
  }
}

// static
void Clipboard::ParseBookmarkClipboardFormat(const std::wstring& bookmark,
                                             std::wstring* title,
                                             std::string* url) {
  const wchar_t* const kDelim = L"\r\n";

  const size_t title_end = bookmark.find_first_of(kDelim);
  if (title)
    title->assign(bookmark.substr(0, title_end));

  if (url) {
    const size_t url_start = bookmark.find_first_not_of(kDelim, title_end);
    if (url_start != std::wstring::npos)
      *url = WideToUTF8(bookmark.substr(url_start, std::wstring::npos));
  }
}

// static
Clipboard::FormatType Clipboard::GetUrlFormatType() {
  return ClipboardUtil::GetUrlFormat()->cfFormat;
}

// static
Clipboard::FormatType Clipboard::GetUrlWFormatType() {
  return ClipboardUtil::GetUrlWFormat()->cfFormat;
}

// static
Clipboard::FormatType Clipboard::GetMozUrlFormatType() {
  return ClipboardUtil::GetMozUrlFormat()->cfFormat;
}

// static
Clipboard::FormatType Clipboard::GetPlainTextFormatType() {
  return ClipboardUtil::GetPlainTextFormat()->cfFormat;
}

// static
Clipboard::FormatType Clipboard::GetPlainTextWFormatType() {
  return ClipboardUtil::GetPlainTextWFormat()->cfFormat;
}

// static
Clipboard::FormatType Clipboard::GetFilenameFormatType() {
  return ClipboardUtil::GetFilenameFormat()->cfFormat;
}

// static
Clipboard::FormatType Clipboard::GetFilenameWFormatType() {
  return ClipboardUtil::GetFilenameWFormat()->cfFormat;
}

// MS HTML Format
// static
Clipboard::FormatType Clipboard::GetHtmlFormatType() {
  return ClipboardUtil::GetHtmlFormat()->cfFormat;
}

// static
Clipboard::FormatType Clipboard::GetBitmapFormatType() {
  return CF_BITMAP;
}

// Firefox text/html
// static
Clipboard::FormatType Clipboard::GetTextHtmlFormatType() {
  return ClipboardUtil::GetTextHtmlFormat()->cfFormat;
}

// static
Clipboard::FormatType Clipboard::GetCFHDropFormatType() {
  return ClipboardUtil::GetCFHDropFormat()->cfFormat;
}

// static
Clipboard::FormatType Clipboard::GetFileDescriptorFormatType() {
  return ClipboardUtil::GetFileDescriptorFormat()->cfFormat;
}

// static
Clipboard::FormatType Clipboard::GetFileContentFormatZeroType() {
  return ClipboardUtil::GetFileContentFormatZero()->cfFormat;
}

// static
Clipboard::FormatType Clipboard::GetWebKitSmartPasteFormatType() {
  return ClipboardUtil::GetWebKitSmartPasteFormat()->cfFormat;
}

