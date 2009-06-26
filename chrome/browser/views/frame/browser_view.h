// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW_H_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW_H_

#include <set>

#include "base/gfx/native_widget_types.h"
#include "base/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#if defined(OS_WIN)
#include "chrome/browser/hang_monitor/hung_plugin_action.h"
#include "chrome/browser/hang_monitor/hung_window_detector.h"
#endif
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/views/frame/browser_frame.h"
#if defined(OS_WIN)
#include "views/controls/menu/native_menu_win.h"
#endif
#include "views/controls/menu/simple_menu_model.h"
#include "views/window/client_view.h"
#include "views/window/window_delegate.h"

// NOTE: For more information about the objects and files in this directory,
// view: http://dev.chromium.org/developers/design-documents/browser-window

class BookmarkBarView;
class Browser;
class BrowserBubble;
class DownloadShelfView;
class EncodingMenuModel;
class ExtensionShelf;
class FullscreenExitBubble;
class HtmlDialogUIDelegate;
class InfoBarContainer;
class LocationBarView;
class StatusBubbleViews;
class TabContentsContainer;
class TabStrip;
class ToolbarView;
class ZoomMenuModel;

namespace views {
class Menu;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView
//
//  A ClientView subclass that provides the contents of a browser window,
//  including the TabStrip, toolbars, download shelves, the content area etc.
//
class BrowserView : public BrowserWindow,
                    public BrowserWindowTesting,
                    public NotificationObserver,
                    public TabStripModelObserver,
                    public views::SimpleMenuModel::Delegate,
                    public views::WindowDelegate,
                    public views::ClientView {
 public:
  // Explicitly sets how windows are shown. Use a value of -1 to give the
  // default behavior. This is used during testing and not generally useful
  // otherwise.
  static void SetShowState(int state);

  explicit BrowserView(Browser* browser);
  virtual ~BrowserView();

  void set_frame(BrowserFrame* frame) { frame_ = frame; }
  BrowserFrame* frame() const { return frame_; }

  // Returns a pointer to the BrowserView* interface implementation (an
  // instance of this object, typically) for a given native window, or NULL if
  // there is no such association.
  static BrowserView* GetBrowserViewForNativeWindow(gfx::NativeWindow window);

  // Returns the show flag that should be used to show the frame containing
  // this view.
  int GetShowState() const;

  // Called by the frame to notify the BrowserView that it was moved, and that
  // any dependent popup windows should be repositioned.
  void WindowMoved();

  // Called by the frame to notify the BrowserView that a move or resize was
  // initiated.
  void WindowMoveOrResizeStarted();

  // Returns the bounds of the toolbar, in BrowserView coordinates.
  gfx::Rect GetToolbarBounds() const;

  // Returns the bounds of the content area, in the coordinates of the
  // BrowserView's parent.
  gfx::Rect GetClientAreaBounds() const;

  // Returns true if the Find Bar should be rendered such that it appears to
  // blend with the Bookmarks Bar. False if it should appear to blend with the
  // main Toolbar. The return value will vary depending on whether or not the
  // Bookmark Bar is always shown.
  bool ShouldFindBarBlendWithBookmarksBar() const;

  // Returns the constraining bounding box that should be used to lay out the
  // FindBar within. This is _not_ the size of the find bar, just the bounding
  // box it should be laid out within. The coordinate system of the returned
  // rect is in the coordinate system of the frame, since the FindBar is a child
  // window.
  gfx::Rect GetFindBarBoundingBox() const;

  // Returns the preferred height of the TabStrip. Used to position the OTR
  // avatar icon.
  int GetTabStripHeight() const;

  // Returns the bounds of the TabStrip. Used by themed views to determine the
  // offset of IDR_THEME_TOOLBAR.
  gfx::Rect GetTabStripBounds() const;

  // Accessor for the TabStrip.
  TabStrip* tabstrip() const { return tabstrip_; }

  // Returns true if various window components are visible.
  bool IsToolbarVisible() const;
  bool IsTabStripVisible() const;

  // Returns true if the profile associated with this Browser window is
  // off the record.
  bool IsOffTheRecord() const;

  // Returns true if the non-client view should render the Off-The-Record
  // avatar icon if the window is off the record.
  bool ShouldShowOffTheRecordAvatar() const;

  // Handle the specified |accelerator| being pressed.
  bool AcceleratorPressed(const views::Accelerator& accelerator);

  // Provides the containing frame with the accelerator for the specified
  // command id. This can be used to provide menu item shortcut hints etc.
  // Returns true if an accelerator was found for the specified |cmd_id|, false
  // otherwise.
  bool GetAccelerator(int cmd_id, views::Accelerator* accelerator);

  // Shows the next app-modal dialog box, if there is one to be shown, or moves
  // an existing showing one to the front. Returns true if one was shown or
  // activated, false if none was shown.
  bool ActivateAppModalDialog() const;

  // Called when the activation of the frame changes.
  void ActivationChanged(bool activated);

  // Returns the selected TabContents. Used by our NonClientView's
  // TabIconView::TabContentsProvider implementations.
  // TODO(beng): exposing this here is a bit bogus, since it's only used to
  // determine loading state. It'd be nicer if we could change this to be
  // bool IsSelectedTabLoading() const; or something like that. We could even
  // move it to a WindowDelegate subclass.
  TabContents* GetSelectedTabContents() const;

  // Retrieves the icon to use in the frame to indicate an OTR window.
  SkBitmap GetOTRAvatarIcon();

#if defined(OS_WIN)
  // Called right before displaying the system menu to allow the BrowserView
  // to add or delete entries.
  void PrepareToRunSystemMenu(HMENU menu);
#endif

  // Returns true if the Browser object associated with this BrowserView is a
  // normal-type window (i.e. a browser window, not an app or popup).
  bool IsBrowserTypeNormal() const {
    return browser_->type() == Browser::TYPE_NORMAL;
  }

  // Returns true if the frame containing this BrowserView should show the
  // distributor logo.
  bool ShouldShowDistributorLogo() const {
    return browser_->ShouldShowDistributorLogo();
  }

  // Register preferences specific to this view.
  static void RegisterBrowserViewPrefs(PrefService* prefs);

  // Attach/Detach a BrowserBubble to the browser.
  void AttachBrowserBubble(BrowserBubble *bubble);
  void DetachBrowserBubble(BrowserBubble *bubble);

  // Overridden from BrowserWindow:
  virtual void Show();
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void Close();
  virtual void Activate();
  virtual bool IsActive() const;
  virtual void FlashFrame();
  virtual gfx::NativeWindow GetNativeHandle();
  virtual BrowserWindowTesting* GetBrowserWindowTesting();
  virtual StatusBubble* GetStatusBubble();
  virtual void SelectedTabToolbarSizeChanged(bool is_animating);
  virtual void UpdateTitleBar();
  virtual void UpdateLoadingAnimations(bool should_animate);
  virtual void SetStarredState(bool is_starred);
  virtual gfx::Rect GetNormalBounds() const;
  virtual bool IsMaximized() const;
  virtual void SetFullscreen(bool fullscreen);
  virtual bool IsFullscreen() const;
  virtual LocationBar* GetLocationBar() const;
  virtual void SetFocusToLocationBar();
  virtual void UpdateStopGoState(bool is_loading, bool force);
  virtual void UpdateToolbar(TabContents* contents, bool should_restore_state);
  virtual void FocusToolbar();
  virtual void DestroyBrowser();
  virtual bool IsBookmarkBarVisible() const;
  virtual gfx::Rect GetRootWindowResizerRect() const;
  virtual void DisableInactiveFrame();
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                      Profile* profile);
  virtual void ToggleBookmarkBar();
  virtual void ShowAboutChromeDialog();
  virtual void ShowTaskManager();
  virtual void ShowBookmarkManager();
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked);
  virtual void SetDownloadShelfVisible(bool visible);
  virtual bool IsDownloadShelfVisible() const;
  virtual DownloadShelf* GetDownloadShelf();
  virtual void ShowReportBugDialog();
  virtual void ShowClearBrowsingDataDialog();
  virtual void ShowImportDialog();
  virtual void ShowSearchEnginesDialog();
  virtual void ShowPasswordManager();
  virtual void ShowSelectProfileDialog();
  virtual void ShowNewProfileDialog();
  virtual void ConfirmBrowserCloseWithPendingDownloads();
  virtual void ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                              gfx::NativeWindow parent_window);
  virtual void UserChangedTheme();
  virtual int GetExtraRenderViewHeight() const;
  virtual void TabContentsFocused(TabContents* source);

  // Overridden from BrowserWindowTesting:
  virtual BookmarkBarView* GetBookmarkBarView() const;
  virtual LocationBarView* GetLocationBarView() const;
  virtual views::View* GetTabContentsContainerView() const;
  virtual ToolbarView* GetToolbarView() const;

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from TabStripModelObserver:
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabDeselectedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabStripEmpty();

  // Overridden from views::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          views::Accelerator* accelerator);
  virtual bool IsLabelForCommandIdDynamic(int command_id) const;
  virtual std::wstring GetLabelForCommandId(int command_id) const;
  virtual void ExecuteCommand(int command_id);

  // Overridden from views::WindowDelegate:
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual views::View* GetInitiallyFocusedView();
  virtual bool ShouldShowWindowTitle() const;
  virtual SkBitmap GetWindowIcon();
  virtual bool ShouldShowWindowIcon() const;
  virtual bool ExecuteWindowsCommand(int command_id);
  virtual std::wstring GetWindowName() const;
  virtual void SaveWindowPlacement(const gfx::Rect& bounds,
                                   bool maximized);
  virtual bool GetSavedWindowBounds(gfx::Rect* bounds) const;
  virtual bool GetSavedMaximizedState(bool* maximized) const;
  virtual views::View* GetContentsView();
  virtual views::ClientView* CreateClientView(views::Window* window);

  // Overridden from views::ClientView:
  virtual bool CanClose() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual gfx::Size GetMinimumSize();
  virtual std::string GetClassName() const;

 protected:
  // Overridden from views::View:
  virtual void Layout();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);
  virtual void ChildPreferredSizeChanged(View* child);

 private:
  // Browser window related initializations.
  void Init();

