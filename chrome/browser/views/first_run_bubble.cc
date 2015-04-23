// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/first_run_bubble.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/win_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "views/event.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/image_button.h"
#include "views/controls/label.h"
#include "views/focus/focus_manager.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace {

// How much extra padding to put around our content over what the InfoBubble
// provides.
static const int kBubblePadding = 4;

// How much extra padding to put around our content over what the InfoBubble
// provides in alternative OEM bubble.
static const int kOEMBubblePadding = 4;

// Padding between parts of strings on the same line (for instance,
// "New!" and "Search from the address bar!"
static const int kStringSeparationPadding = 2;

// Margin around close button.
static const int kMarginRightOfCloseButton = 7;

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

// Base class for implementations of the client view which appears inside the
// first run bubble. It is a dialog-ish view, but is not a true dialog.
class FirstRunBubbleViewBase : public views::View,
                               public views::ButtonListener,
                               public views::FocusChangeListener {
 public:
  // Called by FirstRunBubble::Show to request focus for the proper button
  // in the FirstRunBubbleView when it is shown.
  virtual void BubbleShown() = 0;
};

class FirstRunBubbleView : public FirstRunBubbleViewBase {
 public:
  FirstRunBubbleView(FirstRunBubble* bubble_window, Profile* profile)
      : bubble_window_(bubble_window),
        label1_(NULL),
        label2_(NULL),
        label3_(NULL),
        keep_button_(NULL),
        change_button_(NULL) {
    gfx::Font& font =
        ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont);

    label1_ = new views::Label(l10n_util::GetString(IDS_FR_BUBBLE_TITLE));
    label1_->SetFont(font.DeriveFont(3, gfx::Font::BOLD));
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
    keep_button_ = new views::NativeButton(this, keep_str);
    keep_button_->SetIsDefault(true);
    AddChildView(keep_button_);

    std::wstring change_str = l10n_util::GetString(IDS_FR_BUBBLE_CHANGE);
    change_button_ = new views::NativeButton(this, change_str);
    AddChildView(change_button_);
  }

  void BubbleShown() {
    keep_button_->RequestFocus();
  }

  virtual void ButtonPressed(views::Button* sender) {
    bubble_window_->Close();
    if (change_button_ == sender) {
      Browser* browser = BrowserList::GetLastActive();
      if (browser) {
        ShowOptionsWindow(OPTIONS_PAGE_GENERAL, OPTIONS_GROUP_DEFAULT_SEARCH,
                          browser->profile());
      }
    }
  }

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

  virtual gfx::Size GetPreferredSize() {
    return gfx::Size(views::Window::GetLocalizedContentsSize(
        IDS_FIRSTRUNBUBBLE_DIALOG_WIDTH_CHARS,
        IDS_FIRSTRUNBUBBLE_DIALOG_HEIGHT_LINES));
  }

  virtual void FocusWillChange(View* focused_before, View* focused_now) {
    if (focused_before && focused_before->GetClassName() ==
                          views::NativeButton::kViewClassName) {
      views::NativeButton* before =
          static_cast<views::NativeButton*>(focused_before);
      before->SetIsDefault(false);
    }
    if (focused_now && focused_now->GetClassName() ==
                       views::NativeButton::kViewClassName) {
      views::NativeButton* after =
          static_cast<views::NativeButton*>(focused_now);
      after->SetIsDefault(true);
    }
  }

 private:
  FirstRunBubble* bubble_window_;
  views::Label* label1_;
  views::Label* label2_;
  views::Label* label3_;
  views::NativeButton* change_button_;
  views::NativeButton* keep_button_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubbleView);
};

class FirstRunOEMBubbleView : public FirstRunBubbleViewBase {
 public:
  FirstRunOEMBubbleView(FirstRunBubble* bubble_window, Profile* profile)
      : bubble_window_(bubble_window),
        label1_(NULL),
        label2_(NULL),
        label3_(NULL),
        close_button_(NULL) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    gfx::Font& font = rb.GetFont(ResourceBundle::MediumFont);

    label1_ = new views::Label(
              l10n_util::GetString(IDS_FR_OEM_BUBBLE_TITLE_1));
    label1_->SetFont(font.DeriveFont(3, gfx::Font::BOLD));
    label1_->SetColor(SK_ColorRED);
    label1_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(label1_);

    label2_ = new views::Label(
              l10n_util::GetString(IDS_FR_OEM_BUBBLE_TITLE_2));
    label2_->SetFont(font.DeriveFont(3, gfx::Font::BOLD));
    label2_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(label2_);

    gfx::Size ps = GetPreferredSize();

