// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHARACTER_ENCODING_H__
#define CHROME_BROWSER_CHARACTER_ENCODING_H__

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"

class CharacterEncoding {
 public:
  // Enumeration of the types of Browser encoding name we
  // currently support. This is defined outside of Browser
  // to avoid cyclical dependencies.

  // Structure to save encoding information.
  struct EncodingInfo {
    explicit EncodingInfo(int id);
    // Gets string key of EncodingInfo. With this method, we can use
    // l10n_util::SortVectorWithStringKey to sort the encoding menu items
    // by current locale character sequence. We need to keep the order within
    // encoding category name, that's why we use category name as key.
    const std::wstring& GetStringKey() const { return encoding_category_name; }

    // Encoding command id.
    int encoding_id;
    // Encoding display name.
    std::wstring encoding_display_name;
    // Encoding category name.
    std::wstring encoding_category_name;
  };

  // Return canonical encoding name according to the command ID.
  // THIS FUNCTION IS NOT THREADSAFE. You must run this function
  // only in UI thread.
  static std::wstring GetCanonicalEncodingNameByCommandId(int id);

  // Return display name of canonical encoding according to the command
  // ID. THIS FUNCTION IS NOT THREADSAFE. You must run this function
  // only in UI thread.
  static std::wstring GetCanonicalEncodingDisplayNameByCommandId(int id);

  // Return count number of all supported canonical encoding.
  static int GetSupportCanonicalEncodingCount();

  // Return canonical encoding name according to the index, which starts
  // from zero to GetSupportCanonicalEncodingCount() - 1. THIS FUNCTION
  // IS NOT THREADSAFE. You must run this function only in UI thread.
  static std::wstring GetCanonicalEncodingNameByIndex(int index);

  // Return display name of canonical encoding according to the index,
  // which starts from zero to GetSupportCanonicalEncodingCount() - 1.
  // THIS FUNCTION IS NOT THREADSAFE. You must run this function
  // only in UI thread.
  static std::wstring GetCanonicalEncodingDisplayNameByIndex(int index);

  // Return encoding command id according to the index, which starts from
  // zero to GetSupportCanonicalEncodingCount() - 1. Otherwise returns 0.
  static int GetEncodingCommandIdByIndex(int index);

  // Return canonical encoding name according to the encoding alias name. THIS
  // FUNCTION IS NOT THREADSAFE. You must run this function only in UI thread.
  static std::wstring GetCanonicalEncodingNameByAliasName(
      const std::wstring& alias_name);

  // Returns the pointer of a vector of EncodingInfos corresponding to
  // encodings to display in the encoding menu. The locale-dependent static
  // encodings come at the top of the list and recently selected encodings
  // come next. Finally, the rest of encodings are listed.
  // The vector will be created and destroyed by CharacterEncoding.
  // The returned std::vector is maintained by this class. The parameter
  // |locale| points to the current application (UI) locale. The parameter
  // |locale_encodings| is string of static encodings list which is from the
  // corresponding string resource that is stored in the resource bundle.
  // The parameter |recently_select_encodings| is string of encoding list which
  // is from user recently selected. THIS FUNCTION IS NOT THREADSAFE. You must
  // run this function only in UI thread.
  static const std::vector<EncodingInfo>* GetCurrentDisplayEncodings(
      const std::wstring& locale,
      const std::wstring& locale_encodings,
      const std::wstring& recently_select_encodings);

  // This function is for updating |original_selected_encoding_list| with a
  // |new_selected_encoding_id|. If the encoding is already in the original
  // list, then returns false. Otherwise |selected_encoding_list| will return a
  // new string for user selected encoding short list and function returns true.
  static bool UpdateRecentlySelectdEncoding(
      const std::wstring& original_selected_encodings,
      int new_selected_encoding_id,
      std::wstring* selected_encodings);

  // Get encoding command id according to input encoding name. If the name is
  // valid, return corresponding encoding command id. Otherwise return 0;
  static int GetCommandIdByCanonicalEncodingName(
      const std::wstring& encoding_name);

 private:
  // Disallow instantiating it since this class only contains static methods.
  DISALLOW_IMPLICIT_CONSTRUCTORS(CharacterEncoding);
};

#endif  // CHROME_BROWSER_CHARACTER_ENCODING_H__