#if defined(OS_WIN)
  // Creates the system menu.
  void InitSystemMenu();
#endif

  // Layout the TabStrip, returns the coordinate of the bottom of the TabStrip,
  // for laying out subsequent controls.
  int LayoutTabStrip();
  // Layout the following controls, starting at |top|, returns the coordinate
  // of the bottom of the control, for laying out the next control.
  int LayoutToolbar(int top);
  int LayoutBookmarkAndInfoBars(int top);
  int LayoutBookmarkBar(int top);
  int LayoutInfoBar(int top);
  // Layout the TabContents container, between the coordinates |top| and
  // |bottom|.
  void LayoutTabContents(int top, int bottom);
  // Layout the Download Shelf, returns the coordinate of the top of the
  // control, for laying out the previous control.
  int LayoutDownloadShelf(int bottom);
  // Layout the Status Bubble.
  void LayoutStatusBubble(int top);
  // Layout the Extension Shelf
  int LayoutExtensionShelf();

  // Prepare to show the Bookmark Bar for the specified TabContents. Returns
  // true if the Bookmark Bar can be shown (i.e. it's supported for this
  // Browser type) and there should be a subsequent re-layout to show it.
  // |contents| can be NULL.
  bool MaybeShowBookmarkBar(TabContents* contents);

  // Prepare to show an Info Bar for the specified TabContents. Returns true
  // if there is an Info Bar to show and one is supported for this Browser
  // type, and there should be a subsequent re-layout to show it.
  // |contents| can be NULL.
  bool MaybeShowInfoBar(TabContents* contents);

  // Updates various optional child Views, e.g. Bookmarks Bar, Info Bar or the
  // Download Shelf in response to a change notification from the specified
  // |contents|. |contents| can be NULL. In this case, all optional UI will be
  // removed.
  void UpdateUIForContents(TabContents* contents);

  // Updates an optional child View, e.g. Bookmarks Bar, Info Bar, Download
  // Shelf. If |*old_view| differs from new_view, the old_view is removed and
  // the new_view is added. This is intended to be used when swapping in/out
  // child views that are referenced via a field.
  // Returns true if anything was changed, and a re-Layout is now required.
  bool UpdateChildViewAndLayout(views::View* new_view, views::View** old_view);

  // Copy the accelerator table from the app resources into something we can
  // use.
  void LoadAccelerators();

