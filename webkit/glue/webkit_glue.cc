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

#include <objidl.h>
#include <mlang.h>

#include "config.h"
#include "webkit_version.h"
#pragma warning(push, 0)
#include "BackForwardList.h"
#include "Document.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "Frame.h"
#include "HistoryItem.h"
#include "ImageSource.h"
#include "KURL.h"
#include "LogWin.h"
#include "Page.h"
#include "PlatformString.h"
#include "RenderTreeAsText.h"
#include "SharedBuffer.h"
#pragma warning(pop)

#if USE(V8_BINDING) || USE(JAVASCRIPTCORE_BINDINGS)
#include "JSBridge.h"  // for set flags
#endif

#undef LOG
#undef notImplemented
#include "webkit/glue/webkit_glue.h"

#include "base/file_version_info.h"
#include "base/string_util.h"
#include "skia/include/SkBitmap.h"
#include "webkit/glue/event_conversion.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/weburlrequest_impl.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview_impl.h"

//------------------------------------------------------------------------------
// webkit_glue impl:

namespace webkit_glue {

void SetJavaScriptFlags(const std::wstring& str) {
#if USE(V8_BINDING) || USE(JAVASCRIPTCORE_BINDINGS)
  std::string utf8_str = WideToUTF8(str);
  WebCore::JSBridge::setFlags(utf8_str.data(), static_cast<int>(utf8_str.size()));
#endif
}

void SetRecordPlaybackMode(bool value) {
#if USE(V8_BINDING) || USE(JAVASCRIPTCORE_BINDINGS)
  WebCore::JSBridge::setRecordPlaybackMode(value);
#endif
}

static bool layout_test_mode_ = false;

void SetLayoutTestMode(bool enable) {
  layout_test_mode_ = enable;
}

bool IsLayoutTestMode() {
  return layout_test_mode_;
}

MONITORINFOEX GetMonitorInfoForWindowHelper(HWND window) {
  HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFOEX monitorInfo;
  monitorInfo.cbSize = sizeof(MONITORINFOEX);
  GetMonitorInfo(monitor, &monitorInfo);
  return monitorInfo;
}

IMLangFontLink2* GetLangFontLinkHelper() {
  // TODO(hbono): http://b/1072298 Experimentally disabled this code to
  // prevent registry leaks caused by this IMLangFontLink2 interface.
  // If you find any font-rendering regressions. Please feel free to blame me.
  // TODO(hbono): http://b/1072298 The test shell does not use our font metrics
  // but it uses its own font metrics which heavily depend on this
  // IMLangFontLink2 interface. Even though we should change the test shell to
  // use out font metrics and re-baseline such tests, we temporarily let the
  // test shell use this interface until we complete the said change.
  if (!IsLayoutTestMode())
    return NULL;

  static IMultiLanguage *multi_language = NULL;

  if (!multi_language) {
    if (CoCreateInstance(CLSID_CMultiLanguage, 
                         0, 
                         CLSCTX_ALL, 
                         IID_IMultiLanguage, 
                         reinterpret_cast<void**>(&multi_language)) != S_OK) {
        return 0;
    }
  }

  static IMLangFontLink2* lang_font_link;
  if (!lang_font_link) {
    if (multi_language->QueryInterface(
            IID_IMLangFontLink2, 
            reinterpret_cast<void**>(&lang_font_link)) != S_OK) {
      return 0;
    }
  }

  return lang_font_link;
}

std::wstring DumpDocumentText(WebFrame* web_frame) {
  WebFrameImpl* webFrameImpl = static_cast<WebFrameImpl*>(web_frame);
  WebCore::Frame* frame = webFrameImpl->frame();

  // We use the document element's text instead of the body text here because
  // not all documents have a body, such as XML documents.
  return StringToStdWString(frame->document()->documentElement()->innerText());
}

std::wstring DumpFramesAsText(WebFrame* web_frame, bool recursive) {
  WebFrameImpl* webFrameImpl = static_cast<WebFrameImpl*>(web_frame);
  std::wstring result;

  // Add header for all but the main frame.
  if (webFrameImpl->GetParent()) {
    result.append(L"\n--------\nFrame: '");
    result.append(webFrameImpl->GetName());
    result.append(L"'\n--------\n");
  }

  result.append(DumpDocumentText(web_frame));
  result.append(L"\n");

  if (recursive) {
    WebCore::Frame* child = webFrameImpl->frame()->tree()->firstChild();
    for (; child; child = child->tree()->nextSibling()) {
      result.append(
          DumpFramesAsText(WebFrameImpl::FromFrame(child), recursive));
    }
  }

  return result;
}

std::wstring DumpRenderer(WebFrame* web_frame) {
  WebFrameImpl* webFrameImpl = static_cast<WebFrameImpl*>(web_frame);
  WebCore::Frame* frame = webFrameImpl->frame();

  // This implicitly converts from a DeprecatedString.
  return StringToStdWString(WebCore::externalRepresentation(frame->renderer()));
}

std::wstring DumpFrameScrollPosition(WebFrame* web_frame, bool recursive) {
  WebFrameImpl* webFrameImpl = static_cast<WebFrameImpl*>(web_frame);
  WebCore::IntSize offset = webFrameImpl->frameview()->scrollOffset();
  std::wstring result;

  if (offset.width() > 0 || offset.height() > 0) {
    if (webFrameImpl->GetParent()) {
      StringAppendF(&result, L"frame '%s' ", StringToStdWString(
          webFrameImpl->frame()->tree()->name()).c_str());
    }
    StringAppendF(&result, L"scrolled to %d,%d\n",
                  offset.width(), offset.height());
  }

  if (recursive) {
    WebCore::Frame* child = webFrameImpl->frame()->tree()->firstChild();
    for (; child; child = child->tree()->nextSibling()) {
      result.append(DumpFrameScrollPosition(WebFrameImpl::FromFrame(child),
                                            recursive));
    }
  }

  return result;
}

// Returns True if item1 < item2.
static bool HistoryItemCompareLess(PassRefPtr<WebCore::HistoryItem> item1,
                                   PassRefPtr<WebCore::HistoryItem> item2) {
  std::wstring target1 = StringToStdWString(item1->target());
  std::wstring target2 = StringToStdWString(item2->target());
  std::transform(target1.begin(), target1.end(), target1.begin(), tolower);
  std::transform(target2.begin(), target2.end(), target2.begin(), tolower);
  return target1 < target2;
}

// Writes out a HistoryItem into a string in a readable format.
static void DumpHistoryItem(WebCore::HistoryItem* item, int indent,
                            bool is_current, std::wstring* result) {
  if (is_current) {
    result->append(L"curr->");
    result->append(indent - 6, L' ');  // 6 == L"curr->".length()
  } else {
    result->append(indent, L' ');
  }

  result->append(StringToStdWString(item->urlString()));
  if (!item->target().isEmpty()) {
    result->append(L" (in frame \"" + StringToStdWString(item->target()) +
                   L"\")");
  }
  if (item->isTargetItem())
    result->append(L"  **nav target**");
  result->append(L"\n");

  if (item->hasChildren()) {
    WebCore::HistoryItemVector children = item->children();
    // Must sort to eliminate arbitrary result ordering which defeats 
    // reproducible testing.
    std::sort(children.begin(), children.end(), HistoryItemCompareLess);
    for (unsigned i = 0; i < children.size(); i++) {
      DumpHistoryItem(children[i].get(), indent+4, false, result);
    }
  }
}

void DumpBackForwardList(WebView* view, void* previous_history_item,
                         std::wstring* result) {
  result->append(L"\n============== Back Forward List ==============\n");

  WebCore::Frame* frame =
      static_cast<WebFrameImpl*>(view->GetMainFrame())->frame();
  WebCore::BackForwardList* list = frame->page()->backForwardList();

  // Skip everything before the previous_history_item, if it's in the back list.
  // If it isn't found, assume it fell off the end, and include everything.
  int start_index = -list->backListCount();
  WebCore::HistoryItem* prev_item =
      static_cast<WebCore::HistoryItem*>(previous_history_item);
  for (int i = -list->backListCount(); i < 0; ++i) {
    if (prev_item == list->itemAtIndex(i))
      start_index = i+1;
  }

  for (int i = start_index; i < 0; ++i)
    DumpHistoryItem(list->itemAtIndex(i), 8, false, result);

  DumpHistoryItem(list->currentItem(), 8, true, result);

  for (int i = 1; i <= list->forwardListCount(); ++i)
    DumpHistoryItem(list->itemAtIndex(i), 8, false, result);

  result->append(L"===============================================\n");
}

void ResetBeforeTestRun(WebView* view) {
  WebFrameImpl* webframe = static_cast<WebFrameImpl*>(view->GetMainFrame());
  WebCore::Frame* frame = webframe->frame();

  // Reset the main frame name since tests always expect it to be empty.  It
  // is normally not reset between page loads (even in IE and FF).
  if (frame && frame->tree())
    frame->tree()->setName("");

  // This is papering over b/850700.  But it passes a few more tests, so we'll
  // keep it for now.
  if (frame && frame->scriptBridge())
    frame->scriptBridge()->setEventHandlerLineno(0);

  // Reset the last click information so the clicks generated from previous
  // test aren't inherited (otherwise can mistake single/double/triple clicks)
  MakePlatformMouseEvent::ResetLastClick();
}

#ifndef NDEBUG
// The log macro was having problems due to collisions with WTF, so we just
// code here what that would have inlined.
void DumpLeakedObject(const char* file, int line, const char* object, int count) {
  std::string msg = StringPrintf("%s LEAKED %d TIMES", object, count);
  AppendToLog(file, line, msg.c_str());
}
#endif

void CheckForLeaks() {
#ifndef NDEBUG
  int count = WebFrameImpl::live_object_count();
  if (count)
    DumpLeakedObject(__FILE__, __LINE__, "WebFrame", count);
#endif
}

bool DecodeImage(const std::string& image_data, SkBitmap* image) {
   RefPtr<WebCore::SharedBuffer> buffer(
       new WebCore::SharedBuffer(image_data.data(),
                                 static_cast<int>(image_data.length())));
  WebCore::ImageSource image_source;
  image_source.setData(buffer.get(), true);

  if (image_source.frameCount() > 0) {
    *image = *reinterpret_cast<SkBitmap*>(image_source.createFrameAtIndex(0));
    return true;
  }
  // We failed to decode the image.
  return false;
}

// Convert from WebKit types to Glue types and notify the embedder. This should
// not perform complex processing since it may be called a lot.
void NotifyFormStateChanged(const WebCore::Document* document) {
  if (!document)
    return;

  WebCore::Frame* frame = document->frame();
  if (!frame)
    return;

  // Dispatch to the delegate of the view that owns the frame.
  WebFrame* webframe = WebFrameImpl::FromFrame(document->frame());
  WebView* webview = webframe->GetView();
  if (!webview)
    return;
  WebViewDelegate* delegate = webview->GetDelegate();
  if (!delegate)
    return;
  delegate->OnNavStateChanged(webview);
}

std::string GetWebKitVersion() {
  return StringPrintf("%d.%d", WEBKIT_VERSION_MAJOR, WEBKIT_VERSION_MINOR);
}

const std::string& GetDefaultUserAgent() {
  static std::string user_agent;
  static bool generated_user_agent;
  if (!generated_user_agent) {
    OSVERSIONINFO info = {0};
    info.dwOSVersionInfoSize = sizeof(info);
    GetVersionEx(&info);

    // Get the product name and version, and replace Safari's Version/X string
    // with it.  This is done to expose our product name in a manner that is
    // maximally compatible with Safari, we hope!!
    std::string product;
#ifdef CHROME_LAST_MINUTE
    FileVersionInfo* version_info =
        FileVersionInfo::CreateFileVersionInfoForCurrentModule();
    if (version_info)
      product = "Chrome/" + WideToASCII(version_info->product_version());
#endif
    if (product.empty())
      product = "Version/3.1";

    // Derived from Safari's UA string.
    StringAppendF(
        &user_agent,
        "Mozilla/5.0 (Windows; U; Windows NT %d.%d; en-US) AppleWebKit/%d.%d"
        " (KHTML, like Gecko) %s Safari/%d.%d",
        info.dwMajorVersion,
        info.dwMinorVersion,
        WEBKIT_VERSION_MAJOR,
        WEBKIT_VERSION_MINOR,
        product.c_str(),
        WEBKIT_VERSION_MAJOR,
        WEBKIT_VERSION_MINOR
        );

    generated_user_agent = true;
  }

  return user_agent;
}


void NotifyJSOutOfMemory(WebCore::Frame* frame) {
  if (!frame)
    return;

  // Dispatch to the delegate of the view that owns the frame.
  WebFrame* webframe = WebFrameImpl::FromFrame(frame);
  WebView* webview = webframe->GetView();
  if (!webview)
    return;
  WebViewDelegate* delegate = webview->GetDelegate();
  if (!delegate)
    return;
  delegate->JSOutOfMemory();
}

} // namespace webkit_glue
