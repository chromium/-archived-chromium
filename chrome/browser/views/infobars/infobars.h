// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFOBARS_INFOBARS_H_
#define CHROME_BROWSER_VIEWS_INFOBARS_INFOBARS_H_

#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/views/base_button.h"
#include "chrome/views/link.h"
#include "chrome/views/native_button.h"

class InfoBarContainer;
class SlideAnimation;
namespace views {
class Button;
class ExternalFocusTracker;
class ImageView;
class Label;
}

// This file contains implementations for some general purpose InfoBars. See
// chrome/browser/tab_contents/infobar_delegate.h for the delegate interface(s)
// that you must implement to use these.

class InfoBar : public views::View,
                public views::BaseButton::ButtonListener,
                public AnimationDelegate {
 public:
  explicit InfoBar(InfoBarDelegate* delegate);
  virtual ~InfoBar();

  InfoBarDelegate* delegate() const { return delegate_; }

  // Set a link to the parent InfoBarContainer. This must be set before the
  // InfoBar is added to the view hierarchy.
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
  // Overridden from views::View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // Returns the available width of the View for use by child view layout,
  // excluding the close button.
  virtual int GetAvailableWidth() const;

  // Removes our associated InfoBarDelegate from the associated TabContents.
  // (Will lead to this InfoBar being closed).
  void RemoveInfoBar() const;

 private:
  // Overridden from views::Button::ButtonListener:
  virtual void ButtonPressed(views::BaseButton* sender);

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // Called when an InfoBar is added or removed from a view hierarchy to do
  // setup and shutdown.
  void InfoBarAdded();
  void InfoBarRemoved();

  // Destroys the external focus tracker, if present. If |restore_focus| is
  // true, restores focus to the view tracked by the focus tracker before doing
  // so.
  void DestroyFocusTracker(bool restore_focus);

  // Deletes this object (called after a return to the message loop to allow
  // the stack in ViewHierarchyChanged to unwind).
  void DeleteSelf();

  // The InfoBar's container
  InfoBarContainer* container_;

  // The InfoBar's delegate.
  InfoBarDelegate* delegate_;

  // The Close Button at the right edge of the InfoBar.
  views::Button* close_button_;

  // The animation that runs when the InfoBar is opened or closed.
  scoped_ptr<SlideAnimation> animation_;

  // Tracks and stores the last focused view which is not the InfoBar or any of
  // its children. Used to restore focus once the InfoBar is closed.
  scoped_ptr<views::ExternalFocusTracker> focus_tracker_;

  // Used to delete this object after a return to the message loop.
  ScopedRunnableMethodFactory<InfoBar> delete_factory_;

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

class LinkInfoBar : public InfoBar,
                    public views::LinkController {
 public:
  explicit LinkInfoBar(LinkInfoBarDelegate* delegate);
  virtual ~LinkInfoBar();

  // Overridden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Overridden from views::View:
  virtual void Layout();

 private:
  LinkInfoBarDelegate* GetDelegate();

  views::ImageView* icon_;
  views::Label* label_1_;
  views::Label* label_2_;
  views::Link* link_;

  DISALLOW_COPY_AND_ASSIGN(LinkInfoBar);
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
