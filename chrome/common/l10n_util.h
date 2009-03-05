// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with localized
// content.

#ifndef CHROME_COMMON_L10N_UTIL_H_
#define CHROME_COMMON_L10N_UTIL_H_

#include "build/build_config.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "unicode/coll.h"
#include "unicode/locid.h"
#include "unicode/rbbi.h"
#include "unicode/ubidi.h"
#include "unicode/uchar.h"

class FilePath;
class PrefService;

namespace l10n_util {

const char16 kRightToLeftMark = 0x200f;
const char16 kLeftToRightMark = 0x200e;
const char16 kLeftToRightEmbeddingMark = 0x202A;
const char16 kRightToLeftEmbeddingMark = 0x202B;
const char16 kPopDirectionalFormatting = 0x202C;

// This method is responsible for determining the locale as defined below. In
// nearly all cases you shouldn't call this, rather use GetApplicationLocale
// defined on browser_process.
//
// Returns the locale used by the Application.  First we use the value from the
// command line (--lang), second we try the value in the prefs file (passed in
// as |pref_locale|), finally, we fall back on the system locale. We only return
// a value if there's a corresponding resource DLL for the locale.  Otherwise,
// we fall back to en-us.
std::wstring GetApplicationLocale(const std::wstring& pref_locale);

// This method returns the Local Name of the Locale Code. For example, for
// |local_code_wstr| = "en-US", it returns "English (United States)".
// |app_locale_wstr| can be obtained in the UI thread - for example:
// const std::wstring app_locale_wstr = g_browser_process->
//    GetApplicationLocale();
// If |is_for_ui| is true, U+200F is appended so that it can be
// rendered properly in a RTL Chrome.
std::wstring GetLocalName(const std::wstring& locale_code_wstr,
                          const std::wstring& app_locale_wstr,
                          bool is_for_ui);

// Pulls resource string from the string bundle and returns it.
std::wstring GetString(int message_id);

// Get a resource string and replace $1-$2-$3 with |a| and |b|
// respectively.  Additionally, $$ is replaced by $.
std::wstring GetStringF(int message_id,
                        const std::wstring& a);
std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        const std::wstring& b);
std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        const std::wstring& b,
                        const std::wstring& c);

// Variants that return the offset(s) of the replaced parameters. The
// vector based version returns offsets ordered by parameter. For example if
// invoked with a and b offsets[0] gives the offset for a and offsets[1] the
// offset of b regardless of where the parameters end up in the string.
std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        size_t* offset);
std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        const std::wstring& b,
                        std::vector<size_t>* offsets);

// Convenience formatters for a single number.
std::wstring GetStringF(int message_id, int a);
std::wstring GetStringF(int message_id, int64 a);

// Truncates the string to length characters. This breaks the string at
// the first word break before length, adding the horizontal ellipsis
// character (unicode character 0x2026) to render ...
// The supplied string is returned if the string has length characters or
// less.
std::wstring TruncateString(const std::wstring& string, size_t length);

// Returns the lower case equivalent of string.
std::wstring ToLower(const std::wstring& string);

// Represents the text direction returned by the GetTextDirection() function.
enum TextDirection {
  UNKNOWN_DIRECTION,
  RIGHT_TO_LEFT,
  LEFT_TO_RIGHT,
};

// Returns the locale-specific text direction.
// This function retrieves the application locale and determines the text
// direction. Its possible results are listed below:
//  * LEFT_TO_RIGHT: Left-To-Right (e.g. English, Chinese, etc.);
//  * RIGHT_TO_LEFT: Right-To-Left (e.g. Arabic, Hebrew, etc.), and;
//  * UNKNOWN_DIRECTION: unknown (or error).
TextDirection GetTextDirection();

// Given the string in |text|, returns the directionality of the first
// character with strong directionality in the string. If no character in the
// text has strong directionality, LEFT_TO_RIGHT is returned. The Bidi
// character types L, LRE, LRO, R, AL, RLE, and RLO are considered as strong
// directionality characters. Please refer to http://unicode.org/reports/tr9/
// for more information.
TextDirection GetFirstStrongCharacterDirection(const std::wstring& text);

