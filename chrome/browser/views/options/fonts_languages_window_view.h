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

#ifndef CHROME_BROWSER_FONTS_LANGUAGE_WINDOW_H__
#define CHROME_BROWSER_FONTS_LANGUAGE_WINDOW_H__

#include "chrome/views/dialog_delegate.h"
#include "chrome/views/tabbed_pane.h"
#include "chrome/views/view.h"
#include "chrome/views/window.h"

class Profile;
class FontsPageView;
class LanguagesPageView;

///////////////////////////////////////////////////////////////////////////////
// FontsLanguagesWindowView
//
//  The contents of the "Fonts and Languages Preferences" dialog window.
//
class FontsLanguagesWindowView : public ChromeViews::View,
                                 public ChromeViews::DialogDelegate {
 public:
  explicit FontsLanguagesWindowView(Profile* profile);
  virtual ~FontsLanguagesWindowView();

  ChromeViews::Window* container() const { return container_; }
  void set_container(ChromeViews::Window* container) {
    container_ = container;
  }

  // ChromeViews::DialogDelegate implementation:
  virtual bool Accept();
  virtual std::wstring GetWindowTitle() const;

  // ChromeViews::WindowDelegate Methods:
  virtual bool IsModal() const { return true; }

  // ChromeViews::View overrides:
  virtual void Layout();
  virtual void GetPreferredSize(CSize* out);

 protected:
  // ChromeViews::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);
 private:
  // Init the assorted Tabbed pages
  void Init();

  // The Tab view that contains all of the options pages.
  ChromeViews::TabbedPane* tabs_;

  // The Options dialog window.
  ChromeViews::Window* container_;

  // Fonts Page View handle remembered so that prefs is updated only when
  // OK is pressed.
  FontsPageView* fonts_page_;

  // Languages Page View handle remembered so that prefs is updated only when
  // OK is pressed.
  LanguagesPageView* languages_page_;

  // The Profile associated with these options.
  Profile* profile_;

  DISALLOW_EVIL_CONSTRUCTORS(FontsLanguagesWindowView);
};

#endif  // #ifndef CHROME_BROWSER_FONTS_LANGUAGE_WINDOW_H__
