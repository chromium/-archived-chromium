// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(darin): This file will be deleted once we complete the move to
// webkit/api

// In this file, we pretend to be part of the WebKit implementation.
// This is just a temporary hack while glue is still being moved into
// webkit/api.
#define WEBKIT_IMPLEMENTATION 1

#include "config.h"
#include "webkit/glue/glue_util.h"

#include <string>

#include "ChromiumDataObject.h"
#include "CString.h"
#include "HistoryItem.h"
#include "HTMLFormElement.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "KURL.h"
#include "PlatformString.h"
#include "ResourceError.h"

#undef LOG
#include "base/compiler_specific.h"
#include "base/gfx/rect.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "webkit/api/public/WebCursorInfo.h"
#include "webkit/api/public/WebDragData.h"
#include "webkit/api/public/WebForm.h"
#include "webkit/api/public/WebHistoryItem.h"
#include "webkit/api/public/WebPoint.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebSize.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLError.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/api/public/WebURLResponse.h"

namespace webkit_glue {

// String conversions ----------------------------------------------------------

std::string CStringToStdString(const WebCore::CString& str) {
  const char* chars = str.data();
  return std::string(chars ? chars : "", str.length());
}

WebCore::CString StdStringToCString(const std::string& str) {
  return WebCore::CString(str.data(), str.size());
}

std::wstring StringToStdWString(const WebCore::String& str) {
  const UChar* chars = str.characters();
#if defined(WCHAR_T_IS_UTF16)
  return std::wstring(chars ? chars : L"", str.length());
#elif defined(WCHAR_T_IS_UTF32)
  string16 str16(chars ? chars : reinterpret_cast<const char16 *>(L""),
                                 str.length());
  return UTF16ToWide(str16);
#endif
}

string16 StringToString16(const WebCore::String& str) {
  const UChar* chars = str.characters();
  return string16(chars ? chars : (UChar *)L"", str.length());
}

WebCore::String String16ToString(const string16& str) {
  return WebCore::String(reinterpret_cast<const UChar*>(str.data()),
                         str.length());
}

std::string StringToStdString(const WebCore::String& str) {
  std::string ret;
  if (!str.isNull())
    UTF16ToUTF8(str.characters(), str.length(), &ret);
  return ret;
}

WebCore::String StdWStringToString(const std::wstring& str) {
#if defined(WCHAR_T_IS_UTF16)
  return WebCore::String(str.data(), static_cast<unsigned>(str.length()));
#elif defined(WCHAR_T_IS_UTF32)
  string16 str16 = WideToUTF16(str);
  return WebCore::String(str16.data(), static_cast<unsigned>(str16.length()));
#endif
}

WebCore::String StdStringToString(const std::string& str) {
  return WebCore::String::fromUTF8(str.data(),
                                   static_cast<unsigned>(str.length()));
}

WebKit::WebString StringToWebString(const WebCore::String& str) {
  return str;
}

WebCore::String WebStringToString(const WebKit::WebString& str) {
  return str;
}

WebKit::WebCString CStringToWebCString(const WebCore::CString& str) {
  return str;
}

WebCore::CString WebCStringToCString(const WebKit::WebCString& str) {
  return str;
}

WebKit::WebString StdStringToWebString(const std::string& str) {
  return WebKit::WebString::fromUTF8(str.data(), str.size());
}

std::string WebStringToStdString(const WebKit::WebString& str) {
  std::string ret;
  if (!str.isNull())
    UTF16ToUTF8(str.data(), str.length(), &ret);
  return ret;
}

WebKit::WebData SharedBufferToWebData(
    const WTF::PassRefPtr<WebCore::SharedBuffer>& buf) {
  return buf;
}

WTF::PassRefPtr<WebCore::SharedBuffer> WebDataToSharedBuffer(
    const WebKit::WebData& data) {
  return data;
}

FilePath::StringType StringToFilePathString(const WebCore::String& str) {
#if defined(OS_WIN)
  return StringToStdWString(str);
#elif defined(OS_POSIX)
  return base::SysWideToNativeMB(StringToStdWString(str));
#endif
}

WebCore::String FilePathStringToString(const FilePath::StringType& str) {
#if defined(OS_WIN)
  return StdWStringToString(str);
#elif defined(OS_POSIX)
  return StdWStringToString(base::SysNativeMBToWide(str));
#endif
}

// URL conversions -------------------------------------------------------------

GURL KURLToGURL(const WebCore::KURL& url) {
#if USE(GOOGLEURL)
  const WebCore::CString& spec = url.utf8String();
  if (spec.isNull() || 0 == spec.length())
    return GURL();
  return GURL(spec.data(), spec.length(), url.parsed(), url.isValid());
#else
  return StringToGURL(url.string());
#endif
}

WebCore::KURL GURLToKURL(const GURL& url) {
  const std::string& spec = url.possibly_invalid_spec();
#if USE(GOOGLEURL)
  // Convert using the internal structures to avoid re-parsing.
  return WebCore::KURL(WebCore::CString(spec.c_str(),
                                        static_cast<unsigned>(spec.length())),
                       url.parsed_for_possibly_invalid_spec(), url.is_valid());
#else
  return WebCore::KURL(StdWStringToString(UTF8ToWide(spec)));
#endif
}

GURL StringToGURL(const WebCore::String& spec) {
  return GURL(WideToUTF8(StringToStdWString(spec)));
}

WebKit::WebURL KURLToWebURL(const WebCore::KURL& url) {
  return url;
}

WebCore::KURL WebURLToKURL(const WebKit::WebURL& url) {
  return url;
}

// Rect conversions ------------------------------------------------------------

gfx::Rect FromIntRect(const WebCore::IntRect& r) {
    return gfx::Rect(r.x(), r.y(), r.width() < 0 ? 0 : r.width(),
        r.height() < 0 ? 0 : r.height());
}

WebCore::IntRect ToIntRect(const gfx::Rect& r) {
  return WebCore::IntRect(r.x(), r.y(), r.width(), r.height());
}

// Point conversions -----------------------------------------------------------

WebCore::IntPoint WebPointToIntPoint(const WebKit::WebPoint& point) {
  return point;
}

WebKit::WebPoint IntPointToWebPoint(const WebCore::IntPoint& point) {
  return point;
}

// Rect conversions ------------------------------------------------------------

WebCore::IntRect WebRectToIntRect(const WebKit::WebRect& rect) {
  return rect;
}

WebKit::WebRect IntRectToWebRect(const WebCore::IntRect& rect) {
  return rect;
}

// Size conversions ------------------------------------------------------------

WebCore::IntSize WebSizeToIntSize(const WebKit::WebSize& size) {
  return size;
}

WebKit::WebSize IntSizeToWebSize(const WebCore::IntSize& size) {
  return size;
}

// Cursor conversions ----------------------------------------------------------

WebKit::WebCursorInfo CursorToWebCursorInfo(const WebCore::Cursor& cursor) {
  return WebKit::WebCursorInfo(cursor);
}

// DragData conversions --------------------------------------------------------

WebKit::WebDragData ChromiumDataObjectToWebDragData(
    const PassRefPtr<WebCore::ChromiumDataObject>& data) {
  return data;
}

PassRefPtr<WebCore::ChromiumDataObject> WebDragDataToChromiumDataObject(
    const WebKit::WebDragData& data) {
  return data;
}

// WebForm conversions ---------------------------------------------------------

WebKit::WebForm HTMLFormElementToWebForm(
    const WTF::PassRefPtr<WebCore::HTMLFormElement>& form) {
  return form;
}

WTF::PassRefPtr<WebCore::HTMLFormElement> WebFormToHTMLFormElement(
    const WebKit::WebForm& form) {
  return form;
}

// WebHistoryItem conversions --------------------------------------------------

WebKit::WebHistoryItem HistoryItemToWebHistoryItem(
    const WTF::PassRefPtr<WebCore::HistoryItem>& item) {
  return item;
}

WTF::PassRefPtr<WebCore::HistoryItem> WebHistoryItemToHistoryItem(
    const WebKit::WebHistoryItem& item) {
  return item;
}

// WebURLError conversions -----------------------------------------------------

WebKit::WebURLError ResourceErrorToWebURLError(
    const WebCore::ResourceError& error) {
  return error;
}

WebCore::ResourceError WebURLErrorToResourceError(
    const WebKit::WebURLError& error) {
  return error;
}

// WebURLRequest conversions ---------------------------------------------------

WebCore::ResourceRequest* WebURLRequestToMutableResourceRequest(
    WebKit::WebURLRequest* request) {
  return &request->toMutableResourceRequest();
}

const WebCore::ResourceRequest* WebURLRequestToResourceRequest(
    const WebKit::WebURLRequest* request) {
  return &request->toResourceRequest();
}

// WebURLResponse conversions --------------------------------------------------

WebCore::ResourceResponse* WebURLResponseToMutableResourceResponse(
    WebKit::WebURLResponse* response) {
  return &response->toMutableResourceResponse();
}

const WebCore::ResourceResponse* WebURLResponseToResourceResponse(
    const WebKit::WebURLResponse* response) {
  return &response->toResourceResponse();
}

}  // namespace webkit_glue
