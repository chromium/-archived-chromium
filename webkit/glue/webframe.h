// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBFRAME_H_
#define WEBKIT_GLUE_WEBFRAME_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "skia/ext/bitmap_platform_device.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/api/public/WebCanvas.h"
#include "webkit/api/public/WebURL.h"

class GURL;
class WebView;
class WebTextInput;
struct NPObject;

namespace WebKit {
class WebData;
class WebDataSource;
class WebForm;
class WebHistoryItem;
class WebString;
class WebURLRequest;
struct WebConsoleMessage;
struct WebFindOptions;
struct WebRect;
struct WebScriptSource;
struct WebSize;
struct WebURLError;
}

#if WEBKIT_USING_V8
namespace v8 {
  template <class T> class Local;
  class Context;
}
#endif

// Every frame in a web page is represented by one WebFrame, including the
// outermost frame.
class WebFrame {
 public:
  WebFrame() {}

  // The two functions below retrieve WebFrame instances relating the currently
  // executing JavaScript. Since JavaScript can make function calls across
  // frames, though, we need to be more precise.
  //
  // For example, imagine that a JS function in frame A calls a function in
  // frame B, which calls native code, which wants to know what the 'active'
  // frame is.
  //
  // The 'entered context' is the context where execution first entered the
  // script engine; the context that is at the bottom of the JS function stack.
  // RetrieveFrameForEnteredContext() would return Frame A in our example.
  //
  // The 'current context' is the context the JS engine is currently inside of;
  // the context that is at the top of the JS function stack.
  // RetrieveFrameForCurrentContext() would return Frame B in our example.
  static WebFrame* RetrieveFrameForEnteredContext();
  static WebFrame* RetrieveFrameForCurrentContext();

  // Binds a C++ class to a JavaScript property of the window object.  This
  // should generally be used via CppBoundClass::BindToJavascript() instead of
  // calling it directly.
  virtual void BindToWindowObject(const std::wstring& name,
                                  NPObject* object) = 0;

  virtual void CallJSGC() = 0;

  // This grants the currently loaded Document access to all security origins
  // (including file URLs).  Use with care.  The access is revoked when a new
  // document is loaded into this frame.
  virtual void GrantUniversalAccess() = 0;

  virtual NPObject* GetWindowNPObject() = 0;

#if WEBKIT_USING_V8
  // Returns the V8 context for this frame, or an empty handle if there is
  // none.
  virtual v8::Local<v8::Context> GetScriptContext() = 0;
#endif

  // Reload the current document.
  virtual void Reload() = 0;

  // Loads the given WebURLRequest.
  virtual void LoadRequest(const WebKit::WebURLRequest& request) = 0;

  // Loads the given WebHistoryItem.  This corresponds to a back/forward
  // navigation.
  virtual void LoadHistoryItem(const WebKit::WebHistoryItem& item) = 0;

  // Loads the given data with specific mime type and optional text encoding.
  // For HTML data, base_url indicates the security origin of the document and
  // is used to resolve links.  If specified, unreachable_url is reported via
  // WebDataSource::unreachableURL.  If replace is false, then this data will
  // be loaded as a normal navigation.  Otherwise, the current history item
  // will be replaced.
  virtual void LoadData(
      const WebKit::WebData& data,
      const WebKit::WebString& mime_type,
      const WebKit::WebString& text_encoding,
      const WebKit::WebURL& base_url,
      const WebKit::WebURL& unreachable_url = WebKit::WebURL(),
      bool replace = false) = 0;

  // This method is short-hand for calling LoadData, where mime_type is
  // "text/html" and text_encoding is "UTF-8".
  virtual void LoadHTMLString(
      const WebKit::WebData& html,
      const WebKit::WebURL& base_url,
      const WebKit::WebURL& unreachable_url = WebKit::WebURL(),
      bool replace = false) = 0;

