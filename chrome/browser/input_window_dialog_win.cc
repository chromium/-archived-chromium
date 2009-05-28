// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/input_window_dialog.h"

#include "app/l10n_util.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "views/grid_layout.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/standard_layout.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"
#include "grit/generated_resources.h"

// Width to make the text field, in pixels.
static const int kTextfieldWidth = 200;

class ContentView;

// The Windows implementation of the cross platform input dialog interface.
class WinInputWindowDialog : public InputWindowDialog {
 public:
  WinInputWindowDialog(HWND parent,
                       const std::wstring& window_title,
                       const std::wstring& label,
                       const std::wstring& contents,
                       Delegate* delegate);
  virtual ~WinInputWindowDialog();

  virtual void Show();
  virtual void Close();

  const std::wstring& window_title() const { return window_title_; }
  const std::wstring& label() const { return label_; }
  const std::wstring& contents() const { return contents_; }

  InputWindowDialog::Delegate* delegate() { return delegate_.get(); }

 private:
  // Our chrome views window.
  views::Window* window_;

  // Strings to feed to the on screen window.
  std::wstring window_title_;
  std::wstring label_;
  std::wstring contents_;

  // Our delegate. Consumes the window's output.
  scoped_ptr<InputWindowDialog::Delegate> delegate_;
};

// ContentView, as the name implies, is the content view for the InputWindow.
// It registers accelerators that accept/cancel the input.
class ContentView : public views::View,
                    public views::DialogDelegate,
                    public views::Textfield::Controller {
 public:
  explicit ContentView(WinInputWindowDialog* delegate)
      : delegate_(delegate),
        ALLOW_THIS_IN_INITIALIZER_LIST(focus_grabber_factory_(this)) {
    DCHECK(delegate_);
  }

  // views::DialogDelegate overrides:
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual void DeleteDelegate();
  virtual std::wstring GetWindowTitle() const;
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView();

  // views::Textfield::Controller overrides:
  virtual void ContentsChanged(views::Textfield* sender,
                               const std::wstring& new_contents);
  virtual bool HandleKeystroke(views::Textfield*,
                               const views::Textfield::Keystroke&) {
    return false;
  }

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

 private:
  // Set up dialog controls and layout.
  void InitControlLayout();

  // Sets focus to the first focusable element within the dialog.
  void FocusFirstFocusableControl();

  // The Textfield that the user can type into.
  views::Textfield* text_field_;

  // The delegate that the ContentView uses to communicate changes to the
  // caller.
  WinInputWindowDialog* delegate_;

  // Helps us set focus to the first Textfield in the window.
  ScopedRunnableMethodFactory<ContentView> focus_grabber_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentView);
};

///////////////////////////////////////////////////////////////////////////////
// ContentView, views::DialogDelegate implementation:

bool ContentView::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK &&
      !delegate_->delegate()->IsValid(text_field_->text())) {
    return false;
  }
  return true;
}

bool ContentView::Accept() {
  delegate_->delegate()->InputAccepted(text_field_->text());
  return true;
}

bool ContentView::Cancel() {
  delegate_->delegate()->InputCanceled();
  return true;
}

void ContentView::DeleteDelegate() {
  delete delegate_;
}

std::wstring ContentView::GetWindowTitle() const {
  return delegate_->window_title();
}

views::View* ContentView::GetContentsView() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// ContentView, views::Textfield::Controller implementation:

void ContentView::ContentsChanged(views::Textfield* sender,
                                  const std::wstring& new_contents) {
  GetDialogClientView()->UpdateDialogButtons();
}

///////////////////////////////////////////////////////////////////////////////
// ContentView, protected:

void ContentView::ViewHierarchyChanged(bool is_add,
                                       views::View* parent,
                                       views::View* child) {
  if (is_add && child == this)
    InitControlLayout();
}

///////////////////////////////////////////////////////////////////////////////
// ContentView, private:

void ContentView::InitControlLayout() {
  text_field_ = new views::Textfield;
  text_field_->SetText(delegate_->contents());
  text_field_->SetController(this);

  using views::ColumnSet;
  using views::GridLayout;

  // TODO(sky): Vertical alignment should be baseline.
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  ColumnSet* c1 = layout->AddColumnSet(0);
  c1->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  c1->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  c1->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                GridLayout::USE_PREF, kTextfieldWidth, kTextfieldWidth);

  layout->StartRow(0, 0);
  views::Label* label = new views::Label(delegate_->label());
  layout->AddView(label);
  layout->AddView(text_field_);

  MessageLoop::current()->PostTask(FROM_HERE,
      focus_grabber_factory_.NewRunnableMethod(
          &ContentView::FocusFirstFocusableControl));
}

void ContentView::FocusFirstFocusableControl() {
  text_field_->SelectAll();
  text_field_->RequestFocus();
}

WinInputWindowDialog::WinInputWindowDialog(HWND parent,
                                           const std::wstring& window_title,
                                           const std::wstring& label,
                                           const std::wstring& contents,
                                           Delegate* delegate)
    : window_title_(window_title),
      label_(label),
      contents_(contents),
      delegate_(delegate) {
  window_ = views::Window::CreateChromeWindow(parent, gfx::Rect(),
                                              new ContentView(this));
  window_->GetClientView()->AsDialogClientView()->UpdateDialogButtons();
}

WinInputWindowDialog::~WinInputWindowDialog() {
}

void WinInputWindowDialog::Show() {
  window_->Show();
}

void WinInputWindowDialog::Close() {
  window_->Close();
}

// static
InputWindowDialog* InputWindowDialog::Create(HWND parent,
                                             const std::wstring& window_title,
                                             const std::wstring& label,
                                             const std::wstring& contents,
                                             Delegate* delegate) {
  return new WinInputWindowDialog(parent,
                                  window_title,
                                  label,
                                  contents,
                                  delegate);
}
