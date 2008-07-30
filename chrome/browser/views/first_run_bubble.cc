// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/views/first_run_bubble.h"

#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/event.h"
#include "chrome/views/label.h"
#include "chrome/views/native_button.h"

#include "generated_resources.h"

namespace {

// How much extra padding to put around our content over what
// infobubble provides.
static const int kBubblePadding = 4;

// TODO(cpu): bug 1187517. It is possible that there is no default provider.
// we should make sure there is none at first run.
std::wstring GetDefaultSearchEngineName() {
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  const TemplateURL* const default_provider =
      browser->profile()->GetTemplateURLModel()->GetDefaultSearchProvider();
  DCHECK(default_provider);
  return default_provider->short_name();
}

}  // namespace

// Implements the client view inside the first run bubble. It is kind of a
// dialog-ish view, but is not a true dialog.
class FirstRunBubbleView : public ChromeViews::View,
                           public ChromeViews::NativeButton::Listener {
 public:
  explicit FirstRunBubbleView(FirstRunBubble* bubble_window)
      : bubble_window_(bubble_window),
        label1_(NULL),
        label2_(NULL),
        label3_(NULL),
        keep_button_(NULL),
        change_button_(NULL) {
    ChromeFont& font =
        ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont);

    label1_ = new ChromeViews::Label(l10n_util::GetString(IDS_FR_BUBBLE_TITLE));
    label1_->SetFont(font.DeriveFont(3, ChromeFont::BOLD));
    label1_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
    AddChildView(label1_);

    CSize ps;
    GetPreferredSize(&ps);

    label2_ =
        new ChromeViews::Label(l10n_util::GetString(IDS_FR_BUBBLE_SUBTEXT));
    label2_->SetMultiLine(true);
    label2_->SetFont(font);
    label2_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
    label2_->SizeToFit(ps.cx - kBubblePadding * 2);
    AddChildView(label2_);

    std::wstring question_str
        = l10n_util::GetStringF(IDS_FR_BUBBLE_QUESTION,
                                GetDefaultSearchEngineName());
    label3_ = new ChromeViews::Label(question_str);
    label3_->SetMultiLine(true);
    label3_->SetFont(font);
    label3_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
    label3_->SizeToFit(ps.cx - kBubblePadding * 2);
    AddChildView(label3_);

    std::wstring keep_str = l10n_util::GetStringF(IDS_FR_BUBBLE_OK,
                                                  GetDefaultSearchEngineName());
    keep_button_ = new ChromeViews::NativeButton(keep_str, true);
    keep_button_->SetListener(this);
    AddChildView(keep_button_);

    std::wstring change_str = l10n_util::GetString(IDS_FR_BUBBLE_CHANGE);
    change_button_ = new ChromeViews::NativeButton(change_str);
    change_button_->SetListener(this);
    AddChildView(change_button_);
  }

  // Overridden from ChromeViews::View.
  virtual void DidChangeBounds(const CRect& previous, const CRect& current) {
    Layout();
  }

  // Overridden from NativeButton::Listener.
  virtual void ButtonPressed(ChromeViews::NativeButton* sender) {
    bubble_window_->Close();
    if (change_button_ == sender) {
      Browser* browser = BrowserList::GetLastActive();
      if (browser) {
        ShowOptionsWindow(OPTIONS_PAGE_GENERAL, OPTIONS_GROUP_DEFAULT_SEARCH,
                          browser->profile());
      }
    }
  }

  // Overridden from ChromeViews::View.
  virtual void Layout() {
    CSize canvas;
    GetPreferredSize(&canvas);

    CSize pref_size;
    label1_->GetPreferredSize(&pref_size);
    label1_->SetBounds(kBubblePadding, kBubblePadding, pref_size.cx,
                       pref_size.cy);

    int next_v_space = label1_->GetY() + label1_->GetHeight() +
                       kRelatedControlSmallVerticalSpacing;

    label2_->GetPreferredSize(&pref_size);
    label2_->SetBounds(kBubblePadding, next_v_space,
                       canvas.cx - kBubblePadding * 2,
                       pref_size.cy);

    next_v_space = label2_->GetY() + label2_->GetHeight() +
                   kPanelSubVerticalSpacing;

    label3_->GetPreferredSize(&pref_size);
    label3_->SetBounds(kBubblePadding, next_v_space,
                       canvas.cx - kBubblePadding * 2,
                       pref_size.cy);

    change_button_->GetPreferredSize(&pref_size);
    change_button_->SetBounds(canvas.cx - pref_size.cx - kBubblePadding,
                              canvas.cy - pref_size.cy - kButtonVEdgeMargin,
                              pref_size.cx, pref_size.cy);

    keep_button_->GetPreferredSize(&pref_size);
    keep_button_->SetBounds(change_button_->GetX() - pref_size.cx -
                            kRelatedButtonHSpacing, change_button_->GetY(),
                            pref_size.cx, pref_size.cy);
  }

  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child) {
    if (keep_button_)
      keep_button_->RequestFocus();
  }

  // Overridden from ChromeViews::View.
  virtual void GetPreferredSize(CSize *out) {
    DCHECK(out);
    *out = ChromeViews::Window::GetLocalizedContentsSize(
        IDS_FIRSTRUNBUBBLE_DIALOG_WIDTH_CHARS,
        IDS_FIRSTRUNBUBBLE_DIALOG_HEIGHT_LINES).ToSIZE();
  }

 private:
  FirstRunBubble* bubble_window_;
  ChromeViews::Label* label1_;
  ChromeViews::Label* label2_;
  ChromeViews::Label* label3_;
  ChromeViews::NativeButton* change_button_;
  ChromeViews::NativeButton* keep_button_;
  DISALLOW_EVIL_CONSTRUCTORS(FirstRunBubbleView);
};

// Keep the bubble around for kLingerTime milliseconds, to prevent accidental
// closure.
static const int kLingerTime = 1000;

void FirstRunBubble::OnActivate(UINT action, BOOL minimized, HWND window) {
  // We might get re-enabled right before we are closed (sequence is: we get
  // deactivated, we call close, before we are actually closed we get
  // reactivated). Don't do the disabling of the parent in such cases.
  if (action == WA_ACTIVE && !has_been_activated_) {
    has_been_activated_ = true;

    // Disable the browser to prevent accidental rapid clicks from closing the
    // bubble.
    ::EnableWindow(GetParent(), false);

    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        enable_window_method_factory_.NewRunnableMethod(
            &FirstRunBubble::EnableParent),
        kLingerTime);
  }
  InfoBubble::OnActivate(action, minimized, window);
}

void FirstRunBubble::InfoBubbleClosing(InfoBubble* info_bubble) {
  // Make sure our parent window is re-enabled.
  if (!IsWindowEnabled(GetParent()))
    ::EnableWindow(GetParent(), true);
  enable_window_method_factory_.RevokeAll();
}

FirstRunBubble* FirstRunBubble::Show(HWND parent_hwnd,
                                     const gfx::Rect& position_relative_to) {
  FirstRunBubble* window = new FirstRunBubble();
  ChromeViews::View* view = new FirstRunBubbleView(window);
  window->SetDelegate(window);
  window->Init(parent_hwnd, position_relative_to, view);
  BrowserWindow* frame = window->GetHostingWindow();
  DCHECK(frame);
  frame->InfoBubbleShowing();
  window->ShowWindow(SW_SHOW);
  return window;
}

void FirstRunBubble::EnableParent() {
  ::EnableWindow(GetParent(), true);
}
