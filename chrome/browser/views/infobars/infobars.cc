// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/infobars/infobars.h"

#include "base/message_loop.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/infobars/infobar_container.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/slide_animation.h"
#include "chrome/views/background.h"
#include "chrome/views/button.h"
#include "chrome/views/external_focus_tracker.h"
#include "chrome/views/image_view.h"
#include "chrome/views/label.h"
#include "chrome/views/widget.h"

#include "generated_resources.h"

const double kInfoBarHeight = 37.0;

static const int kVerticalPadding = 3;
static const int kHorizontalPadding = 3;
static const int kIconLabelSpacing = 5;
static const int kButtonSpacing = 5;
static const int kWordSpacing = 2;

static const SkColor kBackgroundColorTop = SkColorSetRGB(255, 242, 183);
static const SkColor kBackgroundColorBottom = SkColorSetRGB(250, 230, 145);

static const int kSeparatorLineHeight = 1;
static const SkColor kSeparatorColor = SkColorSetRGB(165, 165, 165);

namespace {
int OffsetY(views::View* parent, const gfx::Size prefsize) {
  return std::max((parent->height() - prefsize.height()) / 2, 0);
}
}

// InfoBarBackground -----------------------------------------------------------

class InfoBarBackground : public views::Background {
 public:
  InfoBarBackground() {
    gradient_background_.reset(
        views::Background::CreateVerticalGradientBackground(
            kBackgroundColorTop, kBackgroundColorBottom));
  }

  // Overridden from views::View:
  virtual void Paint(ChromeCanvas* canvas, views::View* view) const {
    // First paint the gradient background.
    gradient_background_->Paint(canvas, view);

    // Now paint the separator line.
    canvas->FillRectInt(kSeparatorColor, 0,
                        view->height() - kSeparatorLineHeight, view->width(),
                        kSeparatorLineHeight);
  }

 private:
  scoped_ptr<views::Background> gradient_background_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarBackground);
};

// InfoBar, public: ------------------------------------------------------------

InfoBar::InfoBar(InfoBarDelegate* delegate)
    : delegate_(delegate),
      close_button_(new views::Button),
      delete_factory_(this) {
  // We delete ourselves when we're removed from the view hierarchy.
  SetParentOwned(false);

  set_background(new InfoBarBackground);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  close_button_->SetImage(views::Button::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::Button::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::Button::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  close_button_->SetListener(this, 0);
  close_button_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_CLOSE));
  AddChildView(close_button_);

  animation_.reset(new SlideAnimation(this));
  animation_->SetTweenType(SlideAnimation::NONE);
}

InfoBar::~InfoBar() {
}

void InfoBar::AnimateOpen() {
  animation_->Show();
}

void InfoBar::Open() {
  animation_->Reset(1.0);
  animation_->Show();
}

void InfoBar::AnimateClose() {
  DestroyFocusTracker(true);
  animation_->Hide();
}

void InfoBar::Close() {
  GetParent()->RemoveChildView(this);
  // Note that we only tell the delegate we're closed here, and not when we're
  // simply destroyed (by virtue of a tab switch or being moved from window to
  // window), since this action can cause the delegate to destroy itself.
  if (delegate_) {
    delegate_->InfoBarClosed();
    delegate_ = NULL;
  }
}

// InfoBar, views::View overrides: ---------------------------------------------

gfx::Size InfoBar::GetPreferredSize() {
  int height = static_cast<int>(kInfoBarHeight * animation_->GetCurrentValue());
  return gfx::Size(0, height);
}

void InfoBar::Layout() {
  gfx::Size button_ps = close_button_->GetPreferredSize();
  close_button_->SetBounds(width() - kHorizontalPadding - button_ps.width(),
                           OffsetY(this, button_ps), button_ps.width(),
                           button_ps.height());

}

void InfoBar::ViewHierarchyChanged(bool is_add, views::View* parent,
                                   views::View* child) {
  if (child == this) {
    if (is_add) {
      InfoBarAdded();
    } else {
      InfoBarRemoved();
    }
  }
}

// InfoBar, protected: ---------------------------------------------------------

int InfoBar::GetAvailableWidth() const {
  return close_button_->x() - kIconLabelSpacing;
}

void InfoBar::RemoveInfoBar() const {
  container_->RemoveDelegate(delegate());
}

// InfoBar, views::BaseButton::ButtonListener implementation: ------------------

void InfoBar::ButtonPressed(views::BaseButton* sender) {
  if (sender == close_button_)
    RemoveInfoBar();
}

// InfoBar, AnimationDelegate implementation: ----------------------------------

void InfoBar::AnimationProgressed(const Animation* animation) {
  if (container_)
    container_->InfoBarAnimated(true);
}

void InfoBar::AnimationEnded(const Animation* animation) {
  if (container_) {
    container_->InfoBarAnimated(false);

    if (!animation_->IsShowing())
      Close();
  }
}

// InfoBar, private: -----------------------------------------------------------