  // Asks the WebFrame to try and download the alternate error page.  We notify
  // the WebViewDelegate of the results so it can decide whether or not to show
  // something to the user (e.g., a local error page or the alternate error
  // page).
  virtual void LoadAlternateHTMLErrorPage(const WebKit::WebURLRequest& request,
                                          const WebKit::WebURLError& error,
                                          const GURL& error_page_url,
                                          bool replace,
                                          const GURL& fake_url) = 0;

  // Executes JavaScript in the web frame.
  virtual void ExecuteScript(const WebKit::WebScriptSource& source) = 0;

  // Executes JavaScript in a new context associated with the web frame. The
  // script gets its own global scope and its own prototypes for intrinsic
  // JavaScript objects (String, Array, and so-on). It shares the wrappers for
  // all DOM nodes and DOM constructors.
  virtual void ExecuteScriptInNewContext(
      const WebKit::WebScriptSource* sources, int num_sources) = 0;

  // Inserts the given CSS styles at the beginning of the document.
  virtual bool InsertCSSStyles(const std::string& css) = 0;

  // Returns the WebHistoryItem representing the state of the previous page
  // load for later use when loading. The previous page is the page that was
  // loaded before DidCommitLoadForFrame was received.
  //
  // Returns a null item if there is no valid state to return (for example,
  // there is no previous item). Returns true if the previous item's state was
  // retrieved, even if that state may be empty.
  virtual WebKit::WebHistoryItem GetPreviousHistoryItem() const = 0;

  // Returns the WebHistoryItem representing the state of the current page load
  // for later use when loading.
  //
  // Returns a null item if there is no valid state to return (for example,
  // there is no previous item). Returns true if the current item's state was
  // retrieved, even if that state may be empty.
  virtual WebKit::WebHistoryItem GetCurrentHistoryItem() const = 0;

  // Returns the current URL of the frame, or an empty GURL if there is no
  // URL to retrieve (for example, the frame may never have had any content).
  virtual GURL GetURL() const = 0;

  // Returns the URL to the favorite icon for the frame. An empty GURL is
  // returned if the frame has not finished loading, or the frame's URL
  // protocol is not http or https.
  virtual GURL GetFavIconURL() const = 0;

  // Returns the URL to the OpenSearch description document for the frame. If
  // the page does not have a valid document, an empty GURL is returned.
  virtual GURL GetOSDDURL() const = 0;

  // Return the minPrefWidth of the content contained in the current Document
  virtual int GetContentsPreferredWidth() const = 0;

  // Returns the committed data source, which is the last data source that has
  // successfully started loading. Will return NULL if no provisional data
  // has been committed.
  virtual WebKit::WebDataSource* GetDataSource() const = 0;

  // Returns the provisional data source, which is a data source where a
  // request has been made, but we are not sure if we will use data from it
  // (for example, it may be an invalid URL). When the provisional load is
  // "committed," it will become the "real" data source (see GetDataSource
  // above) and the provisional data source will be NULL.
  virtual WebKit::WebDataSource* GetProvisionalDataSource() const = 0;

  //
  //  @method stopLoading
  //  @discussion Stop any pending loads on the frame's data source,
  //  and its children.
  //  - (void)stopLoading;
  virtual void StopLoading() = 0;

  // Returns the frame that opened this frame, or NULL if this window has no
  // opener.
  virtual WebFrame* GetOpener() const = 0;

  // Returns the frame containing this frame, or NULL of this is a top level
  // frame with no parent.
  virtual WebFrame* GetParent() const = 0;

  // Returns the top-most frame in the frame hierarchy containing this frame.
  virtual WebFrame* GetTop() const = 0;

