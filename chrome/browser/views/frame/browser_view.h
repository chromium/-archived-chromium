// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW_H_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW_H_

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/hang_monitor/hung_plugin_action.h"
#include "chrome/browser/hang_monitor/hung_window_detector.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/views/client_view.h"
#include "chrome/views/window_delegate.h"
#ifdef CHROME_PERSONALIZATION
#include "chrome/personalization/personalization.h"
#endif

// NOTE: For more information about the objects and files in this directory,
//       view: https://sites.google.com/a/google.com/the-chrome-project/developers/design-documents/browser-window

class BookmarkBarView;
class Browser;
class BrowserToolbarView;
class EncodingMenuControllerDelegate;
class FindBarWin;
class FullscreenExitBubble;
class InfoBarContainer;
class Menu;
class StatusBubbleViews;
class TabContentsContainerView;

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
  // instance of this object, typically) for a given HWND, or NULL if there is
  // no such association.
  static BrowserView* GetBrowserViewForHWND(HWND window);

  // Returns the show flag that should be used to show the frame containing
  // this view.
  int GetShowState() const;

  // Called by the frame to notify the BrowserView that it was moved, and that
  // any dependent popup windows should be repositioned.
  void WindowMoved();

  // Called by the frame to notify the BrowserView that a move or resize was
  // initiated.
  void WindowMoveOrResizeStarted();

  // Returns whether the browser can be resized _now_.  This differs from
  // CanResize() below, which returns whether the window is ever resizable in
  // principle.
  bool CanCurrentlyResize() const;

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

  // Handles incoming system messages. Returns true if the message was
  // handled.
  bool SystemCommandReceived(UINT notification_code, const gfx::Point& point);

  // Adds view to the set of views that drops are allowed to occur on. You only
  // need invoke this for views whose y-coordinate extends above the tab strip
  // and you want to allow drops on.
  void AddViewToDropList(views::View* view);

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

  // Called right before displaying the system menu to allow the BrowserView
  // to add or delete entries.
  void PrepareToRunSystemMenu(HMENU menu);

  // Possible elements of the Browser window.
  enum WindowFeature {
    FEATURE_TITLEBAR = 1,
    FEATURE_TABSTRIP = 2,
    FEATURE_TOOLBAR = 4,
    FEATURE_LOCATIONBAR = 8,
    FEATURE_BOOKMARKBAR = 16,
    FEATURE_INFOBAR = 32,
    FEATURE_DOWNLOADSHELF = 64
  };

  // Returns true if the Browser object associated with this BrowserView is a
  // normal-type window (i.e. a browser window, not an app or popup).
  bool IsBrowserTypeNormal() const {
    return browser_->type() == Browser::TYPE_NORMAL;
  }

  // Register preferences specific to this view.
  static void RegisterBrowserViewPrefs(PrefService* prefs);

  // Overridden from BrowserWindow:
  virtual void Init();
  virtual void Show();
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void Close();
  virtual void Activate();
  virtual bool IsActive() const;
  virtual void FlashFrame();
  virtual void* GetNativeHandle();
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
  virtual void UpdateStopGoState(bool is_loading);
  virtual void UpdateToolbar(TabContents* contents, bool should_restore_state);
  virtual void FocusToolbar();
  virtual void DestroyBrowser();
  virtual bool IsBookmarkBarVisible() const;
  virtual gfx::Rect GetRootWindowResizerRect() const;
  virtual void ToggleBookmarkBar();
  virtual void ShowFindBar();
  virtual void ShowAboutChromeDialog();
  virtual void ShowBookmarkManager();
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked);
  virtual void ShowReportBugDialog();
  virtual void ShowClearBrowsingDataDialog();
  virtual void ShowImportDialog();
  virtual void ShowSearchEnginesDialog();
  virtual void ShowPasswordManager();
  virtual void ShowSelectProfileDialog();
  virtual void ShowNewProfileDialog();
  virtual void ShowHTMLDialog(HtmlDialogContentsDelegate* delegate,
                              void* parent_window);
  virtual bool GetFindBarWindowInfo(gfx::Point* position,
                                    bool* fully_visible) const;

  // Overridden from BrowserWindowTesting:
  virtual BookmarkBarView* GetBookmarkBarView() const;
  virtual LocationBarView* GetLocationBarView() const;

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from TabStripModelObserver:
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabStripEmpty();

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
                                   bool maximized,
                                   bool always_on_top);
  virtual bool GetSavedWindowBounds(gfx::Rect* bounds) const;
  virtual bool GetSavedMaximizedState(bool* maximized) const;
  virtual void WindowClosing();
  virtual views::View* GetContentsView();
  virtual views::ClientView* CreateClientView(views::Window* window);

  // Overridden from views::ClientView:
  virtual bool CanClose() const;
  virtual int NonClientHitTest(const gfx::Point& point);

  // Is P13N enabled for this browser window?
