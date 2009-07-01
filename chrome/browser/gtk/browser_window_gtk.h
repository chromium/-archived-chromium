// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BROWSER_WINDOW_GTK_H_
#define CHROME_BROWSER_GTK_BROWSER_WINDOW_GTK_H_

#include <gtk/gtk.h>

#include <map>

#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/x11_util.h"

class BookmarkBarGtk;
class Browser;
class BrowserTitlebar;
class BrowserToolbarGtk;
class CustomDrawButton;
class DownloadShelfGtk;
class FindBarGtk;
class InfoBarContainerGtk;
class LocationBar;
class StatusBubbleGtk;
class TabContentsContainerGtk;
class TabStripGtk;

// An implementation of BrowserWindow for GTK.
// Cross-platform code will interact with this object when
// it needs to manipulate the window.

class BrowserWindowGtk : public BrowserWindow,
                         public NotificationObserver,
                         public TabStripModelObserver {
 public:
  explicit BrowserWindowGtk(Browser* browser);
  virtual ~BrowserWindowGtk();

  Browser* browser() const { return browser_.get(); }

  // Process a keyboard input and try to find an accelerator for it.
  void HandleAccelerator(guint keyval, GdkModifierType modifier);

  // Overridden from BrowserWindow
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
  virtual void UpdateDevTools();
  virtual void UpdateLoadingAnimations(bool should_animate);
  virtual void SetStarredState(bool is_starred);
  virtual gfx::Rect GetNormalBounds() const;
  virtual bool IsMaximized() const;
  virtual void SetFullscreen(bool fullscreen);
  virtual bool IsFullscreen() const;
  virtual LocationBar* GetLocationBar() const;
  virtual void SetFocusToLocationBar();
  virtual void UpdateStopGoState(bool is_loading, bool force);
  virtual void UpdateToolbar(TabContents* contents,
                             bool should_restore_state);
  virtual void FocusToolbar();
  virtual bool IsBookmarkBarVisible() const;
  virtual gfx::Rect GetRootWindowResizerRect() const;
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                        Profile* profile);
  virtual void ToggleBookmarkBar();
  virtual void ShowAboutChromeDialog();
  virtual void ShowTaskManager();
  virtual void ShowBookmarkManager();
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked);
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
  virtual void TabContentsFocused(TabContents* tab_contents);

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

  // Accessor for the tab strip.
  TabStripGtk* tabstrip() const { return tabstrip_.get(); }

  void UpdateUIForContents(TabContents* contents);

  void OnBoundsChanged(const gfx::Rect& bounds);
  void OnStateChanged(GdkWindowState state);

  // Returns false if we're not ready to close yet.  E.g., a tab may have an
  // onbeforeunload handler that prevents us from closing.
  bool CanClose() const;

  bool ShouldShowWindowIcon() const;

  // Add the find bar widget to the window hierarchy.
  void AddFindBar(FindBarGtk* findbar);

#if defined(LINUX2)
  // Sets whether a drag is active. If a drag is active the window will not
  // close.
  void set_drag_active(bool drag_active) { drag_active_ = drag_active; }
