// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget_win.h"

#include "app/gfx/canvas.h"
#include "app/gfx/path.h"
#include "app/win_util.h"
#include "base/gfx/native_theme.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "views/accessibility/view_accessibility.h"
#include "views/controls/native_control_win.h"
#include "views/fill_layout.h"
#include "views/focus/focus_util_win.h"
#include "views/views_delegate.h"
#include "views/widget/aero_tooltip_manager.h"
#include "views/widget/default_theme_provider.h"
#include "views/widget/root_view.h"
#include "views/window/window_win.h"

namespace views {

static const DWORD kWindowDefaultChildStyle =
    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
static const DWORD kWindowDefaultStyle = WS_OVERLAPPEDWINDOW;
static const DWORD kWindowDefaultExStyle = 0;

// Property used to link the HWND to its RootView.
static const wchar_t* const kRootViewWindowProperty = L"__ROOT_VIEW__";

bool SetRootViewForHWND(HWND hwnd, RootView* root_view) {
  return ::SetProp(hwnd, kRootViewWindowProperty, root_view) ? true : false;
}

RootView* GetRootViewForHWND(HWND hwnd) {
  return reinterpret_cast<RootView*>(::GetProp(hwnd, kRootViewWindowProperty));
}

NativeControlWin* GetNativeControlWinForHWND(HWND hwnd) {
  return reinterpret_cast<NativeControlWin*>(
      ::GetProp(hwnd, NativeControlWin::kNativeControlWinKey));
}

///////////////////////////////////////////////////////////////////////////////
// Window class tracking.

// static
const wchar_t* const WidgetWin::kBaseClassName =
    L"Chrome_WidgetWin_";

// Window class information used for registering unique windows.
struct ClassInfo {
  UINT style;
  HBRUSH background;

  explicit ClassInfo(int style)
      : style(style),
        background(NULL) {}

  // Compares two ClassInfos. Returns true if all members match.
  bool Equals(const ClassInfo& other) const {
    return (other.style == style && other.background == background);
  }
};

class ClassRegistrar {
 public:
  ~ClassRegistrar() {
    for (RegisteredClasses::iterator i = registered_classes_.begin();
         i != registered_classes_.end(); ++i) {
      UnregisterClass(i->name.c_str(), NULL);
    }
  }

  // Puts the name for the class matching |class_info| in |class_name|, creating
  // a new name if the class is not yet known.
  // Returns true if this class was already known, false otherwise.
  bool RetrieveClassName(const ClassInfo& class_info, std::wstring* name) {
    for (RegisteredClasses::const_iterator i = registered_classes_.begin();
         i != registered_classes_.end(); ++i) {
      if (class_info.Equals(i->info)) {
        name->assign(i->name);
        return true;
      }
    }

    name->assign(std::wstring(WidgetWin::kBaseClassName) +
        IntToWString(registered_count_++));
    return false;
  }

  void RegisterClass(const ClassInfo& class_info,
                     const std::wstring& name,
                     ATOM atom) {
    registered_classes_.push_back(RegisteredClass(class_info, name, atom));
  }

 private:
  // Represents a registered window class.
  struct RegisteredClass {
    RegisteredClass(const ClassInfo& info,
                    const std::wstring& name,
                    ATOM atom)
        : info(info),
          name(name),
          atom(atom) {
    }

    // Info used to create the class.
    ClassInfo info;

    // The name given to the window.
    std::wstring name;

    // The ATOM returned from creating the window.
    ATOM atom;
  };

  ClassRegistrar() : registered_count_(0) { }
  friend struct DefaultSingletonTraits<ClassRegistrar>;

  typedef std::list<RegisteredClass> RegisteredClasses;
  RegisteredClasses registered_classes_;

  // Counter of how many classes have ben registered so far.
  int registered_count_;