// Given the string in |text|, this function creates a copy of the string with
// the appropriate Unicode formatting marks that mark the string direction
// (either left-to-right or right-to-left). The new string is returned in
// |localized_text|. The function checks both the current locale and the
// contents of the string in order to determine the direction of the returned
// string. The function returns true if the string in |text| was properly
// adjusted.
//
// Certain LTR strings are not rendered correctly when the context is RTL. For
// example, the string "Foo!" will appear as "!Foo" if it is rendered as is in
// an RTL context. Calling this function will make sure the returned localized
// string is always treated as a right-to-left string. This is done by
// inserting certain Unicode formatting marks into the returned string.
//
// TODO(idana) bug# 1206120: this function adjusts the string in question only
// if the current locale is right-to-left. The function does not take care of
// the opposite case (an RTL string displayed in an LTR context) since
// adjusting the string involves inserting Unicode formatting characters that
// Windows does not handle well unless right-to-left language support is
// installed. Since the English version of Windows doesn't have right-to-left
// language support installed by default, inserting the direction Unicode mark
// results in Windows displaying squares.
bool AdjustStringForLocaleDirection(const std::wstring& text,
                                    std::wstring* localized_text);

// Returns true if the string contains at least one character with strong right
// to left directionality; that is, a character with either R or AL Unicode
// BiDi character type.
bool StringContainsStrongRTLChars(const std::wstring& text);

// Wraps a string with an LRE-PDF pair which essentialy marks the string as a
// Left-To-Right string. Doing this is useful in order to make sure LTR
// strings are rendered properly in an RTL context.
void WrapStringWithLTRFormatting(std::wstring* text);

// Wraps a string with an RLE-PDF pair which essentialy marks the string as a
// Right-To-Left string. Doing this is useful in order to make sure RTL
// strings are rendered properly in an LTR context.
void WrapStringWithRTLFormatting(std::wstring* text);

// Wraps individual file path components to get them to display correctly in an
// RTL UI. All filepaths should be passed through this function before display
// in UI for RTL locales.
void WrapPathWithLTRFormatting(const FilePath& path,
                               string16* rtl_safe_path);

// Returns the default text alignment to be used when drawing text on a
// ChromeCanvas based on the directionality of the system locale language. This
// function is used by ChromeCanvas::DrawStringInt when the text alignment is
// not specified.
//
// This function returns either ChromeCanvas::TEXT_ALIGN_LEFT or
// ChromeCanvas::TEXT_ALIGN_RIGHT.
int DefaultCanvasTextAlignment();

// Compares the two strings using the specified collator.
UCollationResult CompareStringWithCollator(const Collator* collator,
                                           const std::wstring& lhs,
                                           const std::wstring& rhs);

// Used by SortStringsUsingMethod. Invokes a method on the objects passed to
// operator (), comparing the string results using a collator.
template <class T, class Method>
class StringMethodComparatorWithCollator :
    public std::binary_function<const std::wstring&,
                                const std::wstring&,
                                bool> {
 public:
  StringMethodComparatorWithCollator(Collator* collator, Method method)
      : collator_(collator),
        method_(method) { }

  // Returns true if lhs preceeds rhs.
  bool operator() (T* lhs_t, T* rhs_t) {
    return CompareStringWithCollator(collator_, (lhs_t->*method_)(),
                                     (rhs_t->*method_)()) == UCOL_LESS;
  }

 private:
  Collator* collator_;
  Method method_;
};

// Used by SortStringsUsingMethod. Invokes a method on the objects passed to
// operator (), comparing the string results using <.
template <class T, class Method>
class StringMethodComparator : public std::binary_function<const std::wstring&,
                                                           const std::wstring&,
                                                           bool> {
 public:
  explicit StringMethodComparator(Method method) : method_(method) { }

  // Returns true if lhs preceeds rhs.
  bool operator() (T* lhs_t, T* rhs_t) {
    return (lhs_t->*method_)() < (rhs_t->*method_)();
  }

 private:
  Method method_;
};

