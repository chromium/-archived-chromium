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

#include "chrome/browser/wizard/wizard.h"

#include <math.h>

#include "base/logging.h"
#include "chrome/browser/wizard/wizard_step.h"
#include "chrome/browser/standard_layout.h"

#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/stl_util-inl.h"

#include "chrome/views/accelerator.h"
#include "chrome/views/label.h"
#include "chrome/views/native_button.h"

#include "generated_resources.h"

#include "SkBitmap.h"

static const int kMinButtonWidth = 100;
static const int kWizardWidth = 400;
static const int kWizardHeight = 300;

////////////////////////////////////////////////////////////////////////////////
//
// WizardView is the Wizard top level view
//
////////////////////////////////////////////////////////////////////////////////
class WizardView : public ChromeViews::View,
                   public ChromeViews::NativeButton::Listener {
 public:

  WizardView(Wizard* owner)
      : owner_(owner),
        contents_(NULL),
        custom_navigation_descriptor_(NULL) {
    title_ = new ChromeViews::Label(L"");
    title_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
    title_->SetFont(
        ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::LargeFont));
    AddChildView(title_);

    cancel_ =
        new ChromeViews::NativeButton(l10n_util::GetString(IDS_WIZARD_CANCEL));
    AddChildView(cancel_);
    cancel_->SetListener(this);
    ChromeViews::Accelerator accelerator(VK_ESCAPE, false, false, false);
    cancel_->AddAccelerator(accelerator);

    previous_ = new ChromeViews::NativeButton(
        l10n_util::GetString(IDS_WIZARD_PREVIOUS));
    AddChildView(previous_);
    previous_->SetListener(this);

    next_ = new ChromeViews::NativeButton(l10n_util::GetString(IDS_WIZARD_NEXT));
    AddChildView(next_);
    next_->SetListener(this);
  }

  virtual ~WizardView() {
    // Views provided by WizardStep instances are owned by the step
    if (contents_) {
      RemoveChildView(contents_);
      contents_ = NULL;
    }

    // Sub views are deleted by the view system
  }

  void ComputeButtonSize(int* width, int *height) {
    *width = kMinButtonWidth;
    CSize s;
    cancel_->GetPreferredSize(&s);
    *height = s.cy;
    *width = std::max(*width, static_cast<int>(s.cx));

    previous_->GetPreferredSize(&s);
    *width = std::max(*width, static_cast<int>(s.cx));
    *height = std::max(*height, static_cast<int>(s.cy));

    next_->GetPreferredSize(&s);
    *width = std::max(*width, static_cast<int>(s.cx));
    *height = std::max(*height, static_cast<int>(s.cy));
  }

  virtual void Layout() {
    View* parent = GetParent();
    DCHECK(parent);
    if (!parent)
      return;

    if (title_->GetText().empty()) {
      title_->SetBounds(0, 0, 0, 0);
      title_->SetVisible(false);
    } else {
      CSize s;
      title_->SetVisible(true);
      title_->GetPreferredSize(&s);
      title_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
                        GetWidth() - (2 * kPanelVertMargin),
                        s.cy);
    }

    int bw, bh;
    ComputeButtonSize(&bw, &bh);

    int button_y = GetHeight() - kPanelVertMargin - bh;
    cancel_->SetBounds(kPanelHorizMargin, button_y, bw, bh);
    next_->SetBounds(GetWidth() - kPanelHorizMargin - bw, button_y, bw, bh);
    previous_->SetBounds(next_->GetX() - kPanelHorizMargin - bw, button_y, bw, bh);

    if (contents_) {
      int y = title_->GetY() + title_->GetHeight() + kPanelVertMargin;
      contents_->SetBounds(kPanelHorizMargin, y,
                           GetWidth() - (2 * kPanelHorizMargin),
                           cancel_->GetY() - kPanelVertMargin - y);
      contents_->Layout();
    }
  }

  virtual void GetPreferredSize(CSize* out) {
    CSize s;
    int w = 0, h = 0;

    if (contents_) {
      int extra_margin = (2 * kPanelVertMargin);
      contents_->GetPreferredSize(&s);
      w = s.cx;
      h = s.cy + extra_margin;
    }

    if (!title_->GetText().empty()) {
      title_->GetPreferredSize(&s);
      w = std::max(w, static_cast<int>(s.cx));
      h += s.cy;
    }

    int bw, bh;
    ComputeButtonSize(&bw, &bh);
    h += bh;

    w += (2 * kPanelHorizMargin);
    h += (2 * kPanelVertMargin);
    out->cx = std::max(kWizardWidth, w);
    out->cy = std::max(kWizardHeight, h);
  }

  virtual void ButtonPressed(ChromeViews::NativeButton *sender) {
    if (sender == cancel_)
      owner_->Cancel();
    else if (sender == next_)
      owner_->SelectNextStep();
    else if (sender == previous_)
      owner_->SelectPreviousStep();
  }

  typedef enum {
    FIRST_STEP_STYLE,
    NORMAL_STEP_STYLE,
    LAST_STEP_STYLE,
    CUSTOM_STYLE
  } ContentsStyle;

  void SetContents(ChromeViews::View* v, ContentsStyle s) {
    if (contents_)
      RemoveChildView(contents_);
    custom_navigation_descriptor_ = NULL;
    contents_ = v;

    // Note: we change the navigation button only if a view is provided.
    // if |v| is NULL, this wizard is closing and updating anything may
    // cause a flash.
    if (contents_) {
      switch (s) {
        case FIRST_STEP_STYLE:
          previous_->SetVisible(false);
          next_->SetVisible(true);
          next_->SetLabel(l10n_util::GetString(IDS_WIZARD_NEXT));
          break;
        case NORMAL_STEP_STYLE:
          previous_->SetVisible(true);
          next_->SetVisible(true);
          next_->SetLabel(l10n_util::GetString(IDS_WIZARD_NEXT));
          break;
        case LAST_STEP_STYLE:
          previous_->SetVisible(true);
          next_->SetVisible(true);
          next_->SetLabel(l10n_util::GetString(IDS_WIZARD_DONE));
          break;
      }
      AddChildView(contents_);
      cancel_->SetVisible(true);

      // Reset the button labels to the default values.
      previous_->SetLabel(l10n_util::GetString(IDS_WIZARD_PREVIOUS));
      next_->SetLabel(l10n_util::GetString(IDS_WIZARD_NEXT));
    }
  }

  void SetTitle(const std::wstring& title) {
    title_->SetText(title);
  }

  void NavigationDescriptorChanged() {
    if (custom_navigation_descriptor_) {
      SetCustomNavigationDescriptor(custom_navigation_descriptor_);
    }
  }

  void SetCustomNavigationDescriptor(WizardNavigationDescriptor* wnd) {
    custom_navigation_descriptor_ = wnd;
    if (wnd->custom_default_button_) {
      if (wnd->default_label_.empty()) {
        next_->SetVisible(false);
      } else {
        next_->SetVisible(true);
        next_->SetLabel(wnd->default_label_);
      }
    }

    if (wnd->custom_alternate_button_) {
      if (wnd->alternate_label_.empty()) {
        previous_->SetVisible(false);
      } else {
        previous_->SetVisible(true);
        previous_->SetLabel(wnd->alternate_label_);
      }
    }

    cancel_->SetVisible(wnd->can_cancel_);
  }

  WizardNavigationDescriptor* GetCustomNavigationDescriptor() {
    return custom_navigation_descriptor_;
  }

  void EnableNextButton(bool f) {
    if (next_)
      next_->SetEnabled(f);
  }

  bool IsNextButtonEnabled() {
    if (next_)
      return next_->IsEnabled();
    else
      return false;
  }

  void EnablePreviousButton(bool f) {
    if (previous_)
      previous_->SetEnabled(f);
  }

  bool IsPreviousButtonEnabled() {
    if (previous_)
      return previous_->IsEnabled();
    else
      return false;
  }
 private:

  ChromeViews::Label* title_;
  ChromeViews::NativeButton* next_;
  ChromeViews::NativeButton* previous_;
  ChromeViews::NativeButton* cancel_;
  ChromeViews::View* contents_;
  WizardNavigationDescriptor* custom_navigation_descriptor_;

  Wizard* owner_;
  DISALLOW_EVIL_CONSTRUCTORS(WizardView);
};

