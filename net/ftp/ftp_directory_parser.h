// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Version: MPL 1.1/GPL 2.0/LGPL 2.1
// The contents of this file are subject to the Mozilla Public License Version
// 1.1 (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
// http://www.mozilla.org/MPL/
// Software distributed under the License is distributed on an "AS IS" basis,
// WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
// for the specific language governing rights and limitations under the
// License.
//
// The Original Code is mozilla.org Code.
//
// The Initial Developer of the Original Code is
// Cyrus Patel <cyp@fb14.uni-mainz.de>.
// Portions created by the Initial Developer are Copyright (C) 2002
// the Initial Developer. All Rights Reserved.
//
// Contributor(s):
// Doug Turner <dougt@netscape.com>
//
// Alternatively, the contents of this file may be used under the terms of
// either the GNU General Public License Version 2 or later (the "GPL"), or
// the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
// in which case the provisions of the GPL or the LGPL are applicable instead
// of those above. If you wish to allow use of your version of this file only
// under the terms of either the GPL or the LGPL, and not to allow others to
// use your version of this file under the terms of the MPL, indicate your
// decision by deleting the provisions above and replace them with the notice
// and other provisions required by the GPL or the LGPL. If you do not delete
// the provisions above, a recipient may use your version of this file under
// the terms of any one of the MPL, the GPL or the LGPL.

// Derived from:
// mozilla/netwerk/streamconv/converters/ParseFTPList.h revision 1.3


#ifndef NET_FTP_FTP_DIRECTORY_PARSER_H_
#define NET_FTP_FTP_DIRECTORY_PARSER_H_

#include <time.h>

#include "base/basictypes.h"

namespace net {

struct ListState {
  void*         magic;          // to determine if previously initialized
  int64         now_time;       // needed for year determination.
  struct tm     now_tm;         // needed for year determination.
  int           lstyle;         // LISTing style.
  int           parsed_one;     // returned anything yet?
  char          carry_buf[84];  // for VMS multiline.
  unsigned int  carry_buf_len;  // length of name in carry_buf.
  unsigned int  numlines;       // number of lines seen.
};

enum LineType {
  FTP_TYPE_DIRECTORY,  // LIST line is a directory entry ('result' is valid).
  FTP_TYPE_FILE,       // LIST line is a file's entry ('result' is valid).
  FTP_TYPE_SYMLINK,    // LIST line is a symlink's entry ('result' is valid).
  FTP_TYPE_JUNK,       // LIST line is junk. (cwd, non-file/dir/link, etc).
  FTP_TYPE_COMMENT     // Its not a LIST line (its a "comment").
};

struct ListResult {
  LineType      fe_type;
  const char*   fe_fname;        // pointer to filename
  unsigned int  fe_fnlen;        // length of filename
  const char*   fe_lname;        // pointer to symlink name
  unsigned int  fe_lnlen;        // length of symlink name
  char          fe_size[40];     // size of file in bytes (<= (2^128 - 1))
  int           fe_cinfs;        // file system is definitely case insensitive
  // TODO(ibrar): We should use "base::Time::Exploded" instead of "tm"
  struct tm     fe_time;         // last-modified time
};

// ParseFTPLine() parses line from an FTP LIST command.
//
// Written July 2002 by Cyrus Patel <cyp@fb14.uni-mainz.de>
// with acknowledgements to squid, lynx, wget and ftpmirror.
//
// Arguments:
// 'line': line of FTP data connection output. The line is assumed
// to end at the first '\0' or '\n' or '\r\n'.
// 'state': a structure used internally to track state between
// lines. Needs to be bzero()'d at LIST begin.
// 'result': where ParseFTPList will store the results of the parse
// if 'line' is not a comment and is not junk.
//
// Returns one of the following:
// 'd' - LIST line is a directory entry ('result' is valid)
// 'f' - LIST line is a file's entry ('result' is valid)
// 'l' - LIST line is a symlink's entry ('result' is valid)
// '?' - LIST line is junk. (cwd, non-file/dir/link, etc)
// '"' - its not a LIST line (its a "comment")
//
// It may be advisable to let the end-user see "comments" (particularly when
// the listing results in ONLY such lines) because such a listing may be:
// - an unknown LIST format (NLST or "custom" format for example)
// - an error msg (EPERM,ENOENT,ENFILE,EMFILE,ENOTDIR,ENOTBLK,EEXDEV etc).
// - an empty directory and the 'comment' is a "total 0" line or similar.
// (warning: a "total 0" can also mean the total size is unknown).
//
// ParseFTPList() supports all known FTP LISTing formats:
// - '/bin/ls -l' and all variants (including Hellsoft FTP for NetWare);
// - EPLF (Easily Parsable List Format);
// - Windows NT's default "DOS-dirstyle";
// - OS/2 basic server format LIST format;
// - VMS (MultiNet, UCX, and CMU) LIST format (including multi-line format);
// - IBM VM/CMS, VM/ESA LIST format (two known variants);
// - SuperTCP FTP Server for Win16 LIST format;
// - NetManage Chameleon (NEWT) for Win16 LIST format;
// - '/bin/dls' (two known variants, plus multi-line) LIST format;
// If there are others, then I'd like to hear about them (send me a sample).
//
// NLSTings are not supported explicitely because they cannot be machine
// parsed consistently: NLSTings do not have unique characteristics - even
// the assumption that there won't be whitespace on the line does not hold
// because some nlistings have more than one filename per line and/or
// may have filenames that have spaces in them. Moreover, distinguishing
// between an error message and an NLST line would require ParseList() to
// recognize all the possible strerror() messages in the world.

LineType ParseFTPLine(const char *line,
                      struct ListState *state,
                      struct ListResult *result);

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_PARSER_H_
