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
#include "generated_resources.h"

// Width to make the text field, in pixels.
static const int kTextFieldWidth = 200;

// ContentView ----------------------------------------------------------------

// ContentView, as the name implies, is the content view for the InputWindow.
// It registers accelerators that accept/cancel the input.

class ContentView : public ChromeViews::View,
                    public ChromeViews::DialogDelegate,
                    public ChromeViews::TextField::Controller {
 public:
  explicit ContentView(InputWindowDelegate* delegate)
      : delegate_(delegate),
        focus_grabber_factory_(this) {
    DCHECK(delegate_);
  }

  // ChromeViews::DialogDelegate overrides:
  virtual bool IsDialogButtonEnabled(DialogButton button) const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual void WindowClosing();
  virtual std::wstring GetWindowTitle() const;
  virtual bool IsModal() const { return true; }
  virtual ChromeViews::View* GetContentsView();

  // ChromeViews::TextField::Controller overrides:
  virtual void ContentsChanged(ChromeViews::TextField* sender,
                               const std::wstring& new_contents);
  virtual void HandleKeystroke(ChromeViews::TextField*, UINT, TCHAR, UINT,
                               UINT) {}

 protected:
  // ChromeViews::View overrides:
  virtual void ViewHierarchyChanged(bool is_add, ChromeViews::View* parent,
                                    ChromeViews::View* child);

 private:
  // Set up dialog controls and layout.
  void InitControlLayout();

  // Sets focus to the first focusable element within the dialog.
  void FocusFirstFocusableControl();

  // The TextField that the user can type into.
  ChromeViews::TextField* text_field_;

  // The delegate that the ContentView uses to communicate changes to the
  // caller.
  InputWindowDelegate* delegate_;

  // Helps us set focus to the first TextField in the window.
  ScopedRunnableMethodFactory<ContentView> focus_grabber_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(ContentView);
};

///////////////////////////////////////////////////////////////////////////////
// ContentView, ChromeViews::DialogDelegate implementation:

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

std::wstring ContentView::GetWindowTitle() const {
  return delegate_->GetWindowTitle();
}

ChromeViews::View* ContentView::GetContentsView() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// ContentView, ChromeViews::TextField::Controller implementation:

void ContentView::ContentsChanged(ChromeViews::TextField* sender,
                                  const std::wstring& new_contents) {
  GetDialogClientView()->UpdateDialogButtons();
}

///////////////////////////////////////////////////////////////////////////////
// ContentView, protected:

void ContentView::ViewHierarchyChanged(bool is_add,
                                       ChromeViews::View* parent,
                                       ChromeViews::View* child) {
  if (is_add && child == this)
    InitControlLayout();
}

///////////////////////////////////////////////////////////////////////////////
// ContentView, private:

void ContentView::InitControlLayout() {
  text_field_ = new ChromeViews::TextField;
  text_field_->SetText(delegate_->GetTextFieldContents());
  text_field_->SetController(this);

  using ChromeViews::ColumnSet;
  using ChromeViews::GridLayout;

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
  ChromeViews::Label* label =
      new ChromeViews::Label(delegate_->GetTextFieldLabel());
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

ChromeViews::Window* CreateInputWindow(HWND parent_hwnd,
                                       InputWindowDelegate* delegate) {
  ChromeViews::Window* window =
      ChromeViews::Window::CreateChromeWindow(parent_hwnd, gfx::Rect(),
                                              new ContentView(delegate));
  window->client_view()->AsDialogClientView()->UpdateDialogButtons();
  return window;
}

