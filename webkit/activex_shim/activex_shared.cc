// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/activex_shim/activex_shared.h"

#include <windows.h>
#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/string_util.h"
#include "base/registry.h"
#include "googleurl/src/gurl.h"

namespace activex_shim {

namespace {
struct ActiveXAttribute {
  const char* clsid;
  ActiveXTypes type;
  // allowed_sites is a (list) of simple patterns of sites. Rules are:
  //   - if pattern is empty or NULL pointer, no sites are allowed.
  //   - sites are separated by ";"
  //   - "*" will match any sites.
  //   - "file" matches file url starts with "file://"
  //   - anything else matches the last part of the host.
  // e.g. "95559.com;ecitic.com" allows controls running from 95559.com and
  // ecitic.com.
  // See MatchUrlForSinglePattern for details.
  const char* allowed_sites;
  const char* allowed_codebase_sites;
};
}  // unnamed namespace.

static const ActiveXAttribute activex_attributes[] = {
  {"d27cdb6e-ae6d-11cf-96b8-444553540000", ACTIVEX_FLASH},
  // WMP 7 and later
  {"6bf52a52-394a-11d3-b153-00c04f79faa6", ACTIVEX_WMP, "*", "microsoft.com"},
  // WMP 6.4
  {"22d6f312-b0f6-11d0-94ab-0080c74c7e95", ACTIVEX_WMP, "*", "microsoft.com"},
  {"cfcdaa03-8be4-11cf-b84b-0020afbbccfa", ACTIVEX_REALPLAYER},
  {"02bf25d5-8c17-4b23-bc80-d3488abddc6b", ACTIVEX_QUICKTIME},
  {"166b1bca-3f9c-11cf-8075-444553540000", ACTIVEX_SHOCKWAVE},
  {"4e174456-5ee6-494d-b6f2-2b52898a620e", ACTIVEX_TESTCONTROL, "file"},
};

// See chrome_switches.cc, switches::kAllowAllActiveX. We duplicate the value
// here to avoid dependency on Chrome.
static const wchar_t kAllowAllActiveX[] = L"allow-all-activex";

static const ActiveXAttribute* FindActiveX(const std::string& clsid) {
  for (unsigned int i = 0; i < arraysize(activex_attributes); ++i) {
    if (LowerCaseEqualsASCII(clsid, activex_attributes[i].clsid))
      return &activex_attributes[i];
  }
  return NULL;
}

ActiveXTypes MapClassIdToType(const std::string& clsid) {
  const ActiveXAttribute* attr = FindActiveX(clsid);
  if (attr != NULL)
    return attr->type;
  else
    return ACTIVEX_GENERIC;
}

bool MatchUrlForSinglePattern(const GURL& url, const std::string& pattern) {
  if (pattern.empty())
    return false;
  if (pattern == "*")
    return true;
  if (pattern == "file")
    return url.SchemeIsFile();
  return url.DomainIs(pattern.c_str());
}

bool MatchUrlForPatterns(const GURL& url, const std::string& patterns) {
  std::vector<std::string> v;
  SplitString(patterns, ';', &v);
  for (unsigned int i = 0; i < v.size(); ++i) {
    if (MatchUrlForSinglePattern(url, v[i]))
      return true;
  }
  return false;
}

// Whether allow-all-activex is specified in the command line.
static bool IsAllowAllActiveX() {
  static bool parsed_flag = false;
  static bool allow_all_activex = false;
  if (!parsed_flag) {
    allow_all_activex =
        CommandLine::ForCurrentProcess()->HasSwitch(kAllowAllActiveX);
    parsed_flag = true;
  }
  return allow_all_activex;
}

// If an ActiveX control is allowed to run from a specific URL.
bool IsActiveXAllowed(const std::string& clsid, const GURL& url) {
  if (IsAllowAllActiveX())
    return true;
  const ActiveXAttribute* attr = FindActiveX(clsid);
  if (attr == NULL)
    return false;
  if (attr->allowed_sites == NULL)
    return false;
  return MatchUrlForPatterns(url, attr->allowed_sites);
}

// If an ActiveX control's codebase comes from allowed websites.
bool IsCodebaseAllowed(const std::string& clsid, const std::string& codebase) {
  if (IsAllowAllActiveX())
    return true;
  GURL url(codebase);
  const ActiveXAttribute* attr = FindActiveX(clsid);
  if (attr == NULL)
    return false;
  if (attr->allowed_codebase_sites == NULL)
    return false;
  return MatchUrlForPatterns(url, attr->allowed_codebase_sites);
}

bool GetClsidFromClassidAttribute(const std::string& classid,
                                  std::string* clsid) {
  const unsigned int kClsidPrefixSize = 6;
  if (classid.size() > kClsidPrefixSize) {
    std::string prefix = classid.substr(0, kClsidPrefixSize);
    if (LowerCaseEqualsASCII(prefix, "clsid:")) {
      TrimWhitespace(classid.substr(kClsidPrefixSize), TRIM_ALL, clsid);
      return true;
    }
  }
  return false;
}

std::string GetVersionFromCodebaseAttribute(const std::string& codebase) {
  std::string version;
  size_t pos = codebase.find('#');
  if (pos == std::string::npos)
    return version;
  std::string rest = codebase.substr(pos + 1);
  std::string version_prefix = "version=";
  if (!StartsWithASCII(rest, "version=", false))
    return version;
  size_t i;
  for (i = version_prefix.size(); i < rest.size(); i++) {
    char c = rest[i];
    if (c != ',' && (c > '9' || c < '0'))
      break;
  }
  return rest.substr(version_prefix.size(), i - version_prefix.size());
}

// The version string should always be in the form of "1,2,0,4".
static bool ParseVersionFromCodebase(const std::string& version,
                                     DWORD* version_ms,
                                     DWORD* version_ls) {
  *version_ms = 0;
  *version_ls = 0;
  std::vector<int> v;
  std::vector<std::string> sv;
  SplitString(version, ',', &sv);
  for (size_t i = 0; i < sv.size(); ++i)
    v.push_back(atoi(sv[i].c_str()));
  if (v.size() != 4)
    return false;
  *version_ms = MAKELONG(v[1], v[0]);
  *version_ls = MAKELONG(v[3], v[2]);
  return true;
}

bool IsActiveXInstalled(const std::string& combined_clsid) {
  std::string clsid, version;
  size_t pos = combined_clsid.find('#');
  if (pos == std::string::npos) {
    clsid = combined_clsid;
  } else {
    clsid = combined_clsid.substr(0, pos);
    version = combined_clsid.substr(pos + 1);
  }
  RegKey key;
  std::wstring key_path = std::wstring(L"CLSID\\{") + ASCIIToWide(clsid) + L"}";
  if (!key.Open(HKEY_CLASSES_ROOT, key_path.c_str(), KEY_READ))
    return false;
  // If no specific version is required, any installed version would be fine.
  if (version.empty())
    return true;
  DWORD requested_version_ms = 0;
  DWORD requested_version_ls = 0;
  if (!ParseVersionFromCodebase(version,
                                &requested_version_ms,
                                &requested_version_ls))
    return true;
  // Get the path to the dll.
  std::wstring path;
  RegKey(HKEY_CLASSES_ROOT, (key_path + L"\\InprocServer32").c_str()).
      ReadValue(NULL, &path);
  if (path.empty())
    return false;

  // Get the file version from the dll.
  DWORD file_version_ms, file_version_ls;
  FileVersionInfo* vi = FileVersionInfo::CreateFileVersionInfo(path);
  if (vi == NULL)
    return false;
  if (vi->fixed_file_info() == NULL) {
    delete vi;
    return false;
  }
  file_version_ms = vi->fixed_file_info()->dwFileVersionMS;
  file_version_ls = vi->fixed_file_info()->dwFileVersionLS;
  delete vi;

  // Compare the request version and actual file version.
  if (file_version_ms > requested_version_ms)
    return true;
  else if (file_version_ms < requested_version_ms)
    return false;
  else
    return file_version_ls >= requested_version_ls;
}

bool IsMimeTypeActiveX(const std::string& mimetype) {
  return LowerCaseEqualsASCII(mimetype, "application/x-oleobject") ||
         LowerCaseEqualsASCII(mimetype, "application/oleobject");
}

}  // namespace activex_shim
