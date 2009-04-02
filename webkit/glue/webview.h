// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBVIEW_H__
#define WEBKIT_GLUE_WEBVIEW_H__

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "webkit/glue/webwidget.h"

struct WebDropData;
struct WebPreferences;
class GURL;
class WebDevToolsAgent;
class WebFrame;
class WebViewDelegate;

//
//  @class WebView
//  WebView manages the interaction between WebFrameViews and WebDataSources.
//  Modification of the policies and behavior of the WebKit is largely managed
//  by WebViews and their delegates.
//
//  Typical usage:
//
//  WebView *webView;
//  WebFrame *mainFrame;
//
//  webView  = [[WebView alloc] initWithFrame: NSMakeRect (0,0,640,480)];
//  mainFrame = [webView mainFrame];
//  [mainFrame loadRequest:request];
//
//  WebViews have a WebViewDelegate that the embedding application implements
//  that are required for tasks like opening new windows and controlling the
//  user interface elements in those windows, monitoring the progress of loads,
//  monitoring URL changes, and making determinations about how content of
//  certain types should be handled.
class WebView : public WebWidget {
 public:
  WebView() {}
  virtual ~WebView() {}

  // This method creates a WebView that is initially sized to an empty rect.
  static WebView* Create(WebViewDelegate* delegate,
                         const WebPreferences& prefs);

  // Returns the delegate for this WebView.  This is the pointer that was
  // passed to WebView::Create. The caller must check this value before using
  // it, it will be NULL during closing of the view.
  virtual WebViewDelegate* GetDelegate() = 0;

  // Changes the delegate for this WebView.  It is valid to set this to NULL.
  virtual void SetDelegate(WebViewDelegate* delegate) = 0;

  // Instructs the EditorClient whether to pass editing notifications on to a
  // delegate, if one is present.  This allows embedders that haven't
  // overridden any editor delegate methods to avoid the performance impact of
  // calling them.
  virtual void SetUseEditorDelegate(bool value) = 0;

  // Method that controls whether pressing Tab key cycles through page elements
  // or inserts a '\t' char in text area
  virtual void SetTabKeyCyclesThroughElements(bool value) = 0;

  // Returns whether the current view can be closed, after running any
  // onbeforeunload event handlers.
  virtual bool ShouldClose() = 0;

  //
  //  @method mainFrame
  //  @abstract Return the top level frame.
  //  @discussion Note that even document that are not framesets will have a
  //  mainFrame.
  //  @result The main frame.
  //  - (WebFrame *)mainFrame;
  virtual WebFrame* GetMainFrame() = 0;

  // Returns the currently focused frame.
  virtual WebFrame* GetFocusedFrame() = 0;

  // Sets focus to the frame passed in.
  virtual void SetFocusedFrame(WebFrame* frame) = 0;

  // Returns the frame with the given name, or NULL if not found.
  virtual WebFrame* GetFrameWithName(const std::wstring& name) = 0;

  // Returns the frame previous to the specified frame, by traversing the frame
  // tree, wrapping around if necessary.
  virtual WebFrame* GetPreviousFrameBefore(WebFrame* frame, bool wrap) = 0;

  // Returns the frame after to the specified frame, by traversing the frame
  // tree, wrapping around if necessary.
  virtual WebFrame* GetNextFrameAfter(WebFrame* frame, bool wrap) = 0;

  // ---- TODO(darin): remove from here ----

  //
  //  - (IBAction)stopLoading:(id)sender;
  virtual void StopLoading() = 0;

  // Sets the maximum size to allow WebCore's internal B/F list to grow to.
  // If not called, the list will have the default capacity specified in
  // BackForwardList.cpp.
  virtual void SetBackForwardListSize(int size) = 0;

  // ---- TODO(darin): remove to here ----

  // Restores focus to the previously focused element.
  // This method is invoked when the webview is shown after being
  // hidden, and focus is to be restored. When WebView loses focus, it remembers
  // the frame/element that had focus, so that when this method is invoked
  // focus is then restored.
  virtual void RestoreFocus() = 0;

