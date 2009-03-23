// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/window/dialog_client_view.h"

#include <windows.h>
#include <uxtheme.h>
#include <vsstyle.h>

#include "base/gfx/native_theme.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/controls/button/native_button.h"
#include "chrome/views/window/dialog_delegate.h"
#include "chrome/views/window/window.h"
#include "grit/generated_resources.h"

namespace views {

namespace {

// Updates any of the standard buttons according to the delegate.
void UpdateButtonHelper(NativeButton* button_view,
                        DialogDelegate* delegate,
                        DialogDelegate::DialogButton button) {
  std::wstring label = delegate->GetDialogButtonLabel(button);
  if (!label.empty())
    button_view->SetLabel(label);
  button_view->SetEnabled(delegate->IsDialogButtonEnabled(button));
  button_view->SetVisible(delegate->IsDialogButtonVisible(button));
}

void FillViewWithSysColor(ChromeCanvas* canvas, View* view, COLORREF color) {
  SkColor sk_color =
      SkColorSetRGB(GetRValue(color), GetGValue(color), GetBValue(color));
  canvas->FillRectInt(sk_color, 0, 0, view->width(), view->height());
}

// DialogButton ----------------------------------------------------------------

// DialogButtons is used for the ok/cancel buttons of the window. DialogButton
// forwards AcceleratorPressed to the delegate.

class DialogButton : public NativeButton {
 public:
  DialogButton(ButtonListener* listener,
               Window* owner,
               DialogDelegate::DialogButton type,
               const std::wstring& title,
               bool is_default)
      : NativeButton(listener, title),
        owner_(owner),
        type_(type) {
    SetIsDefault(is_default);
  }

  // Overridden to forward to the delegate.
  virtual bool AcceleratorPressed(const Accelerator& accelerator) {
    if (!owner_->GetDelegate()->AsDialogDelegate()->
        AreAcceleratorsEnabled(type_)) {
      return false;
    }
    return NativeButton::AcceleratorPressed(accelerator);
  }

 private:
  Window* owner_;
  const DialogDelegate::DialogButton type_;

