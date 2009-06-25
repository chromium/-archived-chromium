// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_WIN)
#include <objidl.h>
#include <mlang.h>
#elif defined(OS_LINUX)
#include <sys/utsname.h>
#endif

#include "BackForwardList.h"
#include "Document.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "Frame.h"
#include "HistoryItem.h"
#include "ImageSource.h"
#include "KURL.h"
#include "Page.h"
#include "PlatformString.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "SharedBuffer.h"

#undef LOG
#include "base/file_version_info.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/sys_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/api/public/WebHistoryItem.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebVector.h"
#if defined(OS_WIN)
#include "webkit/api/public/win/WebInputEventFactory.h"
#endif
#include "webkit/glue/event_conversion.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview_impl.h"

#include "webkit_version.h"  // Generated

using WebKit::WebHistoryItem;
using WebKit::WebString;
using WebKit::WebVector;

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
static bool HistoryItemCompareLess(const WebHistoryItem& item1,
                                   const WebHistoryItem& item2) {
  string16 target1 = item1.target();
  string16 target2 = item2.target();
  std::transform(target1.begin(), target1.end(), target1.begin(), tolower);
  std::transform(target2.begin(), target2.end(), target2.begin(), tolower);
  return target1 < target2;
}

// Writes out a HistoryItem into a string in a readable format.
static std::wstring DumpHistoryItem(const WebHistoryItem& item,
                                    int indent, bool is_current) {
  std::wstring result;

  if (is_current) {
    result.append(L"curr->");
    result.append(indent - 6, L' ');  // 6 == L"curr->".length()
  } else {
    result.append(indent, L' ');
  }

  result.append(UTF16ToWideHack(item.urlString()));
  if (!item.target().isEmpty())
    result.append(L" (in frame \"" + UTF16ToWide(item.target()) + L"\")");
  if (item.isTargetItem())
    result.append(L"  **nav target**");
  result.append(L"\n");

  const WebVector<WebHistoryItem>& children = item.children();
  if (!children.isEmpty()) {
    // Must sort to eliminate arbitrary result ordering which defeats
    // reproducible testing.
    // TODO(darin): WebVector should probably just be a std::vector!!
    std::vector<WebHistoryItem> sorted_children;
    for (size_t i = 0; i < children.size(); ++i)
      sorted_children.push_back(children[i]);
    std::sort(sorted_children.begin(), sorted_children.end(),
              HistoryItemCompareLess);
    for (size_t i = 0; i < sorted_children.size(); i++)
      result += DumpHistoryItem(sorted_children[i], indent+4, false);
  }

  return result;
}

std::wstring DumpHistoryState(const std::string& history_state, int indent,
                              bool is_current) {
  return DumpHistoryItem(HistoryItemFromString(history_state), indent,
                         is_current);
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
    frame->script()->setEventHandlerLineNumber(0);

#if defined(OS_WIN)
  // Reset the last click information so the clicks generated from previous
  // test aren't inherited (otherwise can mistake single/double/triple clicks)
  WebKit::WebInputEventFactory::resetLastClickState();
#endif
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
}

// NOTE: This pair of conversion functions are here instead of in glue_util.cc
// since that file will eventually die.  This pair of functions will need to
// remain as the concept of a file-path specific character encoding string type
// will most likely not make its way into WebKit.

FilePath::StringType WebStringToFilePathString(const WebString& str) {
#if defined(OS_POSIX)
  return base::SysWideToNativeMB(UTF16ToWideHack(str));
#elif defined(OS_WIN)
  return UTF16ToWideHack(str);
#endif
}

WebString FilePathStringToWebString(const FilePath::StringType& str) {
#if defined(OS_POSIX)
  return WideToUTF16Hack(base::SysNativeMBToWide(str));
#elif defined(OS_WIN)
  return WideToUTF16Hack(str);
#endif
}

std::string GetWebKitVersion() {
  return StringPrintf("%d.%d", WEBKIT_VERSION_MAJOR, WEBKIT_VERSION_MINOR);
}

