// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"

#include <string.h>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "unicode/numfmt.h"
#include "unicode/ucnv.h"
#include "unicode/ucnv_cb.h"
#include "unicode/ucnv_err.h"
#include "unicode/ustring.h"

namespace {

inline bool IsValidCodepoint(uint32 code_point) {
  // Excludes the surrogate code points ([0xD800, 0xDFFF]) and
  // codepoints larger than 0x10FFFF (the highest codepoint allowed).
  // Non-characters and unassigned codepoints are allowed.
  return code_point < 0xD800u ||
         (code_point >= 0xE000u && code_point <= 0x10FFFFu);
}

// ToUnicodeCallbackSubstitute() is based on UCNV_TO_U_CALLBACK_SUSBSTITUTE
// in source/common/ucnv_err.c.

// Copyright (c) 1995-2006 International Business Machines Corporation
// and others
//
// All rights reserved.
//

// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, and/or
// sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, provided that the above copyright notice(s) and
// this permission notice appear in all copies of the Software and that
// both the above copyright notice(s) and this permission notice appear in
// supporting documentation.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
// OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
// INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
// OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
// OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
// OR PERFORMANCE OF THIS SOFTWARE.
//
// Except as contained in this notice, the name of a copyright holder
// shall not be used in advertising or otherwise to promote the sale, use
// or other dealings in this Software without prior written authorization
// of the copyright holder.

//  ___________________________________________________________________________
//
// All trademarks and registered trademarks mentioned herein are the property
// of their respective owners.

void ToUnicodeCallbackSubstitute(const void* context,
                                 UConverterToUnicodeArgs *to_args,
                                 const char* code_units,
                                 int32_t length,
                                 UConverterCallbackReason reason,
                                 UErrorCode * err) {
  static const UChar kReplacementChar = 0xFFFD;
  if (reason <= UCNV_IRREGULAR) {
      if (context == NULL ||
          (*(reinterpret_cast<const char*>(context)) == 'i' &&
           reason == UCNV_UNASSIGNED)) {
        *err = U_ZERO_ERROR;
        ucnv_cbToUWriteUChars(to_args, &kReplacementChar, 1, 0, err);
      }
      // else the caller must have set the error code accordingly.
  }
  // else ignore the reset, close and clone calls.
}

// ReadUnicodeCharacter --------------------------------------------------------

// Reads a UTF-8 stream, placing the next code point into the given output
// |*code_point|. |src| represents the entire string to read, and |*char_index|
// is the character offset within the string to start reading at. |*char_index|
// will be updated to index the last character read, such that incrementing it
// (as in a for loop) will take the reader to the next character.
//
// Returns true on success. On false, |*code_point| will be invalid.
bool ReadUnicodeCharacter(const char* src, int32 src_len,
                          int32* char_index, uint32* code_point_out) {
  // U8_NEXT expects to be able to use -1 to signal an error, so we must
  // use a signed type for code_point.  But this function returns false
  // on error anyway, so code_point_out is unsigned.
  int32 code_point;
  U8_NEXT(src, *char_index, src_len, code_point);
  *code_point_out = static_cast<uint32>(code_point);

  // The ICU macro above moves to the next char, we want to point to the last
  // char consumed.
  (*char_index)--;

  // Validate the decoded value.
  return IsValidCodepoint(code_point);
}

// Reads a UTF-16 character. The usage is the same as the 8-bit version above.
bool ReadUnicodeCharacter(const char16* src, int32 src_len,
                          int32* char_index, uint32* code_point) {
  if (U16_IS_SURROGATE(src[*char_index])) {
    if (!U16_IS_SURROGATE_LEAD(src[*char_index]) ||
        *char_index + 1 >= src_len ||
        !U16_IS_TRAIL(src[*char_index + 1])) {
      // Invalid surrogate pair.
      return false;
    }

    // Valid surrogate pair.
    *code_point = U16_GET_SUPPLEMENTARY(src[*char_index],
                                        src[*char_index + 1]);
    (*char_index)++;
  } else {
    // Not a surrogate, just one 16-bit word.
    *code_point = src[*char_index];
  }

  return IsValidCodepoint(*code_point);
}

#if defined(WCHAR_T_IS_UTF32)
// Reads UTF-32 character. The usage is the same as the 8-bit version above.
bool ReadUnicodeCharacter(const wchar_t* src, int32 src_len,
                          int32* char_index, uint32* code_point) {
  // Conversion is easy since the source is 32-bit.
  *code_point = src[*char_index];

  // Validate the value.
  return IsValidCodepoint(*code_point);
}
#endif  // defined(WCHAR_T_IS_UTF32)

// WriteUnicodeCharacter -------------------------------------------------------

// Appends a UTF-8 character to the given 8-bit string.
void WriteUnicodeCharacter(uint32 code_point, std::string* output) {
  if (code_point <= 0x7f) {
    // Fast path the common case of one byte.
    output->push_back(code_point);
    return;
  }

  // U8_APPEND_UNSAFE can append up to 4 bytes.
  int32 char_offset = static_cast<int32>(output->length());
  output->resize(char_offset + U8_MAX_LENGTH);

  U8_APPEND_UNSAFE(&(*output)[0], char_offset, code_point);

  // U8_APPEND_UNSAFE will advance our pointer past the inserted character, so
  // it will represent the new length of the string.
  output->resize(char_offset);
}

// Appends the given code point as a UTF-16 character to the STL string.
void WriteUnicodeCharacter(uint32 code_point, string16* output) {
  if (U16_LENGTH(code_point) == 1) {
    // Thie code point is in the Basic Multilingual Plane (BMP).
    output->push_back(static_cast<char16>(code_point));
  } else {
    // Non-BMP characters use a double-character encoding.
    int32 char_offset = static_cast<int32>(output->length());
    output->resize(char_offset + U16_MAX_LENGTH);
    U16_APPEND_UNSAFE(&(*output)[0], char_offset, code_point);
  }
}

#if defined(WCHAR_T_IS_UTF32)
// Appends the given UTF-32 character to the given 32-bit string.
inline void WriteUnicodeCharacter(uint32 code_point, std::wstring* output) {
  // This is the easy case, just append the character.
  output->push_back(code_point);
}
#endif  // defined(WCHAR_T_IS_UTF32)

// Generalized Unicode converter -----------------------------------------------

// Converts the given source Unicode character type to the given destination
// Unicode character type as a STL string. The given input buffer and size
// determine the source, and the given output STL string will be replaced by
// the result.
template<typename SRC_CHAR, typename DEST_STRING>
bool ConvertUnicode(const SRC_CHAR* src, size_t src_len, DEST_STRING* output) {
  output->clear();

  // ICU requires 32-bit numbers.
  bool success = true;
  int32 src_len32 = static_cast<int32>(src_len);
  for (int32 i = 0; i < src_len32; i++) {
    uint32 code_point;
    if (ReadUnicodeCharacter(src, src_len32, &i, &code_point)) {
      WriteUnicodeCharacter(code_point, output);
    } else {
      // TODO(jungshik): consider adding 'Replacement character' (U+FFFD)
      // in place of an invalid codepoint.
      success = false;
    }
  }
  return success;
}


// Guesses the length of the output in UTF-8 in bytes, and reserves that amount
// of space in the given string. We also assume that the input character types
// are unsigned, which will be true for UTF-16 and -32 on our systems. We assume
// the string length is greater than zero.
template<typename CHAR>
void ReserveUTF8Output(const CHAR* src, size_t src_len, std::string* output) {
  if (src[0] < 0x80) {
    // Assume that the entire input will be ASCII.
    output->reserve(src_len);
  } else {
    // Assume that the entire input is non-ASCII and will have 3 bytes per char.
    output->reserve(src_len * 3);
  }
}

// Guesses the size of the output buffer (containing either UTF-16 or -32 data)
// given some UTF-8 input that will be converted to it. See ReserveUTF8Output.
// We assume the source length is > 0.
template<typename STRING>
void ReserveUTF16Or32Output(const char* src, size_t src_len, STRING* output) {
  if (static_cast<unsigned char>(src[0]) < 0x80) {
    // Assume the input is all ASCII, which means 1:1 correspondence.
    output->reserve(src_len);
  } else {
    // Otherwise assume that the UTF-8 sequences will have 2 bytes for each
    // character.
    output->reserve(src_len / 2);
  }
}

bool ConvertFromUTF16(UConverter* converter, const UChar* uchar_src,
                      int uchar_len, OnStringUtilConversionError::Type on_error,
                      std::string* encoded) {
  int encoded_max_length = UCNV_GET_MAX_BYTES_FOR_STRING(uchar_len,
      ucnv_getMaxCharSize(converter));
  encoded->resize(encoded_max_length);

  UErrorCode status = U_ZERO_ERROR;

  // Setup our error handler.
  switch (on_error) {
    case OnStringUtilConversionError::FAIL:
      ucnv_setFromUCallBack(converter, UCNV_FROM_U_CALLBACK_STOP, 0,
                            NULL, NULL, &status);
      break;
    case OnStringUtilConversionError::SKIP:
    case OnStringUtilConversionError::SUBSTITUTE:
      ucnv_setFromUCallBack(converter, UCNV_FROM_U_CALLBACK_SKIP, 0,
                            NULL, NULL, &status);
      break;
    default:
      NOTREACHED();
  }

  // ucnv_fromUChars returns size not including terminating null
  int actual_size = ucnv_fromUChars(converter, &(*encoded)[0],
      encoded_max_length, uchar_src, uchar_len, &status);
  encoded->resize(actual_size);
  ucnv_close(converter);
  if (U_SUCCESS(status))
    return true;
  encoded->clear();  // Make sure the output is empty on error.
  return false;
}

// Set up our error handler for ToUTF-16 converters
void SetUpErrorHandlerForToUChars(OnStringUtilConversionError::Type on_error,
                                  UConverter* converter, UErrorCode* status) {
  switch (on_error) {
    case OnStringUtilConversionError::FAIL:
      ucnv_setToUCallBack(converter, UCNV_TO_U_CALLBACK_STOP, 0,
                          NULL, NULL, status);
      break;
    case OnStringUtilConversionError::SKIP:
      ucnv_setToUCallBack(converter, UCNV_TO_U_CALLBACK_SKIP, 0,
                          NULL, NULL, status);
      break;
    case OnStringUtilConversionError::SUBSTITUTE:
      ucnv_setToUCallBack(converter, ToUnicodeCallbackSubstitute, 0,
                          NULL, NULL, status);
      break;
    default:
      NOTREACHED();
  }
}

inline UConverterType utf32_platform_endian() {
#if U_IS_BIG_ENDIAN
  return UCNV_UTF32_BigEndian;
#else
  return UCNV_UTF32_LittleEndian;
#endif
}

}  // namespace