    label3_ = new views::Label(
              l10n_util::GetString(IDS_FR_OEM_BUBBLE_SUBTEXT));
    label3_->SetMultiLine(true);
    label3_->SetFont(font);
    label3_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    label3_->SizeToFit(ps.width() - kOEMBubblePadding * 2);
    AddChildView(label3_);

    close_button_ = new views::ImageButton(this);
    close_button_->SetImage(views::CustomButton::BS_NORMAL,
        rb.GetBitmapNamed(IDR_CLOSE_BAR));
    close_button_->SetImage(views::CustomButton::BS_HOT,
        rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
    close_button_->SetImage(views::CustomButton::BS_PUSHED,
        rb.GetBitmapNamed(IDR_CLOSE_BAR_P));

    AddChildView(close_button_);
  }

  void BubbleShown() {
    this->RequestFocus();
    // No button in oem_bubble to request focus.
  }

  virtual void ButtonPressed(views::Button* sender) {
    bubble_window_->Close();
  }

  virtual void Layout() {
    gfx::Size canvas = GetPreferredSize();

    // First, draw the close button on the far right.
    gfx::Size sz = close_button_->GetPreferredSize();
    close_button_->SetBounds(canvas.width() - sz.width() -
                             kMarginRightOfCloseButton,
                             kOEMBubblePadding,
                             sz.width(),
                             sz.height());

    gfx::Size pref_size = label1_->GetPreferredSize();
    label1_->SetBounds(kOEMBubblePadding, kOEMBubblePadding,
                       pref_size.width() + kOEMBubblePadding * 2,
                       pref_size.height());

    pref_size = label2_->GetPreferredSize();
    label2_->SetBounds(kOEMBubblePadding * 2 + label1_->
                           GetPreferredSize().width(),
                       kOEMBubblePadding,
                       canvas.width() - kOEMBubblePadding * 2,
                       pref_size.height());

    int next_v_space = label1_->y() + pref_size.height() +
                       kRelatedControlSmallVerticalSpacing;

    pref_size = label3_->GetPreferredSize();
    label3_->SetBounds(kOEMBubblePadding, next_v_space,
                       canvas.width() - kOEMBubblePadding * 2,
                       pref_size.height());
  }

  virtual gfx::Size GetPreferredSize() {
    return gfx::Size(views::Window::GetLocalizedContentsSize(
        IDS_FIRSTRUNOEMBUBBLE_DIALOG_WIDTH_CHARS,
        IDS_FIRSTRUNOEMBUBBLE_DIALOG_HEIGHT_LINES));
  }

  virtual void FocusWillChange(View* focused_before, View* focused_now) {
    // No buttons in oem_bubble to register focus changes.
  }

 private:
  FirstRunBubble* bubble_window_;
  views::Label* label1_;
  views::Label* label2_;
  views::Label* label3_;
  views::ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunOEMBubbleView);
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

    ::EnableWindow(GetParent(), false);

    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        enable_window_method_factory_.NewRunnableMethod(
            &FirstRunBubble::EnableParent),
        kLingerTime);
  }

  // Keep window from automatically closing until kLingerTime has passed.
  if (::IsWindowEnabled(GetParent()))
    InfoBubble::OnActivate(action, minimized, window);
}

void FirstRunBubble::InfoBubbleClosing(InfoBubble* info_bubble,
                                       bool closed_by_escape) {
  // Make sure our parent window is re-enabled.
  if (!IsWindowEnabled(GetParent()))
    ::EnableWindow(GetParent(), true);
  enable_window_method_factory_.RevokeAll();
  GetFocusManager()->RemoveFocusChangeListener(view_);
}

// static
FirstRunBubble* FirstRunBubble::Show(Profile* profile, views::Window* parent,
                                     const gfx::Rect& position_relative_to,
                                     bool use_OEM_bubble) {
  FirstRunBubble* window = new FirstRunBubble();
  FirstRunBubbleViewBase* view = NULL;
  if (use_OEM_bubble)
    view = new FirstRunOEMBubbleView(window, profile);
  else
    view = new FirstRunBubbleView(window, profile);
  window->SetDelegate(window);
  window->set_view(view);
  window->Init(parent, position_relative_to, view);
  window->ShowWindow(SW_SHOW);
  window->GetFocusManager()->AddFocusChangeListener(view);
  view->BubbleShown();
  return window;
}

void FirstRunBubble::EnableParent() {
  ::EnableWindow(GetParent(), true);
  // Reactivate the FirstRunBubble so it responds to OnActivate messages.
  SetWindowPos(GetParent(), 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW | SWP_SHOWWINDOW);
}