void InfoBar::InfoBarAdded() {
  // The container_ pointer must be set before adding to the view hierarchy.
  DCHECK(container_);
  // When we're added to a view hierarchy within a widget, we create an
  // external focus tracker to track what was focused in case we obtain
  // focus so that we can restore focus when we're removed.
  views::Widget* widget = GetWidget();
  if (widget) {
    focus_tracker_.reset(
        new views::ExternalFocusTracker(this,
            views::FocusManager::GetFocusManager(widget->GetHWND())));
  }
}

void InfoBar::InfoBarRemoved() {
  DestroyFocusTracker(false);
  // NULL our container_ pointer so that if Animation::Stop results in
  // AnimationEnded being called, we do not try and delete ourselves twice.
  container_ = NULL;
  animation_->Stop();
  // Finally, clean ourselves up when we're removed from the view hierarchy
  // since no-one refers to us now.
  MessageLoop::current()->PostTask(FROM_HERE,
      delete_factory_.NewRunnableMethod(&InfoBar::DeleteSelf));
}

void InfoBar::DestroyFocusTracker(bool restore_focus) {
  if (focus_tracker_.get()) {
    if (restore_focus)
      focus_tracker_->FocusLastFocusedExternalView();
    focus_tracker_->SetFocusManager(NULL);
    focus_tracker_.reset(NULL);
  }  
}

void InfoBar::DeleteSelf() {
  delete this;
}

// AlertInfoBar, public: -------------------------------------------------------

AlertInfoBar::AlertInfoBar(AlertInfoBarDelegate* delegate)
    : InfoBar(delegate) {
  label_ = new views::Label(
      delegate->GetMessageText(),
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label_);

  icon_ = new views::ImageView;
  if (delegate->GetIcon())
    icon_->SetImage(delegate->GetIcon());
  AddChildView(icon_);
}

AlertInfoBar::~AlertInfoBar() {

}

// AlertInfoBar, views::View overrides: ----------------------------------------

void AlertInfoBar::Layout() {
  // Layout the close button.
  InfoBar::Layout();

  // Layout the icon and text.
  gfx::Size icon_ps = icon_->GetPreferredSize();
  icon_->SetBounds(kHorizontalPadding, OffsetY(this, icon_ps), icon_ps.width(),
                   icon_ps.height());

  gfx::Size text_ps = label_->GetPreferredSize();
  int text_width =
      GetAvailableWidth() - icon_->bounds().right() - kIconLabelSpacing;
  label_->SetBounds(icon_->bounds().right() + kIconLabelSpacing,
                    OffsetY(this, text_ps), text_width, text_ps.height());
}

// AlertInfoBar, private: ------------------------------------------------------

AlertInfoBarDelegate* AlertInfoBar::GetDelegate() {
  return delegate()->AsAlertInfoBarDelegate();
}

// LinkInfoBar, public: --------------------------------------------------------

LinkInfoBar::LinkInfoBar(LinkInfoBarDelegate* delegate)
    : icon_(new views::ImageView),
      label_1_(new views::Label),
      label_2_(new views::Label),
      link_(new views::Link),
      InfoBar(delegate) {
  // Set up the icon.
  if (delegate->GetIcon())
    icon_->SetImage(delegate->GetIcon());
  AddChildView(icon_);

  // Set up the labels.
  size_t offset;
  std::wstring message_text = delegate->GetMessageTextWithOffset(&offset);
  if (offset != std::wstring::npos) {
    label_1_->SetText(message_text.substr(0, offset));
    label_2_->SetText(message_text.substr(offset));
  } else {
    label_1_->SetText(message_text);
  }
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  label_1_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  label_2_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  label_1_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label_2_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label_1_);
  AddChildView(label_2_);

  // Set up the link.
  link_->SetText(delegate->GetLinkText());
  link_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  link_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  link_->SetController(this);
  AddChildView(link_);
}

LinkInfoBar::~LinkInfoBar() {
}

// LinkInfoBar, views::LinkController implementation: --------------------------

void LinkInfoBar::LinkActivated(views::Link* source, int event_flags) {
  DCHECK(source == link_);
  if (GetDelegate()->LinkClicked(
          event_utils::DispositionFromEventFlags(event_flags))) {
    RemoveInfoBar();
  }
}

// LinkInfoBar, views::View overrides: -----------------------------------------