#if defined(OS_WIN)
  // Builds the correct menu for when we have minimal chrome.
  void BuildSystemMenuForBrowserWindow();
  void BuildSystemMenuForPopupWindow();
#endif

  // Retrieves the command id for the specified Windows app command.
  int GetCommandIDForAppCommandID(int app_command_id) const;

  // Callback for the loading animation(s) associated with this view.
  void LoadingAnimationCallback();

  // Initialize the hung plugin detector.
  void InitHangMonitor();

  // Initialize class statics.
  static void InitClass();

  // The BrowserFrame that hosts this view.
  BrowserFrame* frame_;

  // The Browser object we are associated with.
  scoped_ptr<Browser> browser_;

  // Tool/Info bars that we are currently showing. Used for layout.
  // active_bookmark_bar_ is either NULL, if the bookmark bar isn't showing,
  // or is bookmark_bar_view_ if the bookmark bar is showing.
  views::View* active_bookmark_bar_;

  // The TabStrip.
  TabStrip* tabstrip_;

  // The Toolbar containing the navigation buttons, menus and the address bar.
  ToolbarView* toolbar_;

  // The Bookmark Bar View for this window. Lazily created.
  scoped_ptr<BookmarkBarView> bookmark_bar_view_;

  // The download shelf view (view at the bottom of the page).
  scoped_ptr<DownloadShelfView> download_shelf_;

  // The InfoBarContainer that contains InfoBars for the current tab.
  InfoBarContainer* infobar_container_;

  // The distance the FindBar is from the top of the window, in pixels.
  int find_bar_y_;

  // The view that contains the selected TabContents.
  TabContentsContainer* contents_container_;

  // The Status information bubble that appears at the bottom of the window.
  scoped_ptr<StatusBubbleViews> status_bubble_;

  // A mapping between accelerators and commands.
  scoped_ptr< std::map<views::Accelerator, int> > accelerator_table_;

  // True if we have already been initialized.
  bool initialized_;

  // True if we should ignore requests to layout.  This is set while toggling
  // fullscreen mode on and off to reduce jankiness.
  bool ignore_layout_;

  scoped_ptr<FullscreenExitBubble> fullscreen_bubble_;

  // The default favicon image.
  static SkBitmap default_favicon_;

  // The OTR avatar image.
  static SkBitmap otr_avatar_;

