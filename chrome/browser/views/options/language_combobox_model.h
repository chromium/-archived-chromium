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

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_LANGUAGE_COMBOBOX_MODEL_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_LANGUAGE_COMBOBOX_MODEL_H__

#include "chrome/browser/profile.h"
#include "chrome/views/combo_box.h"

///////////////////////////////////////////////////////////////////////////////
// LanguageComboboxModel
//  The model that fills the dropdown of valid UI languages.
class LanguageComboboxModel : public ChromeViews::ComboBox::Model {
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

  virtual ~LanguageComboboxModel() {}

  void InitNativeNames(const std::vector<std::wstring>& locale_codes);

  // Overridden from ChromeViews::Combobox::Model:
  virtual int GetItemCount(ChromeViews::ComboBox* source);

  virtual std::wstring GetItemAt(ChromeViews::ComboBox* source, int index);

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
