// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox_importer_utils.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_parser.h"
#include "googleurl/src/gurl.h"
#include "net/base/base64.h"

using webkit_glue::PasswordForm;

namespace {

// FirefoxURLParameterFilter is used to remove parameter mentioning Firefox from
// the search URL when importing search engines.
class FirefoxURLParameterFilter : public TemplateURLParser::ParameterFilter {
 public:
  FirefoxURLParameterFilter() { }
  ~FirefoxURLParameterFilter() { }

  // TemplateURLParser::ParameterFilter method.
  virtual bool KeepParameter(const std::string& key,
                             const std::string& value) {
    std::string low_value = StringToLowerASCII(value);
    if (low_value.find("mozilla") != std::string::npos ||
        low_value.find("firefox") != std::string::npos ||
        low_value.find("moz:") != std::string::npos )
      return false;
    return true;
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(FirefoxURLParameterFilter);
};
}  // namespace

bool GetFirefoxVersionAndPathFromProfile(const std::wstring& profile_path,
                                         int* version,
                                         std::wstring* app_path) {
  bool ret = false;
  std::wstring compatibility_file(profile_path);
  file_util::AppendToPath(&compatibility_file, L"compatibility.ini");
  std::string content;
  file_util::ReadFileToString(compatibility_file, &content);
  ReplaceSubstringsAfterOffset(&content, 0, "\r\n", "\n");
  std::vector<std::string> lines;
  SplitString(content, '\n', &lines);

  for (size_t i = 0; i < lines.size(); ++i) {
    const std::string& line = lines[i];
    if (line.empty() || line[0] == '#' || line[0] == ';')
      continue;
    size_t equal = line.find('=');
    if (equal != std::string::npos) {
      std::string key = line.substr(0, equal);
      if (key == "LastVersion") {
        *version = line.substr(equal + 1)[0] - '0';
        ret = true;
      } else if (key == "LastAppDir") {
        *app_path = UTF8ToWide(line.substr(equal + 1));
      }
    }
  }
  return ret;
}

void ParseProfileINI(std::wstring file, DictionaryValue* root) {
  // Reads the whole INI file.
  std::string content;
  file_util::ReadFileToString(file, &content);
  ReplaceSubstringsAfterOffset(&content, 0, "\r\n", "\n");
  std::vector<std::string> lines;
  SplitString(content, '\n', &lines);

  // Parses the file.
  root->Clear();
  std::wstring current_section;
  for (size_t i = 0; i < lines.size(); ++i) {
    std::wstring line = UTF8ToWide(lines[i]);
    if (line.empty()) {
      // Skips the empty line.
      continue;
    }
    if (line[0] == L'#' || line[0] == L';') {
      // This line is a comment.
      continue;
    }
    if (line[0] == L'[') {
      // It is a section header.
      current_section = line.substr(1);
      size_t end = current_section.rfind(L']');
      if (end != std::wstring::npos)
        current_section.erase(end);
    } else {
      std::wstring key, value;
      size_t equal = line.find(L'=');
      if (equal != std::wstring::npos) {
        key = line.substr(0, equal);
        value = line.substr(equal + 1);
        // Checks whether the section and key contain a '.' character.
        // Those sections and keys break DictionaryValue's path format,
        // so we discard them.
        if (current_section.find(L'.') == std::wstring::npos &&
            key.find(L'.') == std::wstring::npos)
          root->SetString(current_section + L"." + key, value);
      }
    }
  }
}

bool CanImportURL(const GURL& url) {
  const char* kInvalidSchemes[] = {"wyciwyg", "place", "about", "chrome"};

  // The URL is not valid.
  if (!url.is_valid())
    return false;

  // Filter out the URLs with unsupported schemes.
  for (size_t i = 0; i < arraysize(kInvalidSchemes); ++i) {
    if (url.SchemeIs(kInvalidSchemes[i]))
      return false;
  }

  return true;
}

void ParseSearchEnginesFromXMLFiles(const std::vector<std::wstring>& xml_files,
                                    std::vector<TemplateURL*>* search_engines) {
  DCHECK(search_engines);

  std::map<std::wstring, TemplateURL*> search_engine_for_url;
  std::string content;
  // The first XML file represents the default search engine in Firefox 3, so we
  // need to keep it on top of the list.
  TemplateURL* default_turl = NULL;
  for (std::vector<std::wstring>::const_iterator file_iter = xml_files.begin();
       file_iter != xml_files.end(); ++file_iter) {
    file_util::ReadFileToString(*file_iter, &content);
    TemplateURL* template_url = new TemplateURL();
    FirefoxURLParameterFilter param_filter;
    if (TemplateURLParser::Parse(
        reinterpret_cast<const unsigned char*>(content.data()),
        content.length(), &param_filter, template_url) &&
        template_url->url()) {
      std::wstring url = template_url->url()->url();
      std::map<std::wstring, TemplateURL*>::iterator iter =
          search_engine_for_url.find(url);
      if (iter != search_engine_for_url.end()) {
        // We have already found a search engine with the same URL.  We give
        // priority to the latest one found, as GetSearchEnginesXMLFiles()
        // returns a vector with first Firefox default search engines and then
        // the user's ones.  We want to give priority to the user ones.
        delete iter->second;
        search_engine_for_url.erase(iter);
      }
      // Give this a keyword to facilitate tab-to-search, if possible.
      template_url->set_keyword(
              TemplateURLModel::GenerateKeyword(GURL(WideToUTF8(url)), false));
      template_url->set_show_in_default_list(true);
      search_engine_for_url[url] = template_url;
      if (!default_turl)
        default_turl = template_url;
    } else {
      delete template_url;
    }
    content.clear();
  }

  // Put the results in the |search_engines| vector.
  std::map<std::wstring, TemplateURL*>::iterator t_iter;
  for (t_iter = search_engine_for_url.begin();
       t_iter != search_engine_for_url.end(); ++t_iter) {
    if (t_iter->second == default_turl)
      search_engines->insert(search_engines->begin(), default_turl);
    else
      search_engines->push_back(t_iter->second);
  }
}

bool ReadPrefFile(const std::wstring& path_name,
                  const std::wstring& file_name,
                  std::string* content) {
  if (content == NULL)
    return false;

  std::wstring file = path_name;
  file_util::AppendToPath(&file, file_name.c_str());

  file_util::ReadFileToString(file, content);

  if (content->empty()) {
    NOTREACHED() << L"Firefox preference file " << file_name.c_str()
                 << L" is empty.";
    return false;
  }

  return true;
}

std::string ReadBrowserConfigProp(const std::wstring& app_path,
                                  const std::string& pref_key) {
  std::string content;
  if (!ReadPrefFile(app_path, L"browserconfig.properties", &content))
    return "";

  // This file has the syntax: key=value.
  size_t prop_index = content.find(pref_key + "=");
  if (prop_index == std::string::npos)
    return "";

  size_t start = prop_index + pref_key.length();
  size_t stop = std::string::npos;
  if (start != std::string::npos)
    stop = content.find("\n", start + 1);

  if (start == std::string::npos ||
      stop == std::string::npos || (start == stop)) {
    NOTREACHED() << "Firefox property " << pref_key << " could not be parsed.";
    return "";
  }

  return content.substr(start + 1, stop - start - 1);
}

std::string ReadPrefsJsValue(const std::wstring& profile_path,
                             const std::string& pref_key) {
  std::string content;
  if (!ReadPrefFile(profile_path, L"prefs.js", &content))
    return "";

  // This file has the syntax: user_pref("key", value);
  std::string search_for = std::string("user_pref(\"") + pref_key +
                           std::string("\", ");
  size_t prop_index = content.find(search_for);
  if (prop_index == std::string::npos)
    return "";

  size_t start = prop_index + search_for.length();
  size_t stop = std::string::npos;
  if (start != std::string::npos)
    stop = content.find(")", start + 1);

  if (start == std::string::npos || stop == std::string::npos) {
    NOTREACHED() << "Firefox property " << pref_key << " could not be parsed.";
    return "";
  }

  // String values have double quotes we don't need to return to the caller.
  if (content[start] == '\"' && content[stop - 1] == '\"') {
    ++start;
    --stop;
  }

  return content.substr(start, stop - start);
}

int GetFirefoxDefaultSearchEngineIndex(
    const std::vector<TemplateURL*>& search_engines,
    const std::wstring& profile_path) {
  // The default search engine is contained in the file prefs.js found in the
  // profile directory.
  // It is the "browser.search.selectedEngine" property.
  if (search_engines.empty())
    return -1;

  std::wstring default_se_name = UTF8ToWide(
      ReadPrefsJsValue(profile_path, "browser.search.selectedEngine"));

  if (default_se_name.empty()) {
    // browser.search.selectedEngine does not exist if the user has not changed
    // from the default (or has selected the default).
    // TODO: should fallback to 'browser.search.defaultengine' if selectedEngine
    // is empty.
    return -1;
  }

  int default_se_index = -1;
  for (std::vector<TemplateURL*>::const_iterator iter = search_engines.begin();
       iter != search_engines.end(); ++iter) {
    if (default_se_name == (*iter)->short_name()) {
      default_se_index = static_cast<int>(iter - search_engines.begin());
      break;
    }
  }
  if (default_se_index == -1) {
    NOTREACHED() <<
        "Firefox default search engine not found in search engine list";
  }

  return default_se_index;
}

GURL GetHomepage(const std::wstring& profile_path) {
  std::string home_page_list =
      ReadPrefsJsValue(profile_path, "browser.startup.homepage");

  size_t seperator = home_page_list.find_first_of('|');
  if (seperator == std::string::npos)
    return GURL(home_page_list);

  return GURL(home_page_list.substr(0, seperator));
}

bool IsDefaultHomepage(const GURL& homepage,
                       const std::wstring& app_path) {
  if (!homepage.is_valid())
    return false;

  std::string default_homepages =
      ReadBrowserConfigProp(app_path, "browser.startup.homepage");

  size_t seperator = default_homepages.find_first_of('|');
  if (seperator == std::string::npos)
    return homepage.spec() == GURL(default_homepages).spec();

  // Crack the string into separate homepage urls.
  std::vector<std::string> urls;
  SplitString(default_homepages, '|', &urls);

  for (size_t i = 0; i < urls.size(); ++i) {
    if (homepage.spec() == GURL(urls[i]).spec())
      return true;
  }

  return false;
}

// class NSSDecryptor.

NSSDecryptor::NSSDecryptor()
    : NSS_Init(NULL), NSS_Shutdown(NULL), PK11_GetInternalKeySlot(NULL),
      PK11_CheckUserPassword(NULL), PK11_FreeSlot(NULL),
      PK11_Authenticate(NULL), PK11SDR_Decrypt(NULL), SECITEM_FreeItem(NULL),
      PL_ArenaFinish(NULL), PR_Cleanup(NULL),
      nss3_dll_(NULL), softokn3_dll_(NULL),
      is_nss_initialized_(false) {
}

NSSDecryptor::~NSSDecryptor() {
  Free();
}

bool NSSDecryptor::InitNSS(const std::wstring& db_path,
                           base::NativeLibrary plds4_dll,
                           base::NativeLibrary nspr4_dll) {
  // NSPR DLLs are already loaded now.
  if (plds4_dll == NULL || nspr4_dll == NULL) {
    Free();
    return false;
  }

  // Gets the function address.
  NSS_Init = (NSSInitFunc)
      base::GetFunctionPointerFromNativeLibrary(nss3_dll_, "NSS_Init");
  NSS_Shutdown = (NSSShutdownFunc)
      base::GetFunctionPointerFromNativeLibrary(nss3_dll_, "NSS_Shutdown");
  PK11_GetInternalKeySlot = (PK11GetInternalKeySlotFunc)
      base::GetFunctionPointerFromNativeLibrary(nss3_dll_,
                                                "PK11_GetInternalKeySlot");
  PK11_FreeSlot = (PK11FreeSlotFunc)
      base::GetFunctionPointerFromNativeLibrary(nss3_dll_, "PK11_FreeSlot");
  PK11_Authenticate = (PK11AuthenticateFunc)
      base::GetFunctionPointerFromNativeLibrary(nss3_dll_, "PK11_Authenticate");
  PK11SDR_Decrypt = (PK11SDRDecryptFunc)
      base::GetFunctionPointerFromNativeLibrary(nss3_dll_, "PK11SDR_Decrypt");
  SECITEM_FreeItem = (SECITEMFreeItemFunc)
      base::GetFunctionPointerFromNativeLibrary(nss3_dll_, "SECITEM_FreeItem");
  PL_ArenaFinish = (PLArenaFinishFunc)
      base::GetFunctionPointerFromNativeLibrary(plds4_dll, "PL_ArenaFinish");
  PR_Cleanup = (PRCleanupFunc)
      base::GetFunctionPointerFromNativeLibrary(nspr4_dll, "PR_Cleanup");

  if (NSS_Init == NULL || NSS_Shutdown == NULL ||
      PK11_GetInternalKeySlot == NULL || PK11_FreeSlot == NULL ||
      PK11_Authenticate == NULL || PK11SDR_Decrypt == NULL ||
      SECITEM_FreeItem == NULL || PL_ArenaFinish == NULL ||
      PR_Cleanup == NULL) {
    Free();
    return false;
  }

  SECStatus result = NSS_Init(base::SysWideToNativeMB(db_path).c_str());
  if (result != SECSuccess) {
    Free();
    return false;
  }

  is_nss_initialized_ = true;
  return true;
}

void NSSDecryptor::Free() {
  if (is_nss_initialized_) {
    NSS_Shutdown();
    PL_ArenaFinish();
    PR_Cleanup();
    is_nss_initialized_ = false;
  }
  if (softokn3_dll_ != NULL)
    base::UnloadNativeLibrary(softokn3_dll_);
  if (nss3_dll_ != NULL)
    base::UnloadNativeLibrary(nss3_dll_);
  NSS_Init = NULL;
  NSS_Shutdown = NULL;
  PK11_GetInternalKeySlot = NULL;
  PK11_FreeSlot = NULL;
  PK11_Authenticate = NULL;
  PK11SDR_Decrypt = NULL;
  SECITEM_FreeItem = NULL;
  PL_ArenaFinish = NULL;
  PR_Cleanup = NULL;
  nss3_dll_ = NULL;
  softokn3_dll_ = NULL;
}

// This method is based on some Firefox code in
//   security/manager/ssl/src/nsSDR.cpp
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

std::wstring NSSDecryptor::Decrypt(const std::string& crypt) const {
  // Do nothing if NSS is not loaded.
  if (!nss3_dll_)
    return std::wstring();

  // The old style password is encoded in base64. They are identified
  // by a leading '~'. Otherwise, we should decrypt the text.
  std::string plain;
  if (crypt[0] != '~') {
    std::string decoded_data;
    net::Base64Decode(crypt, &decoded_data);
    PK11SlotInfo* slot = NULL;
    slot = PK11_GetInternalKeySlot();
    SECStatus result = PK11_Authenticate(slot, PR_TRUE, NULL);
    if (result != SECSuccess) {
      PK11_FreeSlot(slot);
      return std::wstring();
    }

    SECItem request;
    request.data = reinterpret_cast<unsigned char*>(
        const_cast<char*>(decoded_data.data()));
    request.len = static_cast<unsigned int>(decoded_data.size());
    SECItem reply;
    reply.data = NULL;
    reply.len = 0;
    result = PK11SDR_Decrypt(&request, &reply, NULL);
    if (result == SECSuccess)
      plain.assign(reinterpret_cast<char*>(reply.data), reply.len);

    SECITEM_FreeItem(&reply, PR_FALSE);
    PK11_FreeSlot(slot);
  } else {
    // Deletes the leading '~' before decoding.
    net::Base64Decode(crypt.substr(1), &plain);
  }

  return UTF8ToWide(plain);
}

// There are three versions of password filess. They store saved user
// names and passwords.
// References:
// http://kb.mozillazine.org/Signons.txt
// http://kb.mozillazine.org/Signons2.txt
// http://kb.mozillazine.org/Signons3.txt
void NSSDecryptor::ParseSignons(const std::string& content,
                                std::vector<PasswordForm>* forms) {
  forms->clear();

  // Splits the file content into lines.
  std::vector<std::string> lines;
  SplitString(content, '\n', &lines);

  // The first line is the file version. We skip the unknown versions.
  if (lines.empty())
    return;
  int version;
  if (lines[0] == "#2c")
    version = 1;
  else if (lines[0] == "#2d")
    version = 2;
  else if (lines[0] == "#2e")
    version = 3;
  else
    return;

  GURL::Replacements rep;
  rep.ClearQuery();
  rep.ClearRef();
  rep.ClearUsername();
  rep.ClearPassword();

  // Reads never-saved list. Domains are stored one per line.
  size_t i;
  for (i = 1; i < lines.size() && lines[i].compare(".") != 0; ++i) {
    PasswordForm form;
    form.origin = GURL(lines[i]).ReplaceComponents(rep);
    form.signon_realm = form.origin.GetOrigin().spec();
    form.blacklisted_by_user = true;
    forms->push_back(form);
  }
  ++i;

  // Reads saved passwords. The information is stored in blocks
  // seperated by lines that only contain a dot. We find a block
  // by the seperator and parse them one by one.
  while (i < lines.size()) {
    size_t begin = i;
    size_t end = i + 1;
    while (end < lines.size() && lines[end].compare(".") != 0)
      ++end;
    i = end + 1;

    // A block has at least five lines.
    if (end - begin < 5)
      continue;

    PasswordForm form;

    // The first line is the site URL.
    // For HTTP authentication logins, the URL may contain http realm,
    // which will be in bracket:
    //   sitename:8080 (realm)
    GURL url;
    std::string realm;
    const char kRealmBracketBegin[] = " (";
    const char kRealmBracketEnd[] = ")";
    if (lines[begin].find(kRealmBracketBegin) != std::string::npos) {
      // In this case, the scheme may not exsit. We assume that the
      // scheme is HTTP.
      if (lines[begin].find("://") == std::string::npos)
        lines[begin] = "http://" + lines[begin];

      size_t start = lines[begin].find(kRealmBracketBegin);
      url = GURL(lines[begin].substr(0, start));

      start += std::string(kRealmBracketBegin).size();
      size_t end = lines[begin].rfind(kRealmBracketEnd);
      realm = lines[begin].substr(start, end - start);
    } else {
      // Don't have http realm. It is the URL that the following passwords
      // belong to.
      url = GURL(lines[begin]);
    }
    // Skips this block if the URL is not valid.
    if (!url.is_valid())
      continue;
    form.origin = url.ReplaceComponents(rep);
    form.signon_realm = form.origin.GetOrigin().spec();
    if (!realm.empty())
      form.signon_realm += realm;
    form.ssl_valid = form.origin.SchemeIsSecure();
    ++begin;

    // There may be multiple username/password pairs for this site.
    // In this case, they are saved in one block without a seperated
    // line (contains a dot).
    while (begin + 4 < end) {
      // The user name.
      form.username_element = UTF8ToWide(lines[begin++]);
      form.username_value = Decrypt(lines[begin++]);
      // The element name has a leading '*'.
      if (lines[begin].at(0) == '*') {
        form.password_element = UTF8ToWide(lines[begin++].substr(1));
        form.password_value = Decrypt(lines[begin++]);
      } else {
        // Maybe the file is bad, we skip to next block.
        break;
      }
      // The action attribute from the form element. This line exists
      // in versin 2 or above.
      if (version >= 2) {
        if (begin < end)
          form.action = GURL(lines[begin]).ReplaceComponents(rep);
        ++begin;
      }
      // Version 3 has an extra line for further use.
      if (version == 3) {
        ++begin;
      }

      forms->push_back(form);
    }
  }
}