// UTF-8 <-> Wide --------------------------------------------------------------

std::string WideToUTF8(const std::wstring& wide) {
  std::string ret;
  if (wide.empty())
    return ret;

  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  WideToUTF8(wide.data(), wide.length(), &ret);
  return ret;
}

bool WideToUTF8(const wchar_t* src, size_t src_len, std::string* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  ReserveUTF8Output(src, src_len, output);
  return ConvertUnicode<wchar_t, std::string>(src, src_len, output);
}

std::wstring UTF8ToWide(const StringPiece& utf8) {
  std::wstring ret;
  if (utf8.empty())
    return ret;

  UTF8ToWide(utf8.data(), utf8.length(), &ret);
  return ret;
}

bool UTF8ToWide(const char* src, size_t src_len, std::wstring* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  ReserveUTF16Or32Output(src, src_len, output);
  return ConvertUnicode<char, std::wstring>(src, src_len, output);
}

// UTF-16 <-> Wide -------------------------------------------------------------

#if defined(WCHAR_T_IS_UTF16)

// When wide == UTF-16, then conversions are a NOP.
string16 WideToUTF16(const std::wstring& wide) {
  return wide;
}

bool WideToUTF16(const wchar_t* src, size_t src_len, string16* output) {
  output->assign(src, src_len);
  return true;
}

