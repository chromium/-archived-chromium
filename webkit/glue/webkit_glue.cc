// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <objidl.h>
#include <mlang.h>
#endif

#include "config.h"
#include "webkit_version.h"
MSVC_PUSH_WARNING_LEVEL(0);
#include "BackForwardList.h"
#include "Document.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "Frame.h"
#include "HistoryItem.h"
#if defined(OS_WIN)  // TODO(port): unnecessary after the webkit merge lands.
#include "ImageSource.h"
#endif
#include "KURL.h"
#include "Page.h"
#include "PlatformString.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "SharedBuffer.h"
MSVC_POP_WARNING();

#undef LOG
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

// Global variable used by the plugin quirk "die after unload".
bool g_forcefully_terminate_plugin_process = false;

void SetJavaScriptFlags(const std::wstring& str) {
#if USE(V8)
  std::string utf8_str = WideToUTF8(str);
  WebCore::ScriptController::setFlags(utf8_str.data(), static_cast<int>(utf8_str.size()));
#endif
}

void SetRecordPlaybackMode(bool value) {
#if USE(V8)
  WebCore::ScriptController::setRecordPlaybackMode(value);
#endif
}


void SetShouldExposeGCController(bool enable) {
#if USE(V8)
  WebCore::ScriptController::setShouldExposeGCController(enable);
#endif
}

static bool layout_test_mode_ = false;

void SetLayoutTestMode(bool enable) {
  layout_test_mode_ = enable;
}

bool IsLayoutTestMode() {
  return layout_test_mode_;
}

void InitializeForTesting() {
  WTF::initializeThreading();
  WebCore::AtomicString::init();
}

void EnableWebCoreNotImplementedLogging() {
  WebCore::LogNotYetImplemented.state = WTFLogChannelOn;
}

std::wstring DumpDocumentText(WebFrame* web_frame) {
  WebFrameImpl* webFrameImpl = static_cast<WebFrameImpl*>(web_frame);
  WebCore::Frame* frame = webFrameImpl->frame();

  // We use the document element's text instead of the body text here because
  // not all documents have a body, such as XML documents.
  WebCore::Element* documentElement = frame->document()->documentElement();
  if (!documentElement) {
    return std::wstring();
  }
  return StringToStdWString(documentElement->innerText());
}

