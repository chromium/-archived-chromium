// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBVIEW_H_
#define WEBKIT_GLUE_WEBVIEW_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "webkit/glue/webwidget.h"

namespace WebKit {
class WebDragData;
struct WebPoint;
}

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

  // Tells all Page instances of this view to update the visited link state for
  // the specified hash.
  static void UpdateVisitedLinkState(uint64 link_hash);

  // Tells all Page instances of this view to update visited state for all their
  // links.
  static void ResetVisitedLinkState();

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

  // Focus the first (last if reverse is true) focusable node.
  virtual void SetInitialFocus(bool reverse) = 0;

  // Clears the focused node (and selection if a text field is focused) to
  // ensure that a text field on the page is not eating keystrokes we send it.
  virtual void ClearFocusedNode() = 0;

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

  // Copy to the clipboard the image located at a particular point in the
  // WebView (if there is such an image)
  virtual void CopyImageAt(int x, int y) = 0;

  // Inspect a particular point in the WebView. (x = -1 || y = -1) is a special
  // case which means inspect the current page and not a specific point.
  virtual void InspectElement(int x, int y) = 0;

  // Show the JavaScript console.
  virtual void ShowJavaScriptConsole() = 0;

  // Notifies the webview that a drag has been cancelled.
  virtual void DragSourceCancelledAt(
      const WebKit::WebPoint& client_point,
      const WebKit::WebPoint& screen_point) = 0;

  // Notifies the webview that a drag has terminated.
  virtual void DragSourceEndedAt(
      const WebKit::WebPoint& client_point,
      const WebKit::WebPoint& screen_point) = 0;

  // Notifies the webview that a drag and drop operation is in progress, with
  // dropable items over the view.
  virtual void DragSourceMovedTo(
      const WebKit::WebPoint& client_point,
      const WebKit::WebPoint& screen_point) = 0;

  // Notfies the webview that the system drag and drop operation has ended.
  virtual void DragSourceSystemDragEnded() = 0;

  // Callback methods when a drag and drop operation is trying to drop data
  // on this webview.
  virtual bool DragTargetDragEnter(
      const WebKit::WebDragData& drag_data, int identity,
      const WebKit::WebPoint& client_point,
      const WebKit::WebPoint& screen_point) = 0;
  virtual bool DragTargetDragOver(
      const WebKit::WebPoint& client_point,
      const WebKit::WebPoint& screen_point) = 0;
  virtual void DragTargetDragLeave() = 0;
  virtual void DragTargetDrop(
      const WebKit::WebPoint& client_point,
      const WebKit::WebPoint& screen_point) = 0;

  // Helper method for drag and drop target operations: return the drag data
  // identity.
  virtual int32 GetDragIdentity() = 0;

  // Helper method for drag and drop target operations: override the default
  // drop effect with either a "copy" (accept true) or "none" (accept false)
  // effect.  Return true on success.
  virtual bool SetDropEffect(bool accept) = 0;

  // Notifies the webview that autofill suggestions are available for a node.
  virtual void AutofillSuggestionsForNode(
      int64 node_id,
      const std::vector<std::wstring>& suggestions,
      int default_suggestion_index) = 0;

  // Hides the autofill popup if any are showing.
  virtual void HideAutofillPopup() = 0;

  // Returns development tools agent instance belonging to this view.
  virtual WebDevToolsAgent* GetWebDevToolsAgent() = 0;

  // Makes the webview transparent. Useful if you want to have some custom
  // background behind it.
  virtual void SetIsTransparent(bool is_transparent) = 0;
  virtual bool GetIsTransparent() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebView);
};

#endif  // WEBKIT_GLUE_WEBVIEW_H_