  DISALLOW_COPY_AND_ASSIGN(ClassRegistrar);
};

///////////////////////////////////////////////////////////////////////////////
// WidgetWin, public

WidgetWin::WidgetWin()
    : close_widget_factory_(this),
      active_mouse_tracking_flags_(0),
      has_capture_(false),
      window_style_(0),
      window_ex_style_(kWindowDefaultExStyle),
      use_layered_buffer_(true),
      layered_alpha_(255),
      delete_on_destroy_(true),
      can_update_layered_window_(true),
      last_mouse_event_was_move_(false),
      is_mouse_down_(false),
      is_window_(false),
      class_style_(CS_DBLCLKS),
      hwnd_(NULL) {
}

WidgetWin::~WidgetWin() {
  MessageLoopForUI::current()->RemoveObserver(this);
}

void WidgetWin::Init(HWND parent, const gfx::Rect& bounds) {
  if (window_style_ == 0)
    window_style_ = parent ? kWindowDefaultChildStyle : kWindowDefaultStyle;

  // See if the style has been overridden.
  opaque_ = !(window_ex_style_ & WS_EX_TRANSPARENT);
  use_layered_buffer_ = (use_layered_buffer_ &&
                         !!(window_ex_style_ & WS_EX_LAYERED));

  // Force creation of the RootView if it hasn't been created yet.
  GetRootView();

  default_theme_provider_.reset(new DefaultThemeProvider());

  // Ensures the parent we have been passed is valid, otherwise CreateWindowEx
  // will fail.
  if (parent && !::IsWindow(parent)) {
    NOTREACHED() << "invalid parent window specified.";
    parent = NULL;
  }

  hwnd_ = CreateWindowEx(window_ex_style_, GetWindowClassName().c_str(), L"",
                         window_style_, bounds.x(), bounds.y(), bounds.width(),
                         bounds.height(), parent, NULL, NULL, this);
  DCHECK(hwnd_);
  SetWindowSupportsRerouteMouseWheel(hwnd_);

  // The window procedure should have set the data for us.
  DCHECK(win_util::GetWindowUserData(hwnd_) == this);

  root_view_->OnWidgetCreated();

  if ((window_style_ & WS_CHILD) == 0) {
    // Top-level widgets get a FocusManager.
    focus_manager_.reset(new FocusManager(this));
  }

  // Sets the RootView as a property, so the automation can introspect windows.
  SetRootViewForHWND(hwnd_, root_view_.get());

  MessageLoopForUI::current()->AddObserver(this);

  // Windows special DWM window frame requires a special tooltip manager so
  // that window controls in Chrome windows don't flicker when you move your
  // mouse over them. See comment in aero_tooltip_manager.h.
  if (GetThemeProvider()->ShouldUseNativeFrame()) {
    tooltip_manager_.reset(new AeroTooltipManager(this));
  } else {
    tooltip_manager_.reset(new TooltipManagerWin(this));
  }

  // This message initializes the window so that focus border are shown for
  // windows.
  ::SendMessage(GetNativeView(),
                WM_CHANGEUISTATE,
                MAKELPARAM(UIS_CLEAR, UISF_HIDEFOCUS),
                0);

  // Bug 964884: detach the IME attached to this window.
  // We should attach IMEs only when we need to input CJK strings.
  ::ImmAssociateContextEx(GetNativeView(), NULL, 0);
}

void WidgetWin::SetContentsView(View* view) {
  DCHECK(view && hwnd_) << "Can't be called until after the HWND is created!";
  // The ContentsView must be set up _after_ the window is created so that its
  // Widget pointer is valid.
  root_view_->SetLayoutManager(new FillLayout);
  if (root_view_->GetChildViewCount() != 0)
    root_view_->RemoveAllChildViews(true);
  root_view_->AddChildView(view);

  // Force a layout now, since the attached hierarchy won't be ready for the
  // containing window's bounds. Note that we call Layout directly rather than
  // calling ChangeSize, since the RootView's bounds may not have changed, which
  // will cause the Layout not to be done otherwise.
  root_view_->Layout();
}

///////////////////////////////////////////////////////////////////////////////
// Widget implementation:

void WidgetWin::GetBounds(gfx::Rect* out, bool including_frame) const {
  CRect crect;
  if (including_frame) {
    GetWindowRect(&crect);
    *out = gfx::Rect(crect);
    return;
  }

  GetClientRect(&crect);
  POINT p = {0, 0};
  ::ClientToScreen(hwnd_, &p);
  out->SetRect(crect.left + p.x, crect.top + p.y,
               crect.Width(), crect.Height());
}

void WidgetWin::SetBounds(const gfx::Rect& bounds) {
  SetWindowPos(NULL, bounds.x(), bounds.y(), bounds.width(), bounds.height(),
               SWP_NOACTIVATE | SWP_NOZORDER);
}

void WidgetWin::SetShape(const gfx::Path& shape) {
  SetWindowRgn(shape.CreateHRGN(), TRUE);
}

void WidgetWin::Close() {
  if (!IsWindow())
    return;  // No need to do anything.

  // Let's hide ourselves right away.
  Hide();

  if (close_widget_factory_.empty()) {
    // And we delay the close so that if we are called from an ATL callback,
    // we don't destroy the window before the callback returned (as the caller
    // may delete ourselves on destroy and the ATL callback would still
    // dereference us when the callback returns).
    MessageLoop::current()->PostTask(FROM_HERE,
        close_widget_factory_.NewRunnableMethod(
            &WidgetWin::CloseNow));
  }
}

void WidgetWin::CloseNow() {
  // We may already have been destroyed if the selection resulted in a tab
  // switch which will have reactivated the browser window and closed us, so
  // we need to check to see if we're still a window before trying to destroy
  // ourself.
  if (IsWindow())
    DestroyWindow();
}

void WidgetWin::Show() {
  if (IsWindow())
    ShowWindow(SW_SHOWNOACTIVATE);
}

void WidgetWin::Hide() {
  if (IsWindow()) {
    // NOTE: Be careful not to activate any windows here (for example, calling
    // ShowWindow(SW_HIDE) will automatically activate another window).  This
    // code can be called while a window is being deactivated, and activating
    // another window will screw up the activation that is already in progress.
    SetWindowPos(NULL, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE |
                 SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
  }
}

gfx::NativeView WidgetWin::GetNativeView() const {
  return hwnd_;
}

static BOOL CALLBACK EnumChildProcForRedraw(HWND hwnd, LPARAM lparam) {
  DWORD process_id;
  GetWindowThreadProcessId(hwnd, &process_id);
  gfx::Rect invalid_rect = *reinterpret_cast<gfx::Rect*>(lparam);

  RECT window_rect;
  GetWindowRect(hwnd, &window_rect);
  invalid_rect.Offset(-window_rect.left, -window_rect.top);

  int flags = RDW_INVALIDATE | RDW_NOCHILDREN | RDW_FRAME;
  if (process_id == GetCurrentProcessId())
    flags |= RDW_UPDATENOW;
  RedrawWindow(hwnd, &invalid_rect.ToRECT(), NULL, flags);
  return TRUE;
}

void WidgetWin::PaintNow(const gfx::Rect& update_rect) {
  if (use_layered_buffer_) {
    PaintLayeredWindow();
  } else if (root_view_->NeedsPainting(false) && IsWindow()) {
    if (!opaque_ && GetParent()) {
      // We're transparent. Need to force painting to occur from our parent.
      CRect parent_update_rect = update_rect.ToRECT();
      POINT location_in_parent = { 0, 0 };
      ClientToScreen(hwnd_, &location_in_parent);
      ::ScreenToClient(GetParent(), &location_in_parent);
      parent_update_rect.OffsetRect(location_in_parent);
      ::RedrawWindow(GetParent(), parent_update_rect, NULL,
                     RDW_UPDATENOW | RDW_INVALIDATE | RDW_ALLCHILDREN);
    } else {
      // Paint child windows that are in a different process asynchronously.
      // This prevents a hang in other processes from blocking this process.

      // Calculate the invalid rect in screen coordinates before  the first
      // RedrawWindow call to the parent HWND, since that will empty update_rect
      // (which comes from a member variable) in the OnPaint call.
      CRect screen_rect_temp;
      GetWindowRect(&screen_rect_temp);
      gfx::Rect screen_rect(screen_rect_temp);
      gfx::Rect invalid_screen_rect = update_rect;
      invalid_screen_rect.Offset(screen_rect.x(), screen_rect.y());

      ::RedrawWindow(hwnd_, &update_rect.ToRECT(), NULL,
                     RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOCHILDREN);

      LPARAM lparam = reinterpret_cast<LPARAM>(&invalid_screen_rect);
      EnumChildWindows(hwnd_, EnumChildProcForRedraw, lparam);
    }
    // As we were created with a style of WS_CLIPCHILDREN redraw requests may
    // result in an empty paint rect in WM_PAINT (this'll happen if a
    // child HWND completely contains the update _rect). In such a scenario
    // RootView would never get a ProcessPaint and always think it needs to
    // be painted (leading to a steady stream of RedrawWindow requests on every
    // event). For this reason we tell RootView it doesn't need to paint
    // here.
    root_view_->ClearPaintRect();
  }
}

void WidgetWin::SetOpacity(unsigned char opacity) {
  layered_alpha_ = static_cast<BYTE>(opacity);
}

RootView* WidgetWin::GetRootView() {
  if (!root_view_.get()) {
    // First time the root view is being asked for, create it now.
    root_view_.reset(CreateRootView());
  }
  return root_view_.get();
}

Widget* WidgetWin::GetRootWidget() const {
  return reinterpret_cast<WidgetWin*>(
      win_util::GetWindowUserData(GetAncestor(hwnd_, GA_ROOT)));
}

bool WidgetWin::IsVisible() const {
  return !!::IsWindowVisible(GetNativeView());
}

bool WidgetWin::IsActive() const {
  return win_util::IsWindowActive(GetNativeView());
}

TooltipManager* WidgetWin::GetTooltipManager() {
  return tooltip_manager_.get();
}

ThemeProvider* WidgetWin::GetThemeProvider() const {
  Widget* widget = GetRootWidget();
  if (widget && widget != this) {
    // Attempt to get the theme provider, and fall back to the default theme
    // provider if not found.
    ThemeProvider* provider = widget->GetThemeProvider();
    if (provider)
      return provider;

    provider = widget->GetDefaultThemeProvider();
    if (provider)
      return provider;
  }
  return default_theme_provider_.get();
}

Window* WidgetWin::GetWindow() {
  return GetWindowImpl(hwnd_);
}

const Window* WidgetWin::GetWindow() const {
  return GetWindowImpl(hwnd_);
}

FocusManager* WidgetWin::GetFocusManager() {
  if (focus_manager_.get())
    return focus_manager_.get();

  WidgetWin* widget = static_cast<WidgetWin*>(GetRootWidget());
  if (widget && widget != this) {
    // WidgetWin subclasses may override GetFocusManager(), for example for
    // dealing with cases where the widget has been unparented.
    return widget->GetFocusManager();
  }
  return NULL;
}

void WidgetWin::SetUseLayeredBuffer(bool use_layered_buffer) {
  if (use_layered_buffer_ == use_layered_buffer)
    return;

  use_layered_buffer_ = use_layered_buffer;
  if (!hwnd_)
    return;

  if (use_layered_buffer_) {
    // Force creation of the buffer at the right size.
    RECT wr;
    GetWindowRect(&wr);
    ChangeSize(0, CSize(wr.right - wr.left, wr.bottom - wr.top));
  } else {
    contents_.reset(NULL);
  }
}

static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM l_param) {
  RootView* root_view =
    reinterpret_cast<RootView*>(GetProp(hwnd, kRootViewWindowProperty));
  if (root_view) {
    *reinterpret_cast<RootView**>(l_param) = root_view;
    return FALSE;  // Stop enumerating.
  }
  return TRUE;  // Keep enumerating.
}

// static
RootView* WidgetWin::FindRootView(HWND hwnd) {
  RootView* root_view =
    reinterpret_cast<RootView*>(GetProp(hwnd, kRootViewWindowProperty));
  if (root_view)
    return root_view;

  // Enumerate all children and check if they have a RootView.
  EnumChildWindows(hwnd, EnumChildProc, reinterpret_cast<LPARAM>(&root_view));

  return root_view;
}

// static
WidgetWin* WidgetWin::GetWidget(HWND hwnd) {
  return reinterpret_cast<WidgetWin*>(win_util::GetWindowUserData(hwnd));
}

////////////////////////////////////////////////////////////////////////////////
// MessageLoop::Observer

void WidgetWin::WillProcessMessage(const MSG& msg) {
}

void WidgetWin::DidProcessMessage(const MSG& msg) {
  if (root_view_->NeedsPainting(true)) {
    PaintNow(root_view_->GetScheduledPaintRect());
  }
}

////////////////////////////////////////////////////////////////////////////////
// FocusTraversable

View* WidgetWin::FindNextFocusableView(
    View* starting_view, bool reverse, Direction direction,
    bool check_starting_view, FocusTraversable** focus_traversable,
    View** focus_traversable_view) {
  return root_view_->FindNextFocusableView(starting_view,
                                           reverse,
                                           direction,
                                           check_starting_view,
                                           focus_traversable,
                                           focus_traversable_view);
}

FocusTraversable* WidgetWin::GetFocusTraversableParent() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

void WidgetWin::SetFocusTraversableParent(FocusTraversable* parent) {
  root_view_->SetFocusTraversableParent(parent);
}

View* WidgetWin::GetFocusTraversableParentView() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

void WidgetWin::SetFocusTraversableParentView(View* parent_view) {
  root_view_->SetFocusTraversableParentView(parent_view);
}

///////////////////////////////////////////////////////////////////////////////
// Message handlers

void WidgetWin::OnCaptureChanged(HWND hwnd) {
  if (has_capture_) {
    if (is_mouse_down_)
      root_view_->ProcessMouseDragCanceled();
    is_mouse_down_ = false;
    has_capture_ = false;
  }
}

void WidgetWin::OnClose() {
  Close();
}

void WidgetWin::OnDestroy() {
  root_view_->OnWidgetDestroyed();

  RemoveProp(hwnd_, kRootViewWindowProperty);
}

LRESULT WidgetWin::OnEraseBkgnd(HDC dc) {
  // This is needed for magical win32 flicker ju-ju
  return 1;
}

LRESULT WidgetWin::OnGetObject(UINT uMsg, WPARAM w_param, LPARAM l_param) {
  LRESULT reference_result = static_cast<LRESULT>(0L);

  // Accessibility readers will send an OBJID_CLIENT message
  if (OBJID_CLIENT == l_param) {
    // If our MSAA root is already created, reuse that pointer. Otherwise,
    // create a new one.
    if (!accessibility_root_) {
      CComObject<ViewAccessibility>* instance = NULL;

      HRESULT hr = CComObject<ViewAccessibility>::CreateInstance(&instance);
      DCHECK(SUCCEEDED(hr));

      if (!instance) {
        // Return with failure.
        return static_cast<LRESULT>(0L);
      }

      CComPtr<IAccessible> accessibility_instance(instance);

      if (!SUCCEEDED(instance->Initialize(root_view_.get()))) {
        // Return with failure.
        return static_cast<LRESULT>(0L);
      }

      // All is well, assign the temp instance to the class smart pointer
      accessibility_root_.Attach(accessibility_instance.Detach());

      if (!accessibility_root_) {
        // Return with failure.
        return static_cast<LRESULT>(0L);
      }
    }

    // Create a reference to ViewAccessibility that MSAA will marshall
    // to the client.
    reference_result = LresultFromObject(IID_IAccessible, w_param,
        static_cast<IAccessible*>(accessibility_root_));
  }
  return reference_result;
}

void WidgetWin::OnKeyDown(TCHAR c, UINT rep_cnt, UINT flags) {
  KeyEvent event(Event::ET_KEY_PRESSED, c, rep_cnt, flags);
  SetMsgHandled(root_view_->ProcessKeyEvent(event));
}

void WidgetWin::OnKeyUp(TCHAR c, UINT rep_cnt, UINT flags) {
  KeyEvent event(Event::ET_KEY_RELEASED, c, rep_cnt, flags);
  SetMsgHandled(root_view_->ProcessKeyEvent(event));
}

void WidgetWin::OnLButtonDown(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_LBUTTON, false, false);
}

