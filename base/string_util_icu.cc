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

#include "base/string_util.h"

#include <string.h>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "unicode/ucnv.h"
#include "unicode/numfmt.h"
#include "unicode/ustring.h"

namespace {

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
  return U_IS_UNICODE_CHAR(code_point);
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

  return U_IS_UNICODE_CHAR(*code_point);
}

#if defined(WCHAR_T_IS_UTF32)
// Reads UTF-32 character. The usage is the same as the 8-bit version above.
bool ReadUnicodeCharacter(const wchar_t* src, int32 src_len,
                        int32* char_index, uint32* code_point) {
  // Conversion is easy since the source is 32-bit.
  *code_point = src[*char_index];

  // Validate the value.
  return U_IS_UNICODE_CHAR(*code_point);
}
#endif  // defined(WCHAR_T_IS_UTF32)

// WriteUnicodeCharacter -------------------------------------------------------

// Appends a UTF-8 character to the given 8-bit string.
void WriteUnicodeCharacter(uint32 code_point, std::basic_string<char>* output) {
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
void WriteUnicodeCharacter(uint32 code_point,
                           std::basic_string<char16>* output) {
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
inline void WriteUnicodeCharacter(uint32 code_point,
                                  std::basic_string<wchar_t>* output) {
  // This is the easy case, just append the character.
  output->push_back(code_point);
}
#endif  // defined(WCHAR_T_IS_UTF32)

// Generalized Unicode converter -----------------------------------------------

// Converts the given source Unicode character type to the given destination
// Unicode character type as a STL string. The given input buffer and size
// determine the source, and the given output STL string will be replaced by
// the result.
template<typename SRC_CHAR, typename DEST_CHAR>
bool ConvertUnicode(const SRC_CHAR* src, size_t src_len,
                    std::basic_string<DEST_CHAR>* output) {
  output->clear();

  // ICU requires 32-bit numbers.
  bool success = true;
  int32 src_len32 = static_cast<int32>(src_len);
  for (int32 i = 0; i < src_len32; i++) {
    uint32 code_point;
    if (ReadUnicodeCharacter(src, src_len32, &i, &code_point))
      WriteUnicodeCharacter(code_point, output);
    else
      success = false;
  }
  return success;
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

  // Intelligently guess the size of the output string. When it's an ASCII
  // character, assume the rest will be ASCII and use a buffer size the same as
  // the input. When it's not ASCII, assume 3-bytes per character as the
  // starting point. This will be resized internally later if it's too small.
  if (static_cast<uint32>(src[0]) < 0x80)
    output->reserve(src_len);
  else
    output->reserve(src_len * 3);
  return ConvertUnicode<wchar_t, char>(src, src_len, output);
}

std::wstring UTF8ToWide(const std::string& utf8) {
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

  // Intelligently guess the size of the output string. When it's an ASCII
  // character, assume the rest will be ASCII and use a buffer size the same as
  // the input. When it's not ASCII, assume the UTF-8 takes 2 bytes per
  // character (this is more conservative than 3 which we use above when
  // converting the other way).
  if (static_cast<unsigned char>(src[0]) < 0x80)
    output->reserve(src_len);
  else
    output->reserve(src_len / 2);
  return ConvertUnicode<char, wchar_t>(src, src_len, output);
}

// UTF-16 <-> Wide -------------------------------------------------------------

#if defined(WCHAR_T_IS_UTF16)

// When wide == UTF-16, then conversions are a NOP.
std::string16 WideToUTF16(const std::wstring& wide) {
  return wide;
}

bool WideToUTF16(const wchar_t* src, size_t src_len, std::string16* output) {
  output->assign(src, src_len);
  return true;
}

std::wstring UTF16ToWide(const std::string16& utf16) {
  return utf16;
}

bool UTF16ToWide(const char16* src, size_t src_len, std::wstring* output) {
  output->assign(src, src_len);
  return true;
}

#elif defined(WCHAR_T_IS_UTF32)

std::string16 WideToUTF16(const std::wstring& wide) {
  std::string16 ret;
  if (wide.empty())
    return ret;

  WideToUTF16(wide.data(), wide.length(), &ret);
  return ret;
}

bool WideToUTF16(const wchar_t* src, size_t src_len, std::string16* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  // Assume that normally we won't have any non-BMP characters so the counts
  // will be the same.
  output->reserve(src_len);
  return ConvertUnicode<wchar_t, char16>(src, src_len, output);
}

std::wstring UTF16ToWide(const std::string16& utf16) {
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
  return ConvertUnicode<char16, wchar_t>(src, src_len, output);
}

#endif  // defined(WCHAR_T_IS_UTF32)

// Codepage <-> Wide -----------------------------------------------------------

// Convert a unicode string into the specified codepage_name.  If the codepage
// isn't found, return false.
bool WideToCodepage(const std::wstring& wide,
                    const char* codepage_name,
                    OnStringUtilConversionError::Type on_error,
                    std::string* encoded) {
  encoded->clear();

  UErrorCode status = U_ZERO_ERROR;
  UConverter* converter = ucnv_open(codepage_name, &status);
  if (!U_SUCCESS(status))
    return false;

  const UChar* uchar_src;
  int uchar_len;
#if defined(WCHAR_T_IS_UTF16)
  uchar_src = wide.c_str();
  uchar_len = static_cast<int>(wide.length());
#elif defined(WCHAR_T_IS_UTF32)
  // When wchar_t is wider than UChar (16 bits), transform |wide| into a
  // UChar* string.  Size the UChar* buffer to be large enough to hold twice
  // as many UTF-16 code points as there are UTF-16 characters, in case each
  // character translates to a UTF-16 surrogate pair, and leave room for a NUL
  // terminator.
  std::vector<UChar> wide_uchar(wide.length() * 2 + 1);
  u_strFromWCS(&wide_uchar[0], wide_uchar.size(), &uchar_len,
               wide.c_str(), wide.length(), &status);
  uchar_src = &wide_uchar[0];
  DCHECK(U_SUCCESS(status)) << "failed to convert wstring to UChar*";
#endif  // defined(WCHAR_T_IS_UTF32)

  int encoded_max_length = UCNV_GET_MAX_BYTES_FOR_STRING(uchar_len,
    ucnv_getMaxCharSize(converter));
  encoded->resize(encoded_max_length);

  // Setup our error handler.
  switch (on_error) {
    case OnStringUtilConversionError::FAIL:
      ucnv_setFromUCallBack(converter, UCNV_FROM_U_CALLBACK_STOP, 0,
                            NULL, NULL, &status);
      break;
    case OnStringUtilConversionError::SKIP:
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

// Converts a string of the given codepage into unicode.
// If the codepage isn't found, return false.
bool CodepageToWide(const std::string& encoded,
                    const char* codepage_name,
                    OnStringUtilConversionError::Type on_error,
                    std::wstring* wide) {
  wide->clear();

  UErrorCode status = U_ZERO_ERROR;
  UConverter* converter = ucnv_open(codepage_name, &status);
  if (!U_SUCCESS(status))
    return false;

  // The worst case is all the input characters are non-BMP (32-bit) ones.
  size_t uchar_max_length = encoded.length() * 2 + 1;

  UChar* uchar_dst;
#if defined(WCHAR_T_IS_UTF16)
  uchar_dst = WriteInto(wide, uchar_max_length);
#elif defined(WCHAR_T_IS_UTF32)
  // When wchar_t is wider than UChar (16 bits), convert into a temporary
  // UChar* buffer.
  std::vector<UChar> wide_uchar(uchar_max_length);
  uchar_dst = &wide_uchar[0];
#endif  // defined(WCHAR_T_IS_UTF32)

  // Setup our error handler.
  switch (on_error) {
    case OnStringUtilConversionError::FAIL:
      ucnv_setToUCallBack(converter, UCNV_TO_U_CALLBACK_STOP, 0,
                          NULL, NULL, &status);
      break;
    case OnStringUtilConversionError::SKIP:
      ucnv_setToUCallBack(converter, UCNV_TO_U_CALLBACK_SKIP, 0,
                          NULL, NULL, &status);
      break;
    default:
      NOTREACHED();
  }

  int actual_size = ucnv_toUChars(converter,
                                  uchar_dst,
                                  static_cast<int>(uchar_max_length),
                                  encoded.data(),
                                  static_cast<int>(encoded.length()),
                                  &status);
  ucnv_close(converter);
  if (!U_SUCCESS(status)) {
    wide->clear();  // Make sure the output is empty on error.
    return false;
  }

#ifdef WCHAR_T_IS_UTF32
  // When wchar_t is wider than UChar (16 bits), it's not possible to wind up
  // with any more wchar_t elements than UChar elements.  ucnv_toUChars
  // returns the number of UChar elements not including the NUL terminator, so
  // leave extra room for that.
  u_strToWCS(WriteInto(wide, actual_size + 1), actual_size + 1, &actual_size,
             uchar_dst, actual_size, &status);
  DCHECK(U_SUCCESS(status)) << "failed to convert UChar* to wstring";
#endif  // WCHAR_T_IS_UTF32

  wide->resize(actual_size);
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
