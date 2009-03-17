// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FONTS_LANGUAGE_WINDOW_H__
#define CHROME_BROWSER_FONTS_LANGUAGE_WINDOW_H__

#include "chrome/views/tabbed_pane.h"
#include "chrome/views/view.h"
#include "chrome/views/window/dialog_delegate.h"
#include "chrome/views/window/window.h"

class Profile;
class FontsPageView;
class LanguagesPageView;

///////////////////////////////////////////////////////////////////////////////
// FontsLanguagesWindowView
//
//  The contents of the "Fonts and Languages Preferences" dialog window.
//
class FontsLanguagesWindowView : public views::View,
                                 public views::DialogDelegate {
 public:
  explicit FontsLanguagesWindowView(Profile* profile);
  virtual ~FontsLanguagesWindowView();

  // views::DialogDelegate implementation:
  virtual bool Accept();

  // views::WindowDelegate Methods:
  virtual bool IsModal() const { return true; }
  virtual std::wstring GetWindowTitle() const;
  virtual views::View* GetContentsView();

  // views::View overrides:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

  // Selects the Languages tab.
  void SelectLanguagesTab();

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);
 private:
  // Init the assorted Tabbed pages
  void Init();

  // The Tab view that contains all of the options pages.
  views::TabbedPane* tabs_;

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