  // Returns the child frame with the given xpath.
  // The document of this frame is used as the context node.
  // The xpath may need a recursive traversal if non-trivial
  // A non-trivial xpath will contain a combination of xpaths
  // (delimited by '\n') leading to an inner subframe.
  //
  // Example: /html/body/iframe/\n/html/body/div/iframe/\n/frameset/frame[0]
  // can be broken into 3 xpaths
  // /html/body/iframe evaluates to an iframe within the root frame
  // /html/body/div/iframe evaluates to an iframe within the level-1 iframe
  // /frameset/frame[0] evaluates to first frame within the level-2 iframe
  virtual WebFrame* GetChildFrame(const std::wstring& xpath) const = 0;

  // Returns a pointer to the WebView that contains this WebFrame.  This
  // pointer is not AddRef'd and is only valid for the lifetime of the WebFrame
  // unless it is AddRef'd separately by the caller.
  virtual WebView* GetView() const = 0;

  // Returns a vector of WebForms (corresponds to document.forms).
  virtual void GetForms(std::vector<WebKit::WebForm>* forms) const = 0;

  // Returns the serialization of the frame's security origin.
  virtual std::string GetSecurityOrigin() const = 0;

  // Fills the contents of this frame into the given string. If the text is
  // longer than max_chars, it will be clipped to that length. Warning: this
  // function may be slow depending on the number of characters retrieved and
  // page complexity. For a typically sized page, expect it to take on the
  // order of milliseconds.
  //
  // If there is room, subframe text will be recursively appended. Each frame
  // will be separated by an empty line.
  virtual void GetContentAsPlainText(int max_chars,
                                     std::wstring* text) const = 0;

  // Searches a frame for a given string.
  //
  // If a match is found, this function will select it (scrolling down to make
  // it visible if needed) and fill in the IntRect (selection_rect) with the
  // location of where the match was found (in screen coordinates).
  //
  // If no match is found, this function clears all tickmarks and highlighting.
  //
  // Returns true if the search string was found, false otherwise.
  virtual bool Find(int request_id,
                    const string16& search_text,
                    const WebKit::WebFindOptions& options,
                    bool wrap_within_frame,
                    WebKit::WebRect* selection_rect) = 0;

  // Notifies the frame that we are no longer interested in searching. This will
  // abort any asynchronous scoping effort already under way (see the function
  // ScopeStringMatches for details) and erase all tick-marks and highlighting
  // from the previous search. If |clear_selection| is true, it will also make
  // sure the end state for the Find operation does not leave a selection.
  // This can occur when the user clears the search string but does not close
  // the find box.
  virtual void StopFinding(bool clear_selection) = 0;

  // Counts how many times a particular string occurs within the frame. It
  // also retrieves the location of the string and updates a vector in the frame
  // so that tick-marks and highlighting can be drawn. This function does its
  // work asynchronously, by running for a certain time-slice and then
  // scheduling itself (co-operative multitasking) to be invoked later
  // (repeating the process until all matches have been found). This allows
  // multiple frames to be searched at the same time and provides a way to
  // cancel at any time (see CancelPendingScopingEffort). The parameter Request
  // specifies what to look for and Reset signals whether this is a brand new
  // request or a continuation of the last scoping effort.
  virtual void ScopeStringMatches(int request_id,
                                  const string16& search_text,
                                  const WebKit::WebFindOptions& options,
                                  bool reset) = 0;

  // Cancels any outstanding requests for scoping string matches on a frame.
  virtual void CancelPendingScopingEffort() = 0;

  // This function is called on the mainframe during the scoping effort to keep
  // a running tally of the accumulated total match-count for all frames. After
  // updating the count it will notify the render-view about the new count.
  virtual void IncreaseMatchCount(int count, int request_id) = 0;

  // Notifies the webview-delegate about a new selection rect. This will result
  // in the browser getting notified. For more information see WebViewDelegate.
  virtual void ReportFindInPageSelection(const WebKit::WebRect& selection_rect,
                                         int active_match_ordinal,
                                         int request_id) = 0;

  // This function is called on the mainframe to reset the total number of
  // matches found during the scoping effort.
  virtual void ResetMatchCount() = 0;

  // Returns true if the frame is visible (defined as width > 0 and height > 0).
  virtual bool Visible() = 0;

