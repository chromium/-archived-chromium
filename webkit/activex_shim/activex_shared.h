// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Thes file contains stuff that should be shared among projects that do some
// special handling with ActiveX.

#ifndef WEBKIT_ACTIVEX_SHIM_ACTIVEX_SHARED_H__
#define WEBKIT_ACTIVEX_SHIM_ACTIVEX_SHARED_H__

#include <string>

class GURL;

namespace activex_shim {

// Well known ActiveX control types that we may need to do special processing
// to support them better.
enum ActiveXTypes {
  ACTIVEX_GENERIC,
  ACTIVEX_FLASH,
  ACTIVEX_WMP,
  ACTIVEX_REALPLAYER,
  ACTIVEX_QUICKTIME,
  ACTIVEX_SHOCKWAVE,
  ACTIVEX_TESTCONTROL,  // Internal test control.
};

// Given a clsid, map it to ActiveXTypes. The given clsid must be in format
// as: d27cdb6e-ae6d-11cf-96b8-444553540000. i.e. no {} at both ends,
// no spaces, case insensitive.
ActiveXTypes MapClassIdToType(const std::string& clsid);

// Parse out the real clsid given from a classid attribute of an object tag.
// the classid string should be in format as:
// clsid:XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
bool GetClsidFromClassidAttribute(const std::string& classid,
                                  std::string* clsid);

// Get version string from codebase attribute of an object tag. e.g.:
// codebase="https://site.cmbchina.com/download/CMBEdit.cab#version=1,2,0,1",
// then we will return "1,2,0,1". if the version part doesn't exist,
// returns empty string.
std::string GetVersionFromCodebaseAttribute(const std::string& codebase);

// Look up the registry to see if ActiveX is installed. Here clsid could be just
// clsid, e.g. "0CA54D3F-CEAE-48AF-9A2B-31909CB9515D". Or it could be combined
// with a version string comes from the codebase:
// "0CA54D3F-CEAE-48AF-9A2B-31909CB9515D#1,2,0,1". In the latter case, we need
// to look up the version info of the dll to see if it meets the version
// requirement.
bool IsActiveXInstalled(const std::string& combined_clsid);

// If an ActiveX control is allowed to run from a specific URL.
bool IsActiveXAllowed(const std::string& clsid, const GURL& url);

// If an ActiveX control's codebase comes from allowed websites.
bool IsCodebaseAllowed(const std::string& clsid, const std::string& codebase);

// Check If a given mimetype is of "application/x-oleobject" or
// "application/oleobject"
bool IsMimeTypeActiveX(const std::string& mimetype);

}  // namespace activex_shim

#endif // #ifndef WEBKIT_ACTIVEX_SHIM_ACTIVEX_SHARED_H__
