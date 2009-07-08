// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_GLUE_UTIL_H_
#define WEBKIT_GLUE_GLUE_UTIL_H_

#include "base/file_path.h"
#include "base/string16.h"

class GURL;

namespace WebCore {
class ChromiumDataObject;
class CString;
class Cursor;
class HistoryItem;
class HTMLFormElement;
class IntPoint;
class IntRect;
class IntSize;
class KURL;
class ResourceError;
class ResourceResponse;
class SharedBuffer;
class String;
struct ResourceRequest;
}

namespace WebKit {
class WebCString;
class WebData;
class WebDragData;
class WebForm;
class WebHistoryItem;
class WebString;
class WebURL;
class WebURLRequest;
class WebURLResponse;
struct WebCursorInfo;
struct WebPoint;
struct WebRect;
struct WebSize;
struct WebURLError;
}

namespace WTF {
template <typename T> class PassRefPtr;
}

namespace gfx {
class Rect;
}

namespace webkit_glue {

// WebCore::CString <-> std::string. All characters are 8-bit and are preserved
// unchanged.
std::string CStringToStdString(const WebCore::CString& str);
WebCore::CString StdStringToCString(const std::string& str);

// WebCore::String <-> std::wstring. We assume that the WebCore::String is in
// UTF-16, and will either copy to a UTF-16 std::wstring (on Windows) or convert
// to a UTF-32 one on Linux and Mac.
std::wstring StringToStdWString(const WebCore::String& str);
WebCore::String StdWStringToString(const std::wstring& str);

// WebCore::String -> string16. This is a direct copy of UTF-16 characters.
string16 StringToString16(const WebCore::String& str);
WebCore::String String16ToString(const string16& str);

// WebCore::String <-> std::string. We assume the WebCore::String is UTF-16 and
// the std::string is UTF-8, and convert as necessary.
std::string StringToStdString(const WebCore::String& str);
WebCore::String StdStringToString(const std::string& str);

// WebCore::String <-> WebString.  No charset conversion.
WebKit::WebString StringToWebString(const WebCore::String& str);
WebCore::String WebStringToString(const WebKit::WebString& str);

// WebCore::CString <-> WebCString.  No charset conversion.
WebKit::WebCString CStringToWebCString(const WebCore::CString& str);
WebCore::CString WebCStringToCString(const WebKit::WebCString& str);

// std::string <-> WebString.  Conversion to/from UTF-8.
WebKit::WebString StdStringToWebString(const std::string& str);
std::string WebStringToStdString(const WebKit::WebString& str);

// WebCore::SharedBuffer <-> WebData.  No charset conversion.
WebKit::WebData SharedBufferToWebData(
    const WTF::PassRefPtr<WebCore::SharedBuffer>& buf);
WTF::PassRefPtr<WebCore::SharedBuffer> WebDataToSharedBuffer(
    const WebKit::WebData& data);

FilePath::StringType StringToFilePathString(const WebCore::String& str);
WebCore::String FilePathStringToString(const FilePath::StringType& str);

GURL KURLToGURL(const WebCore::KURL& url);
WebCore::KURL GURLToKURL(const GURL& url);
GURL StringToGURL(const WebCore::String& spec);

WebKit::WebURL KURLToWebURL(const WebCore::KURL& url);
WebCore::KURL WebURLToKURL(const WebKit::WebURL& url);

gfx::Rect FromIntRect(const WebCore::IntRect& r);
WebCore::IntRect ToIntRect(const gfx::Rect& r);

// WebPoint <-> IntPoint
WebCore::IntPoint WebPointToIntPoint(const WebKit::WebPoint&);
WebKit::WebPoint IntPointToWebPoint(const WebCore::IntPoint&);

// WebRect <-> IntRect
WebCore::IntRect WebRectToIntRect(const WebKit::WebRect&);
WebKit::WebRect IntRectToWebRect(const WebCore::IntRect&);

// WebSize <-> IntSize
WebCore::IntSize WebSizeToIntSize(const WebKit::WebSize&);
WebKit::WebSize IntSizeToWebSize(const WebCore::IntSize&);

// WebCursorInfo <- Cursor
WebKit::WebCursorInfo CursorToWebCursorInfo(const WebCore::Cursor&);

// WebDragData <-> ChromiumDataObject
WebKit::WebDragData ChromiumDataObjectToWebDragData(
    const WTF::PassRefPtr<WebCore::ChromiumDataObject>&);
WTF::PassRefPtr<WebCore::ChromiumDataObject> WebDragDataToChromiumDataObject(
    const WebKit::WebDragData&);

// WebForm <-> HTMLFormElement
WebKit::WebForm HTMLFormElementToWebForm(
    const WTF::PassRefPtr<WebCore::HTMLFormElement>&);
WTF::PassRefPtr<WebCore::HTMLFormElement> WebFormToHTMLFormElement(
    const WebKit::WebForm&);

// WebHistoryItem <-> HistoryItem
WebKit::WebHistoryItem HistoryItemToWebHistoryItem(
    const WTF::PassRefPtr<WebCore::HistoryItem>&);
WTF::PassRefPtr<WebCore::HistoryItem> WebHistoryItemToHistoryItem(
    const WebKit::WebHistoryItem&);

// WebURLError <-> ResourceError
WebKit::WebURLError ResourceErrorToWebURLError(
    const WebCore::ResourceError& error);
WebCore::ResourceError WebURLErrorToResourceError(
    const WebKit::WebURLError& error);

// Exposes the ResourceRequest contained by a WebURLRequest
WebCore::ResourceRequest* WebURLRequestToMutableResourceRequest(
    WebKit::WebURLRequest* req);
const WebCore::ResourceRequest* WebURLRequestToResourceRequest(
    const WebKit::WebURLRequest* req);

// Exposes the ResourceResponse contained by a WebURLResponse
WebCore::ResourceResponse* WebURLResponseToMutableResourceResponse(
    WebKit::WebURLResponse* resp);
const WebCore::ResourceResponse* WebURLResponseToResourceResponse(
    const WebKit::WebURLResponse* resp);

}  // namespace webkit_glue

#endif  // #ifndef WEBKIT_GLUE_GLUE_UTIL_H_