void WidgetWin::OnLButtonUp(UINT flags, const CPoint& point) {
  ProcessMouseReleased(point, flags | MK_LBUTTON);
}

void WidgetWin::OnLButtonDblClk(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_LBUTTON, true, false);
}

void WidgetWin::OnMButtonDown(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_MBUTTON, false, false);
}

void WidgetWin::OnMButtonUp(UINT flags, const CPoint& point) {
  ProcessMouseReleased(point, flags | MK_MBUTTON);
}

void WidgetWin::OnMButtonDblClk(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_MBUTTON, true, false);
}

LRESULT WidgetWin::OnMouseActivate(HWND window, UINT hittest_code,
                                   UINT message) {
  SetMsgHandled(FALSE);
  return MA_ACTIVATE;
}

void WidgetWin::OnMouseMove(UINT flags, const CPoint& point) {
  ProcessMouseMoved(point, flags, false);
}

LRESULT WidgetWin::OnMouseLeave(UINT message, WPARAM w_param, LPARAM l_param) {
  tooltip_manager_->OnMouseLeave();
  ProcessMouseExited();
  return 0;
}

LRESULT WidgetWin::OnMouseWheel(UINT message, WPARAM w_param, LPARAM l_param) {
  // Reroute the mouse-wheel to the window under the mouse pointer if
  // applicable.
  if (message == WM_MOUSEWHEEL &&
      views::RerouteMouseWheel(hwnd_, w_param, l_param)) {
    return 0;
  }

  int flags = GET_KEYSTATE_WPARAM(w_param);
  short distance = GET_WHEEL_DELTA_WPARAM(w_param);
  int x = GET_X_LPARAM(l_param);
  int y = GET_Y_LPARAM(l_param);
  MouseWheelEvent e(distance, x, y, Event::ConvertWindowsFlags(flags));
  return root_view_->ProcessMouseWheelEvent(e) ? 0 : 1;
}

