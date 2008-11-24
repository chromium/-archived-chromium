// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFOBARS_INFOBARS_H_
#define CHROME_BROWSER_VIEWS_INFOBARS_INFOBARS_H_

#include "chrome/browser/infobar_delegate.h"
#include "chrome/views/base_button.h"
#include "chrome/views/native_button.h"

class InfoBarContainer;
class SlideAnimation;
namespace views {
class Button;
class ImageView;
class Label;
}

// This file contains implementations for some general purpose InfoBars. See
// chrome/browser/infobar_delegate.h for the delegate interface(s) that you must
// implement to use these.

class InfoBar : public views::View,
                public views::BaseButton::ButtonListener,
                public AnimationDelegate {
 public:
  explicit InfoBar(InfoBarDelegate* delegate);
  virtual ~InfoBar();

  InfoBarDelegate* delegate() const { return delegate_; }

  void set_container(InfoBarContainer* container) { container_ = container; }

  // Starts animating the InfoBar open.
  void AnimateOpen();

  // Opens the InfoBar immediately.
  void Open();

  // Starts animating the InfoBar closed. It will not be closed until the
  // animation has completed, when |Close| will be called.
  void AnimateClose();
  
  // Closes the InfoBar immediately and removes it from its container. Notifies
  // the delegate that it has closed. The InfoBar is deleted after this function
  // is called.
  void Close();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

 protected:
  // Returns the available width of the View for use by child view layout,
  // excluding the close button.
  virtual int GetAvailableWidth() const;

 private:
  // Overridden from views::Button::ButtonListener:
  virtual void ButtonPressed(views::BaseButton* sender);

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // The InfoBar's container
  InfoBarContainer* container_;

  // The InfoBar's delegate.
  InfoBarDelegate* delegate_;

  // The Close Button at the right edge of the InfoBar.
  views::Button* close_button_;

  // The animation that runs when the InfoBar is opened or closed.
  scoped_ptr<SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(InfoBar);
};

class AlertInfoBar : public InfoBar {
 public:
  explicit AlertInfoBar(AlertInfoBarDelegate* delegate);
  virtual ~AlertInfoBar();

  // Overridden from views::View:
  virtual void Layout();

 protected:
  views::Label* label() const { return label_; }
  views::ImageView* icon() const { return icon_; }

 private:
  AlertInfoBarDelegate* GetDelegate();

  views::Label* label_;
  views::ImageView* icon_;

  DISALLOW_COPY_AND_ASSIGN(AlertInfoBar);
};

class ConfirmInfoBar : public AlertInfoBar,
                       public views::NativeButton::Listener {
 public:
  explicit ConfirmInfoBar(ConfirmInfoBarDelegate* delegate);
  virtual ~ConfirmInfoBar();

  // Overridden from views::View:
  virtual void Layout();

 protected:
  // Overridden from views::View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // Overridden from views::NativeButton::Listener:
  virtual void ButtonPressed(views::NativeButton* sender);

  // Overridden from InfoBar:
  virtual int GetAvailableWidth() const;

 private:
  void Init();

  ConfirmInfoBarDelegate* GetDelegate();

  views::NativeButton* ok_button_;
  views::NativeButton* cancel_button_;

  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmInfoBar);
};


#endif  // #ifndef CHROME_BROWSER_VIEWS_INFOBARS_INFOBARS_H_
