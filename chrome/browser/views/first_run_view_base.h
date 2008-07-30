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

#ifndef CHROME_BROWSER_VIEWS_FIRST_RUN_VIEW_BASE_H__
#define CHROME_BROWSER_VIEWS_FIRST_RUN_VIEW_BASE_H__
#include "chrome/browser/importer.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/view.h"

namespace ChromeViews {

class Window;
class ImageView;
class Separator;
class Throbber;

}

class Profile;
class ImporterHost;

// This class abstracts the code that creates the dialog look for the two
// first-run dialogs. This amounts to the bitmap, the two separators, the
// progress throbber and some common resize code.
class FirstRunViewBase : public ChromeViews::View,
                         public ChromeViews::DialogDelegate {
 public:
  explicit FirstRunViewBase(Profile* profile);
  virtual ~FirstRunViewBase();

  // Overridden from ChromeViews::View.
  virtual void Layout();

  // Overridden from ChromeViews::WindowDelegate.
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;

 protected:
  // Returns the items that the first run process is required to import
  // from other browsers.
  int GetDefaultImportItems() const;

  // Creates the desktop and quick launch shortcut. Existing shortcut is lost.
  bool CreateDesktopShortcut();
  bool CreateQuickLaunchShortcut();

  // Modifies the chrome configuration so that the first-run dialogs are not
  // shown again.
  bool FirstRunComplete();
  
  // Disables the standard buttons of the dialog. Useful when importing.
  void DisableButtons();
  // Computes a tight dialog width given a contained UI element.
  void AdjustDialogWidth(const ChromeViews::View* sub_view);

  // Sets a minimum dialog size.
  void SetMinimumDialogWidth(int width);

  // Returns the background image. It is useful for getting the metrics.
  const ChromeViews::ImageView* background_image() const {
    return background_image_;
  }
  // Returns the computed preferred width of the dialog. This value can change
  // when AdjustDialogWidth() is called during layout.
  int preferred_width() const {
    return preferred_width_;
  }

  scoped_refptr<ImporterHost> importer_host_;
  Profile* profile_;

 private:
  // Initializes the controls on the dialog.
  void SetupControls();
  ChromeViews::ImageView* background_image_;
  ChromeViews::Separator* separator_1_;
  ChromeViews::Separator* separator_2_;
  int preferred_width_;

  DISALLOW_EVIL_CONSTRUCTORS(FirstRunViewBase);
};

#endif  // CHROME_BROWSER_VIEWS_FIRST_RUN_VIEW_BASE_H__
