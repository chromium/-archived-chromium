// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_LANGUAGE_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_LANGUAGE_COMBOBOX_MODEL_H_

#include "chrome/browser/profile.h"
#include "views/controls/combobox/combobox.h"

///////////////////////////////////////////////////////////////////////////////
// LanguageComboboxModel
//  The model that fills the dropdown of valid UI languages.
class LanguageComboboxModel : public views::Combobox::Model {
 public:
  struct LocaleData {
    LocaleData() { }
    LocaleData(const std::wstring& name, const std::string& code)
        : native_name(name), locale_code(code) { }

    std::wstring native_name;
    std::string locale_code;  // E.g., en-us.
  };
  typedef std::map<std::wstring, LocaleData> LocaleDataMap;

  LanguageComboboxModel();

  // Temporary compatibility constructor.
  LanguageComboboxModel(Profile* profile,
                        const std::vector<std::string>& locale_codes);

  virtual ~LanguageComboboxModel() {}

  void InitNativeNames(const std::vector<std::string>& locale_codes);

  // Overridden from views::Combobox::Model:
  virtual int GetItemCount(views::Combobox* source);

  virtual std::wstring GetItemAt(views::Combobox* source, int index);

  // Return the locale for the given index.  E.g., may return pt-BR.
  std::string GetLocaleFromIndex(int index);

  // Returns the index for the given locale.  Returns -1 if the locale is not
  // in the combobox model.
  int GetIndexFromLocale(const std::string& locale);

  // Returns the index of the language currently specified in the user's
  // preference file.  Note that it's possible for language A to be picked
  // while chrome is currently in language B if the user specified language B
  // via --lang.  Since --lang is not a persistent setting, it seems that it
  // shouldn't be reflected in this combo box.  We return -1 if the value in
  // the pref doesn't map to a know language (possible if the user edited the
  // prefs file manually).
  int GetSelectedLanguageIndex(const std::wstring& prefs);

 private:
  // The names of all the locales in the current application locale.
  std::vector<std::wstring> locale_names_;

  // A map of some extra data (LocaleData) keyed off the name of the locale.
  LocaleDataMap native_names_;

  // Profile.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(LanguageComboboxModel);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_OPTIONS_LANGUAGE_COMBOBOX_MODEL_H_