std::wstring UTF16ToWide(const string16& utf16) {
  return utf16;
}

bool UTF16ToWide(const char16* src, size_t src_len, std::wstring* output) {
  output->assign(src, src_len);
  return true;
}

#elif defined(WCHAR_T_IS_UTF32)

string16 WideToUTF16(const std::wstring& wide) {
  string16 ret;
  if (wide.empty())
    return ret;

  WideToUTF16(wide.data(), wide.length(), &ret);
  return ret;
}

bool WideToUTF16(const wchar_t* src, size_t src_len, string16* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  // Assume that normally we won't have any non-BMP characters so the counts
  // will be the same.
  output->reserve(src_len);
  return ConvertUnicode<wchar_t, string16>(src, src_len, output);
}

std::wstring UTF16ToWide(const string16& utf16) {
  std::wstring ret;
  if (utf16.empty())
    return ret;

  UTF16ToWide(utf16.data(), utf16.length(), &ret);
  return ret;
}

bool UTF16ToWide(const char16* src, size_t src_len, std::wstring* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  // Assume that normally we won't have any non-BMP characters so the counts
  // will be the same.
  output->reserve(src_len);
  return ConvertUnicode<char16, std::wstring>(src, src_len, output);
}

#endif  // defined(WCHAR_T_IS_UTF32)

// UTF16 <-> UTF8 --------------------------------------------------------------

#if defined(WCHAR_T_IS_UTF32)

