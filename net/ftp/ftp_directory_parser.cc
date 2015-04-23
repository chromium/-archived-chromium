// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Version: MPL 1.1/GPL 2.0/LGPL 2.1
//
// The contents of this file are subject to the Mozilla Public License Version
// 1.1 (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
// http://www.mozilla.org/MPL/
//
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
// mozilla/netwerk/streamconv/converters/ParseFTPList.cpp revision 1.10

#include "net/ftp/ftp_directory_parser.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "build/build_config.h"
#include "base/basictypes.h"
#include "base/string_util.h"

// #undef anything you don't want to support
#define SUPPORT_LSL   // /bin/ls -l and dozens of variations therof
#define SUPPORT_DLS   // /bin/dls format (very, Very, VERY rare)
#define SUPPORT_EPLF  // Extraordinarily Pathetic List Format
#define SUPPORT_DOS   // WinNT server in 'site dirstyle' dos
#define SUPPORT_VMS   // VMS (all: MultiNet, UCX, CMU-IP)
#define SUPPORT_CMS   // IBM VM/CMS,VM/ESA (z/VM and LISTING forms)
#define SUPPORT_OS2   // IBM TCP/IP for OS/2 - FTP Server
#define SUPPORT_W16   // win16 hosts: SuperTCP or NetManage Chameleon

// Implement the Unix gmtime_r() function for Windows.
#if defined(OS_WIN)
static struct tm *gmtime_r(const time_t *timer, struct tm *result) {
  errno_t error = gmtime_s(result, timer);
  if (error) {
    errno = error;
    return NULL;
  }
  return result;
}
#endif

namespace net {

LineType ParseFTPLine(const char *line,
                      struct ListState *state,
                      struct ListResult *result) {
  unsigned int carry_buf_len;
  unsigned int linelen, pos;
  const char *p;

  if (!line || !state || !result)
    return FTP_TYPE_JUNK;

  memset(result, 0, sizeof(*result));
  if (state->magic != ((void *)ParseFTPLine)) {
    memset(state, 0, sizeof(*state));
    state->magic = ((void *)ParseFTPLine);
  }
  state->numlines++;

  // Carry buffer is only valid from one line to the next.
  carry_buf_len = state->carry_buf_len;
  state->carry_buf_len = 0;

  linelen = 0;

  // Strip leading whitespace.
  while (*line == ' ' || *line == '\t')
    line++;

  // Line is terminated at first '\0' or '\n'.
  p = line;
  while (*p && *p != '\n')
    p++;
  linelen = p - line;

  if (linelen > 0 && *p == '\n' && *(p-1) == '\r')
    linelen--;

