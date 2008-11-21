// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "skia/include/SkBitmap.h"

class AlertInfoBarDelegate;
class ConfirmInfoBarDelegate;
class InfoBar;

// An interface implemented by objects wishing to control an InfoBar.
// Implementing this interface is not sufficient to use an InfoBar, since it
// does not map to a specific InfoBar type. Instead, you must implement either
// AlertInfoBarDelegate or ConfirmInfoBarDelegate, or override with your own
// delegate for your own InfoBar variety.
class InfoBarDelegate {
 public:
  // Returns true if the supplied |delegate| is equal to this one. Equality is
  // left to the implementation to define. This function is called by the
  // TabContents when determining whether or not a delegate should be added
  // because a matching one already exists. If this function returns true, the
  // TabContents will not add the new delegate because it considers one to
  // already be present.
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const {
    return false;
  }

  // Returns true if the InfoBar should be closed automatically after the page
  // is navigated.
  virtual bool ShouldCloseOnNavigate() const { return true; }

  // Called after the InfoBar is closed. The delegate is free to delete itself
  // at this point.
  virtual void InfoBarClosed() {}

  // Called to create the InfoBar. Implementation of this method is
  // platform-specific.
  virtual InfoBar* CreateInfoBar() = 0;

  // Returns a pointer to the ConfirmInfoBarDelegate interface, if implemented.
  virtual AlertInfoBarDelegate* AsAlertInfoBarDelegate() {
    return NULL;
  }

  // Returns a pointer to the ConfirmInfoBarDelegate interface, if implemented.
  virtual ConfirmInfoBarDelegate* AsConfirmInfoBarDelegate() {
    return NULL;
  }
};

// An interface derived from InfoBarDelegate implemented by objects wishing to
// control an AlertInfoBar.
class AlertInfoBarDelegate : public InfoBarDelegate {
 public:
  // Returns the message string to be displayed for the InfoBar.
  virtual std::wstring GetMessageText() const = 0;

  // Return the icon to be shown for this InfoBar. If the returned bitmap is
  // NULL, no icon is shown.
  virtual SkBitmap* GetIcon() const { return NULL; }

  // Overridden from InfoBarDelegate:
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const;
  virtual InfoBar* CreateInfoBar();
  virtual AlertInfoBarDelegate* AsAlertInfoBarDelegate() { return this; }
};

// An interface derived from InfoBarDelegate implemented by objects wishing to
// control a ConfirmInfoBar.
class ConfirmInfoBarDelegate : public AlertInfoBarDelegate {
 public:
  enum InfoBarButton {
    BUTTON_NONE = 0,
    BUTTON_OK,
    BUTTON_CANCEL
  };

  // Return the buttons to be shown for this InfoBar.
  virtual int GetButtons() const {
    return BUTTON_NONE;
  }

  // Return the label for the specified button. The default implementation
  // returns "OK" for the OK button and "Cancel" for the Cancel button.
  virtual std::wstring GetButtonLabel(InfoBarButton button) const;

  // Called when the OK button is pressed.
  virtual void Accept() {}

  // Called when the Cancel button is pressed.
  virtual void Cancel() {}

  // Overridden from InfoBarDelegate:
  virtual InfoBar* CreateInfoBar();
  virtual ConfirmInfoBarDelegate* AsConfirmInfoBarDelegate() {
    return this;
  }
};

// Simple implementations for common use cases ---------------------------------

class SimpleAlertInfoBarDelegate : public AlertInfoBarDelegate {
 public:
  SimpleAlertInfoBarDelegate(const std::wstring& message, SkBitmap* icon);

  // Overridden from AlertInfoBarDelegate:
  virtual std::wstring GetMessageText() const;
  virtual SkBitmap* GetIcon() const;
  virtual void InfoBarClosed();

 private:
  std::wstring message_;
  SkBitmap* icon_;

  DISALLOW_COPY_AND_ASSIGN(SimpleAlertInfoBarDelegate);
};

#endif  // #ifndef CHROME_BROWSER_INFOBAR_DELEGATE_H_
