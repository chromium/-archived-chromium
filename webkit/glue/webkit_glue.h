// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_H__
#define WEBKIT_GLUE_H__

#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
#include "base/string16.h"
#include "webkit/glue/webplugin.h"

// We do not include the header files for these interfaces since this header
// file is included by code in webkit/port
class SharedCursor;
class WebView;
class WebViewDelegate;
class WebRequest;
class WebFrame;
class WebFrameImpl;
class GURL;
struct _NPNetscapeFuncs;
typedef _NPNetscapeFuncs NPNetscapeFuncs;

#ifdef _WIN32
struct IMLangFontLink2;
#endif

namespace WebCore {

class Document;
class Frame;

}  // namespace WebCore

class SkBitmap;

namespace webkit_glue {

//-----------------------------------------------------------------------------
// Functions implemented by JS engines.
void SetJavaScriptFlags(const std::wstring& flags);
void SetRecordPlaybackMode(bool value);

//-----------------------------------------------------------------------------
// Functions implemented by WebKit, called by the embedder:

// Turns on "layout test" mode, which tries to mimic the font and widget sizing
// of the Mac DumpRenderTree.
void SetLayoutTestMode(bool enable);
bool IsLayoutTestMode();

#if defined(OS_WIN)
// Returns the com object pointer for the FontLink interface. This is the
// default way to do this operation. It can be called directly from
// GetLangFontLink.
IMLangFontLink2* GetLangFontLinkHelper();

// Returns the monitor information corresponding to the window.
// This is the default implementation.
MONITORINFOEX GetMonitorInfoForWindowHelper(HWND window);
#endif

// Returns the text of the document element.
std::wstring DumpDocumentText(WebFrame* web_frame);

// Returns the text of the document element and optionally its child frames.
// If recursive is false, this is equivalent to DumpDocumentText followed by
// a newline.  If recursive is true, it recursively dumps all frames as text.
std::wstring DumpFramesAsText(WebFrame* web_frame, bool recursive);

// Returns the renderer's description of its tree (its externalRepresentation).
std::wstring DumpRenderer(WebFrame* web_frame);

// Returns a dump of the scroll position of the webframe.
std::wstring DumpFrameScrollPosition(WebFrame* web_frame, bool recursive);

// Returns a representation of the back/forward list.
void DumpBackForwardList(WebView* view, void* previous_history_item,
                         std::wstring* result);

// Cleans up state left over from the previous test run.
void ResetBeforeTestRun(WebView* view);

// Returns the WebKit version (major.minor).
std::string GetWebKitVersion();

// Called to override the default user agent with a custom one.  Call this
// before anyone actually asks for the user agent in order to prevent
// inconsistent behavior.
void SetUserAgent(const std::string& new_user_agent);

// Returns the user agent, which is usually the default user agent but may be
// overriden by a call to SetUserAgent() (which should be done at startup).
const std::string& GetUserAgent();

// Creates serialized state for the specified URL. This is a variant of
// HistoryItemToString (in glue_serialize) that is used during session restore
// if the saved state is empty.
std::string CreateHistoryStateForURL(const GURL& url);

#ifndef NDEBUG
// Checks various important objects to see if there are any in memory, and
// calls AppendToLog with any leaked objects. Designed to be called on shutdown
void CheckForLeaks();
#endif

// Decodes the image from the data in |image_data| into |image|.
// Returns false if the image could not be decoded.
bool DecodeImage(const std::string& image_data, SkBitmap* image);

//-----------------------------------------------------------------------------
// Functions implemented by the embedder, called by WebKit:

// This function is called to request a prefetch of the DNS resolution for the
// provided hostname.
void PrefetchDns(const std::string& hostname);

// This function is called to request a prefetch of the entire URL, loading it
// into our cache for (expected) future needs.  The given URL may NOT be in 
// canonical form and it will NOT be null-terminated; use the length instead.
void PrecacheUrl(const char16* url, int url_length);

// This function is called to add a line to the application's log file.
void AppendToLog(const char* filename, int line, const char* message);

// Get the mime type (if any) that is associated with the given file extension.
// Returns true if a corresponding mime type exists.
bool GetMimeTypeFromExtension(const std::wstring& ext, std::string* mime_type);

// Get the mime type (if any) that is associated with the given file.  
// Returns true if a corresponding mime type exists.
bool GetMimeTypeFromFile(const std::wstring& file_path, std::string* mime_type);

// Get the preferred extension (if any) associated with the given mime type.
// Returns true if a corresponding file extension exists.
bool GetPreferredExtensionForMimeType(const std::string& mime_type,
                                      std::wstring* ext);

#ifdef _WIN32
// Returns the com object pointer for the FontLink interface
IMLangFontLink2* GetLangFontLink();
#endif

// Sets a cookie string for the given URL.  The policy_url argument indicates
// the URL of the topmost frame, which may be useful for determining whether or
// not to allow this cookie setting.  NOTE: the cookie string is a standard
// cookie string of the form "name=value; option1=x; option2=y"
void SetCookie(const GURL& url, const GURL& policy_url,
               const std::string& cookie);

// Returns all cookies in the form "a=1; b=2; c=3" for the given URL.  NOTE:
// this string should not include any options that may have been specified when
// the cookie was set.  Semicolons delimit individual cookies in this context.
std::string GetCookies(const GURL& url, const GURL& policy_url);

// Gather usage statistics from the in-memory cache and inform our host, if
// applicable.
void NotifyCacheStats();

// Glue to get resources from the embedder.

// Gets a localized string given a message id.  Returns an empty string if the
// message id is not found.
std::wstring GetLocalizedString(int message_id);

// Returns the raw data for a resource.  This resource must have been
// specified as BINDATA in the relevant .rc file.
std::string GetDataResource(int resource_id);

// Returns an SkBitmap for a resource.  This resource must have been
// specified as BINDATA in the relevant .rc file.
SkBitmap* GetBitmapResource(int resource_id);

#ifdef _WIN32
// Loads and returns a cursor.
HCURSOR LoadCursor(int cursor_id);
#endif

// Glue to access the clipboard.

// Clear the clipboard.  It is usually a good idea to clear the clipboard
// before writing content to the clipboard.
void ClipboardClear();

// Adds UNICODE and ASCII text to the clipboard.
void ClipboardWriteText(const std::wstring& text);

// Adds HTML to the clipboard.  The url parameter is optional, but especially
// useful if the HTML fragment contains relative links
void ClipboardWriteHTML(const std::wstring& html, const GURL& url);

// Adds a bookmark to the clipboard
void ClipboardWriteBookmark(const std::wstring& title, const GURL& url);

// Adds a bitmap to the clipboard
void ClipboardWriteBitmap(const SkBitmap& bitmap);

// Used by WebKit to determine whether WebKit wrote the clipboard last
void ClipboardWriteWebSmartPaste();

// Tests whether the clipboard contains a certain format
bool ClipboardIsFormatAvailable(unsigned int format);

// Reads UNICODE text from the clipboard, if available.
void ClipboardReadText(std::wstring* result);

// Reads ASCII text from the clipboard, if available.
void ClipboardReadAsciiText(std::string* result);

// Reads HTML from the clipboard, if available.
void ClipboardReadHTML(std::wstring* markup, GURL* url);

// Gets the directory where the application data and libraries exist.  This
// may be a versioned subdirectory, or it may be the same directory as the
// GetExeDirectory(), depending on the embedder's implementation.
// Path is an output parameter to receive the path.
// Returns true if successful, false otherwise.
bool GetApplicationDirectory(std::wstring* path);

// Gets the URL where the inspector's HTML file resides. It must use the
// protocol returned by GetUIResourceProtocol.
GURL GetInspectorURL();

// Gets the protocol that is used for all user interface resources, including
// the Inspector. It must end with "-resource".
std::string GetUIResourceProtocol();

// Gets the directory where the launching executable resides on disk.
// Path is an output parameter to receive the path.
// Returns true if successful, false otherwise.
bool GetExeDirectory(std::wstring* path);

// Embedders implement this function to return the list of plugins to Webkit.
bool GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins);