#ifdef CHROME_PERSONALIZATION
  virtual bool IsPersonalizationEnabled() const {
    return personalization_enabled_;
  }

  void EnablePersonalization(bool enable_personalization) {
    personalization_enabled_ = enable_personalization;
  }
#endif

 protected:
  // Overridden from views::View:
  virtual void Layout();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);
  // As long as ShouldForwardToTabStrip returns true, drag and drop methods
  // are forwarded to the tab strip.
  virtual bool CanDrop(const OSExchangeData& data);
  virtual void OnDragEntered(const views::DropTargetEvent& event);
  virtual int OnDragUpdated(const views::DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const views::DropTargetEvent& event);

 private:
  // Information saved before going into fullscreen mode, used to restore the
  // window afterwards.
  struct SavedWindowInfo {
    bool maximized;
    LONG style;
    LONG ex_style;
    RECT window_rect;
  };

  // Creates the system menu.
  void InitSystemMenu();

  // Returns true if the Browser object associated with this BrowserView
  // supports the specified feature.
  bool SupportsWindowFeature(WindowFeature feature) const;

  // Returns true if the event should be forwarded to the TabStrip. This
  // returns true if y coordinate is less than the bottom of the tab strip, and
  // is not over another child view.
  virtual bool ShouldForwardToTabStrip(const views::DropTargetEvent& event);

  // Creates and returns a new DropTargetEvent in the coordinates of the
  // TabStrip.
  views::DropTargetEvent* MapEventToTabStrip(
      const views::DropTargetEvent& event);

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
  // Layout the Download Shelf, returns the coordinate of the top of the\
  // control, for laying out the previous control.
  int LayoutDownloadShelf();
  // Layout the Status Bubble.
  void LayoutStatusBubble(int top);

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

  // Prepare to show a Download Shelf for the specified TabContents. Returns
  // true if there is a Download Shelf to show and one is supported for this
  // Browser type, and there should be a subsequent re-layout to show it.
  // |contents| can be NULL.
  bool MaybeShowDownloadShelf(TabContents* contents);

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

  // Builds the correct menu for when we have minimal chrome.
  void BuildMenuForTabStriplessWindow(Menu* menu, int insertion_index);

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
  views::View* active_download_shelf_;

  // The TabStrip.
  TabStrip* tabstrip_;

  // The Toolbar containing the navigation buttons, menus and the address bar.
  BrowserToolbarView* toolbar_;

  // The Bookmark Bar View for this window. Lazily created.
  scoped_ptr<BookmarkBarView> bookmark_bar_view_;

  // The InfoBarContainer that contains InfoBars for the current tab.
  InfoBarContainer* infobar_container_;

  // The Find Bar. This may be NULL if there is no Find Bar, and if it is
  // non-NULL, it may or may not be visible.
  scoped_ptr<FindBarWin> find_bar_;

  // The distance the FindBar is from the top of the window, in pixels.
  int find_bar_y_;

  // The view that contains the selected TabContents.
  TabContentsContainerView* contents_container_;

  // The Status information bubble that appears at the bottom of the window.
  scoped_ptr<StatusBubbleViews> status_bubble_;

  // A mapping between accelerators and commands.
  scoped_ptr<std::map<views::Accelerator, int>> accelerator_table_;

  // True if we have already been initialized.
  bool initialized_;

  // True if we're in fullscreen mode.
  bool fullscreen_;

  // Saved window information from before entering fullscreen mode.
  SavedWindowInfo saved_window_info_;

  scoped_ptr<FullscreenExitBubble> fullscreen_bubble_;

  // Lazily created representation of the system menu.
  scoped_ptr<Menu> system_menu_;

  // The default favicon image.
  static SkBitmap default_favicon_;

  // Initially set in CanDrop by invoking the same method on the TabStrip.
  bool can_drop_;

  // If true, drag and drop events are being forwarded to the tab strip.
  // This is used to determine when to send OnDragExited and OnDragExited
  // to the tab strip.
  bool forwarding_to_tab_strip_;

  // Set of additional views drops are allowed on. We do NOT own these.
  std::set<views::View*> dropable_views_;

  // The OTR avatar image.
  static SkBitmap otr_avatar_;

  // The delegate for the encoding menu.
  scoped_ptr<EncodingMenuControllerDelegate> encoding_menu_delegate_;

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

  // The timer used to update frames for the Loading Animation.
  base::RepeatingTimer<BrowserView> loading_animation_timer_;

  // P13N stuff
#ifdef CHROME_PERSONALIZATION
  FramePersonalization personalization_;
  bool personalization_enabled_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BrowserView);
};

#endif  // CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW_H_
