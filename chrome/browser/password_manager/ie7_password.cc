// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/ie7_password.h"

#include <wincrypt.h>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"

namespace {

// Structures that IE7/IE8 use to store a username/password.
// Some of the fields might have been incorrectly reverse engineered.
struct PreHeader {
  DWORD pre_header_size;  // Size of this header structure. Always 12.
  DWORD header_size;      // Size of the real Header: sizeof(Header) +
                          // item_count * sizeof(Entry);
  DWORD data_size;        // Size of the data referenced by the entries.
};

struct Header {
  char wick[4];             // The string "WICK". I don't know what it means.
  DWORD fixed_header_size;  // The size of this structure without the entries:
                            // sizeof(Header).
  DWORD item_count;         // Number of entries. It should always be 2. One for
                            // the username, and one for the password.
  wchar_t two_letters[2];   // Two unknown bytes.
  DWORD unknown[2];         // Two unknown DWORDs.
};

struct Entry {
  DWORD offset;         // Offset where the data referenced by this entry is
                        // located.
  FILETIME time_stamp;  // Timestamp when the password got added.
  DWORD string_length;  // The length of the data string.
};

// Main data structure.
struct PasswordEntry {
  PreHeader pre_header;  // Contains the size of the different sections.
  Header header;         // Contains the number of items.
  Entry entry[1];        // List of entries containing a string. The first one
                         // is the username, the second one if the password.
};

// Cleans up a crypt prov and a crypt hash.
void CleanupHashContext(HCRYPTPROV prov, HCRYPTHASH hash) {
  if (hash)
    CryptDestroyHash(hash);
  if (prov)
    CryptReleaseContext(prov, 0);
}

}  // namespace

namespace ie7_password {

bool GetUserPassFromData(const std::vector<unsigned char>& data,
                         std::wstring* username,
                         std::wstring* password) {
  const PasswordEntry* information =
      reinterpret_cast<const PasswordEntry*>(&data.front());

  // Some expected values. If it's not what we expect we don't even try to
  // understand the data.
  if (information->pre_header.pre_header_size != sizeof(PreHeader))
    return false;

  if (information->header.item_count != 2)  // Username and Password
    return false;

  if (information->header.fixed_header_size != sizeof(Header))
    return false;

  const uint8* ptr = &data.front();
  const uint8* offset_to_data = ptr + information->pre_header.header_size +
                                information->pre_header.pre_header_size;

  const Entry* user_entry = information->entry;
  const Entry* pass_entry = user_entry+1;

  *username = reinterpret_cast<const wchar_t*>(offset_to_data +
                                               user_entry->offset);
  *password = reinterpret_cast<const wchar_t*>(offset_to_data +
                                               pass_entry->offset);
  return true;
}

std::wstring GetUrlHash(const std::wstring& url) {
  HCRYPTPROV prov = NULL;
  HCRYPTHASH hash = NULL;

  std::wstring lower_case_url = StringToLowerASCII(url);
  BOOL result = CryptAcquireContext(&prov, 0, 0, PROV_RSA_FULL, 0);
  if (!result) {
    if (GetLastError() == NTE_BAD_KEYSET) {
      // The keyset does not exist. Create one.
      result = CryptAcquireContext(&prov, 0, 0, PROV_RSA_FULL, CRYPT_NEWKEYSET);
    }
  }

  if (!result) {
    DCHECK(false);
    return std::wstring();
  }

  // Initialize the hash.
  if (!CryptCreateHash(prov, CALG_SHA1, 0, 0, &hash)) {
    CleanupHashContext(prov, hash);
    DCHECK(false);
    return std::wstring();
  }

  // Add the data to the hash.
  const unsigned char* buffer =
      reinterpret_cast<const unsigned char*>(lower_case_url.c_str());
  DWORD data_len = static_cast<DWORD>((lower_case_url.size() + 1) *
                                      sizeof(wchar_t));
  if (!CryptHashData(hash, buffer, data_len, 0)) {
    CleanupHashContext(prov, hash);
    DCHECK(false);
    return std::wstring();
  }

  // Get the size of the resulting hash.
  DWORD hash_len = 0;
  DWORD buffer_size = sizeof(hash_len);
  if (!CryptGetHashParam(hash, HP_HASHSIZE,
                         reinterpret_cast<unsigned char*>(&hash_len),
                         &buffer_size, 0)) {
    CleanupHashContext(prov, hash);
    DCHECK(false);
    return std::wstring();
  }

  // Get the hash data.
  scoped_array<unsigned char> new_buffer(new unsigned char[hash_len]);
  if (!CryptGetHashParam(hash, HP_HASHVAL, new_buffer.get(), &hash_len, 0)) {
    CleanupHashContext(prov, hash);
    DCHECK(false);
    return std::wstring();
  }

  std::wstring url_hash;

  // Transform the buffer to an hexadecimal string.
  unsigned char checksum = 0;
  for (DWORD i = 0; i < hash_len; ++i) {
    checksum += new_buffer.get()[i];
    url_hash += StringPrintf(L"%2.2X", new_buffer.get()[i]);
  }

  url_hash += StringPrintf(L"%2.2X", checksum);

  CleanupHashContext(prov, hash);
  return url_hash;
}

bool DecryptPassword(const std::wstring& url,
                     const std::vector<unsigned char>& data,
                     std::wstring* username, std::wstring* password) {
  std::wstring lower_case_url = StringToLowerASCII(url);
  DATA_BLOB input = {0};
  DATA_BLOB output = {0};
  DATA_BLOB url_key = {0};

  input.pbData = const_cast<unsigned char*>(&data.front());
  input.cbData = static_cast<DWORD>((data.size()) *
                                    sizeof(std::string::value_type));

  url_key.pbData = reinterpret_cast<unsigned char*>(
                      const_cast<wchar_t*>(lower_case_url.data()));
  url_key.cbData = static_cast<DWORD>((lower_case_url.size() + 1) *
                                      sizeof(std::wstring::value_type));

  if (CryptUnprotectData(&input, NULL, &url_key, NULL, NULL,
                         CRYPTPROTECT_UI_FORBIDDEN, &output)) {
    // Now that we have the decrypted information, we need to understand it.
    std::vector<unsigned char> decrypted_data;
    decrypted_data.resize(output.cbData);
    memcpy(&decrypted_data.front(), output.pbData, output.cbData);

    GetUserPassFromData(decrypted_data, username, password);

    LocalFree(output.pbData);
    return true;
  }

  return false;
}

}  // namespace ie7_password
