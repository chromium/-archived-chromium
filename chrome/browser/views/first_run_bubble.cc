// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/first_run_bubble.h"

#include "base/win_util.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/event.h"
#include "chrome/views/label.h"
#include "chrome/views/native_button.h"
#include "chrome/views/window.h"

#include "chromium_strings.h"
#include "generated_resources.h"

namespace {

// How much extra padding to put around our content over what
// infobubble provides.
static const int kBubblePadding = 4;

std::wstring GetDefaultSearchEngineName(Profile* profile) {
  if (!profile) {
    NOTREACHED();
    return std::wstring();
  }
  const TemplateURL* const default_provider =
      profile->GetTemplateURLModel()->GetDefaultSearchProvider();
  if (!default_provider) {
    // TODO(cpu): bug 1187517. It is possible to have no default provider.
    // returning an empty string is a stopgap measure for the crash
    // http://code.google.com/p/chromium/issues/detail?id=2573
    return std::wstring();
  }
  return default_provider->short_name();
}

}  // namespace

// Implements the client view inside the first run bubble. It is kind of a
// dialog-ish view, but is not a true dialog.
class FirstRunBubbleView : public views::View,
                           public views::NativeButton::Listener {
 public:
  FirstRunBubbleView(FirstRunBubble* bubble_window, Profile* profile)
      : bubble_window_(bubble_window),
        label1_(NULL),
        label2_(NULL),
        label3_(NULL),
        keep_button_(NULL),
        change_button_(NULL) {
    ChromeFont& font =
        ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont);

    label1_ = new views::Label(l10n_util::GetString(IDS_FR_BUBBLE_TITLE));
    label1_->SetFont(font.DeriveFont(3, ChromeFont::BOLD));
    label1_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(label1_);

    gfx::Size ps = GetPreferredSize();

    label2_ = new views::Label(l10n_util::GetString(IDS_FR_BUBBLE_SUBTEXT));
    label2_->SetMultiLine(true);
    label2_->SetFont(font);
    label2_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    label2_->SizeToFit(ps.width() - kBubblePadding * 2);
    AddChildView(label2_);

    std::wstring question_str
        = l10n_util::GetStringF(IDS_FR_BUBBLE_QUESTION,
                                GetDefaultSearchEngineName(profile));
    label3_ = new views::Label(question_str);
    label3_->SetMultiLine(true);
    label3_->SetFont(font);
    label3_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    label3_->SizeToFit(ps.width() - kBubblePadding * 2);
    AddChildView(label3_);

    std::wstring keep_str =
        l10n_util::GetStringF(IDS_FR_BUBBLE_OK,
                              GetDefaultSearchEngineName(profile));
    keep_button_ = new views::NativeButton(keep_str, true);
    keep_button_->SetListener(this);
    AddChildView(keep_button_);

    std::wstring change_str = l10n_util::GetString(IDS_FR_BUBBLE_CHANGE);
    change_button_ = new views::NativeButton(change_str);
    change_button_->SetListener(this);
    AddChildView(change_button_);
  }

  // Overridden from NativeButton::Listener.
  virtual void ButtonPressed(views::NativeButton* sender) {
    bubble_window_->Close();
    if (change_button_ == sender) {
      Browser* browser = BrowserList::GetLastActive();
      if (browser) {
        ShowOptionsWindow(OPTIONS_PAGE_GENERAL, OPTIONS_GROUP_DEFAULT_SEARCH,
                          browser->profile());
      }
    }
  }

  // Overridden from views::View.
  virtual void Layout() {
    gfx::Size canvas = GetPreferredSize();

    // The multiline business that follows is dirty hacks to get around
    // bug 1325257.
    label1_->SetMultiLine(false);
    gfx::Size pref_size = label1_->GetPreferredSize();
    label1_->SetMultiLine(true);
    label1_->SizeToFit(canvas.width() - kBubblePadding * 2);
    label1_->SetBounds(kBubblePadding, kBubblePadding, 
                       canvas.width() - kBubblePadding * 2,
                       pref_size.height());

    int next_v_space = label1_->y() + pref_size.height() +
                       kRelatedControlSmallVerticalSpacing;

    pref_size = label2_->GetPreferredSize();
    label2_->SetBounds(kBubblePadding, next_v_space,
                       canvas.width() - kBubblePadding * 2,
                       pref_size.height());

    next_v_space = label2_->y() + label2_->height() +
                   kPanelSubVerticalSpacing;

    pref_size = label3_->GetPreferredSize();
    label3_->SetBounds(kBubblePadding, next_v_space,
                       canvas.width() - kBubblePadding * 2,
                       pref_size.height());

    pref_size = change_button_->GetPreferredSize();
    change_button_->SetBounds(
        canvas.width() - pref_size.width() - kBubblePadding,
        canvas.height() - pref_size.height() - kButtonVEdgeMargin,
        pref_size.width(), pref_size.height());

    pref_size = keep_button_->GetPreferredSize();
    keep_button_->SetBounds(change_button_->x() - pref_size.width() -
                            kRelatedButtonHSpacing, change_button_->y(),
                            pref_size.width(), pref_size.height());
  }

  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child) {
    if (keep_button_)
      keep_button_->RequestFocus();
  }

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() {
    return gfx::Size(views::Window::GetLocalizedContentsSize(
        IDS_FIRSTRUNBUBBLE_DIALOG_WIDTH_CHARS,
        IDS_FIRSTRUNBUBBLE_DIALOG_HEIGHT_LINES));
  }

 private:
  FirstRunBubble* bubble_window_;
  views::Label* label1_;
  views::Label* label2_;
  views::Label* label3_;
  views::NativeButton* change_button_;
  views::NativeButton* keep_button_;
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

void FirstRunBubble::InfoBubbleClosing(InfoBubble* info_bubble,
                                       bool closed_by_escape) {
  // Make sure our parent window is re-enabled.
  if (!IsWindowEnabled(GetParent()))
    ::EnableWindow(GetParent(), true);
  enable_window_method_factory_.RevokeAll();
}

FirstRunBubble* FirstRunBubble::Show(Profile* profile, HWND parent_hwnd,
                                     const gfx::Rect& position_relative_to) {
  FirstRunBubble* window = new FirstRunBubble();
  views::View* view = new FirstRunBubbleView(window, profile);
  window->SetDelegate(window);
  window->Init(parent_hwnd, position_relative_to, view);
  window->ShowWindow(SW_SHOW);
  return window;
}

void FirstRunBubble::EnableParent() {
  ::EnableWindow(GetParent(), true);
}