// Returns true if the plugins run in the same process as the renderer, and
// false otherwise.
bool IsPluginRunningInRendererProcess();

#ifdef _WIN32
// Asks the browser to load the font.
bool EnsureFontLoaded(HFONT font);

// Returns the monitor information corresponding to the window.
MONITORINFOEX GetMonitorInfoForWindow(HWND window);
#endif

// Functions implemented by webkit_glue for WebKit ----------------------------

// Notifies the embedder that a form element value has changed. The document
// pointer, which MAY BE NULL, indicates the document that owns the form
// element that changed, if any.
void NotifyFormStateChanged(const WebCore::Document* document);

// Returns a bool indicating if the Null plugin should be enabled or not.
bool IsDefaultPluginEnabled();

#ifdef _WIN32
// Downloads the file specified by the URL. On sucess a WM_COPYDATA message
// will be sent to the caller_window.
bool DownloadUrl(const std::string& url, HWND caller_window);
#endif

// Returns the plugin finder URL.
bool GetPluginFinderURL(std::string* plugin_finder_url);

// Returns the locale that this instance of webkit is running as.  This is of
// the form language-country (e.g., en-US or pt-BR).
std::wstring GetWebKitLocale();

// Notifies the browser that the current page runs out of JS memory.
void NotifyJSOutOfMemory(WebCore::Frame* frame);

} // namespace webkit_glue

#endif  // WEBKIT_GLUE_H__