// Sorts the objects in |elements| using the method |method|, which must return
// a string. Sorting is done using a collator, unless a collator can not be
// found in which case the strings are sorted using the operator <.
template <class T, class Method>
void SortStringsUsingMethod(const std::wstring& locale,
                            std::vector<T*>* elements,
                            Method method) {
  UErrorCode error = U_ZERO_ERROR;
  Locale loc(WideToUTF8(locale).c_str());
  scoped_ptr<Collator> collator(Collator::createInstance(loc, error));
  if (U_FAILURE(error)) {
    sort(elements->begin(), elements->end(),
         StringMethodComparator<T,Method>(method));
    return;
  }

  std::sort(elements->begin(), elements->end(),
      StringMethodComparatorWithCollator<T,Method>(collator.get(), method));
}

// Compares two elements' string keys and returns true if the first element's
// string key is less than the second element's string key. The Element must
// have a method like the follow format to return the string key.
// const std::wstring& GetStringKey() const;
// This uses the locale specified in the constructor.
template <class Element>
class StringComparator : public std::binary_function<const Element&,
                                                     const Element&,
                                                     bool> {
 public:
  explicit StringComparator(Collator* collator)
      : collator_(collator) { }

  // Returns true if lhs precedes rhs.
  bool operator()(const Element& lhs, const Element& rhs) {
    const std::wstring& lhs_string_key = lhs.GetStringKey();
    const std::wstring& rhs_string_key = rhs.GetStringKey();

    return StringComparator<std::wstring>(collator_)(lhs_string_key,
                                                     rhs_string_key);
  }

 private:
  Collator* collator_;
};

// Specialization of operator() method for std::wstring version.
template <>
bool StringComparator<std::wstring>::operator()(const std::wstring& lhs,
                                                const std::wstring& rhs);

// In place sorting of |elements| of a vector according to the string key of
// each element in the vector by using collation rules for |locale|.
// |begin_index| points to the start position of elements in the vector which
// want to be sorted. |end_index| points to the end position of elements in the
// vector which want to be sorted
template <class Element>
void SortVectorWithStringKey(const std::wstring& locale,
                             std::vector<Element>* elements,
                             unsigned int begin_index,
                             unsigned int end_index,
                             bool needs_stable_sort) {
  DCHECK(begin_index >= 0 && begin_index < end_index &&
         end_index <= static_cast<unsigned int>(elements->size()));
  UErrorCode error = U_ZERO_ERROR;
  Locale loc(WideToASCII(locale).c_str());
  scoped_ptr<Collator> collator(Collator::createInstance(loc, error));
  if (U_FAILURE(error))
    collator.reset();
  StringComparator<Element> c(collator.get());
  if (needs_stable_sort) {
    stable_sort(elements->begin() + begin_index,
                elements->begin() + end_index,
                c);
  } else {
    sort(elements->begin() + begin_index, elements->begin() + end_index, c);
  }
}

template <class Element>
void SortVectorWithStringKey(const std::wstring& locale,
                             std::vector<Element>* elements,
                             bool needs_stable_sort) {
  SortVectorWithStringKey<Element>(locale, elements, 0, elements->size(),
                                   needs_stable_sort);
}

// In place sorting of strings using collation rules for |locale|.
void SortStrings(const std::wstring& locale,
                 std::vector<std::wstring>* strings);

// Returns a vector of available locale codes. E.g., a vector containing
// en-US, es, fr, fi, pt-PT, pt-BR, etc.
const std::vector<std::wstring>& GetAvailableLocales();

// A simple wrapper class for the bidirectional iterator of ICU.
// This class uses the bidirectional iterator of ICU to split a line of
// bidirectional texts into visual runs in its display order.
class BiDiLineIterator {
 public:
  BiDiLineIterator() : bidi_(NULL) { }
  ~BiDiLineIterator();

  // Initializes the bidirectional iterator with the specified text.  Returns
  // whether initialization succeeded.
  UBool Open(const std::wstring& text, bool right_to_left, bool url);

  // Returns the number of visual runs in the text, or zero on error.
  int CountRuns();

  // Gets the logical offset, length, and direction of the specified visual run.
  UBiDiDirection GetVisualRun(int index, int* start, int* length);

  // Given a start position, figure out where the run ends (and the BiDiLevel).
  void GetLogicalRun(int start, int* end, UBiDiLevel* level);

 private:
  UBiDi* bidi_;

  DISALLOW_COPY_AND_ASSIGN(BiDiLineIterator);
};

}

#endif  // CHROME_COMMON_L10N_UTIL_H_
