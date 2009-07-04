// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/extensions/extension_view.h"
#endif

class Browser;
class Extension;
class ExtensionProcessManager;
class RenderProcessHost;
class RenderWidgetHost;
class RenderWidgetHostView;
class TabContents;
struct WebPreferences;

// This class is the browser component of an extension component's RenderView.
// It handles setting up the renderer process, if needed, with special
// privileges available to extensions.  It may have a view to be shown in the
// in the browser UI, or it may be hidden.
class ExtensionHost : public RenderViewHostDelegate,
                      public RenderViewHostDelegate::View,
                      public ExtensionFunctionDispatcher::Delegate {
 public:
  // Enable DOM automation in created render view hosts.
  static void EnableDOMAutomation() { enable_dom_automation_ = true; }

  ExtensionHost(Extension* extension, SiteInstance* site_instance,
                const GURL& url);
  ~ExtensionHost();

#if defined(TOOLKIT_VIEWS)
  void set_view(ExtensionView* view) { view_.reset(view); }
  ExtensionView* view() const { return view_.get(); }
#endif

  // Create an ExtensionView and tie it to this host and |browser|.
  void CreateView(Browser* browser);

  Extension* extension() { return extension_; }
  RenderViewHost* render_view_host() const { return render_view_host_; }
  RenderProcessHost* render_process_host() const;
  SiteInstance* site_instance() const;
  bool did_stop_loading() const { return did_stop_loading_; }

  // Returns true if the render view is initialized and didn't crash.
  bool IsRenderViewLive() const;

  // Initializes our RenderViewHost by creating its RenderView and navigating
  // to this host's url.  Uses host_view for the RenderViewHost's view (can be
  // NULL).
  void CreateRenderView(RenderWidgetHostView* host_view);

  // Restarts extension's renderer process. Can be called only if the renderer
  // process crashed.
  void RecoverCrashedExtension();

  // RenderViewHostDelegate
  virtual const GURL& GetURL() const { return url_; }
  virtual void RenderViewCreated(RenderViewHost* render_view_host);
  virtual void RenderViewGone(RenderViewHost* render_view_host);
  virtual WebPreferences GetWebkitPrefs();
  virtual void RunJavaScriptMessage(
      const std::wstring& message,
      const std::wstring& default_prompt,
      const GURL& frame_url,
      const int flags,
      IPC::Message* reply_msg,
      bool* did_suppress_message);
  virtual void DidStopLoading(RenderViewHost* render_view_host);
  virtual RenderViewHostDelegate::View* GetViewDelegate() const;
  virtual void DidInsertCSS();
  virtual void ProcessDOMUIMessage(const std::string& message,
                                   const std::string& content,
                                   int request_id,
                                   bool has_callback);

  // RenderViewHostDelegate::View
  virtual void CreateNewWindow(int route_id,
                               base::WaitableEvent* modal_dialog_event);
  virtual void CreateNewWidget(int route_id, bool activatable);
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture,
                                 const GURL& creator_url);
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos);
  virtual void ShowContextMenu(const ContextMenuParams& params);
  virtual void StartDragging(const WebDropData& drop_data);
  virtual void UpdateDragCursor(bool is_drop_target);
  virtual void GotFocus();
  virtual void TakeFocus(bool reverse);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);
  virtual void HandleMouseEvent();
  virtual void HandleMouseLeave();
  virtual void UpdatePreferredWidth(int pref_width);

 private:
  // Whether to allow DOM automation for created RenderViewHosts. This is used
  // for testing.
  static bool enable_dom_automation_;

  // ExtensionFunctionDispatcher::Delegate
  // If this ExtensionHost has a view, this returns the Browser that view is a
  // part of.  If this is a global background page, we use the active Browser
  // instead.
  virtual Browser* GetBrowser();

  // The extension that we're hosting in this view.
  Extension* extension_;

  // The profile that this host is tied to.
  Profile* profile_;

#if defined(TOOLKIT_VIEWS)
  // Optional view that shows the rendered content in the UI.
  scoped_ptr<ExtensionView> view_;
#endif

  // The host for our HTML content.
  RenderViewHost* render_view_host_;

  // Common implementations of some RenderViewHostDelegate::View methods.
  RenderViewHostDelegateViewHelper delegate_view_helper_;

  // Whether the RenderWidget has reported that it has stopped loading.
  bool did_stop_loading_;

  // The URL being hosted.
  GURL url_;

  scoped_ptr<ExtensionFunctionDispatcher> extension_function_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHost);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_