void LinkInfoBar::Layout() {
  // Layout the close button.
  InfoBar::Layout();

  // Layout the icon.
  gfx::Size icon_ps = icon_->GetPreferredSize();
  icon_->SetBounds(kHorizontalPadding, OffsetY(this, icon_ps), icon_ps.width(),
                   icon_ps.height());

  int label_1_x = icon_->bounds().right() + kIconLabelSpacing;

  // Figure out the amount of space available to the rest of the content now
  // that the close button and the icon have been positioned.
  int available_width = GetAvailableWidth() - label_1_x;

  // Layout the left label.
  gfx::Size label_1_ps = label_1_->GetPreferredSize();
  label_1_->SetBounds(label_1_x, OffsetY(this, label_1_ps), label_1_ps.width(),
                      label_1_ps.height());

  // Layout the link.
  gfx::Size link_ps = link_->GetPreferredSize();
  bool has_second_label = !label_2_->GetText().empty();
  if (has_second_label) {
    // Embed the link in the text string between the two labels.
    link_->SetBounds(label_1_->bounds().right() + kWordSpacing,
                     OffsetY(this, link_ps), link_ps.width(), link_ps.height());
  } else {
    // Right-align the link toward the edge of the InfoBar.
    link_->SetBounds(label_1_x + available_width - link_ps.width(),
                     OffsetY(this, link_ps), link_ps.width(), link_ps.height());
  }

  // Layout the right label (we do this regardless of whether or not it has
  // text).
  gfx::Size label_2_ps = label_2_->GetPreferredSize();
  label_2_->SetBounds(link_->bounds().right() + kWordSpacing,
                      OffsetY(this, label_2_ps), label_2_ps.width(),
                      label_2_ps.height());
}

// LinkInfoBar, private: -------------------------------------------------------

LinkInfoBarDelegate* LinkInfoBar::GetDelegate() {
  return delegate()->AsLinkInfoBarDelegate();
}

// ConfirmInfoBar, public: -----------------------------------------------------

ConfirmInfoBar::ConfirmInfoBar(ConfirmInfoBarDelegate* delegate)
    : ok_button_(NULL),
      cancel_button_(NULL),
      initialized_(false),
      AlertInfoBar(delegate) {
  ok_button_ = new views::NativeButton(
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
  ok_button_->SetListener(this);
  cancel_button_ = new views::NativeButton(
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  cancel_button_->SetListener(this);
}

ConfirmInfoBar::~ConfirmInfoBar() {
}

// ConfirmInfoBar, views::View overrides: --------------------------------------

void ConfirmInfoBar::Layout() {
  InfoBar::Layout();
  int available_width = InfoBar::GetAvailableWidth();
  int ok_button_width = 0;
  int cancel_button_width = 0;
  gfx::Size ok_ps = ok_button_->GetPreferredSize();
  gfx::Size cancel_ps = cancel_button_->GetPreferredSize();

  if (GetDelegate()->GetButtons() & ConfirmInfoBarDelegate::BUTTON_OK) {
    ok_button_width = ok_ps.width();
  } else {
    ok_button_->SetVisible(false);
  }
  if (GetDelegate()->GetButtons() & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    cancel_button_width = cancel_ps.width();
  } else {
    cancel_button_->SetVisible(false);
  }

  cancel_button_->SetBounds(available_width - cancel_button_width,
                            OffsetY(this, cancel_ps), cancel_ps.width(),
                            cancel_ps.height());
  int spacing = cancel_button_width > 0 ? kButtonSpacing : 0;
  ok_button_->SetBounds(cancel_button_->x() - spacing - ok_button_width,
                        OffsetY(this, ok_ps), ok_ps.width(), ok_ps.height());
  AlertInfoBar::Layout();
}

void ConfirmInfoBar::ViewHierarchyChanged(bool is_add,
                                          views::View* parent,
                                          views::View* child) {
  InfoBar::ViewHierarchyChanged(is_add, parent, child);
  if (is_add && child == this && !initialized_) {
    Init();
    initialized_ = true;
  }
}

// ConfirmInfoBar, views::NativeButton::Listener implementation: ---------------

void ConfirmInfoBar::ButtonPressed(views::NativeButton* sender) {
  if (sender == ok_button_) {
    if (GetDelegate()->Accept())
      RemoveInfoBar();
  } else if (sender == cancel_button_) {
    if (GetDelegate()->Cancel())
      RemoveInfoBar();
  } else {
    NOTREACHED();
  }
}

// ConfirmInfoBar, InfoBar overrides: ------------------------------------------

int ConfirmInfoBar::GetAvailableWidth() const {
  if (ok_button_)
    return ok_button_->x() - kButtonSpacing;
  if (cancel_button_)
    return cancel_button_->x() - kButtonSpacing;
  return InfoBar::GetAvailableWidth();
}

// ConfirmInfoBar, private: ----------------------------------------------------

ConfirmInfoBarDelegate* ConfirmInfoBar::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}

void ConfirmInfoBar::Init() {
  AddChildView(ok_button_);
  AddChildView(cancel_button_);
}

// AlertInfoBarDelegate, InfoBarDelegate overrides: ----------------------------

InfoBar* AlertInfoBarDelegate::CreateInfoBar() {
  return new AlertInfoBar(this);
}

// LinkInfoBarDelegate, InfoBarDelegate overrides: -----------------------------

InfoBar* LinkInfoBarDelegate::CreateInfoBar() {
  return new LinkInfoBar(this);
}

// ConfirmInfoBarDelegate, InfoBarDelegate overrides: --------------------------

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  return new ConfirmInfoBar(this);
}