LRESULT WidgetWin::OnMouseRange(UINT msg, WPARAM w_param, LPARAM l_param) {
  tooltip_manager_->OnMouse(msg, w_param, l_param);
  SetMsgHandled(FALSE);
  return 0;
}

void WidgetWin::OnNCLButtonDblClk(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_LBUTTON, true, true));
}

void WidgetWin::OnNCLButtonDown(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_LBUTTON, false, true));
}

void WidgetWin::OnNCLButtonUp(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

void WidgetWin::OnNCMButtonDblClk(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_MBUTTON, true, true));
}

void WidgetWin::OnNCMButtonDown(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_MBUTTON, false, true));
}

void WidgetWin::OnNCMButtonUp(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

LRESULT WidgetWin::OnNCMouseLeave(UINT uMsg, WPARAM w_param, LPARAM l_param) {
  ProcessMouseExited();
  return 0;
}

LRESULT WidgetWin::OnNCMouseMove(UINT flags, const CPoint& point) {
  // NC points are in screen coordinates.
  CPoint temp = point;
  MapWindowPoints(HWND_DESKTOP, GetNativeView(), &temp, 1);
  ProcessMouseMoved(temp, 0, true);

  // We need to process this message to stop Windows from drawing the window
  // controls as the mouse moves over the title bar area when the window is
  // maximized.
  return 0;
}

void WidgetWin::OnNCRButtonDblClk(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_RBUTTON, true, true));
}