  // DON'T strip trailing whitespace.
  if (linelen > 0) {
    static const char *month_names = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *tokens[16];
    unsigned int toklen[(sizeof(tokens)/sizeof(tokens[0]))];
    unsigned int linelen_sans_wsp;
    unsigned int numtoks = 0;
    unsigned int tokmarker = 0;
    unsigned int month_num = 0;
    char tbuf[4];
    int lstyle = 0;

    // VMS long filename carryover buffer.
    if (carry_buf_len) {
      tokens[0] = state->carry_buf;
      toklen[0] = carry_buf_len;
      numtoks++;
    }

    pos = 0;
    while (pos < linelen && numtoks < (sizeof(tokens)/sizeof(tokens[0]))) {
      while (pos < linelen &&
          (line[pos] == ' ' || line[pos] == '\t' || line[pos] == '\r'))
        pos++;
      if (pos < linelen) {
        tokens[numtoks] = &line[pos];
        while (pos < linelen &&
            (line[pos] != ' ' && line[pos] != '\t' && line[pos] != '\r'))
          pos++;
        if (tokens[numtoks] != &line[pos]) {
          toklen[numtoks] = (&line[pos] - tokens[numtoks]);
          numtoks++;
        }
      }
    }
    linelen_sans_wsp = &(tokens[numtoks-1][toklen[numtoks-1]]) - tokens[0];
    if (numtoks == (sizeof(tokens)/sizeof(tokens[0]))) {
      pos = linelen;
      while (pos > 0 && (line[pos-1] == ' ' || line[pos-1] == '\t'))
        pos--;
      linelen_sans_wsp = pos;
    }

#if defined(SUPPORT_EPLF)
    // EPLF handling must come somewhere before /bin/dls handling.
    if (!lstyle && (!state->lstyle || state->lstyle == 'E')) {
      if (*line == '+' && linelen > 4 && numtoks >= 2) {
        pos = 1;
        while (pos < (linelen-1)) {
          p = &line[pos++];
          if (*p == '/')
            result->fe_type = FTP_TYPE_DIRECTORY;  // Its a dir.
          else if (*p == 'r')
            result->fe_type = FTP_TYPE_FILE;
          else if (*p == 'm') {
            if (isdigit(line[pos])) {
              while (pos < linelen && isdigit(line[pos]))
                pos++;
              if (pos < linelen && line[pos] == ',') {
                uint64 seconds = 0;
                seconds = StringToInt64(p + 1);
                time_t tim = static_cast<time_t>(seconds);
                gmtime_r(&tim, &result->fe_time);
              }
            }
          } else if (*p == 's') {
              if (isdigit(line[pos])) {
                while (pos < linelen && isdigit(line[pos]))
                  pos++;
                if (pos < linelen && line[pos] == ',' &&
                    ((&line[pos]) - (p+1)) <
                    static_cast<int>(sizeof(result->fe_size) - 1)) {
                  memcpy(result->fe_size, p + 1,
                      (unsigned)(&line[pos] - (p + 1)));
                  result->fe_size[(&line[pos] - (p+1))] = '\0';
                }
              }
          } else if (isalpha(*p)) {
              while (pos < linelen && *++p != ',')
                pos++;
            } else if (*p != '\t' || (p + 1) != tokens[1]) {
                break;  // Its not EPLF after all.
            } else {
                state->parsed_one = 1;
                state->lstyle = lstyle = 'E';

                p = &(line[linelen_sans_wsp]);
                result->fe_fname = tokens[1];
                result->fe_fnlen = p - tokens[1];

                // Access denied.
                if (!result->fe_type) {
                  // is assuming 'f'ile correct?
                  result->fe_type = FTP_TYPE_FILE;
                  return FTP_TYPE_JUNK;  // NO! junk it.
                }
                return result->fe_type;
              }
              if (pos >= (linelen-1) || line[pos] != ',')
                break;
              pos++;
        }  // while (pos < linelen)
        memset(result, 0, sizeof(*result));
      }  // if (*line == '+' && linelen > 4 && numtoks >= 2).
    }  // if (!lstyle && (!state->lstyle || state->lstyle == 'E')).
#endif  // SUPPORT_EPLF

#if defined(SUPPORT_VMS)
    // try VMS Multinet/UCX/CMS server
    if (!lstyle && (!state->lstyle || state->lstyle == 'V')) {
      // Legal characters in a VMS file/dir spec are [A-Z0-9$.-_~].
      // '$' cannot begin a filename and `-' cannot be used as the first
      // or last character. '.' is only valid as a directory separator
      // and <file>.<type> separator. A canonical filename spec might look
      // like this: DISK$VOL:[DIR1.DIR2.DIR3]FILE.TYPE;123
      // All VMS FTP servers LIST in uppercase.
      // We need to be picky about this in order to support
      // multi-line listings correctly.
      if ((!state->parsed_one &&
          numtoks == 1) || (numtoks == 2 && toklen[0] == 9 &&
          memcmp(tokens[0], "Directory", 9) == 0)) {
          // If no dirstyle has been detected yet, and this line is a
          // VMS list's dirname, then turn on VMS dirstyle.
          // eg "ACA:[ANONYMOUS]", "DISK$FTP:[ANONYMOUS]", "SYS$ANONFTP:"
          p = tokens[0];
          pos = toklen[0];
          if (numtoks == 2) {
            p = tokens[1];
            pos = toklen[1];
          }
          pos--;
          if (pos >= 3) {
            while (pos > 0 && p[pos] != '[') {
              pos--;
              if (p[pos] == '-' || p[pos] == '$') {
                if (pos == 0 || p[pos-1] == '[' || p[pos-1] == '.' ||
                  (p[pos] == '-' && (p[pos+1] == ']' || p[pos+1] == '.')))
                  break;
              } else if (p[pos] != '.' && p[pos] != '~' &&
                  !isdigit(p[pos]) && !isalpha(p[pos])) {
                    break;
                } else if (isalpha(p[pos]) && p[pos] != toupper(p[pos])) {
                    break;
                }
            }
            if (pos > 0) {
              pos--;
              if (p[pos] != ':' || p[pos+1] != '[')
                pos = 0;
            }
          }
          if (pos > 0 && p[pos] == ':') {
            while (pos > 0) {
              pos--;
              if (p[pos] != '$' && p[pos] != '_' && p[pos] != '-' &&
                p[pos] != '~' && !isdigit(p[pos]) && !isalpha(p[pos]))
                break;
              else if (isalpha(p[pos]) && p[pos] != toupper(p[pos]))
                break;
            }
            if (pos == 0) {
              state->lstyle = 'V';
              return FTP_TYPE_JUNK;  // Its junk.
            }
          }
          // fallthrough
      } else if ((tokens[0][toklen[0]-1]) != ';') {
          if (numtoks == 1 && (state->lstyle == 'V' && !carry_buf_len))
            lstyle = 'V';
          else if (numtoks < 4);
          else if (toklen[1] >= 10 && memcmp(tokens[1], "%RMS-E-PRV", 10) == 0)
            lstyle = 'V';
          else if ((&line[linelen] - tokens[1]) >= 22 &&
            memcmp(tokens[1], "insufficient privilege", 22) == 0)
            lstyle = 'V';
          else if (numtoks != 4 && numtoks != 6) {
          } else if (numtoks == 6 && (
              toklen[5] < 4 || *tokens[5] != '(' ||
              (tokens[5][toklen[5]-1]) != ')')) {
          } else if ((toklen[2] == 10 || toklen[2] == 11) &&
              (tokens[2][toklen[2]-5]) == '-' &&
              (tokens[2][toklen[2]-9]) == '-' &&
              (((toklen[3] == 4 || toklen[3] == 5 || toklen[3] == 7 ||
              toklen[3] == 8) &&
              (tokens[3][toklen[3]-3]) == ':') ||
              ((toklen[3] == 10 || toklen[3] == 11) &&
              (tokens[3][toklen[3]-3]) == '.')) &&
              //  Time in [H]H:MM[:SS[.CC]] format.
              isdigit(*tokens[1]) &&  // size
              isdigit(*tokens[2]) &&  // date
              isdigit(*tokens[3])  // time
              ) {
            lstyle = 'V';
          }
          if (lstyle == 'V') {
            // MultiNet FTP:
            // LOGIN.COM;2 1 4-NOV-1994 04:09 [ANONYMOUS] (RWE,RWE,,)
            // PUB.DIR;1 1 27-JAN-1994 14:46 [ANONYMOUS] (RWE,RWE,RE,RWE)
            // README.FTP;1 %RMS-E-PRV, insufficient privilege or file
            // protection violation
            // ROUSSOS.DIR;1 1 27-JAN-1994 14:48 [CS,ROUSSOS] (RWE,RWE,RE,R)
            // S67-50903.JPG;1 328 22-SEP-1998 16:19 [ANONYMOUS] (RWED,RWED,,)
            // UCX FTP:
            // CII-MANUAL.TEX;1 213/216 29-JAN-1996 03:33:12[ANONYMOU,ANONYMOUS]
            // (RWED,RWED,,)
            // CMU/VMS-IP FTP
            // [VMSSERV.FILES]ALARM.DIR;1 1/3 5-MAR-1993 18:09
            // TCPware FTP
            // FOO.BAR;1 4 5-MAR-1993 18:09:01.12
            // Long filename example:
            // THIS-IS-A-LONG-VMS-FILENAME.AND-THIS-IS-A-LONG-VMS-FILETYPE\r\n
            // 213[/nnn] 29-JAN-1996 03:33[:nn] [ANONYMOU,ANONYMOUS] (RWED,RWED,,)
            tokmarker = 0;
            p = tokens[0];
            pos = 0;
            if (*p == '[' && toklen[0] >= 4) {
              if (p[1] != ']') {
                p++;
                pos++;
              }
              while (lstyle && pos < toklen[0] && *p != ']') {
                if (*p != '$' && *p != '.' && *p != '_' && *p != '-' &&
                  *p != '~' && !isdigit(*p) && !isalpha(*p))
                  lstyle = 0;
                pos++;
                p++;
              }
              if (lstyle && pos < (toklen[0]-1) && *p == ']') {
                pos++;
                p++;
                tokmarker = pos;  // length of leading "[DIR1.DIR2.etc]"
              }
            }
            while (lstyle && pos < toklen[0] && *p != ';') {
              if (*p != '$' && *p != '.' && *p != '_' && *p != '-' &&
                *p != '~' && !isdigit(*p) && !isalpha(*p))
                lstyle = 0;
              else if (isalpha(*p) && *p != toupper(*p))
                lstyle = 0;
              p++;
              pos++;
            }
            if (lstyle && *p == ';') {
              if (pos == 0 || pos == (toklen[0]-1))
                lstyle = 0;
              for (pos++;lstyle && pos < toklen[0];pos++) {
                if (!isdigit(tokens[0][pos]))
                  lstyle = 0;
              }
            }
            pos = (p - tokens[0]);  // => fnlength sans ";####"
            pos -= tokmarker;  // => fnlength sans "[DIR1.DIR2.etc]"
            p = &(tokens[0][tokmarker]);  // offset of basename.

            if (!lstyle || pos > 80) {
              // VMS filenames can't be longer than that.
              lstyle = 0;
            } else if (numtoks == 1) {
                // If VMS has been detected and there is only one token and
                // that token was a VMS filename then this is a multiline
                // VMS LIST entry.
                if (pos >= (sizeof(state->carry_buf) - 1))
                  pos = (sizeof(state->carry_buf) - 1);  // Shouldn't happen.
                memcpy(state->carry_buf, p, pos);
                state->carry_buf_len = pos;
                return FTP_TYPE_JUNK;  // Tell caller to treat as junk.
            } else if (isdigit(*tokens[1])) {  // not no-privs message.
                for (pos = 0; lstyle && pos < (toklen[1]); pos++) {
                  if (!isdigit((tokens[1][pos])) && (tokens[1][pos]) != '/')
                    lstyle = 0;
                }
                if (lstyle && numtoks > 4) {  // Multinet or UCX but not CMU.
                  for (pos = 1; lstyle && pos < (toklen[5]-1); pos++) {
                    p = &(tokens[5][pos]);
                    if (*p != 'R' && *p != 'W' && *p != 'E' &&
                        *p != 'D' && *p != ',')
                      lstyle = 0;
                  }
                }
              }
          }  // passed initial tests
      }  // else if ((tokens[0][toklen[0]-1]) != ';')

      if (lstyle == 'V') {
        state->parsed_one = 1;
        state->lstyle = lstyle;
        if (isdigit(*tokens[1])) {  // not permission denied etc
          // strip leading directory name
          if (*tokens[0] == '[') {  // CMU server
            pos = toklen[0] - 1;
            p = tokens[0] + 1;
            while (*p != ']') {
              p++;
              pos--;
            }
            toklen[0] = --pos;
            tokens[0] = ++p;
          }
          pos = 0;
          while (pos < toklen[0] && (tokens[0][pos]) != ';')
            pos++;

          result->fe_cinfs = 1;
          result->fe_type = FTP_TYPE_FILE;
          result->fe_fname = tokens[0];
          result->fe_fnlen = pos;

          if (pos > 4) {
            p = &(tokens[0][pos-4]);
            if (p[0] == '.' && p[1] == 'D' && p[2] == 'I' && p[3] == 'R') {
              result->fe_fnlen -= 4;
              result->fe_type = FTP_TYPE_DIRECTORY;
            }
          }

          if (result->fe_type != FTP_TYPE_DIRECTORY) {
            // #### or used/allocated form. If used/allocated form, then
            // 'used' is the size in bytes if and only if 'used'<=allocated.
            // If 'used' is size in bytes then it can be > 2^32
            // If 'used' is not size in bytes then it is size in blocks.
            pos = 0;
            while (pos < toklen[1] && (tokens[1][pos]) != '/')
              pos++;
            // size requires multiplication by blocksize.
            //
            // We could assume blocksize is 512 (like Lynx does) and
            // shift by 9, but that might not be right. Even if it
            // were, doing that wouldn't reflect what the file's
            // real size was. The sanest thing to do is not use the
            // LISTing's filesize, so we won't (like ftpmirror).
            //
            // ulltoa(((uint64)fsz)<<9, result->fe_size, 10);
            //
            // A block is always 512 bytes on OpenVMS, compute size.
            // So its rounded up to the next block, so what, its better
            // than not showing the size at all.
            // A block is always 512 bytes on OpenVMS, compute size.
            // So its rounded up to the next block, so what, its better
            // than not showing the size at all.
            uint64 size = strtoul(tokens[1], NULL, 10) * 512;
            base::snprintf(result->fe_size, sizeof(result->fe_size),
                           "%lld", size);
          }  // if (result->fe_type != FTP_TYPE_DIRECTORY)

          p = tokens[2] + 2;
          if (*p == '-')
            p++;
          tbuf[0] = p[0];
          tbuf[1] = tolower(p[1]);
          tbuf[2] = tolower(p[2]);
          month_num = 0;
          for (pos = 0; pos < (12*3); pos += 3) {
            if (tbuf[0] == month_names[pos+0] &&
                tbuf[1] == month_names[pos + 1] &&
                tbuf[2] == month_names[pos + 2])
              break;
            month_num++;
          }
          if (month_num >= 12)
            month_num = 0;
          result->fe_time.tm_mon  = month_num;
          result->fe_time.tm_mday = StringToInt(tokens[2]);
          result->fe_time.tm_year = StringToInt(p + 4);
          // NSPR wants year as XXXX

          p = tokens[3] + 2;
          if (*p == ':')
            p++;
          if (p[2] == ':')
            result->fe_time.tm_sec = StringToInt(p + 3);
          result->fe_time.tm_hour = StringToInt(tokens[3]);
          result->fe_time.tm_min = StringToInt(p);
          return result->fe_type;
        }  // if (isdigit(*tokens[1]))
        return FTP_TYPE_JUNK;  // junk
      }  // if (lstyle == 'V')
    }  // if (!lstyle && (!state->lstyle || state->lstyle == 'V'))
#endif  // SUPPORT_VMS

#if defined(SUPPORT_CMS)
    // Virtual Machine/Conversational Monitor System (IBM Mainframe)
    if (!lstyle && (!state->lstyle || state->lstyle == 'C')) {  // VM/CMS
      // LISTing according to mirror.pl
      // Filename FileType Fm Format Lrecl Records Blocks Date Time
      // LASTING GLOBALV A1 V 41 21 1 9/16/91 15:10:32
      // J43401 NETLOG A0 V 77 1 1 9/12/91 12:36:04
      // PROFILE EXEC A1 V 17 3 1 9/12/91 12:39:07
      // DIRUNIX SCRIPT A1 V 77 1216 17 1/04/93 20:30:47
      // MAIL PROFILE A2 F 80 1 1 10/14/92 16:12:27
      // BADY2K TEXT A0 V 1 1 1 1/03/102 10:11:12
      // AUTHORS A1 DIR - - - 9/20/99 10:31:11
      //
      // LISTing from vm.marist.edu and vm.sc.edu
      // 220-FTPSERVE IBM VM Level 420 at VM.MARIST.EDU, 04:58:12 EDT WEDNESDAY 2002-07-10
      // AUTHORS DIR - - - 1999-09-20 10:31:11 -
      // HARRINGTON DIR - - - 1997-02-12 15:33:28 -
      // PICS DIR - - - 2000-10-12 15:43:23 -
      // SYSFILE DIR - - - 2000-07-20 17:48:01 -
      // WELCNVT EXEC V 72 9 1 1999-09-20 17:16:18 -
      // WELCOME EREADME F 80 21 1 1999-12-27 16:19:00 -
      // WELCOME README V 82 21 1 1999-12-27 16:19:04 -
      // README ANONYMOU V 71 26 1 1997-04-02 12:33:20 TCP291
      // README ANONYOLD V 71 15 1 1995-08-25 16:04:27 TCP291
      if (numtoks >= 7 && (toklen[0]+toklen[1]) <= 16) {
        for (pos = 1; !lstyle && (pos+5) < numtoks; pos++) {
          p = tokens[pos];
          if ((toklen[pos] == 1 && (*p == 'F' || *p == 'V')) ||
            (toklen[pos] == 3 && *p == 'D' && p[1] == 'I' && p[2] == 'R')) {
              if (toklen[pos+5] == 8 && (tokens[pos+5][2]) == ':' &&
                (tokens[pos+5][5]) == ':') {
                  p = tokens[pos+4];
                  if ((toklen[pos+4] == 10 && p[4] == '-' && p[7] == '-') ||
                      (toklen[pos+4] >= 7 && toklen[pos+4] <= 9 &&
                      p[((p[1] != '/')?(2):(1))] == '/' &&
                      p[((p[1] != '/')?(5):(4))] == '/')) {
                    // Y2K bugs possible ("7/06/102" or "13/02/101")
                    if ((*tokens[pos+1] == '-' &&
                      *tokens[pos+2] == '-' &&
                      *tokens[pos+3] == '-') ||
                      (isdigit(*tokens[pos+1]) &&
                      isdigit(*tokens[pos+2]) &&
                      isdigit(*tokens[pos+3]))) {
                        lstyle = 'C';
                        tokmarker = pos;
                    }  // if ((*tokens[pos+1] == '-' &&
                  }  // if ((toklen[pos+4] == 10
              }  // toklen[pos+5] == 8
          }  // if ((toklen[pos] == 1 && (*p == 'F'
        }  // for (pos = 1; !lstyle && (pos+5) < numtoks; pos++)
      }  // if (numtoks >= 7)
      // extra checking if first pass
      if (lstyle && !state->lstyle) {
        for (pos = 0, p = tokens[0]; lstyle && pos < toklen[0]; pos++, p++) {
          if (isalpha(*p) && toupper(*p) != *p)
            lstyle = 0;
        }
        for (pos = tokmarker+1; pos <= tokmarker+3; pos++) {
          if (!(toklen[pos] == 1 && *tokens[pos] == '-')) {
            for (p = tokens[pos]; lstyle && p < (tokens[pos] + toklen[pos]);
                p++) {
              if (!isdigit(*p))
                lstyle = 0;
            }  // for (p = tokens[pos]; lstyle
          }  // if (!(toklen[pos] == 1
        }  // for (pos = tokmarker+1;
        for (pos = 0, p = tokens[tokmarker+4];
            lstyle && pos < toklen[tokmarker+4]; pos++, p++) {
          if (*p == '/') {
            // There may be Y2K bugs in the date. Don't simplify to
            // pos != (len-3) && pos != (len-6) like time is done.
            if ((tokens[tokmarker + 4][1]) == '/') {
              if (pos != 1 && pos != 4)
              lstyle = 0;
            } else if (pos != 2 && pos != 5) {
              lstyle = 0;
            }
          } else if (*p != '-' && !isdigit(*p)) {
            lstyle = 0;
          } else if (*p == '-' && pos != 4 && pos != 7) {
            lstyle = 0;
          }
        }
        for (pos = 0, p = tokens[tokmarker+5];
          lstyle && pos < toklen[tokmarker+5]; pos++, p++) {
          if (*p != ':' && !isdigit(*p))
            lstyle = 0;
          else if (*p == ':' && pos != (toklen[tokmarker+5]-3)
            && pos != (toklen[tokmarker+5]-6))
            lstyle = 0;
        }
      }  // if (lstyle && !state->lstyle)

      if (lstyle == 'C') {
        state->parsed_one = 1;
        state->lstyle = lstyle;

        p = tokens[tokmarker+4];
        if (toklen[tokmarker+4] == 10) {  // newstyle: YYYY-MM-DD format
          result->fe_time.tm_year = StringToInt(p + 0) - 1900;
          result->fe_time.tm_mon  = StringToInt(p + 5) - 1;
          result->fe_time.tm_mday = StringToInt(p + 8);
        } else {  // oldstyle: [M]M/DD/YY format
            pos = toklen[tokmarker + 4];
            result->fe_time.tm_mon  = StringToInt(p) - 1;
            result->fe_time.tm_mday = StringToInt((p + pos)-5);
            result->fe_time.tm_year = StringToInt((p + pos)-2);
            if (result->fe_time.tm_year < 70)
              result->fe_time.tm_year += 100;
        }
        p = tokens[tokmarker + 5];
        pos = toklen[tokmarker + 5];
        result->fe_time.tm_hour = StringToInt(p);
        result->fe_time.tm_min = StringToInt((p + pos) - 5);
        result->fe_time.tm_sec = StringToInt((p + pos) - 2);

        result->fe_cinfs = 1;
        result->fe_fname = tokens[0];
        result->fe_fnlen = toklen[0];
        result->fe_type = FTP_TYPE_FILE;

        p = tokens[tokmarker];
        if (toklen[tokmarker] == 3 && *p == 'D' && p[1] == 'I' && p[2] == 'R')
          result->fe_type = FTP_TYPE_DIRECTORY;

        if ((toklen[tokmarker+4] == 10 && tokmarker > 1) ||
            (toklen[tokmarker+4] != 10 && tokmarker > 2)) {
          // have a filetype column
          char *dot;
          p = &(tokens[0][toklen[0]]);
          memcpy(&dot, &p, sizeof(dot));  // NASTY
          *dot++ = '.';
          p = tokens[1];
          for (pos = 0; pos < toklen[1]; pos++)
            *dot++ = *p++;
          result->fe_fnlen += 1 + toklen[1];
        }
        // oldstyle LISTING:
        // files/dirs not on the 'A' minidisk are not RETRievable/CHDIRable
        // if (toklen[tokmarker+4] != 10 && *tokens[tokmarker-1] != 'A')
        // return FTP_TYPE_JUNK;

        // VM/CMS LISTings have no usable filesize field.
        // Have to use the 'SIZE' command for that.
        return result->fe_type;
      }  // if (lstyle == 'C' && (!state->lstyle || state->lstyle == lstyle))
    }  // VM/CMS
#endif  // SUPPORT_CMS

#if defined(SUPPORT_DOS)  // WinNT DOS dirstyle
    if (!lstyle && (!state->lstyle || state->lstyle == 'W')) {
      // "10-23-00 01:27PM <DIR> veronist"
      // "06-15-00 07:37AM <DIR> zoe"
      // "07-14-00 01:35PM 2094926 canprankdesk.tif"
      // "07-21-00 01:19PM 95077 Jon Kauffman Enjoys the Good Life.jpg"
      // "07-21-00 01:19PM 52275 Name Plate.jpg"
      // "07-14-00 01:38PM 2250540 Valentineoffprank-HiRes.jpg"
      if ((numtoks >= 4) && toklen[0] == 8 && toklen[1] == 7 &&
        (*tokens[2] == '<' || isdigit(*tokens[2]))) {
          p = tokens[0];
          if (isdigit(p[0]) && isdigit(p[1]) && p[2] == '-' &&
            isdigit(p[3]) && isdigit(p[4]) && p[5] == '-' &&
            isdigit(p[6]) && isdigit(p[7])) {
              p = tokens[1];
              if (isdigit(p[0]) && isdigit(p[1]) && p[2] == ':' &&
                isdigit(p[3]) && isdigit(p[4]) &&
                (p[5] == 'A' || p[5] == 'P') && p[6] == 'M') {
                  lstyle = 'W';
                  if (!state->lstyle) {
                    p = tokens[2];
                    // <DIR> or <JUNCTION>
                    if (*p != '<' || p[toklen[2] - 1] != '>') {
                      for (pos = 1; (lstyle && pos < toklen[2]); pos++) {
                        if (!isdigit(*++p))
                          lstyle = 0;
                      }
                    }
                  }
              }
          }
      }
      if (lstyle == 'W') {
        state->parsed_one = 1;
        state->lstyle = lstyle;

        p = &(line[linelen_sans_wsp]);  // line end sans wsp
        result->fe_cinfs = 1;
        result->fe_fname = tokens[3];
        result->fe_fnlen = p - tokens[3];
        result->fe_type = FTP_TYPE_DIRECTORY;

        if (*tokens[2] != '<') {  // not <DIR> or <JUNCTION>
          result->fe_type = FTP_TYPE_FILE;
          pos = toklen[2];
          while (pos > (sizeof(result->fe_size) - 1))
            pos = (sizeof(result->fe_size) - 1);
          memcpy(result->fe_size, tokens[2], pos);
          result->fe_size[pos] = '\0';
        } else if ((tokens[2][1]) != 'D') {  // not <DIR>
            result->fe_type = FTP_TYPE_JUNK;  // unknown until junc for sure
            if (result->fe_fnlen > 4) {
              p = result->fe_fname;
              for (pos = result->fe_fnlen - 4; pos > 0; pos--) {
                if (p[0] == ' ' && p[3] == ' ' && p[2] == '>' &&
                  (p[1] == '=' || p[1] == '-')) {
                  result->fe_type = FTP_TYPE_SYMLINK;
                  result->fe_fnlen = p - result->fe_fname;
                  result->fe_lname = p + 4;
                  result->fe_lnlen = &(line[linelen_sans_wsp])
                    - result->fe_lname;
                  break;
                }
                p++;
              }
            }
        }

        result->fe_time.tm_mon = StringToInt(tokens[0]+0);
        if (result->fe_time.tm_mon != 0) {
          result->fe_time.tm_mon--;
          result->fe_time.tm_mday = StringToInt(tokens[0]+3);
          result->fe_time.tm_year = StringToInt(tokens[0]+6);
          if (result->fe_time.tm_year < 80)
            result->fe_time.tm_year += 100;
        }

        result->fe_time.tm_hour = StringToInt(tokens[1]+0);
        result->fe_time.tm_min = StringToInt(tokens[1]+3);
        if ((tokens[1][5]) == 'P' && result->fe_time.tm_hour < 12)
          result->fe_time.tm_hour += 12;

        // the caller should do this (if dropping "." and ".." is desired)
        // if (result->fe_type == FTP_TYPE_DIRECTORY && result->fe_fname[0]
        // == '.' &&
        // (result->fe_fnlen == 1 || (result->fe_fnlen == 2 &&
        // result->fe_fname[1] == '.')))
        // return FTP_TYPE_JUNK;

        return result->fe_type;
      }  // if (lstyle == 'W' && (!state->lstyle || state->lstyle == lstyle))
    }  // if (!lstyle && (!state->lstyle || state->lstyle == 'W'))
#endif

#if defined(SUPPORT_OS2)
    if (!lstyle && (!state->lstyle || state->lstyle == 'O')) {  // OS/2 test
      // 220 server IBM TCP/IP for OS/2 - FTP Server ver 23:04:36 on
      // Jan 15 1997 ready.
      // fixed position, space padded columns. I have only a vague idea
      // of what the contents between col 18 and 34 might be: All I can infer
      // is that there may be attribute flags in there and there may be
      // a " DIR" in there.
      //
      // 1 2 3 4 5 6
      // 0123456789012345678901234567890123456789012345678901234567890123456789
      // ----- size -------|??????????????? MM-DD-YY| HH:MM| nnnnnnnnn....
      // 0 DIR 04-11-95 16:26 .
      // 0 DIR 04-11-95 16:26 ..
      // 0 DIR 04-11-95 16:26 ADDRESS
      // 612 RHSA 07-28-95 16:45 air_tra1.bag
      // 195 A 08-09-95 10:23 Alfa1.bag
      // 0 RHS DIR 04-11-95 16:26 ATTACH
      // 372 A 08-09-95 10:26 Aussie_1.bag
      // 310992 06-28-94 09:56 INSTALL.EXE
      // 1 2 3 4
      // 01234567890123456789012345678901234567890123456789
      // dirlist from the mirror.pl project, col positions from Mozilla.

      p = &(line[toklen[0]]);
      // \s(\d\d-\d\d-\d\d)\s+(\d\d:\d\d)\s
      if (numtoks >= 4 && toklen[0] <= 18 && isdigit(*tokens[0]) &&
        (linelen - toklen[0]) >= (53-18) &&
        p[18-18] == ' ' && p[34-18] == ' ' &&
        p[37-18] == '-' && p[40-18] == '-' && p[43-18] == ' ' &&
        p[45-18] == ' ' && p[48-18] == ':' && p[51-18] == ' ' &&
        isdigit(p[35-18]) && isdigit(p[36-18]) &&
        isdigit(p[38-18]) && isdigit(p[39-18]) &&
        isdigit(p[41-18]) && isdigit(p[42-18]) &&
        isdigit(p[46-18]) && isdigit(p[47-18]) &&
        isdigit(p[49-18]) && isdigit(p[50-18])) {
          lstyle = 'O';  // OS/2
          if (!state->lstyle) {
            for (pos = 1; lstyle && pos < toklen[0]; pos++) {
              if (!isdigit(tokens[0][pos]))
                lstyle = 0;
            }
          }
      }

      if (lstyle == 'O') {
        state->parsed_one = 1;
        state->lstyle = lstyle;

        p = &(line[toklen[0]]);

        result->fe_cinfs = 1;
        result->fe_fname = &p[53-18];
        result->fe_fnlen = (&(line[linelen_sans_wsp]))
          - (result->fe_fname);
        result->fe_type = FTP_TYPE_FILE;

        // I don't have a real listing to determine exact pos, so scan.
        for (pos = (18-18); pos < ((35-18)-4); pos++) {
          if (p[pos+0] == ' ' && p[pos+1] == 'D' &&
            p[pos+2] == 'I' && p[pos+3] == 'R') {
              result->fe_type = FTP_TYPE_DIRECTORY;
              break;
          }
        }

        if (result->fe_type != FTP_TYPE_DIRECTORY) {
          pos = toklen[0];
          if (pos > (sizeof(result->fe_size)-1))
            pos = (sizeof(result->fe_size)-1);
          memcpy(result->fe_size, tokens[0], pos);
          result->fe_size[pos] = '\0';
        }

        result->fe_time.tm_mon = StringToInt(&p[35-18]) - 1;
        result->fe_time.tm_mday = StringToInt(&p[38-18]);
        result->fe_time.tm_year = StringToInt(&p[41-18]);
        if (result->fe_time.tm_year < 80)
          result->fe_time.tm_year += 100;
        result->fe_time.tm_hour = StringToInt(&p[46-18]);
        result->fe_time.tm_min = StringToInt(&p[49-18]);

        // The caller should do this (if dropping "." and ".." is desired)
        // if (result->fe_type == FTP_TYPE_DIRECTORY &&
        //     result->fe_fname[0] == '.' && (result->fe_fnlen == 1 ||
        //     (result->fe_fnlen == 2 && result->fe_fname[1] == '.')))
        //   return FTP_TYPE_JUNK;

        return result->fe_type;
      }  // if (lstyle == 'O')
    }  // if (!lstyle && (!state->lstyle || state->lstyle == 'O'))
#endif  // SUPPORT_OS2

#if defined(SUPPORT_LSL)
    if (!lstyle && (!state->lstyle || state->lstyle == 'U')) {  // /bin/ls & co.
      // UNIX-style listing, without inum and without blocks
      // "-rw-r--r-- 1 root other 531 Jan 29 03:26 README"
      // "dr-xr-xr-x 2 root other 512 Apr 8 1994 etc"
      // "dr-xr-xr-x 2 root 512 Apr 8 1994 etc"
      // "lrwxrwxrwx 1 root other 7 Jan 25 00:17 bin -> usr/bin"
      // Also produced by Microsoft's FTP servers for Windows:
      // "---------- 1 owner group 1803128 Jul 10 10:18 ls-lR.Z"
      // "d--------- 1 owner group 0 May 9 19:45 Softlib"
      // Also WFTPD for MSDOS:
      // "-rwxrwxrwx 1 noone nogroup 322 Aug 19 1996 message.ftp"
      // Hellsoft for NetWare:
      // "d[RWCEMFA] supervisor 512 Jan 16 18:53 login"
      // "-[RWCEMFA] rhesus 214059 Oct 20 15:27 cx.exe"
      // Newer Hellsoft for NetWare: (netlab2.usu.edu)
      // - [RWCEAFMS] NFAUUser 192 Apr 27 15:21 HEADER.html
      // d [RWCEAFMS] jrd 512 Jul 11 03:01 allupdates
      // Also NetPresenz for the Mac:
      // "-------r-- 326 1391972 1392298 Nov 22 1995 MegaPhone.sit"
      // "drwxrwxr-x folder 2 May 10 1996 network"
      // Protected directory:
      // "drwx-wx-wt 2 root wheel 512 Jul 1 02:15 incoming"
      // uid/gid instead of username/groupname:
      // "drwxr-xr-x 2 0 0 512 May 28 22:17 etc"

      if (numtoks >= 6) {
        // there are two perm formats (Hellsoft/NetWare and *IX strmode(3)).
        // Scan for size column only if the perm format is one or the other.
        if (toklen[0] == 1 || (tokens[0][1]) == '[') {
          if (*tokens[0] == FTP_TYPE_DIRECTORY || *tokens[0] == '-') {
            pos = toklen[0]-1;
            p = tokens[0] + 1;
            if (pos == 0) {
              p = tokens[1];
              pos = toklen[1];
            }
            if ((pos == 9 || pos == 10) &&
              (*p == '[' && p[pos-1] == ']') &&
              (p[1] == 'R' || p[1] == '-') &&
              (p[2] == 'W' || p[2] == '-') &&
              (p[3] == 'C' || p[3] == '-') &&
              (p[4] == 'E' || p[4] == '-')) {
                // rest is FMA[S] or AFM[S]
                lstyle = 'U';  // very likely one of the NetWare servers
            }
          }
        } else if ((toklen[0] == 10 || toklen[0] == 11) &&
            strchr("-bcdlpsw?DFam", *tokens[0])) {
              p = &(tokens[0][1]);
              if ((p[0] == 'r' || p[0] == '-') &&
                  (p[1] == 'w' || p[1] == '-') &&
                  (p[3] == 'r' || p[3] == '-') &&
                  (p[4] == 'w' || p[4] == '-') &&
                  (p[6] == 'r' || p[6] == '-') &&
                  (p[7] == 'w' || p[7] == '-')) {
                  // 'x'/p[9] can be S|s|x|-|T|t or implementation specific
                lstyle = 'U';  // very likely /bin/ls
              }
          }
      }
      if (lstyle == 'U') {  // first token checks out
        lstyle = 0;
        for (pos = (numtoks-5); !lstyle && pos > 1; pos--) {
          // scan for: (\d+)\s+([A-Z][a-z][a-z])\s+
          // (\d\d\d\d|\d\:\d\d|\d\d\:\d\d|\d\:\d\d\:\d\d|\d\d\:\d\d\:\d\d)
          // \s+(.+)$
          if (isdigit(*tokens[pos])  // size
            // (\w\w\w)
            && toklen[pos+1] == 3 && isalpha(*tokens[pos+1]) &&
            isalpha(tokens[pos+1][1]) && isalpha(tokens[pos+1][2])
            // (\d|\d\d)
            && isdigit(*tokens[pos+2]) &&
            (toklen[pos+2] == 1 ||
            (toklen[pos+2] == 2 && isdigit(tokens[pos+2][1])))
            && toklen[pos+3] >= 4 && isdigit(*tokens[pos+3])
            // (\d\:\d\d\:\d\d|\d\d\:\d\d\:\d\d)
            && (toklen[pos+3] <= 5 || (
            (toklen[pos+3] == 7 || toklen[pos+3] == 8) &&
            (tokens[pos+3][toklen[pos+3]-3]) == ':'))
            && isdigit(tokens[pos+3][toklen[pos+3]-2])
            && isdigit(tokens[pos+3][toklen[pos+3]-1])
            && (
            // (\d\d\d\d)
            ((toklen[pos+3] == 4 || toklen[pos+3] == 5) &&
            isdigit(tokens[pos+3][1]) &&
            isdigit(tokens[pos+3][2]) )
            // (\d\:\d\d|\d\:\d\d\:\d\d)
            || ((toklen[pos+3] == 4 || toklen[pos+3] == 7) &&
            (tokens[pos+3][1]) == ':' &&
            isdigit(tokens[pos+3][2]) && isdigit(tokens[pos+3][3]))
            // (\d\d\:\d\d|\d\d\:\d\d\:\d\d)
            || ((toklen[pos+3] == 5 || toklen[pos+3] == 8) &&
            isdigit(tokens[pos+3][1]) && (tokens[pos+3][2]) == ':' &&
            isdigit(tokens[pos+3][3]) && isdigit(tokens[pos+3][4])))) {
              lstyle = 'U';  // assume /bin/ls or variant format
              tokmarker = pos;
              // check that size is numeric
              p = tokens[tokmarker];
              unsigned int i;
              for (i = 0; i < toklen[tokmarker]; i++) {
                if (!isdigit(*p++)) {
                  lstyle = 0;
                  break;
                }
              }
              if (lstyle) {
                month_num = 0;
                p = tokens[tokmarker+1];
                for (i = 0; i < (12*3); i+=3) {
                  if (p[0] == month_names[i+0] &&
                    p[1] == month_names[i+1] &&
                    p[2] == month_names[i+2])
                    break;
                  month_num++;
                }
                if (month_num >= 12)
                  lstyle = 0;
              }
          }  // relative position test
        }  // for (pos = (numtoks-5); !lstyle && pos > 1; pos--)
      }  // if (lstyle == 'U')

      if (lstyle == 'U') {
        state->parsed_one = 1;
        state->lstyle = lstyle;

        result->fe_cinfs = 0;
        result->fe_type = FTP_TYPE_JUNK;
        if (*tokens[0] == 'd' || *tokens[0] == 'D')
          result->fe_type = FTP_TYPE_DIRECTORY;
        else if (*tokens[0] == 'l')
          result->fe_type = FTP_TYPE_SYMLINK;
        else if (*tokens[0] == '-' || *tokens[0] == 'F')
          result->fe_type = FTP_TYPE_FILE;  // (hopefully a regular file)

        if (result->fe_type != FTP_TYPE_DIRECTORY) {
          pos = toklen[tokmarker];
          if (pos > (sizeof(result->fe_size)-1))
            pos = (sizeof(result->fe_size)-1);
          memcpy(result->fe_size, tokens[tokmarker], pos);
          result->fe_size[pos] = '\0';
        }

        result->fe_time.tm_mon = month_num;
        result->fe_time.tm_mday = StringToInt(tokens[tokmarker+2]);
        if (result->fe_time.tm_mday == 0)
          result->fe_time.tm_mday++;

        p = tokens[tokmarker+3];
        pos = (unsigned int)StringToInt(p);
        if (p[1] == ':')  // one digit hour
          p--;
        if (p[2] != ':') {  // year
          result->fe_time.tm_year = pos;
        } else {
            result->fe_time.tm_hour = pos;
            result->fe_time.tm_min = StringToInt(p+3);
            if (p[5] == ':')
              result->fe_time.tm_sec = StringToInt(p+6);

            if (!state->now_time) {
              time_t now = time(NULL);
              state->now_time = now * 1000000;
              gmtime_r(&now, &state->now_tm);
            }

            result->fe_time.tm_year = state->now_tm.tm_year;
            if ( (( state->now_tm.tm_mon << 5) + state->now_tm.tm_mday) <
              ((result->fe_time.tm_mon << 5) + result->fe_time.tm_mday) )
              result->fe_time.tm_year--;
        }  // time/year

        result->fe_fname = tokens[tokmarker+4];
        result->fe_fnlen = (&(line[linelen_sans_wsp]))
          - (result->fe_fname);

        if (result->fe_type == FTP_TYPE_SYMLINK && result->fe_fnlen > 4) {
          p = result->fe_fname + 1;
          for (pos = 1; pos < (result->fe_fnlen - 4); pos++) {
            if (*p == ' ' && p[1] == '-' && p[2] == '>' && p[3] == ' ') {
              result->fe_lname = p + 4;
              result->fe_lnlen = (&(line[linelen_sans_wsp]))
                - (result->fe_lname);
              result->fe_fnlen = pos;
              break;
            }
            p++;
          }
        }

#if defined(SUPPORT_LSLF)  // some (very rare) servers return ls -lF
        if (result->fe_fnlen > 1) {
          p = result->fe_fname[result->fe_fnlen-1];
          pos = result->fe_type;
          if (pos == FTP_TYPE_DIRECTORY) {
            if (*p == '/') result->fe_fnlen--;  // directory
          } else if (pos == FTP_TYPE_SYMLINK) {
              if (*p == '@') result->fe_fnlen--;  // symlink
          } else if (pos == FTP_TYPE_FILE) {
              if (*p == '*') result->fe_fnlen--;  // executable
            } else if (*p == '=' || *p == '%' || *p == '|') {
                result->fe_fnlen--;  // socket, whiteout, fifo
            }
        }
#endif  // SUPPORT_LSLF
        // the caller should do this (if dropping "." and ".." is desired)
        // if (result->fe_type == FTP_TYPE_DIRECTORY &&
        //     result->fe_fname[0] == '.' && (result->fe_fnlen == 1 ||
        //     (result->fe_fnlen == 2 && result->fe_fname[1] == '.')))
        //   return FTP_TYPE_JUNK;
        return result->fe_type;
      }  // if (lstyle == 'U')
    }  // if (!lstyle && (!state->lstyle || state->lstyle == 'U'))
#endif  // SUPPORT_LSL

#if defined(SUPPORT_W16)  // 16bit Windows
    // old SuperTCP suite FTP server for Win3.1
    if (!lstyle && (!state->lstyle || state->lstyle == 'w')) {
      // old NetManage Chameleon TCP/IP suite FTP server for Win3.1
      // SuperTCP dirlist from the mirror.pl project
      // mon/day/year separator may be '/' or '-'.
      // . <DIR> 11-16-94 17:16
      // .. <DIR> 11-16-94 17:16
      // INSTALL <DIR> 11-16-94 17:17
      // CMT <DIR> 11-21-94 10:17
      // DESIGN1.DOC 11264 05-11-95 14:20
      // README.TXT 1045 05-10-95 11:01
      // WPKIT1.EXE 960338 06-21-95 17:01
      // CMT.CSV 0 07-06-95 14:56
      //
      // Chameleon dirlist guessed from lynx
      // . <DIR> Nov 16 1994 17:16
      // .. <DIR> Nov 16 1994 17:16
      // INSTALL <DIR> Nov 16 1994 17:17
      // CMT <DIR> Nov 21 1994 10:17
      // DESIGN1.DOC 11264 May 11 1995 14:20 A
      // README.TXT 1045 May 10 1995 11:01
      // WPKIT1.EXE 960338 Jun 21 1995 17:01 R
      // CMT.CSV 0 Jul 06 1995 14:56 RHA
      if (numtoks >= 4 && toklen[0] < 13 &&
        ((toklen[1] == 5 && *tokens[1] == '<') || isdigit(*tokens[1]))) {
          if (numtoks == 4
            && (toklen[2] == 8 || toklen[2] == 9)
            && (((tokens[2][2]) == '/' && (tokens[2][5]) == '/') ||
            ((tokens[2][2]) == '-' && (tokens[2][5]) == '-'))
            && (toklen[3] == 4 || toklen[3] == 5)
            && (tokens[3][toklen[3]-3]) == ':'
            && isdigit(tokens[2][0]) && isdigit(tokens[2][1])
            && isdigit(tokens[2][3]) && isdigit(tokens[2][4])
            && isdigit(tokens[2][6]) && isdigit(tokens[2][7])
            && (toklen[2] < 9 || isdigit(tokens[2][8]))
            && isdigit(tokens[3][toklen[3]-1])
            && isdigit(tokens[3][toklen[3]-2])
            && isdigit(tokens[3][toklen[3]-4]) && isdigit(*tokens[3])) {
              lstyle = 'w';
          } else if ((numtoks == 6 || numtoks == 7)
              && toklen[2] == 3 && toklen[3] == 2
              && toklen[4] == 4 && toklen[5] == 5
              && (tokens[5][2]) == ':'
              && isalpha(tokens[2][0]) && isalpha(tokens[2][1])
              && isalpha(tokens[2][2])
              && isdigit(tokens[3][0]) && isdigit(tokens[3][1])
              && isdigit(tokens[4][0]) && isdigit(tokens[4][1])
              && isdigit(tokens[4][2]) && isdigit(tokens[4][3])
              && isdigit(tokens[5][0]) && isdigit(tokens[5][1])
              && isdigit(tokens[5][3]) && isdigit(tokens[5][4])) {
                // could also check that (&(tokens[5][5]) - tokens[2]) == 17
                lstyle = 'w';
            }
            if (lstyle && state->lstyle != lstyle) {  // first time
              p = tokens[1];
              if (toklen[1] != 5 || p[0] != '<' || p[1] != 'D' ||
                p[2] != 'I' || p[3] != 'R' || p[4] != '>') {
                  for (pos = 0; lstyle && pos < toklen[1]; pos++) {
                    if (!isdigit(*p++))
                      lstyle = 0;
                  }
              }  // not <DIR>
            }  // if (first time)
      }  // if (numtoks == ...)

      if (lstyle == 'w') {
        state->parsed_one = 1;
        state->lstyle = lstyle;

        result->fe_cinfs = 1;
        result->fe_fname = tokens[0];
        result->fe_fnlen = toklen[0];
        result->fe_type = FTP_TYPE_DIRECTORY;

        p = tokens[1];
        if (isdigit(*p)) {
          result->fe_type = FTP_TYPE_FILE;
          pos = toklen[1];
          if (pos > (sizeof(result->fe_size) - 1))
            pos = sizeof(result->fe_size) - 1;
          memcpy(result->fe_size, p, pos);
          result->fe_size[pos] = '\0';
        }

        p = tokens[2];
        if (toklen[2] == 3) {  // Chameleon
          tbuf[0] = toupper(p[0]);
          tbuf[1] = tolower(p[1]);
          tbuf[2] = tolower(p[2]);
          for (pos = 0; pos < (12*3); pos+=3) {
            if (tbuf[0] == month_names[pos+0] &&
              tbuf[1] == month_names[pos+1] &&
              tbuf[2] == month_names[pos+2]) {
              result->fe_time.tm_mon = pos/3;
              result->fe_time.tm_mday = StringToInt(tokens[3]);
              result->fe_time.tm_year = StringToInt(tokens[4]) - 1900;
              break;
            }
          }
          pos = 5;  // Chameleon toknum of date field
        } else {
          result->fe_time.tm_mon = StringToInt(p+0)-1;
          result->fe_time.tm_mday = StringToInt(p+3);
          result->fe_time.tm_year = StringToInt(p+6);
          if (result->fe_time.tm_year < 80)  // SuperTCP
            result->fe_time.tm_year += 100;

          pos = 3;  // SuperTCP toknum of date field
        }

        result->fe_time.tm_hour = StringToInt(tokens[pos]);
        result->fe_time.tm_min = StringToInt(&(tokens[pos][toklen[pos]-2]));

        // the caller should do this (if dropping "." and ".." is desired)
        // if (result->fe_type == FTP_TYPE_DIRECTORY &&
        //     result->fe_fname[0] == '.' && (result->fe_fnlen == 1 ||
        //     (result->fe_fnlen == 2 && result->fe_fname[1] == '.')))
        //   return FTP_TYPE_JUNK;

        return result->fe_type;
      }  // (lstyle == 'w')
    }  // if (!lstyle && (!state->lstyle || state->lstyle == 'w'))
#endif  // SUPPORT_W16

#if defined(SUPPORT_DLS)  // dls -dtR
    if (!lstyle &&
        (state->lstyle == 'D' || (!state->lstyle && state->numlines == 1))) {
      // /bin/dls lines have to be immediately recognizable (first line)
      // I haven't seen an FTP server that delivers a /bin/dls listing,
      // but can infer the format from the lynx and mirror.pl projects.
      // Both formats are supported.
      //
      // Lynx says:
      // README 763 Information about this server\0
      // bin/ - \0
      // etc/ = \0
      // ls-lR 0 \0
      // ls-lR.Z 3 \0
      // pub/ = Public area\0
      // usr/ - \0
      // morgan 14 -> ../real/morgan\0
      // TIMIT.mostlikely.Z\0
      // 79215 \0
      //
      // mirror.pl says:
      // filename: ^(\S*)\s+
      // size: (\-|\=|\d+)\s+
      // month/day: ((\w\w\w\s+\d+|\d+\s+\w\w\w)\s+
      // time/year: (\d+:\d+|\d\d\d\d))\s+
      // rest: (.+)
      //
      // README 763 Jul 11 21:05 Information about this server
      // bin/ - Apr 28 1994
      // etc/ = 11 Jul 21:04
      // ls-lR 0 6 Aug 17:14
      // ls-lR.Z 3 05 Sep 1994
      // pub/ = Jul 11 21:04 Public area
      // usr/ - Sep 7 09:39
      // morgan 14 Apr 18 09:39 -> ../real/morgan
      // TIMIT.mostlikely.Z
      // 79215 Jul 11 21:04
      if (!state->lstyle && line[linelen-1] == ':' &&
        linelen >= 2 && toklen[numtoks-1] != 1) {
        // code in mirror.pl suggests that a listing may be preceded
        // by a PWD line in the form "/some/dir/names/here:"
        // but does not necessarily begin with '/'. *sigh*
        pos = 0;
        p = line;
        while (pos < (linelen-1)) {
          // illegal (or extremely unusual) chars in a dirspec
          if (*p == '<' || *p == '|' || *p == '>' ||
            *p == '?' || *p == '*' || *p == '\\')
            break;
          if (*p == '/' && pos < (linelen-2) && p[1] == '/')
            break;
          pos++;
          p++;
        }
        if (pos == (linelen-1)) {
          state->lstyle = 'D';
          return FTP_TYPE_JUNK;
        }
      }

      if (!lstyle && numtoks >= 2) {
        pos = 22;  // pos of (\d+|-|=) if this is not part of a multiline
        if (state->lstyle && carry_buf_len)  // first is from previous line
          pos = toklen[1]-1;  // and is 'as-is' (may contain whitespace)
        if (linelen > pos) {
          p = &line[pos];
          if ((*p == '-' || *p == '=' || isdigit(*p)) &&
            ((linelen == (pos+1)) ||
            (linelen >= (pos+3) && p[1] == ' ' && p[2] == ' '))) {
            tokmarker = 1;
            if (!carry_buf_len) {
              pos = 1;
              while (pos < numtoks && (tokens[pos]+toklen[pos]) < (&line[23]))
                pos++;
              tokmarker = 0;
              if ((tokens[pos]+toklen[pos]) == (&line[23]))
                tokmarker = pos;
            }
            if (tokmarker) {
              lstyle = 'D';
              if (*tokens[tokmarker] == '-' || *tokens[tokmarker] == '=') {
                if (toklen[tokmarker] != 1 ||
                  (tokens[tokmarker - 1][toklen[tokmarker - 1] - 1]) != '/')
                  lstyle = 0;
              } else {
                for (pos = 0; lstyle && pos < toklen[tokmarker]; pos++) {
                  if (!isdigit(tokens[tokmarker][pos]))
                    lstyle = 0;
                }
              }
              if (lstyle && !state->lstyle) {  // first time
                // scan for illegal (or incredibly unusual) chars in fname
                for (p = tokens[0]; lstyle &&
                  p < &(tokens[tokmarker - 1][toklen[tokmarker - 1]]); p++) {
                  if (*p == '<' || *p == '|' || *p == '>' ||
                    *p == '?' || *p == '*' || *p == '/' || *p == '\\')
                    lstyle = 0;
                }
              }
            }  // size token found
          }  // expected chars behind expected size token
        }  // if (linelen > pos)
      }  // if (!lstyle && numtoks >= 2)

      if (!lstyle && state->lstyle == 'D' && !carry_buf_len) {
        // the filename of a multi-line entry can be identified
        // correctly only if dls format had been previously established.
        // This should always be true because there should be entries
        // for '.' and/or '..' and/or CWD that precede the rest of the
        // listing.
        pos = linelen;
        if (pos > (sizeof(state->carry_buf) - 1))
          pos = sizeof(state->carry_buf) - 1;
        memcpy(state->carry_buf, line, pos);
        state->carry_buf_len = pos;
        return FTP_TYPE_JUNK;
      }

      if (lstyle == 'D') {
        state->parsed_one = 1;
        state->lstyle = lstyle;

        p = &(tokens[tokmarker-1][toklen[tokmarker - 1]]);
        result->fe_fname = tokens[0];
        result->fe_fnlen = p - tokens[0];
        result->fe_type = FTP_TYPE_FILE;

        if (result->fe_fname[result->fe_fnlen - 1] == '/') {
          if (result->fe_lnlen == 1) {
            result->fe_type = FTP_TYPE_JUNK;
          } else {
            result->fe_fnlen--;
            result->fe_type = FTP_TYPE_DIRECTORY;
          }
        } else if (isdigit(*tokens[tokmarker])) {
            pos = toklen[tokmarker];
            if (pos > (sizeof(result->fe_size) - 1))
              pos = sizeof(result->fe_size) - 1;
            memcpy(result->fe_size, tokens[tokmarker], pos);
            result->fe_size[pos] = '\0';
        }

        if ((tokmarker+3) < numtoks &&
          (&(tokens[numtoks - 1][toklen[numtoks-1]]) -
          tokens[tokmarker + 1]) >= (1 + 1 + 3 + 1 + 4)) {
            pos = (tokmarker + 3);
            p = tokens[pos];
            pos = toklen[pos];

            if ((pos == 4 || pos == 5) && isdigit(*p) &&
              isdigit(p[pos-1]) && isdigit(p[pos - 2])
              && ((pos == 5 && p[2] == ':') ||
              (pos == 4 && (isdigit(p[1]) || p[1] == ':')))) {
                month_num = tokmarker + 1;  // assumed position of month field
                pos = tokmarker + 2;  // assumed position of mday field
                if (isdigit(*tokens[month_num])) {  // positions are reversed
                  month_num++;
                  pos--;
                }
                p = tokens[month_num];
                if (isdigit(*tokens[pos])
                  && (toklen[pos] == 1 ||
                  (toklen[pos] == 2 && isdigit(tokens[pos][1])))
                  && toklen[month_num] == 3
                  && isalpha(*p) && isalpha(p[1]) && isalpha(p[2])) {
                  pos = StringToInt(tokens[pos]);
                  if (pos > 0 && pos <= 31) {
                    result->fe_time.tm_mday = pos;
                    month_num = 1;
                    for (pos = 0; pos < (12*3); pos += 3) {
                      if (p[0] == month_names[pos + 0] &&
                        p[1] == month_names[pos + 1] &&
                        p[2] == month_names[pos + 2])
                        break;
                      month_num++;
                    }
                    if (month_num > 12)
                      result->fe_time.tm_mday = 0;
                    else
                      result->fe_time.tm_mon = month_num - 1;
                  }
                }
                if (result->fe_time.tm_mday) {
                  tokmarker += 3;  // skip mday/mon/yrtime (to find " -> ")
                  p = tokens[tokmarker];

                  pos = StringToInt(p);
                  if (pos > 24) {
                    result->fe_time.tm_year = pos - 1900;
                  } else {
                    if (p[1] == ':')
                      p--;
                    result->fe_time.tm_hour = pos;
                    result->fe_time.tm_min = StringToInt(p + 3);
                    if (!state->now_time) {
                      time_t now = time(NULL);
                      state->now_time = now * 1000000;
                      gmtime_r(&now, &state->now_tm);
                    }
                    result->fe_time.tm_year = state->now_tm.tm_year;
                    if ((( state->now_tm.tm_mon << 4) +
                        state->now_tm.tm_mday) <
                        ((result->fe_time.tm_mon << 4) +
                        result->fe_time.tm_mday))
                      result->fe_time.tm_year--;
                  }  // got year or time
                }  // got month/mday
            }  // may have year or time
        }  // enough remaining to possibly have date/time

        if (numtoks > (tokmarker+2)) {
          pos = tokmarker+1;
          p = tokens[pos];
          if (toklen[pos] == 2 && *p == '-' && p[1] == '>') {
            p = &(tokens[numtoks - 1][toklen[numtoks - 1]]);
            result->fe_type = FTP_TYPE_SYMLINK;
            result->fe_lname = tokens[pos + 1];
            result->fe_lnlen = p - result->fe_lname;
            if (result->fe_lnlen > 1 &&
              result->fe_lname[result->fe_lnlen -1] == '/')
              result->fe_lnlen--;
          }
        }  // if (numtoks > (tokmarker+2))

        // The caller should do this (if dropping "." and ".." is desired)
        // if (result->fe_type == FTP_TYPE_DIRECTORY &&
        //     result->fe_fname[0] == '.' && (result->fe_fnlen == 1 ||
        //     (result->fe_fnlen == 2 && result->fe_fname[1] == '.')))
        //   return FTP_TYPE_JUNK;
        return result->fe_type;
      }  // if (lstyle == 'D')
    }  // if (!lstyle && (!state->lstyle || state->lstyle == 'D'))
#endif
  }  // if (linelen > 0)

  if (state->parsed_one || state->lstyle)  // junk if we fail to parse
    return FTP_TYPE_JUNK;  // this time but had previously parsed successfully
  return FTP_TYPE_COMMENT;  // its part of a comment or error message
}

}  // namespace net