namespace {

struct UserAgentState {
  UserAgentState()
      : user_agent_requested(false),
        user_agent_is_overridden(false) {
  }

  std::string user_agent;
  std::string mimic_safari_user_agent;
  bool user_agent_requested;
  bool user_agent_is_overridden;
};

Singleton<UserAgentState> g_user_agent;

std::string BuildOSCpuInfo() {
  std::string os_cpu;

#if defined(OS_WIN) || defined(OS_MACOSX)
  int32 os_major_version = 0;
  int32 os_minor_version = 0;
  int32 os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
#else
  // Should work on any Posix system.
  struct utsname unixinfo;
  uname(&unixinfo);

  std::string cputype;
  // special case for biarch systems
  if (strcmp(unixinfo.machine, "x86_64") == 0 &&
      sizeof(void*) == sizeof(int32)) {
    cputype.assign("i686 (x86_64)");
  } else {
    cputype.assign(unixinfo.machine);
  }
#endif

  StringAppendF(
      &os_cpu,
#if defined(OS_WIN)
      "Windows NT %d.%d",
      os_major_version,
      os_minor_version
#elif defined(OS_MACOSX)
      "Intel Mac OS X %d_%d_%d",
      os_major_version,
      os_minor_version,
      os_bugfix_version
#else
      "%s %s",
      unixinfo.sysname, // e.g. Linux
      cputype.c_str()   // e.g. i686
#endif
  );

  return os_cpu;
}

void BuildUserAgent(bool mimic_safari, std::string* result) {
  const char kUserAgentPlatform[] =
#if defined(OS_WIN)
      "Windows";
#elif defined(OS_MACOSX)
      "Macintosh";
#elif defined(OS_LINUX)
      "X11";              // strange, but that's what Firefox uses
#else
      "?";
#endif

  const char kUserAgentSecurity = 'U'; // "US" strength encryption

  // TODO(port): figure out correct locale
  const char kUserAgentLocale[] = "en-US";

  // Get the product name and version, and replace Safari's Version/X string
  // with it.  This is done to expose our product name in a manner that is
  // maximally compatible with Safari, we hope!!
  std::string product;

  if (!mimic_safari) {
    scoped_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfoForCurrentModule());
    if (version_info.get())
      product = "Chrome/" + WideToASCII(version_info->product_version());
  }

  if (product.empty())
    product = "Version/3.2.1";

  // Derived from Safari's UA string.
  StringAppendF(
      result,
      "Mozilla/5.0 (%s; %c; %s; %s) AppleWebKit/%d.%d"
      " (KHTML, like Gecko) %s Safari/%d.%d",
      kUserAgentPlatform,
      kUserAgentSecurity,
      BuildOSCpuInfo().c_str(),
      kUserAgentLocale,
      WEBKIT_VERSION_MAJOR,
      WEBKIT_VERSION_MINOR,
      product.c_str(),
      WEBKIT_VERSION_MAJOR,
      WEBKIT_VERSION_MINOR
      );
}

void SetUserAgentToDefault() {
  BuildUserAgent(false, &g_user_agent->user_agent);
}

}  // namespace

void SetUserAgent(const std::string& new_user_agent) {
  // If you combine this with the previous line, the function only works the
  // first time.
  DCHECK(!g_user_agent->user_agent_requested) <<
      "Setting the user agent after someone has "
      "already requested it can result in unexpected behavior.";
  g_user_agent->user_agent_is_overridden = true;
  g_user_agent->user_agent = new_user_agent;
}

const std::string& GetUserAgent(const GURL& url) {
  if (g_user_agent->user_agent.empty())
    SetUserAgentToDefault();
  g_user_agent->user_agent_requested = true;
  if (!g_user_agent->user_agent_is_overridden) {
    // For hotmail, we need to spoof as Safari (bug 4111).
    if (MatchPattern(url.host(), "*.mail.live.com")) {
      if (g_user_agent->mimic_safari_user_agent.empty())
        BuildUserAgent(true, &g_user_agent->mimic_safari_user_agent);
      return g_user_agent->mimic_safari_user_agent;
    }
  }
  return g_user_agent->user_agent;
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