  // Focus the first (last if reverse is true) focusable node.
  virtual void SetInitialFocus(bool reverse) = 0;

  // Stores the focused node and clears it if |frame| is the focused frame.
  // TODO(jcampan): http://b/issue?id=1157486 this is needed to work-around
  // issues caused by the fix for bug #792423 and should be removed when that
  // bug is fixed.
  virtual void StoreFocusForFrame(WebFrame* frame) = 0;

  // Requests the webview to download an image. When done, the delegate is
  // notified by way of DidDownloadImage. Returns true if the request was
  // successfully started, false otherwise. id is used to uniquely identify the
  // request and passed back to the DidDownloadImage method. If the image has
  // multiple frames, the frame whose size is image_size is returned. If the
  // image doesn't have a frame at the specified size, the first is returned.
  virtual bool DownloadImage(int id, const GURL& image_url, int image_size) = 0;

  // Replace the standard setting for the WebView with |preferences|.
  virtual void SetPreferences(const WebPreferences& preferences) = 0;
  virtual const WebPreferences& GetPreferences() = 0;

  // Set the encoding of the current main frame. The value comes from
  // the encoding menu. WebKit uses the function named
  // SetCustomTextEncodingName to do override encoding job.
  virtual void SetPageEncoding(const std::wstring& encoding_name) = 0;

  // Return the canonical encoding name of current main webframe in webview.
  virtual std::wstring GetMainFrameEncodingName() = 0;

  // Change the text zoom level. It will make the zoom level 20% larger or
  // smaller. If text_only is set, the text size will be changed. When unset,
  // the entire page's zoom factor will be changed.
  //
  // You can only have either text zoom or full page zoom at one time. Changing
  // the mode will change things in weird ways. Generally the app should only
  // support text zoom or full page zoom, and not both.
  //
  // ResetZoom will reset both full page and text zoom.
  virtual void ZoomIn(bool text_only) = 0;
  virtual void ZoomOut(bool text_only) = 0;
  virtual void ResetZoom() = 0;

  // Insert text into the current editor.
  virtual void InsertText(const string16& text) = 0;

  // Copy to the clipboard the image located at a particular point in the
  // WebView (if there is such an image)
  virtual void CopyImageAt(int x, int y) = 0;

  // Inspect a particular point in the WebView. (x = -1 || y = -1) is a special
  // case which means inspect the current page and not a specific point.
  virtual void InspectElement(int x, int y) = 0;

  // Show the JavaScript console.
  virtual void ShowJavaScriptConsole() = 0;

  // Notifies the webview that a drag has terminated.
  virtual void DragSourceEndedAt(
      int client_x, int client_y, int screen_x, int screen_y) = 0;

  // Notifies the webview that a drag and drop operation is in progress, with
  // dropable items over the view.
  virtual void DragSourceMovedTo(
      int client_x, int client_y, int screen_x, int screen_y) = 0;

  // Notfies the webview that the system drag and drop operation has ended.
  virtual void DragSourceSystemDragEnded() = 0;

  // Callback methods when a drag and drop operation is trying to drop
  // something on the renderer.
  virtual bool DragTargetDragEnter(const WebDropData& drop_data,
      int client_x, int client_y, int screen_x, int screen_y) = 0;
  virtual bool DragTargetDragOver(
      int client_x, int client_y, int screen_x, int screen_y) = 0;
  virtual void DragTargetDragLeave() = 0;
  virtual void DragTargetDrop(
      int client_x, int client_y, int screen_x, int screen_y) = 0;
  virtual int32 GetDragIdentity() = 0;

  // Notifies the webview that autofill suggestions are available for a node.
  virtual void AutofillSuggestionsForNode(
      int64 node_id,
      const std::vector<std::wstring>& suggestions,
      int default_suggestion_index) = 0;

  // Hides the autofill popup if any are showing.
  virtual void HideAutofillPopup() = 0;

  // Returns development tools agent instance belonging to this view.
  virtual WebDevToolsAgent* GetWebDevToolsAgent() = 0;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WebView);
};

#endif  // WEBKIT_GLUE_WEBVIEW_H__
