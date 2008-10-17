// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OLD_FRAMES_VISTA_FRAME_H__
#define CHROME_BROWSER_VIEWS_OLD_FRAMES_VISTA_FRAME_H__

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
#include "chrome/views/container.h"
#include "chrome/views/root_view.h"
#include "chrome/views/hwnd_view.h"
#include "chrome/views/image_view.h"
#include "chrome/views/tooltip_manager.h"

#define VISTA_FRAME_CLASSNAME L"Chrome_VistaFrame"

class BookmarkBarView;
class Browser;
class BrowserView;
class TabContentsContainerView;
class views::FocusManager;
class SkBitmap;
class TabStrip;

////////////////////////////////////////////////////////////////////////////////
//
// VistaFrame class
//
// A CWindowImpl subclass that implements our main frame on Windows Vista
//
////////////////////////////////////////////////////////////////////////////////
class VistaFrame : public BrowserWindow,
                   public CWindowImpl<VistaFrame,
                                      CWindow,
                                      CWinTraits<WS_OVERLAPPEDWINDOW |
                                                 WS_CLIPCHILDREN>>,
                   public views::Container,
                   public views::AcceleratorTarget {
 public:
  // Create a new VistaFrame given the bounds and provided browser.
  // |bounds| may be empty to let Windows decide where to place the window.
  // The browser_window object acts both as a container for the actual
  // browser contents as well as a source for the tab strip and the toolbar.
  // |is_off_the_record| defines whether this window should represent an off the
  // record session.
  //
  // Note: this method creates an HWND for the frame but doesn't initialize
  // the view hierarchy. The browser creates its HWND from the frame HWND
  // and then calls Init() on the frame to finalize the initialization
  static VistaFrame* CreateFrame(const gfx::Rect& bounds,
                                 Browser* browser,
                                 bool is_off_the_record);

  virtual ~VistaFrame();

  ////////////////////////////////////////////////////////////////////////////////
  // Events
  ////////////////////////////////////////////////////////////////////////////////

  DECLARE_FRAME_WND_CLASS_EX(VISTA_FRAME_CLASSNAME,
                             IDR_MAINFRAME,
                             CS_DBLCLKS,
                             NULL)

  BEGIN_MSG_MAP(VistaFrame)
    MESSAGE_RANGE_HANDLER(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseRange)
    MESSAGE_HANDLER(WM_NCMOUSELEAVE, OnMouseRange)
    MESSAGE_HANDLER(WM_NCMOUSEMOVE, OnMouseRange)
    MESSAGE_RANGE_HANDLER(WM_SETTINGCHANGE, WM_SETTINGCHANGE, OnSettingChange)
    MSG_WM_LBUTTONDOWN(OnMouseButtonDown)
    MSG_WM_LBUTTONUP(OnLButtonUp);
    MSG_WM_LBUTTONDBLCLK(OnMouseButtonDblClk)
    MSG_WM_MBUTTONDOWN(OnMouseButtonDown)
    MSG_WM_MBUTTONUP(OnMButtonUp);
    MSG_WM_MBUTTONDBLCLK(OnMouseButtonDblClk);
    MSG_WM_RBUTTONDOWN(OnMouseButtonDown)
    MSG_WM_RBUTTONUP(OnRButtonUp);
    MSG_WM_NCMBUTTONDOWN(OnNCMButtonDown)
    MSG_WM_NCRBUTTONDOWN(OnNCRButtonDown)
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
    MSG_WM_SIZE(OnSize)
    MSG_WM_MOVE(OnMove)
    MSG_WM_MOVING(OnMoving)
    MSG_WM_ACTIVATE(OnActivate)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_ERASEBKGND(OnEraseBkgnd)
    MSG_WM_NCHITTEST(OnNCHitTest)
    MSG_WM_NCCALCSIZE(OnNCCalcSize)
    MSG_WM_CAPTURECHANGED(OnCaptureChanged)
    MSG_WM_NOTIFY(OnNotify)
    MSG_WM_NCLBUTTONDOWN(OnNCLButtonDown)
    MSG_WM_NCACTIVATE(OnNCActivate)
    MSG_WM_MOUSEACTIVATE(OnMouseActivate)
    MSG_WM_POWERBROADCAST(OnPowerBroadcast)
    MSG_WM_THEMECHANGED(OnThemeChanged)
    REFLECT_NOTIFICATIONS()
  END_MSG_MAP()

  void OnEndSession(BOOL ending, UINT logoff);
  void OnActivate(UINT n_state, BOOL is_minimized, HWND other);
  int OnMouseActivate(CWindow wndTopLevel, UINT nHitTest, UINT message);
  void OnMouseButtonDown(UINT flags, const CPoint& pt);
  void OnMouseButtonUp(UINT flags, const CPoint& pt);
  void OnLButtonUp(UINT flags, const CPoint& pt);
  void OnMouseButtonDblClk(UINT flags, const CPoint& pt);
  void OnMButtonUp(UINT flags, const CPoint& pt);
  void OnRButtonUp(UINT flags, const CPoint& pt);
  void OnNCMButtonDown(UINT flags, const CPoint& pt);
  void OnNCRButtonDown(UINT flags, const CPoint& pt);
  void OnMouseMove(UINT flags, const CPoint& pt);
  void OnMouseLeave();
  void OnKeyDown(TCHAR c, UINT rep_cnt, UINT flags);
  void OnKeyUp(TCHAR c, UINT rep_cnt, UINT flags);
  LRESULT OnGetObject(UINT uMsg, WPARAM w_param, LPARAM l_param);
  LRESULT OnAppCommand(
      HWND w_param, short app_command, WORD device, int keystate);
  void OnCommand(UINT notification_code, int command_id, HWND window);
  void OnSysCommand(UINT notification_code, CPoint click);
  void OnMove(const CSize& size);
  void OnMoving(UINT param, const LPRECT new_bounds);
  void OnSize(UINT param, const CSize& size);
  void OnFinalMessage(HWND hwnd);
  void OnPaint(HDC dc);
  LRESULT OnEraseBkgnd(HDC dc);

  void ArmOnMouseLeave();
  void OnCaptureChanged(HWND hwnd);
  LRESULT OnMouseRange(UINT u_msg, WPARAM w_param, LPARAM l_param,
                       BOOL& handled);
  LRESULT OnNotify(int w_param, NMHDR* hdr);
  LRESULT OnSettingChange(UINT u_msg, WPARAM w_param, LPARAM l_param,
                          BOOL& handled);
  LRESULT OnNCActivate(BOOL param);
  BOOL OnPowerBroadcast(DWORD power_event, DWORD data);
  void OnThemeChanged();

  ////////////////////////////////////////////////////////////////////////////////
  // BrowserWindow implementation
  ////////////////////////////////////////////////////////////////////////////////

  virtual void Init();
  virtual void Show(int command, bool adjust_to_fit);
  virtual void Close();
  virtual void* GetPlatformID();
  virtual void ShowTabContents(TabContents* contents);
  virtual TabStrip* GetTabStrip() const;
  virtual void ContinueDetachConstrainedWindowDrag(
      const gfx::Point& mouse_pt,
      int frame_component);
  virtual void SizeToContents(const gfx::Rect& contents_bounds);
  virtual void SetAcceleratorTable(
      std::map<views::Accelerator, int>* accelerator_table);
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);
  virtual gfx::Rect GetNormalBounds();
  virtual bool IsMaximized();
  virtual gfx::Rect GetBoundsForContentBounds(const gfx::Rect content_rect);
  virtual void InfoBubbleShowing();
  virtual ToolbarStarToggle* GetStarButton() const;
  virtual LocationBarView* GetLocationBarView() const;
  virtual GoButton* GetGoButton() const;
  virtual BookmarkBarView* GetBookmarkBarView();
  virtual BrowserView* GetBrowserView() const;
  virtual void UpdateToolbar(TabContents* contents, bool should_restore_state);
  virtual void ProfileChanged(Profile* profile);
  virtual void FocusToolbar();
  virtual bool IsBookmarkBarVisible() const;

  ////////////////////////////////////////////////////////////////////////////////
  // views::Container
  ////////////////////////////////////////////////////////////////////////////////

  virtual void GetBounds(CRect *out, bool including_frame) const;
  virtual void MoveToFront(bool should_activate);
  virtual HWND GetHWND() const;
  virtual void PaintNow(const CRect& update_rect);
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

 protected:
  explicit VistaFrame(Browser* browser);

  // For subclassers.
  virtual bool IsTabStripVisible() const { return true; }
  virtual bool IsToolBarVisible() const { return true; }
  virtual bool SupportsBookmarkBar() const { return true; }

  // Invoked after the HWND has been created but before the window is showing.
  // This registers tooltips. If you override, be sure and invoke this
  // implementation.
  virtual void InitAfterHWNDCreated();

  // Override as needed.
  virtual LRESULT OnNCHitTest(const CPoint& pt);
  virtual LRESULT OnNCCalcSize(BOOL w_param, LPARAM l_param);
  virtual void OnNCLButtonDown(UINT flags, const CPoint& pt);

  // Create a default tab strip. Override as needed.
  virtual TabStrip* CreateTabStrip(Browser* browser);

  // Set whether this frame represents an off the record session. This is
  // currently only called during initialization.
  void SetIsOffTheRecord(bool value);

  // Layout the various views
  virtual void Layout();

  // Return the tab contents container.
  TabContentsContainerView* GetTabContentsContainer();

  virtual void DestroyBrowser();

  // The Browser that created this frame
  Browser* browser_;

  // Top level frame view
  class VistaFrameView;
  VistaFrameView* frame_view_;

  friend class VistaFrameView;

  // The view we use to display the background under tab / toolbar area
  class VistaFrameView : public FrameView {
  public:
    explicit VistaFrameView(VistaFrame* parent)
        : FrameView(parent),
          contents_offset_(0),
          parent_(parent) {
    }

    virtual void Paint(ChromeCanvas* canvas);

    void SetContentsOffset(int o);

    // Accessibility information
    bool GetAccessibleRole(VARIANT* role);

    // Returns a brief, identifying string, containing a unique, readable name.
    bool GetAccessibleName(std::wstring* name);

    // Assigns an accessible string name.
    void SetAccessibleName(const std::wstring& name);

   protected:
    // Overriden to return false if over the min/max/close buttons of the frame.
    virtual bool ShouldForwardToTabStrip(const views::DropTargetEvent& event);

   private:
    void PaintContentsBorder(ChromeCanvas* canvas,
           int x,
           int y,
           int w,
           int h);

    int contents_offset_;

    VistaFrame* parent_;

    // Storage of strings needed for accessibility.
    std::wstring accessible_name_;

    DISALLOW_EVIL_CONSTRUCTORS(VistaFrameView);
  };

 private:
  //
  // View events propagation
  //
  bool ProcessMousePressed(const CPoint& pt, UINT flags, bool dbl_click);
  void ProcessMouseDragged(const CPoint& pt, UINT flags);
  void ProcessMouseReleased(const CPoint& pt, UINT flags);
  void ProcessMouseMoved(const CPoint &pt, UINT flags);
  void ProcessMouseExited();

  // Reset the black DWM frame (the black non-glass area with lightly-colored
  // border that sits behind our content). Generally there's no single place
  // to call this, as Vista likes to reset the frame in different situations
  // (return from maximize, non-composited > composited, app launch), and
  // the notifications we receive and the timing of those notifications
  // varies so wildly that we end up having to call this in many different
  // message handlers.
  void ResetDWMFrame();

  // Initialize static members the first time this is invoked
  static void InitializeIfNeeded();

  // Updates a single view. If *view differs from new_view the old view is
  // removed and the new view is added.
  //
  // This is intended to be used when swapping in/out child views that are
  // referenced via a field.
  //
  // This function returns true if anything was changed. The caller should
  // ensure that Layout() is eventually called in this case.
  bool UpdateChildViewAndLayout(views::View* new_view, views::View** view);

  // Implementation for ShelfVisibilityChanged(). Updates the various shelf
  // fields. If one of the shelves has changed (or it's size has changed) and
  // current_tab is non-NULL layout occurs.
  void ShelfVisibilityChangedImpl(TabContents* current_tab);

  // Root view
  views::RootView root_view_;

  // Tooltip Manager
  scoped_ptr<views::TooltipManager> tooltip_manager_;

  // The optional container for the off the record icon.
  views::ImageView* off_the_record_image_;

  // The container for the distributor logo.
  views::ImageView* distributor_logo_;

  // The view that contains the tabs and any associated controls.
  TabStrip* tabstrip_;

  // The bookmark bar. This is lazily created.
  scoped_ptr<BookmarkBarView> bookmark_bar_view_;

  // The visible bookmark bar. NULL if none is visible.
  views::View* active_bookmark_bar_;

  // Browser contents
  TabContentsContainerView* tab_contents_container_;

  // Whether we enabled our custom window yet.
  bool custom_window_enabled_;

  // Whether we saved the window placement yet.
  bool saved_window_placement_;

  // whether we are expecting a mouse leave event
  bool on_mouse_leave_armed_;

  // whether we are currently processing a view level drag session
  bool in_drag_session_;

  // A view positioned at the bottom of the frame.
  views::View* shelf_view_;

  // A view positioned beneath the bookmark bar view.
  // Implementation mirrors shelf_view_
  views::View* info_bar_view_;

  // We need to own the text of the menu, the Windows API does not copy it.
  std::wstring task_manager_label_text_;

  // A mapping between accelerators and commands.
  scoped_ptr<std::map<views::Accelerator, int>> accelerator_table_;

  // Whether this frame needs a layout or not.
  bool needs_layout_;

  // Whether this frame represents an off the record session.
  bool is_off_the_record_;

  // Static resources
  static bool g_initialized;
  static SkBitmap** g_bitmaps;

  // Instance of accessibility information and handling for MSAA root
  CComPtr<IAccessible> accessibility_root_;

  // This is used to make sure we visually look active when the bubble is shown
  // even though we aren't active. When the info bubble is shown it takes
  // activation. Visually we still want the frame to look active. To make the
  // frame look active when we get WM_NCACTIVATE and ignore_ncactivate_ is
  // true, we tell the frame it is still active. When the info bubble closes we
  // tell the window it is no longer active. This results in painting the
  // active frame even though we aren't active.
  bool ignore_ncactivate_;

  // Whether we should save the bounds of the window. We don't want to
  // save the default size of windows for certain classes of windows
  // like unconstrained popups. Defaults to true.
  bool should_save_window_placement_;

  // A view that holds the client-area contents of the browser window.
  BrowserView* browser_view_;

  DISALLOW_EVIL_CONSTRUCTORS(VistaFrame);
};
#endif  // CHROME_BROWSER_VIEWS_OLD_FRAMES_VISTA_FRAME_H__