bool UTF8ToUTF16(const char* src, size_t src_len, string16* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  ReserveUTF16Or32Output(src, src_len, output);
  return ConvertUnicode<char, string16>(src, src_len, output);
}

string16 UTF8ToUTF16(const std::string& utf8) {
  string16 ret;
  if (utf8.empty())
    return ret;

  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  UTF8ToUTF16(utf8.data(), utf8.length(), &ret);
  return ret;
}

bool UTF16ToUTF8(const char16* src, size_t src_len, std::string* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  ReserveUTF8Output(src, src_len, output);
  return ConvertUnicode<char16, std::string>(src, src_len, output);
}

std::string UTF16ToUTF8(const string16& utf16) {
  std::string ret;
  if (utf16.empty())
    return ret;

  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  UTF16ToUTF8(utf16.data(), utf16.length(), &ret);
  return ret;
}

#elif defined(WCHAR_T_IS_UTF16)
// Easy case since we can use the "wide" versions we already wrote above.

bool UTF8ToUTF16(const char* src, size_t src_len, string16* output) {
  return UTF8ToWide(src, src_len, output);
}

string16 UTF8ToUTF16(const std::string& utf8) {
  return UTF8ToWide(utf8);
}

bool UTF16ToUTF8(const char16* src, size_t src_len, std::string* output) {
  return WideToUTF8(src, src_len, output);
}

std::string UTF16ToUTF8(const string16& utf16) {
  return WideToUTF8(utf16);
}

#endif

// Codepage <-> Wide/UTF-16  ---------------------------------------------------

// Convert a wstring into the specified codepage_name.  If the codepage
// isn't found, return false.
bool WideToCodepage(const std::wstring& wide,
                    const char* codepage_name,
                    OnStringUtilConversionError::Type on_error,
                    std::string* encoded) {
#if defined(WCHAR_T_IS_UTF16)
  return UTF16ToCodepage(wide, codepage_name, on_error, encoded);
#elif defined(WCHAR_T_IS_UTF32)
  encoded->clear();

  UErrorCode status = U_ZERO_ERROR;
  UConverter* converter = ucnv_open(codepage_name, &status);
  if (!U_SUCCESS(status))
    return false;

  int utf16_len;
  // When wchar_t is wider than UChar (16 bits), transform |wide| into a
  // UChar* string.  Size the UChar* buffer to be large enough to hold twice
  // as many UTF-16 code units (UChar's) as there are Unicode code points,
  // in case each code points translates to a UTF-16 surrogate pair,
  // and leave room for a NUL terminator.
  std::vector<UChar> utf16(wide.length() * 2 + 1);
  u_strFromWCS(&utf16[0], utf16.size(), &utf16_len,
               wide.c_str(), wide.length(), &status);
  DCHECK(U_SUCCESS(status)) << "failed to convert wstring to UChar*";

  return ConvertFromUTF16(converter, &utf16[0], utf16_len, on_error, encoded);
#endif  // defined(WCHAR_T_IS_UTF32)
}

// Convert a UTF-16 string into the specified codepage_name.  If the codepage
// isn't found, return false.
bool UTF16ToCodepage(const string16& utf16,
                    const char* codepage_name,
                    OnStringUtilConversionError::Type on_error,
                    std::string* encoded) {
  encoded->clear();

  UErrorCode status = U_ZERO_ERROR;
  UConverter* converter = ucnv_open(codepage_name, &status);
  if (!U_SUCCESS(status))
    return false;

  return ConvertFromUTF16(converter, utf16.c_str(),
                          static_cast<int>(utf16.length()), on_error, encoded);
}

// Converts a string of the given codepage into wstring.
// If the codepage isn't found, return false.
bool CodepageToWide(const std::string& encoded,
                    const char* codepage_name,
                    OnStringUtilConversionError::Type on_error,
                    std::wstring* wide) {
#if defined(WCHAR_T_IS_UTF16)
  return CodepageToUTF16(encoded, codepage_name, on_error, wide);
#elif defined(WCHAR_T_IS_UTF32)
  wide->clear();

  UErrorCode status = U_ZERO_ERROR;
  UConverter* converter = ucnv_open(codepage_name, &status);
  if (!U_SUCCESS(status))
    return false;

  // The maximum length in 4 byte unit of UTF-32 output would be
  // at most the same as the number of bytes in input. In the worst
  // case of GB18030 (excluding escaped-based encodings like ISO-2022-JP),
  // this can be 4 times larger than actually needed.
  size_t wchar_max_length = encoded.length() + 1;

  // The byte buffer and its length to pass to ucnv_toAlgorithimic.
  char* byte_buffer = reinterpret_cast<char*>(
      WriteInto(wide, wchar_max_length));
  int byte_buffer_length = static_cast<int>(wchar_max_length) * 4;

  SetUpErrorHandlerForToUChars(on_error, converter, &status);
  int actual_size = ucnv_toAlgorithmic(utf32_platform_endian(),
                                       converter,
                                       byte_buffer,
                                       byte_buffer_length,
                                       encoded.data(),
                                       static_cast<int>(encoded.length()),
                                       &status);
  ucnv_close(converter);

  if (!U_SUCCESS(status)) {
    wide->clear();  // Make sure the output is empty on error.
    return false;
  }

  // actual_size is # of bytes.
  wide->resize(actual_size / 4);
  return true;
#endif  // defined(WCHAR_T_IS_UTF32)
}