void WidgetWin::OnNCRButtonDown(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_RBUTTON, false, true));
}

void WidgetWin::OnNCRButtonUp(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

LRESULT WidgetWin::OnNotify(int w_param, NMHDR* l_param) {
  // We can be sent this message before the tooltip manager is created, if a
  // subclass overrides OnCreate and creates some kind of Windows control there
  // that sends WM_NOTIFY messages.
  if (tooltip_manager_.get()) {
    bool handled;
    LRESULT result = tooltip_manager_->OnNotify(w_param, l_param, &handled);
    SetMsgHandled(handled);
    return result;
  }
  SetMsgHandled(FALSE);
  return 0;
}

void WidgetWin::OnPaint(HDC dc) {
  root_view_->OnPaint(GetNativeView());
}

void WidgetWin::OnRButtonDown(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_RBUTTON, false, false);
}

void WidgetWin::OnRButtonUp(UINT flags, const CPoint& point) {
  ProcessMouseReleased(point, flags | MK_RBUTTON);
}

void WidgetWin::OnRButtonDblClk(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_RBUTTON, true, false);
}

void WidgetWin::OnSize(UINT param, const CSize& size) {
  ChangeSize(param, size);
}

void WidgetWin::OnThemeChanged() {
  // Notify NativeTheme.
  gfx::NativeTheme::instance()->CloseHandles();
}

