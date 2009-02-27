// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_FIREFOX_IMPORTER_UTILS_H_
#define CHROME_BROWSER_IMPORTER_FIREFOX_IMPORTER_UTILS_H_

#include "base/basictypes.h"
#include "base/values.h"
#include "build/build_config.h"
#include "webkit/glue/password_form.h"

class GURL;
class TemplateURL;

// Detects which version of Firefox is installed. Returns its
// major version, and drops the minor version. Returns 0 if
// failed. If there are indicators of both FF2 and FF3 it is
// biased to return the biggest version.
int GetCurrentFirefoxMajorVersion();

#if defined(OS_WIN)
// Gets the full path of the profiles.ini file. This file records
// the profiles that can be used by Firefox.  Returns an empty
// string if failed.
std::wstring GetProfilesINI();

// Parses the profile.ini file, and stores its information in |root|.
// This file is a plain-text file. Key/value pairs are stored one per
// line, and they are separeated in different sections. For example:
//   [General]
//   StartWithLastProfile=1
//
//   [Profile0]
//   Name=default
//   IsRelative=1
//   Path=Profiles/abcdefeg.default
// We set "[value]" in path "<Section>.<Key>". For example, the path
// "Genenral.StartWithLastProfile" has the value "1".
void ParseProfileINI(std::wstring file, DictionaryValue* root);
#endif

// Detects where Firefox lives.  Returns a empty string if Firefox
// is not installed.
std::wstring GetFirefoxInstallPath();

// Returns true if we want to add the URL to the history. We filter
// out the URL with a unsupported scheme.
bool CanImportURL(const GURL& url);

// Parses the OpenSearch XML files in |xml_files| and populates |search_engines|
// with the resulting TemplateURLs.
void ParseSearchEnginesFromXMLFiles(const std::vector<std::wstring>& xml_files,
                                    std::vector<TemplateURL*>* search_engines);

// Returns the index of the default search engine in the |search_engines| list.
// If none is found, -1 is returned.
int GetFirefoxDefaultSearchEngineIndex(
    const std::vector<TemplateURL*>& search_engines,
    const std::wstring& profile_path);

// Returns the home page set in Firefox in a particular profile.
GURL GetHomepage(const std::wstring& profile_path);

// Checks to see if this home page is a default home page, as specified by
// the resource file browserconfig.properties in the Firefox application
// directory.
bool IsDefaultHomepage(const GURL& homepage, const std::wstring& app_path);

// The following declarations of functions and types are from Firefox
// NSS library.
// source code:
//   security/nss/lib/util/seccomon.h
//   security/nss/lib/nss/nss.h
// The license block is:

/* ***** BEGIN LICENSE BLOCK *****
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is the Netscape security libraries.
*
* The Initial Developer of the Original Code is
* Netscape Communications Corporation.
* Portions created by the Initial Developer are Copyright (C) 1994-2000
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the MPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the MPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK ***** */

enum SECItemType {
  siBuffer = 0,
  siClearDataBuffer = 1,
  siCipherDataBuffer = 2,
  siDERCertBuffer = 3,
  siEncodedCertBuffer = 4,
  siDERNameBuffer = 5,
  siEncodedNameBuffer = 6,
  siAsciiNameString = 7,
  siAsciiString = 8,
  siDEROID = 9,
  siUnsignedInteger = 10,
  siUTCTime = 11,
  siGeneralizedTime = 12
};

struct SECItem {
  SECItemType type;
  unsigned char *data;
  unsigned int len;
};

enum SECStatus {
  SECWouldBlock = -2,
  SECFailure = -1,
  SECSuccess = 0
};

typedef int PRBool;
#define PR_TRUE 1
#define PR_FALSE 0

typedef enum { PR_FAILURE = -1, PR_SUCCESS = 0 } PRStatus;

typedef struct PK11SlotInfoStr PK11SlotInfo;

typedef SECStatus (*NSSInitFunc)(const char *configdir);
typedef SECStatus (*NSSShutdownFunc)(void);
typedef PK11SlotInfo* (*PK11GetInternalKeySlotFunc)(void);
typedef void (*PK11FreeSlotFunc)(PK11SlotInfo *slot);
typedef SECStatus (*PK11CheckUserPasswordFunc)(PK11SlotInfo *slot, char *pw);
typedef SECStatus
    (*PK11AuthenticateFunc)(PK11SlotInfo *slot, PRBool loadCerts, void *wincx);
typedef SECStatus
    (*PK11SDRDecryptFunc)(SECItem *data, SECItem *result, void *cx);
typedef void (*SECITEMFreeItemFunc)(SECItem *item, PRBool free_it);
typedef void (*PLArenaFinishFunc)(void);
typedef PRStatus (*PRCleanupFunc)(void);

// A wrapper for Firefox NSS decrypt component.
class NSSDecryptor {
 public:
  NSSDecryptor();
  ~NSSDecryptor();

  // Loads NSS3 library and returns true if successful.
  // |dll_path| indicates the location of NSS3 DLL files, and |db_path|
  // is the location of the database file that stores the keys.
  bool Init(const std::wstring& dll_path, const std::wstring& db_path);

  // Frees the libraries.
  void Free();

  // Decrypts Firefox stored passwords. Before using this method,
  // make sure Init() returns true.
  std::wstring Decrypt(const std::string& crypt) const;

  // Parses the Firefox password file content, decrypts the
  // username/password and reads other related information.
  // The result will be stored in |forms|.
  void ParseSignons(const std::string& content,
                    std::vector<PasswordForm>* forms);

 private:
  // Methods in Firefox security components.
  NSSInitFunc NSS_Init;
  NSSShutdownFunc NSS_Shutdown;
  PK11GetInternalKeySlotFunc PK11_GetInternalKeySlot;
  PK11CheckUserPasswordFunc PK11_CheckUserPassword;
  PK11FreeSlotFunc PK11_FreeSlot;
  PK11AuthenticateFunc PK11_Authenticate;
  PK11SDRDecryptFunc PK11SDR_Decrypt;
  SECITEMFreeItemFunc SECITEM_FreeItem;
  PLArenaFinishFunc PL_ArenaFinish;
  PRCleanupFunc PR_Cleanup;

  // Libraries necessary for decrypting the passwords.
  static const wchar_t kNSS3Library[];
  static const wchar_t kSoftokn3Library[];
  static const wchar_t kPLDS4Library[];
  static const wchar_t kNSPR4Library[];

#if defined(OS_WIN)
  // NSS3 module handles.
  HMODULE nss3_dll_;
  HMODULE softokn3_dll_;
#endif

  // True if NSS_Init() has been called
  bool is_nss_initialized_;

  DISALLOW_COPY_AND_ASSIGN(NSSDecryptor);
};

#endif  // CHROME_BROWSER_IMPORTER_FIREFOX_IMPORTER_UTILS_H_