std::wstring DumpFramesAsText(WebFrame* web_frame, bool recursive) {
  WebFrameImpl* webFrameImpl = static_cast<WebFrameImpl*>(web_frame);
  std::wstring result;

  // Add header for all but the main frame. Skip empty frames.
  if (webFrameImpl->GetParent() &&
      webFrameImpl->frame()->document()->documentElement()) {
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

  WebCore::String frameText =
      WebCore::externalRepresentation(frame->contentRenderer());
  return StringToStdWString(frameText);
}

std::wstring DumpFrameScrollPosition(WebFrame* web_frame, bool recursive) {
  WebFrameImpl* webFrameImpl = static_cast<WebFrameImpl*>(web_frame);
  WebCore::IntSize offset = webFrameImpl->frameview()->scrollOffset();
  std::wstring result;

  if (offset.width() > 0 || offset.height() > 0) {
    if (webFrameImpl->GetParent()) {
      StringAppendF(&result, L"frame '%ls' ", StringToStdWString(
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
  if (frame && frame->script())
    frame->script()->setEventHandlerLineno(0);

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
#if defined(OS_WIN)  // TODO(port): unnecessary after the webkit merge lands.
   RefPtr<WebCore::SharedBuffer> buffer(
       WebCore::SharedBuffer::create(image_data.data(),
                                     static_cast<int>(image_data.length())));
  WebCore::ImageSource image_source;
  image_source.setData(buffer.get(), true);

  if (image_source.frameCount() > 0) {
    *image = *reinterpret_cast<SkBitmap*>(image_source.createFrameAtIndex(0));
    return true;
  }
  // We failed to decode the image.
  return false;
#else
  // This ought to work; we just need the webkit merge.
  NOTIMPLEMENTED();
  return false;
#endif
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

namespace {

std::string* user_agent = NULL;
bool user_agent_requested = false;

void SetUserAgentToDefault() {
  static std::string default_user_agent;
#if defined(OS_WIN) || defined(OS_MACOSX)
  int32 os_major_version = 0;
  int32 os_minor_version = 0;
  int32 os_bugfix_version = 0;
#if defined(OS_WIN)
  OSVERSIONINFO info = {0};
  info.dwOSVersionInfoSize = sizeof(info);
  GetVersionEx(&info);
  os_major_version = info.dwMajorVersion;
  os_minor_version = info.dwMinorVersion;
#elif defined(OS_MACOSX)
  Gestalt(gestaltSystemVersionMajor, 
      reinterpret_cast<SInt32*>(&os_major_version));
  Gestalt(gestaltSystemVersionMinor, 
      reinterpret_cast<SInt32*>(&os_minor_version));
  Gestalt(gestaltSystemVersionBugFix, 
      reinterpret_cast<SInt32*>(&os_bugfix_version));
#endif

  // Get the product name and version, and replace Safari's Version/X string
  // with it.  This is done to expose our product name in a manner that is
  // maximally compatible with Safari, we hope!!
  std::string product;

  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (version_info.get())
    product = "Chrome/" + WideToASCII(version_info->product_version());

  if (product.empty())
    product = "Version/3.1";

  // Derived from Safari's UA string.
  StringAppendF(
      &default_user_agent,
#if defined(OS_WIN)
      "Mozilla/5.0 (Windows; U; Windows NT %d.%d; en-US) AppleWebKit/%d.%d"
#elif defined(OS_MACOSX)
      "Mozilla/5.0 (Macintosh; U; Intel Mac OS X %d_%d_%d; en-US) "
      "AppleWebKit/%d.%d"
#endif
      " (KHTML, like Gecko) %s Safari/%d.%d",
      os_major_version,
      os_minor_version,
#if defined(OS_MACOSX)
      os_bugfix_version,
#endif
      WEBKIT_VERSION_MAJOR,
      WEBKIT_VERSION_MINOR,
      product.c_str(),
      WEBKIT_VERSION_MAJOR,
      WEBKIT_VERSION_MINOR
      );
#elif defined(OS_LINUX)
  // TODO(agl): We don't have version information embedded in files under Linux
  // so we use the following string which is based off the UA string for
  // Windows. Some solution for embedding the Chrome version number needs to be
  // found here.
  StringAppendF(
      &default_user_agent,
      "Mozilla/5.0 (Linux; U; en-US) AppleWebKit/525.13 "
      "(KHTML, like Gecko) Chrome/0.2.149.27 Safari/525.13");
#else
  // TODO(port): we need something like FileVersionInfo for our UA string.
  NOTIMPLEMENTED();
#endif
  user_agent = &default_user_agent;
}

};

void SetUserAgent(const std::string& new_user_agent) {
  DCHECK(!user_agent_requested) << "Setting the user agent after someone has "
      "already requested it can result in unexpected behavior.";
  static std::string overridden_user_agent;
  overridden_user_agent = new_user_agent;  // If you combine this with the
                                           // previous line, the function only
                                           // works the first time.
  user_agent = &overridden_user_agent;
}

const std::string& GetUserAgent() {
  if (!user_agent)
    SetUserAgentToDefault();
  user_agent_requested = true;
  return *user_agent;
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

void SetForcefullyTerminatePluginProcess(bool value) {
  if (IsPluginRunningInRendererProcess()) {
    // Ignore this quirk when the plugins are not running in their own process.
    return;
  }

  g_forcefully_terminate_plugin_process = value;
}

bool ShouldForcefullyTerminatePluginProcess() {
  return g_forcefully_terminate_plugin_process;
}

} // namespace webkit_glue