void WidgetWin::OnFinalMessage(HWND window) {
  if (delete_on_destroy_)
    delete this;
}

///////////////////////////////////////////////////////////////////////////////
// WidgetWin, protected:

void WidgetWin::TrackMouseEvents(DWORD mouse_tracking_flags) {
  // Begin tracking mouse events for this HWND so that we get WM_MOUSELEAVE
  // when the user moves the mouse outside this HWND's bounds.
  if (active_mouse_tracking_flags_ == 0 || mouse_tracking_flags & TME_CANCEL) {
    if (mouse_tracking_flags & TME_CANCEL) {
      // We're about to cancel active mouse tracking, so empty out the stored
      // state.
      active_mouse_tracking_flags_ = 0;
    } else {
      active_mouse_tracking_flags_ = mouse_tracking_flags;
    }

    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = mouse_tracking_flags;
    tme.hwndTrack = GetNativeView();
    tme.dwHoverTime = 0;
    TrackMouseEvent(&tme);
  } else if (mouse_tracking_flags != active_mouse_tracking_flags_) {
    TrackMouseEvents(active_mouse_tracking_flags_ | TME_CANCEL);
    TrackMouseEvents(mouse_tracking_flags);
  }
}

bool WidgetWin::ProcessMousePressed(const CPoint& point,
                                    UINT flags,
                                    bool dbl_click,
                                    bool non_client) {
  last_mouse_event_was_move_ = false;
  // Windows gives screen coordinates for nonclient events, while the RootView
  // expects window coordinates; convert if necessary.
  gfx::Point converted_point(point);
  if (non_client)
    View::ConvertPointToView(NULL, root_view_.get(), &converted_point);
  MouseEvent mouse_pressed(Event::ET_MOUSE_PRESSED,
                           converted_point.x(),
                           converted_point.y(),
                           (dbl_click ? MouseEvent::EF_IS_DOUBLE_CLICK : 0) |
                           (non_client ? MouseEvent::EF_IS_NON_CLIENT : 0) |
                           Event::ConvertWindowsFlags(flags));
  if (root_view_->OnMousePressed(mouse_pressed)) {
    is_mouse_down_ = true;
    if (!has_capture_) {
      SetCapture();
      has_capture_ = true;
    }
    return true;
  }
  return false;
}

void WidgetWin::ProcessMouseDragged(const CPoint& point, UINT flags) {
  last_mouse_event_was_move_ = false;
  MouseEvent mouse_drag(Event::ET_MOUSE_DRAGGED,
                        point.x,
                        point.y,
                        Event::ConvertWindowsFlags(flags));
  root_view_->OnMouseDragged(mouse_drag);
}

void WidgetWin::ProcessMouseReleased(const CPoint& point, UINT flags) {
  last_mouse_event_was_move_ = false;
  MouseEvent mouse_up(Event::ET_MOUSE_RELEASED,
                      point.x,
                      point.y,
                      Event::ConvertWindowsFlags(flags));
  // Release the capture first, that way we don't get confused if
  // OnMouseReleased blocks.
  if (has_capture_ && ReleaseCaptureOnMouseReleased()) {
    has_capture_ = false;
    ReleaseCapture();
  }
  is_mouse_down_ = false;
  root_view_->OnMouseReleased(mouse_up, false);
}