  // Selects all the text in the frame.
  virtual void SelectAll() = 0;

  //
  //  - (void)copy:(id)sender;
  virtual void Copy() = 0;

  //
  //  - (void)cut:(id)sender;
  virtual void Cut() = 0;

  //
  //  - (void)paste:(id)sender;
  virtual void Paste() = 0;

  // Replace the selection text by a given text.
  virtual void Replace(const std::wstring& text) = 0;

  // Toggle spell check on and off.
  virtual void ToggleSpellCheck() = 0;

  // Return whether spell check is enabled or not in this frame.
  virtual bool SpellCheckEnabled() = 0;

  //
  //  - (void)delete:(id)sender;
  // Delete as in similar to Cut, not as in teardown
  virtual void Delete() = 0;

  // Undo the last text editing command.
  virtual void Undo() = 0;

  // Redo the last undone text editing command.
  virtual void Redo() = 0;

  // Clear any text selection in the frame.
  virtual void ClearSelection() = 0;

  // Checks if there is currently a selected area (indicates that GetSelection
  // would return a non-empty string).
  virtual bool HasSelection() = 0;

  // Returns the selected text if there is any.  If |as_html| is true, returns
  // the selection as HTML.  The return value is encoded in utf-8.
  virtual std::string GetSelection(bool as_html) = 0;

  // Returns the full HTML of the page.
  virtual std::string GetFullPageHtml() = 0;

  // This function sets a flag within WebKit to instruct it to render the page
  // as View-Source (showing the HTML source for the page).
  virtual void SetInViewSourceMode(bool enable) = 0;

  // This function returns whether this frame is in "view-source" mode.
  virtual bool GetInViewSourceMode() const = 0;

  // Returns the frame name.
  virtual std::wstring GetName() = 0;

  // Returns a pointer to the WebTextInput object associated with the frame.
  // The caller does not own the object returned.
  virtual WebTextInput* GetTextInput() = 0;

  // Executes a webkit editor command. The supported commands are a
  // superset of those accepted by javascript:document.execCommand().
  // This method is exposed in order to implement
  // javascript:layoutTestController.execCommand()
  virtual bool ExecuteEditCommandByName(const std::string& name,
                                        const std::string& value) = 0;

  // Checks whether a webkit editor command is currently enabled. This
  // method is exposed in order to implement
  // javascript:layoutTestController.isCommandEnabled()
  virtual bool IsEditCommandEnabled(const std::string& name) = 0;

  // Adds a message to the frame's console.
  virtual void AddMessageToConsole(const WebKit::WebConsoleMessage&) = 0;

  // Tells the current page to close, running the onunload handler.
  // TODO(creis): We'd rather use WebView::Close(), but that sets its delegate_
  // to NULL, preventing any JavaScript dialogs in the onunload handler from
  // appearing.  This lets us shortcut that for now, but we should refactor
  // close messages so that this isn't necessary.
  virtual void ClosePage() = 0;

  // The current scroll offset from the top of frame in pixels.
  virtual WebKit::WebSize ScrollOffset() const = 0;

  // Reformats the WebFrame for printing.  page_size is the page size in
  // pixels.  Returns the number of pages that can be printed at the given page
  // size.
  virtual int PrintBegin(const WebKit::WebSize& page_size) = 0;

  // Prints one page, and returns the calculated page shrinking factor (usually
  // between 1/1.25 and 1/2).  Returns 0 if the page number is invalid or not
  // in printing mode.
  virtual float PrintPage(int page_to_print, WebKit::WebCanvas* canvas) = 0;

  // Reformats the WebFrame for screen display.
  virtual void PrintEnd() = 0;

  // Only for test_shell
  virtual int PendingFrameUnloadEventCount() const = 0;

 protected:
  virtual ~WebFrame() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebFrame);
};

#endif  // WEBKIT_GLUE_WEBFRAME_H_