////////////////////////////////////////////////////////////////////////////////
//
// Wizard implementation
//
////////////////////////////////////////////////////////////////////////////////

Wizard::Wizard(WizardDelegate* d) : delegate_(d),
                                    view_(NULL),
                                    state_(NULL),
                                    selected_step_(-1),
                                    is_running_(false) {
}

Wizard::~Wizard() {
  Abort();
  delete view_;
  RemoveAllSteps();

  delete state_;

  STLDeleteContainerPairSecondPointers(images_.begin(), images_.end());
}

void Wizard::SetState(DictionaryValue* state) {
  delete state_;
  state_ = state;
}

DictionaryValue* Wizard::GetState() {
  return state_;
}

void Wizard::AddStep(WizardStep* s) {
  steps_.push_back(s);
}

int Wizard::GetStepCount() const {
  return static_cast<int>(steps_.size());
}

WizardStep* Wizard::GetStepAt(int index) {
  DCHECK(index >= 0 && index < static_cast<int>(steps_.size()));
  return steps_[index];
}

void Wizard::RemoveAllSteps() {
  DCHECK(!is_running_);

  int c = static_cast<int>(steps_.size());
  while (--c >= 0)
    steps_[c]->Dispose();
  steps_.clear();
}

ChromeViews::View* Wizard::GetTopLevelView() {
  if (!view_) {
    view_ = new WizardView(this);
    // We own the view and it should never be deleted by the parent
    // view
    view_->SetParentOwned(false);
  }
  return view_;
}