  DISALLOW_COPY_AND_ASSIGN(DialogButton);
};

}  // namespace

// static
ChromeFont DialogClientView::dialog_button_font_;
static const int kDialogMinButtonWidth = 75;
static const int kDialogButtonLabelSpacing = 16;
static const int kDialogButtonContentSpacing = 5;

// The group used by the buttons.  This name is chosen voluntarily big not to
// conflict with other groups that could be in the dialog content.
static const int kButtonGroup = 6666;

///////////////////////////////////////////////////////////////////////////////
// DialogClientView, public:

DialogClientView::DialogClientView(Window* owner, View* contents_view)
    : ClientView(owner, contents_view),
      ok_button_(NULL),
      cancel_button_(NULL),
      extra_view_(NULL),
      accepted_(false),
      default_button_(NULL) {
  InitClass();
}

DialogClientView::~DialogClientView() {
}

void DialogClientView::ShowDialogButtons() {
  DialogDelegate* dd = GetDialogDelegate();
  int buttons = dd->GetDialogButtons();
  if (buttons & DialogDelegate::DIALOGBUTTON_OK && !ok_button_) {
    std::wstring label =
        dd->GetDialogButtonLabel(DialogDelegate::DIALOGBUTTON_OK);
    if (label.empty())
      label = l10n_util::GetString(IDS_OK);
    bool is_default_button =
        (dd->GetDefaultDialogButton() & DialogDelegate::DIALOGBUTTON_OK) != 0;
    ok_button_ = new DialogButton(this, window(),
                                  DialogDelegate::DIALOGBUTTON_OK, label,
                                  is_default_button);
    ok_button_->SetGroup(kButtonGroup);
    if (is_default_button)
      default_button_ = ok_button_;
    if (!(buttons & DialogDelegate::DIALOGBUTTON_CANCEL))
      ok_button_->AddAccelerator(Accelerator(VK_ESCAPE, false, false, false));
    AddChildView(ok_button_);
  }
  if (buttons & DialogDelegate::DIALOGBUTTON_CANCEL && !cancel_button_) {
    std::wstring label =
        dd->GetDialogButtonLabel(DialogDelegate::DIALOGBUTTON_CANCEL);
    if (label.empty()) {
      if (buttons & DialogDelegate::DIALOGBUTTON_OK) {
        label = l10n_util::GetString(IDS_CANCEL);
      } else {
        label = l10n_util::GetString(IDS_CLOSE);
      }
    }
    bool is_default_button =
        (dd->GetDefaultDialogButton() & DialogDelegate::DIALOGBUTTON_CANCEL)
        != 0;
    cancel_button_ = new DialogButton(this, window(),
                                      DialogDelegate::DIALOGBUTTON_CANCEL,
                                      label, is_default_button);
    cancel_button_->SetGroup(kButtonGroup);
    cancel_button_->AddAccelerator(Accelerator(VK_ESCAPE, false, false, false));
    if (is_default_button)
      default_button_ = ok_button_;
    AddChildView(cancel_button_);
  }
  if (!buttons) {
    // Register the escape key as an accelerator which will close the window
    // if there are no dialog buttons.
    AddAccelerator(Accelerator(VK_ESCAPE, false, false, false));
  }
}

void DialogClientView::SetDefaultButton(NativeButton* new_default_button) {
  if (default_button_ && default_button_ != new_default_button) {
    default_button_->SetIsDefault(false);
    default_button_ = NULL;
  }

  if (new_default_button) {
    default_button_ = new_default_button;
    default_button_->SetIsDefault(true);
  }
}

void DialogClientView::FocusWillChange(View* focused_before,
                                       View* focused_now) {
  NativeButton* new_default_button = NULL;
  if (focused_now &&
      focused_now->GetClassName() == NativeButton::kViewClassName) {
    new_default_button = static_cast<NativeButton*>(focused_now);
  } else {
    // The focused view is not a button, get the default button from the
    // delegate.
    DialogDelegate* dd = GetDialogDelegate();
    if ((dd->GetDefaultDialogButton() & DialogDelegate::DIALOGBUTTON_OK) != 0)
      new_default_button = ok_button_;
    if ((dd->GetDefaultDialogButton() & DialogDelegate::DIALOGBUTTON_CANCEL)
        != 0)
      new_default_button = cancel_button_;
  }
  SetDefaultButton(new_default_button);
}

// Changing dialog labels will change button widths.
void DialogClientView::UpdateDialogButtons() {
  DialogDelegate* dd = GetDialogDelegate();
  int buttons = dd->GetDialogButtons();

  if (buttons & DialogDelegate::DIALOGBUTTON_OK)
    UpdateButtonHelper(ok_button_, dd, DialogDelegate::DIALOGBUTTON_OK);

  if (buttons & DialogDelegate::DIALOGBUTTON_CANCEL)
    UpdateButtonHelper(cancel_button_, dd, DialogDelegate::DIALOGBUTTON_CANCEL);

  LayoutDialogButtons();
  SchedulePaint();
}

void DialogClientView::AcceptWindow() {
  if (accepted_) {
    // We should only get into AcceptWindow once.
    NOTREACHED();
    return;
  }
  accepted_ = true;
  if (GetDialogDelegate()->Accept(false))
    window()->Close();
}

void DialogClientView::CancelWindow() {
  // Call the standard Close handler, which checks with the delegate before
  // proceeding. This checking _isn't_ done here, but in the WM_CLOSE handler,
  // so that the close box on the window also shares this code path.
  window()->Close();
}

///////////////////////////////////////////////////////////////////////////////
// DialogClientView, ClientView overrides:

bool DialogClientView::CanClose() const {
  if (!accepted_) {
    DialogDelegate* dd = GetDialogDelegate();
    int buttons = dd->GetDialogButtons();
    if (buttons & DialogDelegate::DIALOGBUTTON_CANCEL)
      return dd->Cancel();
    if (buttons & DialogDelegate::DIALOGBUTTON_OK)
      return dd->Accept(true);
  }
  return true;
}

void DialogClientView::WindowClosing() {
  FocusManager* focus_manager = GetFocusManager();
  DCHECK(focus_manager);
  if (focus_manager)
     focus_manager->RemoveFocusChangeListener(this);
  ClientView::WindowClosing();
}

int DialogClientView::NonClientHitTest(const gfx::Point& point) {
  if (size_box_bounds_.Contains(point.x() - x(), point.y() - y()))
    return HTBOTTOMRIGHT;
  return ClientView::NonClientHitTest(point);
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, View overrides:

void DialogClientView::Paint(ChromeCanvas* canvas) {
  FillViewWithSysColor(canvas, this, GetSysColor(COLOR_3DFACE));
}

void DialogClientView::PaintChildren(ChromeCanvas* canvas) {
  View::PaintChildren(canvas);
  if (!window()->IsMaximized() && !window()->IsMinimized())
    PaintSizeBox(canvas);
}

void DialogClientView::Layout() {
  if (has_dialog_buttons())
    LayoutDialogButtons();
  LayoutContentsView();
}

void DialogClientView::ViewHierarchyChanged(bool is_add, View* parent,
                                            View* child) {
  if (is_add && child == this) {
    // Can only add and update the dialog buttons _after_ they are added to the
    // view hierarchy since they are native controls and require the
    // Container's HWND.
    ShowDialogButtons();
    ClientView::ViewHierarchyChanged(is_add, parent, child);

    FocusManager* focus_manager = GetFocusManager();
    // Listen for focus change events so we can update the default button.
    DCHECK(focus_manager);  // bug #1291225: crash reports seem to indicate it
                            // can be NULL.
    if (focus_manager)
      focus_manager->AddFocusChangeListener(this);

    // The "extra view" must be created and installed after the contents view
    // has been inserted into the view hierarchy.
    CreateExtraView();
    UpdateDialogButtons();
    Layout();
  }
}

gfx::Size DialogClientView::GetPreferredSize() {
  gfx::Size prefsize = contents_view()->GetPreferredSize();
  int button_height = 0;
  if (has_dialog_buttons()) {
    if (cancel_button_)
      button_height = cancel_button_->height();
    else
      button_height = ok_button_->height();
    // Account for padding above and below the button.
    button_height += kDialogButtonContentSpacing + kButtonVEdgeMargin;
  }
  prefsize.Enlarge(0, button_height);
  return prefsize;
}

bool DialogClientView::AcceleratorPressed(const Accelerator& accelerator) {
  DCHECK(accelerator.GetKeyCode() == VK_ESCAPE);  // We only expect Escape key.
  window()->Close();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, ButtonListener implementation:

void DialogClientView::ButtonPressed(Button* sender) {
  if (sender == ok_button_) {
    AcceptWindow();
  } else if (sender == cancel_button_) {
    CancelWindow();
  } else {
    NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, private:

void DialogClientView::PaintSizeBox(ChromeCanvas* canvas) {
  if (window()->GetDelegate()->CanResize() ||
      window()->GetDelegate()->CanMaximize()) {
    HDC dc = canvas->beginPlatformPaint();
    SIZE gripper_size = { 0, 0 };
    gfx::NativeTheme::instance()->GetThemePartSize(
        gfx::NativeTheme::STATUS, dc, SP_GRIPPER, 1, NULL, TS_TRUE,
        &gripper_size);

    // TODO(beng): (http://b/1085509) In "classic" rendering mode, there isn't
    //             a theme-supplied gripper. We should probably improvise
    //             something, which would also require changing |gripper_size|
    //             to have different default values, too...
    size_box_bounds_ = GetLocalBounds(false);
    size_box_bounds_.set_x(size_box_bounds_.right() - gripper_size.cx);
    size_box_bounds_.set_y(size_box_bounds_.bottom() - gripper_size.cy);
    RECT native_bounds = size_box_bounds_.ToRECT();
    gfx::NativeTheme::instance()->PaintStatusGripper(
        dc, SP_PANE, 1, 0, &native_bounds);
    canvas->endPlatformPaint();
  }
}

int DialogClientView::GetButtonWidth(int button) const {
  DialogDelegate* dd = GetDialogDelegate();
  std::wstring button_label = dd->GetDialogButtonLabel(
      static_cast<DialogDelegate::DialogButton>(button));
  int string_width = dialog_button_font_.GetStringWidth(button_label);
  return std::max(string_width + kDialogButtonLabelSpacing,
                  kDialogMinButtonWidth);
}

int DialogClientView::GetButtonsHeight() const {
  if (has_dialog_buttons()) {
    if (cancel_button_)
      return cancel_button_->height() + kDialogButtonContentSpacing;
    return ok_button_->height() + kDialogButtonContentSpacing;
  }
  return 0;
}

void DialogClientView::LayoutDialogButtons() {
  gfx::Rect extra_bounds;
  if (cancel_button_) {
    gfx::Size ps = cancel_button_->GetPreferredSize();
    gfx::Rect lb = GetLocalBounds(false);
    int button_width = GetButtonWidth(DialogDelegate::DIALOGBUTTON_CANCEL);
    int button_x = lb.right() - button_width - kButtonHEdgeMargin;
    int button_y = lb.bottom() - ps.height() - kButtonVEdgeMargin;
    cancel_button_->SetBounds(button_x, button_y, button_width, ps.height());
    // The extra view bounds are dependent on this button.
    extra_bounds.set_width(std::max(0, cancel_button_->x()));
    extra_bounds.set_y(cancel_button_->y());
  }
  if (ok_button_) {
    gfx::Size ps = ok_button_->GetPreferredSize();
    gfx::Rect lb = GetLocalBounds(false);
    int button_width = GetButtonWidth(DialogDelegate::DIALOGBUTTON_OK);
    int ok_button_right = lb.right() - kButtonHEdgeMargin;
    if (cancel_button_)
      ok_button_right = cancel_button_->x() - kRelatedButtonHSpacing;
    int button_x = ok_button_right - button_width;
    int button_y = lb.bottom() - ps.height() - kButtonVEdgeMargin;
    ok_button_->SetBounds(button_x, button_y, ok_button_right - button_x,
                          ps.height());
    // The extra view bounds are dependent on this button.
    extra_bounds.set_width(std::max(0, ok_button_->x()));
    extra_bounds.set_y(ok_button_->y());
  }
  if (extra_view_) {
    gfx::Size ps = extra_view_->GetPreferredSize();
    gfx::Rect lb = GetLocalBounds(false);
    extra_bounds.set_x(lb.x() + kButtonHEdgeMargin);
    extra_bounds.set_height(ps.height());
    extra_view_->SetBounds(extra_bounds);
  }
}

void DialogClientView::LayoutContentsView() {
  gfx::Rect lb = GetLocalBounds(false);
  lb.set_height(std::max(0, lb.height() - GetButtonsHeight()));
  contents_view()->SetBounds(lb);
  contents_view()->Layout();
}

void DialogClientView::CreateExtraView() {
  View* extra_view = GetDialogDelegate()->GetExtraView();
  if (extra_view && !extra_view_) {
    extra_view_ = extra_view;
    extra_view_->SetGroup(kButtonGroup);
    AddChildView(extra_view_);
  }
}

DialogDelegate* DialogClientView::GetDialogDelegate() const {
  DialogDelegate* dd = window()->GetDelegate()->AsDialogDelegate();
  DCHECK(dd);
  return dd;
}

// static
void DialogClientView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    dialog_button_font_ = rb.GetFont(ResourceBundle::BaseFont);
    initialized = true;
  }
}

}  // namespace views
