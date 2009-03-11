// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_LANGUAGE_COMBOBOX_MODEL_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_LANGUAGE_COMBOBOX_MODEL_H__

#include "chrome/browser/profile.h"
#include "chrome/views/combo_box.h"

///////////////////////////////////////////////////////////////////////////////
// LanguageComboboxModel
//  The model that fills the dropdown of valid UI languages.
class LanguageComboboxModel : public views::ComboBox::Model {
 public:
  struct LocaleData {
    LocaleData() { }
    LocaleData(const std::wstring& name, const std::wstring& code)
        : native_name(name), locale_code(code) { }

    std::wstring native_name;
    std::wstring locale_code;  // E.g., en-us.
  };
  typedef std::map<std::wstring, LocaleData> LocaleDataMap;

  LanguageComboboxModel();

  // Overload using a profile and customized local_codes vector.
  LanguageComboboxModel(Profile* profile,
                        const std::vector<std::wstring>& locale_codes);

  // Temporary compatibility constructor.
  // TODO(port): remove this once locale codes are all switched to ASCII.
  LanguageComboboxModel(Profile* profile,
                        const std::vector<std::string>& locale_codes);

  virtual ~LanguageComboboxModel() {}

  void InitNativeNames(const std::vector<std::wstring>& locale_codes);

  // Overridden from views::Combobox::Model:
  virtual int GetItemCount(views::ComboBox* source);

  virtual std::wstring GetItemAt(views::ComboBox* source, int index);

  // Return the locale for the given index.  E.g., may return pt-BR.
  std::wstring GetLocaleFromIndex(int index);

  // Returns the index for the given locale.  Returns -1 if the locale is not
  // in the combobox model.
  int GetIndexFromLocale(const std::wstring& locale);

  // Returns the index of the language currently specified in the user's
  // preference file.  Note that it's possible for language A to be picked
  // while chrome is currently in language B if the user specified language B
  // via --lang.  Since --lang is not a persistent setting, it seems that it
  // shouldn't be reflected in this combo box.  We return -1 if the value in
  // the pref doesn't map to a know language (possible if the user edited the
  // prefs file manually).
  int GetSelectedLanguageIndex(const std::wstring& prefs);

 private:
  // The name all the locales in the current application locale.
  std::vector<std::wstring> locale_names_;

  // A map of some extra data (LocaleData) keyed off the name of the locale.
  LocaleDataMap native_names_;

  // Profile.
  Profile* profile_;

  DISALLOW_EVIL_CONSTRUCTORS(LanguageComboboxModel);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_OPTIONS_LANGUAGE_COMBOBOX_MODEL_H__
