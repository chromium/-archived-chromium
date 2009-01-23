// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with localized
// content.

#ifndef CHROME_COMMON_L10N_UTIL_H_
#define CHROME_COMMON_L10N_UTIL_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "third_party/icu38/public/common/unicode/ubidi.h"

class PrefService;

namespace l10n_util {

const wchar_t kRightToLeftMark[] = L"\x200f";
const wchar_t kLeftToRightMark[] = L"\x200e";

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

// Returns the locale-dependent extended window styles.
// This function is used for adding locale-dependent extended window styles
// (e.g. WS_EX_LAYOUTRTL, WS_EX_RTLREADING, etc.) when creating a window.
// Callers should OR this value into their extended style value when creating
// a window.
int GetExtendedStyles();

// TODO(xji):
// This is a temporary name, it will eventually replace GetExtendedStyles
int GetExtendedTooltipStyles();

// Returns the default text alignment to be used when drawing text on a
// ChromeCanvas based on the directionality of the system locale language. This
// function is used by ChromeCanvas::DrawStringInt when the text alignment is
// not specified.
//
// This function returns either ChromeCanvas::TEXT_ALIGN_LEFT or
// ChromeCanvas::TEXT_ALIGN_RIGHT.
int DefaultCanvasTextAlignment();

#if defined(OS_WIN)
// Give an HWND, this function sets the WS_EX_LAYOUTRTL extended style for the
// underlying window. When this style is set, the UI for the window is going to
// be mirrored. This is generally done for the UI of right-to-left languages
// such as Hebrew.
void HWNDSetRTLLayout(HWND hwnd);
#endif

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