// Converts a string of the given codepage into UTF-16.
// If the codepage isn't found, return false.
bool CodepageToUTF16(const std::string& encoded,
                     const char* codepage_name,
                     OnStringUtilConversionError::Type on_error,
                     string16* utf16) {
  utf16->clear();

  UErrorCode status = U_ZERO_ERROR;
  UConverter* converter = ucnv_open(codepage_name, &status);
  if (!U_SUCCESS(status))
    return false;

  // Even in the worst case, the maximum length in 2-byte units of UTF-16
  // output would be at most the same as the number of bytes in input. There
  // is no single-byte encoding in which a character is mapped to a
  // non-BMP character requiring two 2-byte units.
  //
  // Moreover, non-BMP characters in legacy multibyte encodings
  // (e.g. EUC-JP, GB18030) take at least 2 bytes. The only exceptions are
  // BOCU and SCSU, but we don't care about them.
  size_t uchar_max_length = encoded.length() + 1;

  SetUpErrorHandlerForToUChars(on_error, converter, &status);
  int actual_size = ucnv_toUChars(converter,
                                  WriteInto(utf16, uchar_max_length),
                                  static_cast<int>(uchar_max_length),
                                  encoded.data(),
                                  static_cast<int>(encoded.length()),
                                  &status);
  ucnv_close(converter);
  if (!U_SUCCESS(status)) {
    utf16->clear();  // Make sure the output is empty on error.
    return false;
  }

  utf16->resize(actual_size);
  return true;
}

// Number formatting -----------------------------------------------------------

namespace {

struct NumberFormatSingletonTraits
    : public DefaultSingletonTraits<NumberFormat> {
  static NumberFormat* New() {
    UErrorCode status = U_ZERO_ERROR;
    NumberFormat* formatter = NumberFormat::createInstance(status);
    DCHECK(U_SUCCESS(status));
    return formatter;
  }
  // There's no ICU call to destroy a NumberFormat object other than
  // operator delete, so use the default Delete, which calls operator delete.
  // This can cause problems if a different allocator is used by this file than
  // by ICU.
};

}  // namespace

std::wstring FormatNumber(int64 number) {
  NumberFormat* number_format =
      Singleton<NumberFormat, NumberFormatSingletonTraits>::get();

  if (!number_format) {
    // As a fallback, just return the raw number in a string.
    return StringPrintf(L"%lld", number);
  }
  UnicodeString ustr;
  number_format->format(number, ustr);

#if defined(WCHAR_T_IS_UTF16)
  return std::wstring(ustr.getBuffer(),
                      static_cast<std::wstring::size_type>(ustr.length()));
#elif defined(WCHAR_T_IS_UTF32)
  wchar_t buffer[64];  // A int64 is less than 20 chars long,  so 64 chars
                       // leaves plenty of room for formating stuff.
  int length = 0;
  UErrorCode error = U_ZERO_ERROR;
  u_strToWCS(buffer, 64, &length, ustr.getBuffer(), ustr.length() , &error);
  if (U_FAILURE(error)) {
    NOTREACHED();
    // As a fallback, just return the raw number in a string.
    return StringPrintf(L"%lld", number);
  }
  return std::wstring(buffer, static_cast<std::wstring::size_type>(length));
#endif  // defined(WCHAR_T_IS_UTF32)
}

TrimPositions TrimWhitespaceUTF8(const std::string& input,
                                 TrimPositions positions,
                                 std::string* output) {
  // This implementation is not so fast since it converts the text encoding
  // twice. Please feel free to file a bug if this function hurts the
  // performance of Chrome.
  DCHECK(IsStringUTF8(input));
  std::wstring input_wide = UTF8ToWide(input);
  std::wstring output_wide;
  TrimPositions result = TrimWhitespace(input_wide, positions, &output_wide);
  *output = WideToUTF8(output_wide);
  return result;
}