void WidgetWin::ProcessMouseMoved(const CPoint &point, UINT flags,
                                  bool is_nonclient) {
  // Windows only fires WM_MOUSELEAVE events if the application begins
  // "tracking" mouse events for a given HWND during WM_MOUSEMOVE events.
  // We need to call |TrackMouseEvents| to listen for WM_MOUSELEAVE.
  if (!has_capture_)
    TrackMouseEvents(is_nonclient ? TME_NONCLIENT | TME_LEAVE : TME_LEAVE);
  if (has_capture_ && is_mouse_down_) {
    ProcessMouseDragged(point, flags);
  } else {
    gfx::Point screen_loc(point);
    View::ConvertPointToScreen(root_view_.get(), &screen_loc);
    if (last_mouse_event_was_move_ && last_mouse_move_x_ == screen_loc.x() &&
        last_mouse_move_y_ == screen_loc.y()) {
      // Don't generate a mouse event for the same location as the last.
      return;
    }
    last_mouse_move_x_ = screen_loc.x();
    last_mouse_move_y_ = screen_loc.y();
    last_mouse_event_was_move_ = true;
    MouseEvent mouse_move(Event::ET_MOUSE_MOVED,
                          point.x,
                          point.y,
                          Event::ConvertWindowsFlags(flags));
    root_view_->OnMouseMoved(mouse_move);
  }
}

void WidgetWin::ProcessMouseExited() {
  last_mouse_event_was_move_ = false;
  root_view_->ProcessOnMouseExited();
  // Reset our tracking flag so that future mouse movement over this WidgetWin
  // results in a new tracking session.
  active_mouse_tracking_flags_ = 0;
}

void WidgetWin::ChangeSize(UINT size_param, const CSize& size) {
  CRect rect;
  if (use_layered_buffer_) {
    GetWindowRect(&rect);
    SizeContents(rect);
  } else {
    GetClientRect(&rect);
  }

  // Resizing changes the size of the view hierarchy and thus forces a
  // complete relayout.
  root_view_->SetBounds(0, 0, rect.Width(), rect.Height());
  root_view_->SchedulePaint();

  if (use_layered_buffer_)
    PaintNow(gfx::Rect(rect));
}

RootView* WidgetWin::CreateRootView() {
  return new RootView(this);
}

///////////////////////////////////////////////////////////////////////////////
// WidgetWin, private:

// static
Window* WidgetWin::GetWindowImpl(HWND hwnd) {
  // NOTE: we can't use GetAncestor here as constrained windows are a Window,
  // but not a top level window.
  HWND parent = hwnd;
  while (parent) {
    WidgetWin* widget =
        reinterpret_cast<WidgetWin*>(win_util::GetWindowUserData(parent));
    if (widget && widget->is_window_)
      return static_cast<WindowWin*>(widget);
    parent = ::GetParent(parent);
  }
  return NULL;
}

void WidgetWin::SizeContents(const CRect& window_rect) {
  contents_.reset(new gfx::Canvas(window_rect.Width(),
                                  window_rect.Height(),
                                  false));
}

void WidgetWin::PaintLayeredWindow() {
  // Painting monkeys with our cliprect, so we need to save it so that the
  // call to UpdateLayeredWindow updates the entire window, not just the
  // cliprect.
  contents_->save(SkCanvas::kClip_SaveFlag);
  gfx::Rect dirty_rect = root_view_->GetScheduledPaintRect();
  contents_->ClipRectInt(dirty_rect.x(), dirty_rect.y(), dirty_rect.width(),
                         dirty_rect.height());
  root_view_->ProcessPaint(contents_.get());
  contents_->restore();

  UpdateWindowFromContents(contents_->getTopPlatformDevice().getBitmapDC());
}

void WidgetWin::UpdateWindowFromContents(HDC dib_dc) {
  DCHECK(use_layered_buffer_);
  if (can_update_layered_window_) {
    CRect wr;
    GetWindowRect(&wr);
    CSize size(wr.right - wr.left, wr.bottom - wr.top);
    CPoint zero_origin(0, 0);
    CPoint window_position = wr.TopLeft();

    BLENDFUNCTION blend = {AC_SRC_OVER, 0, layered_alpha_, AC_SRC_ALPHA};
    ::UpdateLayeredWindow(
        hwnd_, NULL, &window_position, &size, dib_dc, &zero_origin,
        RGB(0xFF, 0xFF, 0xFF), &blend, ULW_ALPHA);
  }
}

