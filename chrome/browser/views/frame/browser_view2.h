// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW2_H_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW2_H_

#include "chrome/browser/browser_type.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/common/pref_member.h"
#include "chrome/views/client_view.h"
#include "chrome/views/window_delegate.h"

// NOTE: For more information about the objects and files in this directory,
//       view: https://sites.google.com/a/google.com/the-chrome-project/developers/design-documents/browser-window

class BookmarkBarView;
class Browser;
class BrowserToolbarView;
class EncodingMenuControllerDelegate;
class Menu;
class StatusBubble;
class TabContentsContainerView;

///////////////////////////////////////////////////////////////////////////////
// BrowserView2
//
//  A ClientView subclass that provides the contents of a browser window,
//  including the TabStrip, toolbars, download shelves, the content area etc.
//
class BrowserView2 : public BrowserWindow,
                     public NotificationObserver,
                     public TabStripModelObserver,
                     public views::WindowDelegate,
                     public views::ClientView {
 public:
  explicit BrowserView2(Browser* browser);
  virtual ~BrowserView2();

  void set_frame(BrowserFrame* frame) { frame_ = frame; }

  // Called by the frame to notify the BrowserView2 that it was moved, and that
  // any dependent popup windows should be repositioned.
  void WindowMoved();

  // Returns the bounds of the toolbar, in BrowserView2 coordinates.
  gfx::Rect GetToolbarBounds() const;

  // Returns the bounds of the content area, in the coordinates of the
  // BrowserView2's parent.
  gfx::Rect GetClientAreaBounds() const;

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

  // Called right before displaying the system menu to allow the BrowserView2
  // to add or delete entries.
  void PrepareToRunSystemMenu(HMENU menu);

  // Called after the system menu has ended, and disposes of the
  // current System menu object.
  void SystemMenuEnded();

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

  // Returns true if the Browser object associated with this BrowserView2
  // supports the specified feature.
  bool SupportsWindowFeature(WindowFeature feature) const;

  // Returns the set of WindowFeatures supported by the specified BrowserType.
  static unsigned int FeaturesForBrowserType(BrowserType::Type type);

  // Overridden from BrowserWindow:
  virtual void Init();
  virtual void Show(int command, bool adjust_to_fit);
  virtual void Close();
  virtual void* GetPlatformID();
  virtual TabStrip* GetTabStrip() const;
  virtual StatusBubble* GetStatusBubble();
  virtual void SelectedTabToolbarSizeChanged(bool is_animating);
  virtual void UpdateTitleBar();
  virtual void Activate();
  virtual void FlashFrame();
  virtual void ContinueDetachConstrainedWindowDrag(
      const gfx::Point& mouse_point,
      int frame_component);
  virtual void SizeToContents(const gfx::Rect& contents_bounds);
  virtual void SetAcceleratorTable(
      std::map<views::Accelerator, int>* accelerator_table);
  virtual void ValidateThrobber();
  virtual gfx::Rect GetNormalBounds();
  virtual bool IsMaximized();
  virtual gfx::Rect GetBoundsForContentBounds(const gfx::Rect content_rect);
  virtual void InfoBubbleShowing();
  virtual void InfoBubbleClosing();
  virtual ToolbarStarToggle* GetStarButton() const;
  virtual LocationBarView* GetLocationBarView() const;
  virtual GoButton* GetGoButton() const;
  virtual BookmarkBarView* GetBookmarkBarView();
  virtual BrowserView* GetBrowserView() const;
  virtual void UpdateToolbar(TabContents* contents, bool should_restore_state);
  virtual void FocusToolbar();
  virtual void DestroyBrowser();
  virtual bool IsBookmarkBarVisible() const;

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
  virtual views::View* GetInitiallyFocusedView() const;
  virtual bool ShouldShowWindowTitle() const;
  virtual SkBitmap GetWindowIcon();
  virtual bool ShouldShowWindowIcon() const;
  virtual bool ExecuteWindowsCommand(int command_id);
  virtual void SaveWindowPosition(const CRect& bounds,
                                  bool maximized,
                                  bool always_on_top);
  virtual bool RestoreWindowPosition(CRect* bounds,
                                     bool* maximized,
                                     bool* always_on_top);
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

  // Initialize class statics.
  static void InitClass();

  // The BrowserFrame that hosts this view.
  BrowserFrame* frame_;

  // The Browser object we are associated with.
  scoped_ptr<Browser> browser_;

  // Tool/Info bars that we are currently showing. Used for layout.
  views::View* active_bookmark_bar_;
  views::View* active_info_bar_;
  views::View* active_download_shelf_;

  // The TabStrip.
  TabStrip* tabstrip_;

  // The Toolbar containing the navigation buttons, menus and the address bar.
  BrowserToolbarView* toolbar_;

  // The Bookmark Bar View for this window. Lazily created.
  scoped_ptr<BookmarkBarView> bookmark_bar_view_;

  // The view that contains the selected TabContents.
  TabContentsContainerView* contents_container_;

  // The Status information bubble that appears at the bottom of the window.
  scoped_ptr<StatusBubble> status_bubble_;

  // A mapping between accelerators and commands.
  scoped_ptr<std::map<views::Accelerator, int>> accelerator_table_;

  // A PrefMember to track the "always show bookmark bar" pref.
  BooleanPrefMember show_bookmark_bar_pref_;

  // True if we have already been initialized.
  bool initialized_;

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

  // P13N stuff
#ifdef CHROME_PERSONALIZATION
  FramePersonalization personalization_;
  bool personalization_enabled_;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(BrowserView2);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW2_H_

