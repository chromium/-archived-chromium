// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OLD_FRAMES_XP_FRAME_H__
#define CHROME_BROWSER_VIEWS_OLD_FRAMES_XP_FRAME_H__

#include <windows.h>
#include <atlbase.h>
#include <atlcrack.h>
#include <atlapp.h>
#include <atlframe.h>

#include "base/message_loop.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/views/old_frames/frame_view.h"
#include "chrome/browser/views/status_bubble.h"
#include "chrome/views/button.h"
#include "chrome/views/container.h"
#include "chrome/views/hwnd_view.h"
#include "chrome/views/root_view.h"
#include "chrome/views/image_view.h"
#ifdef CHROME_PERSONALIZATION
#include "chrome/personalization/personalization.h"
#endif

#define XP_FRAME_CLASSNAME L"Chrome_XPFrame"

class BrowserView;
class BookmarkBarView;
class Browser;
class TabContentsContainerView;
class TabStrip;
class TemporaryPlaceholder;

////////////////////////////////////////////////////////////////////////////////
//
// XPFrame class
//
// A CWindowImpl subclass that implements our main frame on Windows XP
//
// The window paints and handles its title bar and controls. It also supports
// a ChromeView hierarchy for the tabs and toolbar
//
////////////////////////////////////////////////////////////////////////////////
class XPFrame : public BrowserWindow,
                public CWindowImpl<XPFrame,
                                   CWindow,
                                   CWinTraits<WS_SYSMENU |
                                              WS_MINIMIZEBOX |
                                              WS_MAXIMIZEBOX |
                                              WS_CLIPCHILDREN>>,
                public views::BaseButton::ButtonListener,
                public views::Container,
                public views::AcceleratorTarget {
 public:

  // Create a new XPFrame given the bounds and provided browser.
  // |bounds| may be empty to let Windows decide where to place the window.
  // The browser_window object acts both as a container for the actual
  // browser contents as well as a source for the tab strip and the toolbar.
  // |is_off_the_record| defines whether this window should represent an off the
  // record session.
  //
  // Note: this method creates an HWND for the frame but doesn't initialize
  // the view hierarchy. The browser creates its HWND from the frame HWND
  // and then calls Init() on the frame to finalize the initialization
  static XPFrame* CreateFrame(const gfx::Rect& bounds, Browser* browser,
                              bool is_off_the_record);

  ////////////////////////////////////////////////////////////////////////////////
  // BrowserWindow implementation
  ////////////////////////////////////////////////////////////////////////////////
  virtual ~XPFrame();
  virtual void Init();
  virtual void Show(int command, bool adjust_to_fit);
  virtual void Close();
  virtual void* GetPlatformID();
  virtual void SetAcceleratorTable(
      std::map<views::Accelerator, int>* accelerator_table);
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);
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
  virtual void ProfileChanged(Profile* profile);
  virtual void FocusToolbar();
  virtual bool IsBookmarkBarVisible() const;

  //
  // CWindowImpl event management magic. See atlcrack.h
  //
  DECLARE_FRAME_WND_CLASS_EX(XP_FRAME_CLASSNAME,
                             IDR_MAINFRAME,
                             CS_DBLCLKS,
                             WHITE_BRUSH)

  BEGIN_MSG_MAP(HaloFrame)
    MESSAGE_RANGE_HANDLER(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    MESSAGE_RANGE_HANDLER(WM_NCMOUSEMOVE, WM_NCMOUSEMOVE, OnMouseRange)
    MESSAGE_RANGE_HANDLER(WM_SETTINGCHANGE, WM_SETTINGCHANGE, OnSettingChange)
    MSG_WM_NCCALCSIZE(OnNCCalcSize);
    MSG_WM_NCPAINT(OnNCPaint);

    MSG_WM_NCACTIVATE(OnNCActivate)
    MSG_WM_LBUTTONDOWN(OnMouseButtonDown)
    MSG_WM_LBUTTONUP(OnLButtonUp);
    MSG_WM_LBUTTONDBLCLK(OnMouseButtonDblClk)
    MSG_WM_MBUTTONDOWN(OnMouseButtonDown)
    MSG_WM_MBUTTONUP(OnMButtonUp);
    MSG_WM_MBUTTONDBLCLK(OnMouseButtonDblClk);
    MSG_WM_RBUTTONDOWN(OnMouseButtonDown)
    MSG_WM_NCLBUTTONDOWN(OnNCLButtonDown)
    MSG_WM_NCMBUTTONDOWN(OnNCMButtonDown)
    MSG_WM_NCRBUTTONDOWN(OnNCRButtonDown)
    MSG_WM_RBUTTONUP(OnRButtonUp);
    MSG_WM_RBUTTONDBLCLK(OnMouseButtonDblClk);
    MESSAGE_HANDLER_EX(WM_GETOBJECT, OnGetObject); // To avoid edit atlcrack.h.
    MSG_WM_KEYDOWN(OnKeyDown);
    MSG_WM_KEYUP(OnKeyUp);
    MSG_WM_MOUSEMOVE(OnMouseMove)
    MSG_WM_MOUSELEAVE(OnMouseLeave)
    MSG_WM_CLOSE(Close)  // Note: Calls Close() directly, there is no OnClose()
    MSG_WM_ENDSESSION(OnEndSession)
    MSG_WM_APPCOMMAND(OnAppCommand)
    MSG_WM_COMMAND(OnCommand)
    MSG_WM_SYSCOMMAND(OnSysCommand)
    MSG_WM_ACTIVATE(OnActivate)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_ERASEBKGND(OnEraseBkgnd)
    MSG_WM_GETMINMAXINFO(OnMinMaxInfo)
    MSG_WM_CAPTURECHANGED(OnCaptureChanged)
    MSG_WM_SIZE(OnSize)
    MSG_WM_MOVE(OnMove)
    MSG_WM_MOVING(OnMoving)
    MSG_WM_NCHITTEST(OnNCHitTest)
    MSG_WM_INITMENU(OnInitMenu)
    MSG_WM_NOTIFY(OnNotify)
    MSG_WM_MOUSEACTIVATE(OnMouseActivate)
    MSG_WM_POWERBROADCAST(OnPowerBroadcast)
    MSG_WM_THEMECHANGED(OnThemeChanged)
    REFLECT_NOTIFICATIONS()
  END_MSG_MAP()

  void OnFinalMessage(HWND hwnd);

  // views::BaseButton::ButtonListener
  void ButtonPressed(views::BaseButton *sender);

  //
  // Container
  virtual void GetBounds(CRect *out, bool including_frame) const;
  virtual void MoveToFront(bool should_activate);
  virtual HWND GetHWND() const;
  virtual void PaintNow(const gfx::Rect& update_rect);
  virtual views::RootView* GetRootView();
  virtual bool IsVisible();
  virtual bool IsActive();
  virtual views::TooltipManager* GetTooltipManager();
  virtual StatusBubble* GetStatusBubble();
  virtual bool GetAccelerator(int cmd_id, views::Accelerator* accelerator);

  virtual void ShelfVisibilityChanged();
  virtual void SelectedTabToolbarSizeChanged(bool is_animating);
  virtual void SetWindowTitle(const std::wstring& title);
  virtual void Activate();
  virtual void FlashFrame();

  virtual void ShowTabContents(TabContents* contents);
  virtual TabStrip* GetTabStrip() const;
  void SizeToContents(const gfx::Rect& contents_bounds);

  // Returns true if the frame should be rendered in an active state.
  bool PaintAsActive() const { return is_active_ || paint_as_active_; }

 protected:
  // Invoked after the HWND has been created but before the window is showing.
  // This registers tooltips. If you override, be sure and invoke this
  // implementation.
  virtual void InitAfterHWNDCreated();

  XPFrame(Browser* browser);

  // Offer subclasses an opportunity to change how the tabstrip is created.
  // The default implementation allocates a normal tab strip.
  virtual TabStrip* CreateTabStrip(Browser* browser);

  // Override and return false if no tab strip or toolbar should be visible.
  // Default method returns true.
  virtual bool IsTabStripVisible() const { return true; }

  // Override and return false if no toolbar should  be visible. Default method
  // returns true.
  virtual bool IsToolBarVisible() const { return true; }

#ifdef CHROME_PERSONALIZATION
  virtual bool PersonalizationEnabled() const {
    return personalization_enabled_;
  }

  void EnablePersonalization(bool enable_personalization) {
   personalization_enabled_ = enable_personalization;
  }
#endif

  // Return the frame view.
  views::View* GetFrameView() { return frame_view_; }

  // Return the tab contents container.
  TabContentsContainerView* GetTabContentsContainer() {
    return tab_contents_container_;
  }

  // Return the X origin of the the first frame control button.
  int GetButtonXOrigin() {
    return min_button_->x();
  }

  // Return the Y location of the contents or infobar.
  int GetContentsYOrigin();

  // Give subclasses an opportunity to never show the bookmark bar. We use this
  // for the application window. Default method returns true.
  virtual bool SupportsBookmarkBar() const { return true; }

  virtual LRESULT OnNCHitTest(const CPoint& pt);

  // Layout all views given the available size
  virtual void Layout();

  // Set whether this frame represents an off the record session. This is
  // currently only called during initialization.
  void SetIsOffTheRecord(bool value);

  virtual void DestroyBrowser();

  // The Browser instance that created this instance
  Browser* browser_;
 private:
  enum FrameAction {FA_NONE = 0, FA_RESIZING, FA_FORWARDING};

  enum ResizeMode {RM_UNDEFINED = 0, RM_NORTH, RM_NORTH_EAST, RM_EAST,
                   RM_SOUTH_EAST, RM_SOUTH, RM_SOUTH_WEST, RM_WEST,
                   RM_NORTH_WEST};

  enum ResizeCursor {RC_VERTICAL = 0, RC_HORIZONTAL, RC_NESW, RC_NWSE};

  LRESULT OnMouseRange(UINT u_msg, WPARAM w_param, LPARAM l_param,
                       BOOL& handled);
  LRESULT OnNotify(int w_param, NMHDR* hdr);
  LRESULT OnSettingChange(UINT u_msg, WPARAM w_param, LPARAM l_param,
                          BOOL& handled);

  // The view we use for our background
  class XPFrameView : public FrameView {
   public:
    XPFrameView(XPFrame* parent) : FrameView(parent), parent_(parent) {
    }
    virtual void Paint(ChromeCanvas* canvas);
    virtual std::string GetClassName() const {
      return "chrome/browser/views/XPFrameView";
    }
    // Accessibility information
    bool GetAccessibleRole(VARIANT* role);

    // Returns a brief, identifying string, containing a unique, readable name.
    bool GetAccessibleName(std::wstring* name);

    // Assigns an accessible string name.
    void SetAccessibleName(const std::wstring& name);

   protected:
    // Overriden to return false if maximized and over the min/max/close
    // button.
    virtual bool ShouldForwardToTabStrip(const views::DropTargetEvent& event);

   private:
    void PaintFrameBorder(ChromeCanvas* canvas);
    void PaintFrameBorderZoomed(ChromeCanvas* canvas);
    void PaintContentsBorder(ChromeCanvas* canvas,
                             int x,
                             int y,
                             int w,
                             int h);
    void PaintContentsBorderZoomed(ChromeCanvas* canvas,
                            int x,
                            int y,
                            int w);
    XPFrame* parent_;

    // Storage of strings needed for accessibility.
    std::wstring accessible_name_;

    DISALLOW_EVIL_CONSTRUCTORS(XPFrameView);
  };

  friend class XPFrameView;

  //
  // Windows event handlers
  //

  void OnEndSession(BOOL ending, UINT logoff);

  LRESULT OnNCCalcSize(BOOL w_param, LPARAM l_param);
  LRESULT OnNCPaint(HRGN param);

  void OnMove(const CSize& size);
  void OnMoving(UINT param, const LPRECT new_bounds);
  void OnSize(UINT param, const CSize& size);
  void OnMouseButtonDown(UINT flags, const CPoint& pt);
  void OnNCLButtonDown(UINT flags, const CPoint& pt);
  void OnNCMButtonDown(UINT flags, const CPoint& pt);
  void OnNCRButtonDown(UINT flags, const CPoint& pt);
  void OnMouseButtonUp(UINT flags, const CPoint& pt);
  void OnMouseButtonDblClk(UINT flags, const CPoint& pt);
  void OnLButtonUp(UINT flags, const CPoint& pt);
  void OnMButtonUp(UINT flags, const CPoint& pt);
  void OnRButtonUp(UINT flags, const CPoint& pt);
  void OnMouseMove(UINT flags, const CPoint& pt);
  void OnMouseLeave();
  void OnKeyDown(TCHAR c, UINT rep_cnt, UINT flags);
  void OnKeyUp(TCHAR c, UINT rep_cnt, UINT flags);
  LRESULT OnGetObject(UINT uMsg, WPARAM w_param, LPARAM l_param);

  LRESULT OnAppCommand(
     HWND w_param, short app_command, WORD device, int keystate);
  void OnCommand(UINT notification_code, int command_id, HWND window);
  void OnSysCommand(UINT notification_code, CPoint click);
  void OnActivate(UINT n_state, BOOL is_minimized, HWND other);
  int OnMouseActivate(CWindow wndTopLevel, UINT nHitTest, UINT message);
  void OnPaint(HDC dc);
  LRESULT OnEraseBkgnd(HDC dc);
  void OnMinMaxInfo(LPMINMAXINFO mm_info);
  void OnCaptureChanged(HWND hwnd);
  void OnInitMenu(HMENU menu);
  void ArmOnMouseLeave();
  void ShowSystemMenu(int x, int y);

  LRESULT OnNCActivate(BOOL param);
  BOOL OnPowerBroadcast(DWORD power_event, DWORD data);
  void OnThemeChanged();

  // Window resize
  // Note: we cannot use the standard window resize because we don't have
  // a frame. Returning HTSIZE from WM_NCHITTEST doesn't work
  void StartWindowResize(ResizeMode mode, const CPoint &pt);
  void ProcessWindowResize(const CPoint &pt);
  void StopWindowResize();

  // Compute a ResizeMode given a point (x,y) and the current size
  // of the window (width,height). This method returns UNDEFINED
  // if no resizing should occur.
  ResizeMode ComputeResizeMode(int x, int y, int width, int height);

  //
  // Change the cursor as needed given the desired ResizeMode
  void SetResizeCursor(ResizeMode r_mode);

  // Check whether the selected tab needs some extra painting during
  // move or resize because it obstructs its contents (WebContents)
  bool ShouldRefreshCurrentTabContents();

  //
  // View events propagation
  //
  bool ProcessMousePressed(const CPoint& pt, UINT flags, bool dbl_click);
  void ProcessMouseDragged(const CPoint& pt, UINT flags);
  void ProcessMouseReleased(const CPoint& pt, UINT flags);
  void ProcessMouseMoved(const CPoint &pt, UINT flags);
  void ProcessMouseExited();

  // Updates either the infobar or the bottom shelf depending on the views
  // passed in.
  void UpdateShelfViews(int view_top, int preferred_height,
                        views::View* new_view,
                        views::View** current_view,
                        int* last_height);

  // If the workaround to prevent taskbar from hiding behind maximizing
  // xp_frame is enabled.
  bool ShouldWorkAroundAutoHideTaskbar();

  // Updates a single view. If *view differs from new_view the old view is
  // removed and the new view is added.
  //
  // This is intended to be used when swapping in/out child views that are
  // referenced via a field.
  //
  // This function returns true if anything was changed. The caller should
  // ensure that Layout() is eventually called in this case.
  bool UpdateChildViewAndLayout(views::View* new_view, views::View** view);

  // Return the set of bitmaps that should be used to draw this frame.
  SkBitmap** GetFrameBitmaps();

  // Implementation for ShelfVisibilityChanged(). Updates the various shelf
  // fields. If one of the shelves has changed (or it's size has changed) and
  // current_tab is non-NULL layout occurs.
  void ShelfVisibilityChangedImpl(TabContents* current_tab);

  // Root view
  views::RootView root_view_;

  // Top level view used to render the frame itself including the title bar
  XPFrameView* frame_view_;

  // Browser contents
  TabContentsContainerView* tab_contents_container_;

  // Frame buttons
  views::Button* min_button_;
  views::Button* max_button_;
  views::Button* restore_button_;
  views::Button* close_button_;

  // Whether we should save the bounds of the window. We don't want to
  // save the default size of windows for certain classes of windows
  // like unconstrained popups. Defaults to true.
  bool should_save_window_placement_;

  // Whether we saved the window placement yet.
  bool saved_window_placement_;

  // Current frame ui action
  FrameAction current_action_;

  // Whether the frame is currently active
  bool is_active_;

  // whether we are expecting a mouse leave event
  bool on_mouse_leave_armed_;

  // Point containing the coordinate of the first event during
  // a window dragging or resizing session
  CPoint drag_origin_;

  // Rectangle containing the original window bounds during
  // a window dragging or resizing session. It is in screen
  // coordinate system
  CRect  previous_bounds_;

  // Initialize static members the first time this is invoked
  static void InitializeIfNeeded();

  // cursor storage
  HCURSOR previous_cursor_;

  // Current resize mode
  ResizeMode current_resize_mode_;

  // Frame min size
  CSize minimum_size_;

  scoped_ptr<views::TooltipManager> tooltip_manager_;

  // A view positioned at the bottom of the frame.
  views::View* shelf_view_;

  // A view positioned beneath the bookmark bar.
  // Implementation mirrors shelf_view_
  views::View* info_bar_view_;

  // The view that contains the tabs and any associated controls.
  TabStrip* tabstrip_;

  // The bookmark bar. This is lazily created.
  scoped_ptr<BookmarkBarView> bookmark_bar_view_;

  // The visible bookmark bar. NULL if none is visible.
  views::View* active_bookmark_bar_;

  // The optional container for the off the record icon.
  views::ImageView* off_the_record_image_;

  // The container for the distributor logo.
  views::ImageView* distributor_logo_;

  // We need to own the text of the menu, the Windows API does not copy it.
  std::wstring task_manager_label_text_;

  // A mapping between accelerators and commands.
  scoped_ptr<std::map<views::Accelerator, int>> accelerator_table_;

  // Whether this frame represents an off the record session.
  bool is_off_the_record_;

#ifdef CHROME_PERSONALIZATION
  FramePersonalization personalization_;
  bool personalization_enabled_;
#endif

  // Set during layout. Total height of the title bar.
  int title_bar_height_;

  // Whether this frame needs a layout or not.
  bool needs_layout_;

  static bool g_initialized;
  static HCURSOR g_resize_cursors[4];
  static SkBitmap** g_bitmaps;
  static SkBitmap** g_otr_bitmaps;

  // Instance of accessibility information and handling for MSAA root
  CComPtr<IAccessible> accessibility_root_;

  // See note in VistaFrame for a description of this.
  bool ignore_ncactivate_;
  bool paint_as_active_;

  // A view that holds the client-area contents of the browser window.
  BrowserView* browser_view_;

  DISALLOW_EVIL_CONSTRUCTORS(XPFrame);
};

#endif  // CHROME_BROWSER_VIEWS_OLD_FRAMES_XP_FRAME_H__