#if defined(OS_WIN)
  // The additional items we insert into the system menu.
  scoped_ptr<views::SystemMenuModel> system_menu_contents_;
  scoped_ptr<ZoomMenuModel> zoom_menu_contents_;
  scoped_ptr<EncodingMenuModel> encoding_menu_contents_;
  // The wrapped system menu itself.
  scoped_ptr<views::NativeMenuWin> system_menu_;

  // This object is used to perform periodic actions in a worker
  // thread. It is currently used to monitor hung plugin windows.
  WorkerThreadTicker ticker_;

  // This object is initialized with the frame window HWND. This
  // object is also passed as a tick handler with the ticker_ object.
  // It is used to periodically monitor for hung plugin windows
  HungWindowDetector hung_window_detector_;

  // This object is invoked by hung_window_detector_ when it detects a hung
  // plugin window.
  HungPluginAction hung_plugin_action_;
#endif

  // The timer used to update frames for the Loading Animation.
  base::RepeatingTimer<BrowserView> loading_animation_timer_;

  // A bottom bar for showing extensions.
  ExtensionShelf* extension_shelf_;

  typedef std::set<BrowserBubble*> BubbleSet;
  BubbleSet browser_bubbles_;

  DISALLOW_COPY_AND_ASSIGN(BrowserView);
};

#endif  // CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW_H_