std::wstring WidgetWin::GetWindowClassName() {
  ClassInfo class_info(initial_class_style());
  std::wstring name;
  if (Singleton<ClassRegistrar>()->RetrieveClassName(class_info, &name))
    return name;

  // No class found, need to register one.
  WNDCLASSEX class_ex;
  class_ex.cbSize = sizeof(WNDCLASSEX);
  class_ex.style = class_info.style;
  class_ex.lpfnWndProc = &WidgetWin::WndProc;
  class_ex.cbClsExtra = 0;
  class_ex.cbWndExtra = 0;
  class_ex.hInstance = NULL;
  class_ex.hIcon = NULL;
  if (ViewsDelegate::views_delegate)
    class_ex.hIcon = ViewsDelegate::views_delegate->GetDefaultWindowIcon();
  class_ex.hCursor = LoadCursor(NULL, IDC_ARROW);
  class_ex.hbrBackground = reinterpret_cast<HBRUSH>(class_info.background + 1);
  class_ex.lpszMenuName = NULL;
  class_ex.lpszClassName = name.c_str();
  class_ex.hIconSm = class_ex.hIcon;
  ATOM atom = RegisterClassEx(&class_ex);
  DCHECK(atom);

  Singleton<ClassRegistrar>()->RegisterClass(class_info, name, atom);

  return name;
}

// Get the source HWND of the specified message. Depending on the message, the
// source HWND is encoded in either the WPARAM or the LPARAM value.
HWND GetControlHWNDForMessage(UINT message, WPARAM w_param, LPARAM l_param) {
  // Each of the following messages can be sent by a child HWND and must be
  // forwarded to its associated NativeControlWin for handling.
  switch (message) {
    case WM_NOTIFY:
      return reinterpret_cast<NMHDR*>(l_param)->hwndFrom;
    case WM_COMMAND:
      return reinterpret_cast<HWND>(l_param);
    case WM_CONTEXTMENU:
      return reinterpret_cast<HWND>(w_param);
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
      return reinterpret_cast<HWND>(l_param);
  }
  return NULL;
}

// Some messages may be sent to us by a child HWND managed by
// NativeControlWin. If this is the case, this function will forward those
// messages on to the object associated with the source HWND and return true,
// in which case the window procedure must not do any further processing of
// the message. If there is no associated NativeControlWin, the return value
// will be false and the WndProc can continue processing the message normally.
// |l_result| contains the result of the message processing by the control and
// must be returned by the WndProc if the return value is true.
bool ProcessNativeControlMessage(UINT message,
                                 WPARAM w_param,
                                 LPARAM l_param,
                                 LRESULT* l_result) {
  *l_result = 0;

  HWND control_hwnd = GetControlHWNDForMessage(message, w_param, l_param);
  if (IsWindow(control_hwnd)) {
    NativeControlWin* wrapper = GetNativeControlWinForHWND(control_hwnd);
    if (wrapper)
      return wrapper->ProcessMessage(message, w_param, l_param, l_result);
  }

  return false;
}

// static
LRESULT CALLBACK WidgetWin::WndProc(HWND window, UINT message,
                                    WPARAM w_param, LPARAM l_param) {
  if (message == WM_NCCREATE) {
    CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(l_param);
    WidgetWin* widget = reinterpret_cast<WidgetWin*>(cs->lpCreateParams);
    DCHECK(widget);
    win_util::SetWindowUserData(window, widget);
    widget->hwnd_ = window;
    return TRUE;
  }

  WidgetWin* widget = reinterpret_cast<WidgetWin*>(
      win_util::GetWindowUserData(window));
  if (!widget)
    return 0;

  LRESULT result = 0;

  // First allow messages sent by child controls to be processed directly by
  // their associated views. If such a view is present, it will handle the
  // message *instead of* this WidgetWin.
  if (ProcessNativeControlMessage(message, w_param, l_param, &result))
    return result;

  // Otherwise we handle everything else.
  if (!widget->ProcessWindowMessage(window, message, w_param, l_param, result))
    result = DefWindowProc(window, message, w_param, l_param);
  if (message == WM_NCDESTROY)
    widget->OnFinalMessage(window);
  if (message == WM_ACTIVATE)
    PostProcessActivateMessage(widget, LOWORD(w_param));
  return result;
}

// static
void WidgetWin::PostProcessActivateMessage(WidgetWin* widget,
                                           int activation_state) {
  if (!widget->focus_manager_.get()) {
    NOTREACHED();
    return;
  }
  if (WA_INACTIVE == activation_state) {
    widget->focus_manager_->StoreFocusedView();
  } else {
    // We must restore the focus after the message has been DefProc'ed as it
    // does set the focus to the last focused HWND.
    widget->focus_manager_->RestoreFocusedView();
  }
}
}  // namespace views
