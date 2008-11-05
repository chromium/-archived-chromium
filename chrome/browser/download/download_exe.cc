// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "chrome/browser/download/download_util.h"

namespace download_util {

// For file extensions taken from mozilla:

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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@netscape.com>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Brodie Thiesfield <brofield@jellycan.com>
 *   Jungshik Shin <jshin@i18nl10n.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

static const wchar_t* const g_executables[] = {
  L"ad",
  L"ade",
  L"adp",
  L"app",
  L"application",
  L"asp",
  L"asx",
  L"bas",
  L"bat",
  L"chm",
  L"cmd",
  L"com",
  L"cpl",
  L"crt",
  L"dll",
  L"exe",
  L"fxp",
  L"hlp",
  L"hta",
  L"htm",
  L"html",
  L"inf",
  L"ins",
  L"isp",
  L"jar",
  L"js",
  L"jse",
  L"lnk",
  L"mad",
  L"maf",
  L"mag",
  L"mam",
  L"maq",
  L"mar",
  L"mas",
  L"mat",
  L"mau",
  L"mav",
  L"maw",
  L"mda",
  L"mdb",
  L"mde",
  L"mdt",
  L"mdw",
  L"mdz",
  L"msc",
  L"msh",
  L"mshxml",
  L"msi",
  L"msp",
  L"mst",
  L"ops",
  L"pcd",
  L"pif",
  L"plg",
  L"prf",
  L"prg",
  L"pst",
  L"reg",
  L"scf",
  L"scr",
  L"sct",
  L"shb",
  L"shs",
  L"shtm",
  L"shtml",
  L"url",
  L"vb",
  L"vbe",
  L"vbs",
  L"vsd",
  L"vsmacros",
  L"vss",
  L"vst",
  L"vsw",
  L"ws",
  L"wsc",
  L"wsf",
  L"wsh",
  L"xht",
  L"xhtm",
  L"xhtml"
};

void InitializeExeTypes(std::set<std::wstring>* exe_extensions) {
  DCHECK(exe_extensions);
  for (size_t i = 0; i < arraysize(g_executables); ++i)
    exe_extensions->insert(g_executables[i]);
}

}  // namespace download_util