void Wizard::Start() {
  DCHECK(view_ && view_->GetParent());
  DCHECK(steps_.size() > 0);
  DCHECK(!is_running_);
  is_running_ = true;
  SelectStepAt(0);
  view_->Layout();
}

void Wizard::Reset() {
  is_running_ = false;
  if (selected_step_ != -1)
    steps_[selected_step_]->WillBecomeInvisible(this,
                                                WizardStep::CANCEL_ACTION);
  selected_step_ = -1;
  view_->EnableNextButton(true);
  view_->EnablePreviousButton(true);
  view_->SetContents(NULL, WizardView::NORMAL_STEP_STYLE);

  if (view_->GetParent())
    view_->GetParent()->RemoveChildView(view_);
}

void Wizard::WizardDone(bool commit) {
 if (is_running_) {
    Reset();
    is_running_ = false;
    if (delegate_)
      delegate_->WizardClosed(commit);
  }
}

void Wizard::Abort() {
  WizardDone(false);
}

// Note: this private method doesn't call WillBecomeInvisible because
// it needs to be called before deciding what step to select next.
void Wizard::SelectStepAt(int index) {
  DCHECK(index >= 0 && index < static_cast<int>(steps_.size()));

  if (index == selected_step_)
    return;

  selected_step_ = index;

  WizardView::ContentsStyle style = WizardView::NORMAL_STEP_STYLE;
  if (selected_step_ == 0)
    style = WizardView::FIRST_STEP_STYLE;
  else if (selected_step_ == (static_cast<int>(steps_.size()) - 1))
    style = WizardView::LAST_STEP_STYLE;

  view_->SetContents(steps_[selected_step_]->GetView(this), style);
  steps_[selected_step_]->DidBecomeVisible(this);
  WizardNavigationDescriptor* wnd =
      steps_[selected_step_]->GetNavigationDescriptor();
  if (wnd)
    view_->SetCustomNavigationDescriptor(wnd);
  view_->SetTitle(steps_[selected_step_]->GetTitle(this));

  CSize s;
  view_->GetPreferredSize(&s);
  if (view_->GetWidth() != s.cx || view_->GetHeight() != s.cy)
    delegate_->ResizeTopLevelView(s.cx, s.cy);
  view_->Layout();
  view_->SchedulePaint();
}

void Wizard::NavigationDescriptorChanged() {
  view_->NavigationDescriptorChanged();
}

void Wizard::Cancel() {
  Abort();
}

void Wizard::SelectStep(bool is_forward) {
  int delta = is_forward ? 1 : -1;
  WizardNavigationDescriptor* wnd = view_->GetCustomNavigationDescriptor();
  if (wnd) {
    if (is_forward && wnd->custom_default_button_)
      delta = wnd->default_offset_;
    else if (!is_forward && wnd->custom_alternate_button_)
      delta = wnd->alternate_offset_;
  }

  if (selected_step_ != -1) {
    steps_[selected_step_]->WillBecomeInvisible(
        this, is_forward ? WizardStep::DEFAULT_ACTION :
        WizardStep::ALTERNATE_ACTION);
  }

  bool step_selected = false;
  int i;
  for (i = selected_step_ + delta;
       i >= 0 && i < static_cast<int>(steps_.size()); i += delta) {
    if (steps_[i]->IsEnabledFor(this)) {
      SelectStepAt(i);
      step_selected = true;
      break;
    }
  }

  if (!step_selected && is_forward) {
    WizardDone(true);
  }
}

void Wizard::SelectNextStep() {
  SelectStep(true);
}

void Wizard::SelectPreviousStep() {
  SelectStep(false);
}

void Wizard::SetImage(const std::wstring& image_name, SkBitmap* image) {
  ImageMap::iterator i = images_.find(image_name);
  if (i != images_.end())
    delete i->second;
  images_[image_name] = image;
}

SkBitmap* Wizard::GetImage(const std::wstring& image_name, bool remove_image) {
  ImageMap::iterator i = images_.find(image_name);
  SkBitmap* bitmap = NULL;
  if (i != images_.end()) {
    bitmap = i->second;
    if (remove_image) {
      images_.erase(i);
    }
  }
  return bitmap;
}

void Wizard::EnableNextStep(bool flag) {
  view_->EnableNextButton(flag);
}

bool Wizard::IsNextStepEnabled() const {
  return view_->IsNextButtonEnabled();
}

void Wizard::EnablePreviousStep(bool flag) {
  view_->EnablePreviousButton(flag);
}

bool Wizard::IsPreviousStepEnabled() const {
  return view_->IsPreviousButtonEnabled();
}