#endif

  // Reset the mouse cursor to the default cursor if it was set to something
  // else for the custom frame.
  void ResetCustomFrameCursor();

  // Returns the BrowserWindowGtk registered with |window|.
  static BrowserWindowGtk* GetBrowserWindowForNativeWindow(
      gfx::NativeWindow window);

  // Retrieves the GtkWindow associated with |xid|, which is the X Window
  // ID of the top-level X window of this object.
  static GtkWindow* GetBrowserWindowForXID(XID xid);

  Browser* browser() {
    return browser_.get();
  }

 protected:
  virtual void DestroyBrowser();
  // Top level window.
  GtkWindow* window_;
  // GtkAlignment that holds the interior components of the chromium window.
  GtkWidget* window_container_;
  // VBox that holds everything below the tabs.
  GtkWidget* content_vbox_;
  // VBox that holds everything below the toolbar.
  GtkWidget* render_area_vbox_;

  scoped_ptr<Browser> browser_;

  // The download shelf view (view at the bottom of the page).
  scoped_ptr<DownloadShelfGtk> download_shelf_;

 private:
  // Show or hide the bookmark bar.
  void MaybeShowBookmarkBar(TabContents* contents, bool animate);

  // Sets the default size for the window and the the way the user is allowed to
  // resize it.
  void SetGeometryHints();

  // Set up the window icon (potentially used in window border or alt-tab list).
  void SetWindowIcon();

  // Connect to signals on |window_|.
  void ConnectHandlersToSignals();

  // Create the various UI components.
  void InitWidgets();

  // Set up background color of the window (depends on if we're incognito or
  // not).
  void SetBackgroundColor();

  // Called when the window size changed.
  void OnSizeChanged(int width, int height);

  // Applies the window shape to if we're in custom drawing mode.
  void UpdateWindowShape(int width, int height);

  // Connect accelerators that aren't connected to menu items (like ctrl-o,
  // ctrl-l, etc.).
  void ConnectAccelerators();

  // Change whether we're showing the custom blue frame.
  // Must be called once at startup.
  // Triggers relayout of the content.
  void UpdateCustomFrame();

  // Save the window position in the prefs.
  void SaveWindowPosition();

  // Callback for when the custom frame alignment needs to be redrawn.
  // The content area includes the toolbar and web page but not the tab strip.
  static gboolean OnCustomFrameExpose(GtkWidget* widget, GdkEventExpose* event,
                                      BrowserWindowGtk* window);

  static gboolean OnGtkAccelerator(GtkAccelGroup* accel_group,
                                   GObject* acceleratable,
                                   guint keyval,
                                   GdkModifierType modifier,
                                   BrowserWindowGtk* browser_window);

  // Mouse move and mouse button press callbacks.
  static gboolean OnMouseMoveEvent(GtkWidget* widget,
                                   GdkEventMotion* event,
                                   BrowserWindowGtk* browser);
  static gboolean OnButtonPressEvent(GtkWidget* widget,
                                     GdkEventButton* event,
                                     BrowserWindowGtk* browser);

  // Maps and Unmaps the xid of |widget| to |window|.
  static void MainWindowMapped(GtkWidget* widget, BrowserWindowGtk* window);
  static void MainWindowUnMapped(GtkWidget* widget, BrowserWindowGtk* window);

  // A small shim for browser_->ExecuteCommand.
  void ExecuteBrowserCommand(int id);

  // Callback for the loading animation(s) associated with this window.
  void LoadingAnimationCallback();

  // Shows UI elements for supported window features.
  void ShowSupportedWindowFeatures();

  // Hides UI elements for unsupported window features.
  void HideUnsupportedWindowFeatures();

  // Helper functions that query |browser_| concerning support for UI features
  // in this window. (For example, a popup window might not support a tabstrip).
  bool IsTabStripSupported();
  bool IsToolbarSupported();
  bool IsBookmarkBarSupported();

  // Checks to see if the mouse pointer at |x|, |y| is over the border of the
  // custom frame (a spot that should trigger a window resize). Returns true if
  // it should and sets |edge|.
  bool GetWindowEdge(int x, int y, GdkWindowEdge* edge);

  NotificationRegistrar registrar_;

  gfx::Rect bounds_;
  GdkWindowState state_;

  // Whether we are full screen. Since IsFullscreen() gets called before
  // OnStateChanged(), we can't rely on |state_| & GDK_WINDOW_STATE_FULLSCREEN.
  bool full_screen_;

  // The container for the titlebar + tab strip.
  scoped_ptr<BrowserTitlebar> titlebar_;

  // The object that manages all of the widgets in the toolbar.
  scoped_ptr<BrowserToolbarGtk> toolbar_;

  // The object that manages the bookmark bar.
  scoped_ptr<BookmarkBarGtk> bookmark_bar_;

  // The status bubble manager.  Always non-NULL.
  scoped_ptr<StatusBubbleGtk> status_bubble_;

  // A container that manages the GtkWidget*s that are the webpage display
  // (along with associated infobars, shelves, and other things that are part
  // of the content area).
  scoped_ptr<TabContentsContainerGtk> contents_container_;

  // The tab strip.  Always non-NULL.
  scoped_ptr<TabStripGtk> tabstrip_;

  // The container for info bars. Always non-NULL.
  scoped_ptr<InfoBarContainerGtk> infobar_container_;

  // The timer used to update frames for the Loading Animation.
  base::RepeatingTimer<BrowserWindowGtk> loading_animation_timer_;

  // Whether we're showing the custom chrome frame or the window manager
  // decorations.
  BooleanPrefMember use_custom_frame_;

#if defined(LINUX2)
  // True if a drag is active. See description above setter for details.
  bool drag_active_;
#endif

  // A map which translates an X Window ID into its respective GtkWindow.
  static std::map<XID, GtkWindow*> xid_map_;

  // The current window cursor.  We set it to a resize cursor when over the
  // custom frame border.  We set it to NULL if we want the default cursor.
  GdkCursor* frame_cursor_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowGtk);
};

#endif  // CHROME_BROWSER_GTK_BROWSER_WINDOW_GTK_H_
