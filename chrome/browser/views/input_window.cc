// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/input_window.h"

#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/label.h"
#include "chrome/views/text_field.h"
#include "chrome/views/window.h"
#include "grit/generated_resources.h"

// Width to make the text field, in pixels.
static const int kTextFieldWidth = 200;

// ContentView ----------------------------------------------------------------

// ContentView, as the name implies, is the content view for the InputWindow.
// It registers accelerators that accept/cancel the input.

class ContentView : public views::View,
                    public views::DialogDelegate,
                    public views::TextField::Controller {
 public:
  explicit ContentView(InputWindowDelegate* delegate)
      : delegate_(delegate),
        focus_grabber_factory_(this) {
    DCHECK(delegate_);
  }

  // views::DialogDelegate overrides:
  virtual bool IsDialogButtonEnabled(DialogButton button) const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual void WindowClosing();
  virtual void DeleteDelegate();
  virtual std::wstring GetWindowTitle() const;
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView();

  // views::TextField::Controller overrides:
  virtual void ContentsChanged(views::TextField* sender,
                               const std::wstring& new_contents);
  virtual void HandleKeystroke(views::TextField*, UINT, TCHAR, UINT, UINT) {}

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

 private:
  // Set up dialog controls and layout.
  void InitControlLayout();

  // Sets focus to the first focusable element within the dialog.
  void FocusFirstFocusableControl();

  // The TextField that the user can type into.
  views::TextField* text_field_;

  // The delegate that the ContentView uses to communicate changes to the
  // caller.
  InputWindowDelegate* delegate_;

  // Helps us set focus to the first TextField in the window.
  ScopedRunnableMethodFactory<ContentView> focus_grabber_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(ContentView);
};

///////////////////////////////////////////////////////////////////////////////
// ContentView, views::DialogDelegate implementation:

bool ContentView::IsDialogButtonEnabled(DialogButton button) const {
  if (button == DIALOGBUTTON_OK && !delegate_->IsValid(text_field_->GetText()))
    return false;
  return true;
}

bool ContentView::Accept() {
  delegate_->InputAccepted(text_field_->GetText());
  return true;
}

bool ContentView::Cancel() {
  delegate_->InputCanceled();
  return true;
}

void ContentView::WindowClosing() {
  delegate_->WindowClosing();
}

void ContentView::DeleteDelegate() {
  delegate_->DeleteDelegate();
}

std::wstring ContentView::GetWindowTitle() const {
  return delegate_->GetWindowTitle();
}

views::View* ContentView::GetContentsView() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// ContentView, views::TextField::Controller implementation:

void ContentView::ContentsChanged(views::TextField* sender,
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
  text_field_ = new views::TextField;
  text_field_->SetText(delegate_->GetTextFieldContents());
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
                GridLayout::USE_PREF, kTextFieldWidth, kTextFieldWidth);

  layout->StartRow(0, 0);
  views::Label* label = new views::Label(delegate_->GetTextFieldLabel());
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

views::Window* CreateInputWindow(HWND parent_hwnd,
                                 InputWindowDelegate* delegate) {
  views::Window* window =
      views::Window::CreateChromeWindow(parent_hwnd, gfx::Rect(),
                                              new ContentView(delegate));
  window->client_view()->AsDialogClientView()->UpdateDialogButtons();
  return window;
}
