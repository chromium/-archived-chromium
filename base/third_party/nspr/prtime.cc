/*
* Portions are Copyright (C) 2007 Google Inc
*
* ***** BEGIN LICENSE BLOCK *****
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
* The Original Code is the Netscape Portable Runtime (NSPR).
*
* The Initial Developer of the Original Code is
* Netscape Communications Corporation.
* Portions created by the Initial Developer are Copyright (C) 1998-2000
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
* ***** END LICENSE BLOCK *****
*
*/

/*
 * prtime.cc --
 * NOTE: The original nspr file name is prtime.c
 *
 *     NSPR date and time functions
 *
 * CVS revision 3.31
 */

/*
* The following functions were copied from the NSPR prtime.c file.
* PR_ParseTimeString
* PR_ImplodeTime
* This was modified to use the Win32 SYSTEMTIME/FILETIME structures
* and the timezone offsets are applied to the FILETIME structure.
* All types and defines have been defined in the base/third_party/prtime.h file
* These have been copied from the following nspr files. We have only copied over
* the types we need.
* 1. prtime.h
* 2. prtypes.h
* 3. prlong.h
*/

#include <windows.h>
#include <time.h>
#include "base/third_party/nspr/prtime.h"

/*
 *------------------------------------------------------------------------
 *
 * PR_ImplodeTime --
 *
 *     Cf. time_t mktime(struct tm *tp)
 *     Note that 1 year has < 2^25 seconds.  So an PRInt32 is large enough.
 *
 *------------------------------------------------------------------------
 */
PRTime
PR_ImplodeTime(const PRExplodedTime *exploded)
{
   // Create the system struct representing our exploded time.
    SYSTEMTIME st = {0};
    FILETIME ft = {0};
    ULARGE_INTEGER uli = {0};

    st.wYear = exploded->tm_year;
    st.wMonth = exploded->tm_month + 1;
    st.wDayOfWeek = exploded->tm_wday;
    st.wDay = exploded->tm_mday;
    st.wHour = exploded->tm_hour;
    st.wMinute = exploded->tm_min;
    st.wSecond = exploded->tm_sec;
    st.wMilliseconds = exploded->tm_usec/1000;
     // Convert to FILETIME.
    if (!SystemTimeToFileTime(&st, &ft)) {
      NOTREACHED() << "Unable to convert time";
      return 0;
    }
    // Apply offsets.
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    // From second to 100-ns
    uli.QuadPart -=
        (exploded->tm_params.tp_gmt_offset +
         exploded->tm_params.tp_dst_offset) * 10000000i64;  // 7 zeros
    // Convert to PRTime
    uli.QuadPart -= 116444736000000000i64; // from Windows epoch to NSPR epoch
    uli.QuadPart /= 10;  // from 100-nanosecond to microsecond
    return (PRTime)uli.QuadPart;
}

/*
 * The following code implements PR_ParseTimeString().  It is based on
 * ns/lib/xp/xp_time.c, revision 1.25, by Jamie Zawinski <jwz@netscape.com>.
 */

/*
 * We only recognize the abbreviations of a small subset of time zones
 * in North America, Europe, and Japan.
 *
 * PST/PDT: Pacific Standard/Daylight Time
 * MST/MDT: Mountain Standard/Daylight Time
 * CST/CDT: Central Standard/Daylight Time
 * EST/EDT: Eastern Standard/Daylight Time
 * AST: Atlantic Standard Time
 * NST: Newfoundland Standard Time
 * GMT: Greenwich Mean Time
 * BST: British Summer Time
 * MET: Middle Europe Time
 * EET: Eastern Europe Time
 * JST: Japan Standard Time
 */

typedef enum
{
  TT_UNKNOWN,

  TT_SUN, TT_MON, TT_TUE, TT_WED, TT_THU, TT_FRI, TT_SAT,

  TT_JAN, TT_FEB, TT_MAR, TT_APR, TT_MAY, TT_JUN,
  TT_JUL, TT_AUG, TT_SEP, TT_OCT, TT_NOV, TT_DEC,

  TT_PST, TT_PDT, TT_MST, TT_MDT, TT_CST, TT_CDT, TT_EST, TT_EDT,
  TT_AST, TT_NST, TT_GMT, TT_BST, TT_MET, TT_EET, TT_JST
} TIME_TOKEN;

/*
 * This parses a time/date string into a PRTime
 * (microseconds after "1-Jan-1970 00:00:00 GMT").
 * It returns PR_SUCCESS on success, and PR_FAILURE
 * if the time/date string can't be parsed.
 *
 * Many formats are handled, including:
 *
 *   14 Apr 89 03:20:12
 *   14 Apr 89 03:20 GMT
 *   Fri, 17 Mar 89 4:01:33
 *   Fri, 17 Mar 89 4:01 GMT
 *   Mon Jan 16 16:12 PDT 1989
 *   Mon Jan 16 16:12 +0130 1989
 *   6 May 1992 16:41-JST (Wednesday)
 *   22-AUG-1993 10:59:12.82
 *   22-AUG-1993 10:59pm
 *   22-AUG-1993 12:59am
 *   22-AUG-1993 12:59 PM
 *   Friday, August 04, 1995 3:54 PM
 *   06/21/95 04:24:34 PM
 *   20/06/95 21:07
 *   95-06-08 19:32:48 EDT
 *
 * If the input string doesn't contain a description of the timezone,
 * we consult the `default_to_gmt' to decide whether the string should
 * be interpreted relative to the local time zone (PR_FALSE) or GMT (PR_TRUE).
 * The correct value for this argument depends on what standard specified
 * the time string which you are parsing.
 */

PRStatus
PR_ParseTimeString(
        const char *string,
        PRBool default_to_gmt,
        PRTime *result)
{
  PRExplodedTime tm;
  TIME_TOKEN dotw = TT_UNKNOWN;
  TIME_TOKEN month = TT_UNKNOWN;
  TIME_TOKEN zone = TT_UNKNOWN;
  int zone_offset = -1;
  int date = -1;
  PRInt32 year = -1;
  int hour = -1;
  int min = -1;
  int sec = -1;

  const char *rest = string;

#ifdef DEBUG
  int iterations = 0;
#endif

  PR_ASSERT(string && result);
  if (!string || !result) return PR_FAILURE;

  while (*rest)
        {

#ifdef DEBUG
          if (iterations++ > 1000)
                {
                  PR_ASSERT(0);
                  return PR_FAILURE;
                }
#endif

          switch (*rest)
                {
                case 'a': case 'A':
                  if (month == TT_UNKNOWN &&
                          (rest[1] == 'p' || rest[1] == 'P') &&
                          (rest[2] == 'r' || rest[2] == 'R'))
                        month = TT_APR;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 's' || rest[1] == 's') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_AST;
                  else if (month == TT_UNKNOWN &&
                                   (rest[1] == 'u' || rest[1] == 'U') &&
                                   (rest[2] == 'g' || rest[2] == 'G'))
                        month = TT_AUG;
                  break;
                case 'b': case 'B':
                  if (zone == TT_UNKNOWN &&
                          (rest[1] == 's' || rest[1] == 'S') &&
                          (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_BST;
                  break;
                case 'c': case 'C':
                  if (zone == TT_UNKNOWN &&
                          (rest[1] == 'd' || rest[1] == 'D') &&
                          (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_CDT;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 's' || rest[1] == 'S') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_CST;
                  break;
                case 'd': case 'D':
                  if (month == TT_UNKNOWN &&
                          (rest[1] == 'e' || rest[1] == 'E') &&
                          (rest[2] == 'c' || rest[2] == 'C'))
                        month = TT_DEC;
                  break;
                case 'e': case 'E':
                  if (zone == TT_UNKNOWN &&
                          (rest[1] == 'd' || rest[1] == 'D') &&
                          (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_EDT;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 'e' || rest[1] == 'E') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_EET;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 's' || rest[1] == 'S') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_EST;
                  break;
                case 'f': case 'F':
                  if (month == TT_UNKNOWN &&
                          (rest[1] == 'e' || rest[1] == 'E') &&
                          (rest[2] == 'b' || rest[2] == 'B'))
                        month = TT_FEB;
                  else if (dotw == TT_UNKNOWN &&
                                   (rest[1] == 'r' || rest[1] == 'R') &&
                                   (rest[2] == 'i' || rest[2] == 'I'))
                        dotw = TT_FRI;
                  break;
                case 'g': case 'G':
                  if (zone == TT_UNKNOWN &&
                          (rest[1] == 'm' || rest[1] == 'M') &&
                          (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_GMT;
                  break;
                case 'j': case 'J':
                  if (month == TT_UNKNOWN &&
                          (rest[1] == 'a' || rest[1] == 'A') &&
                          (rest[2] == 'n' || rest[2] == 'N'))
                        month = TT_JAN;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 's' || rest[1] == 'S') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_JST;
                  else if (month == TT_UNKNOWN &&
                                   (rest[1] == 'u' || rest[1] == 'U') &&
                                   (rest[2] == 'l' || rest[2] == 'L'))
                        month = TT_JUL;
                  else if (month == TT_UNKNOWN &&
                                   (rest[1] == 'u' || rest[1] == 'U') &&
                                   (rest[2] == 'n' || rest[2] == 'N'))
                        month = TT_JUN;
                  break;
                case 'm': case 'M':
                  if (month == TT_UNKNOWN &&
                          (rest[1] == 'a' || rest[1] == 'A') &&
                          (rest[2] == 'r' || rest[2] == 'R'))
                        month = TT_MAR;
                  else if (month == TT_UNKNOWN &&
                                   (rest[1] == 'a' || rest[1] == 'A') &&
                                   (rest[2] == 'y' || rest[2] == 'Y'))
                        month = TT_MAY;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 'd' || rest[1] == 'D') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_MDT;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 'e' || rest[1] == 'E') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_MET;
                  else if (dotw == TT_UNKNOWN &&
                                   (rest[1] == 'o' || rest[1] == 'O') &&
                                   (rest[2] == 'n' || rest[2] == 'N'))
                        dotw = TT_MON;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 's' || rest[1] == 'S') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_MST;
                  break;
                case 'n': case 'N':
                  if (month == TT_UNKNOWN &&
                          (rest[1] == 'o' || rest[1] == 'O') &&
                          (rest[2] == 'v' || rest[2] == 'V'))
                        month = TT_NOV;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 's' || rest[1] == 'S') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_NST;
                  break;
                case 'o': case 'O':
                  if (month == TT_UNKNOWN &&
                          (rest[1] == 'c' || rest[1] == 'C') &&
                          (rest[2] == 't' || rest[2] == 'T'))
                        month = TT_OCT;
                  break;
                case 'p': case 'P':
                  if (zone == TT_UNKNOWN &&
                          (rest[1] == 'd' || rest[1] == 'D') &&
                          (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_PDT;
                  else if (zone == TT_UNKNOWN &&
                                   (rest[1] == 's' || rest[1] == 'S') &&
                                   (rest[2] == 't' || rest[2] == 'T'))
                        zone = TT_PST;
                  break;
                case 's': case 'S':
                  if (dotw == TT_UNKNOWN &&
                          (rest[1] == 'a' || rest[1] == 'A') &&
                          (rest[2] == 't' || rest[2] == 'T'))
                        dotw = TT_SAT;
                  else if (month == TT_UNKNOWN &&
                                   (rest[1] == 'e' || rest[1] == 'E') &&
                                   (rest[2] == 'p' || rest[2] == 'P'))
                        month = TT_SEP;
                  else if (dotw == TT_UNKNOWN &&
                                   (rest[1] == 'u' || rest[1] == 'U') &&
                                   (rest[2] == 'n' || rest[2] == 'N'))
                        dotw = TT_SUN;
                  break;
                case 't': case 'T':
                  if (dotw == TT_UNKNOWN &&
                          (rest[1] == 'h' || rest[1] == 'H') &&
                          (rest[2] == 'u' || rest[2] == 'U'))
                        dotw = TT_THU;
                  else if (dotw == TT_UNKNOWN &&
                                   (rest[1] == 'u' || rest[1] == 'U') &&
                                   (rest[2] == 'e' || rest[2] == 'E'))
                        dotw = TT_TUE;
                  break;
                case 'u': case 'U':
                  if (zone == TT_UNKNOWN &&
                          (rest[1] == 't' || rest[1] == 'T') &&
                          !(rest[2] >= 'A' && rest[2] <= 'Z') &&
                          !(rest[2] >= 'a' && rest[2] <= 'z'))
                        /* UT is the same as GMT but UTx is not. */
                        zone = TT_GMT;
                  break;
                case 'w': case 'W':
                  if (dotw == TT_UNKNOWN &&
                          (rest[1] == 'e' || rest[1] == 'E') &&
                          (rest[2] == 'd' || rest[2] == 'D'))
                        dotw = TT_WED;
                  break;

                case '+': case '-':
                  {
                        const char *end;
                        int sign;
                        if (zone_offset != -1)
                          {
                                /* already got one... */
                                rest++;
                                break;
                          }
                        if (zone != TT_UNKNOWN && zone != TT_GMT)
                          {
                                /* GMT+0300 is legal, but PST+0300 is not. */
                                rest++;
                                break;
                          }

                        sign = ((*rest == '+') ? 1 : -1);
                        rest++; /* move over sign */
                        end = rest;
                        while (*end >= '0' && *end <= '9')
                          end++;
                        if (rest == end) /* no digits here */
                          break;

                        if ((end - rest) == 4)
                          /* offset in HHMM */
                          zone_offset = (((((rest[0]-'0')*10) + (rest[1]-'0')) * 60) +
                                                         (((rest[2]-'0')*10) + (rest[3]-'0')));
                        else if ((end - rest) == 2)
                          /* offset in hours */
                          zone_offset = (((rest[0]-'0')*10) + (rest[1]-'0')) * 60;
                        else if ((end - rest) == 1)
                          /* offset in hours */
                          zone_offset = (rest[0]-'0') * 60;
                        else
                          /* 3 or >4 */
                          break;

                        zone_offset *= sign;
                        zone = TT_GMT;
                        break;
                  }

                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                  {
                        int tmp_hour = -1;
                        int tmp_min = -1;
                        int tmp_sec = -1;
                        const char *end = rest + 1;
                        while (*end >= '0' && *end <= '9')
                          end++;

                        /* end is now the first character after a range of digits. */

                        if (*end == ':')
                          {
                                if (hour >= 0 && min >= 0) /* already got it */
                                  break;

                                /* We have seen "[0-9]+:", so this is probably HH:MM[:SS] */
                                if ((end - rest) > 2)
                                  /* it is [0-9][0-9][0-9]+: */
                                  break;
                                else if ((end - rest) == 2)
                                  tmp_hour = ((rest[0]-'0')*10 +
                                                          (rest[1]-'0'));
                                else
                                  tmp_hour = (rest[0]-'0');

                                /* move over the colon, and parse minutes */

                                rest = ++end;
                                while (*end >= '0' && *end <= '9')
                                  end++;

                                if (end == rest)
                                  /* no digits after first colon? */
                                  break;
                                else if ((end - rest) > 2)
                                  /* it is [0-9][0-9][0-9]+: */
                                  break;
                                else if ((end - rest) == 2)
                                  tmp_min = ((rest[0]-'0')*10 +
                                                         (rest[1]-'0'));
                                else
                                  tmp_min = (rest[0]-'0');

                                /* now go for seconds */
                                rest = end;
                                if (*rest == ':')
                                  rest++;
                                end = rest;
                                while (*end >= '0' && *end <= '9')
                                  end++;

                                if (end == rest)
                                  /* no digits after second colon - that's ok. */
                                  ;
                                else if ((end - rest) > 2)
                                  /* it is [0-9][0-9][0-9]+: */
                                  break;
                                else if ((end - rest) == 2)
                                  tmp_sec = ((rest[0]-'0')*10 +
                                                         (rest[1]-'0'));
                                else
                                  tmp_sec = (rest[0]-'0');

                                /* If we made it here, we've parsed hour and min,
                                   and possibly sec, so it worked as a unit. */

                                /* skip over whitespace and see if there's an AM or PM
                                   directly following the time.
                                 */
                                if (tmp_hour <= 12)
                                  {
                                        const char *s = end;
                                        while (*s && (*s == ' ' || *s == '\t'))
                                          s++;
                                        if ((s[0] == 'p' || s[0] == 'P') &&
                                                (s[1] == 'm' || s[1] == 'M'))
                                          /* 10:05pm == 22:05, and 12:05pm == 12:05 */
                                          tmp_hour = (tmp_hour == 12 ? 12 : tmp_hour + 12);
                                        else if (tmp_hour == 12 &&
                                                         (s[0] == 'a' || s[0] == 'A') &&
                                                         (s[1] == 'm' || s[1] == 'M'))
                                          /* 12:05am == 00:05 */
                                          tmp_hour = 0;
                                  }

                                hour = tmp_hour;
                                min = tmp_min;
                                sec = tmp_sec;
                                rest = end;
                                break;
                          }
                        else if ((*end == '/' || *end == '-') &&
                                         end[1] >= '0' && end[1] <= '9')
                          {
                                /* Perhaps this is 6/16/95, 16/6/95, 6-16-95, or 16-6-95
                                   or even 95-06-05...
                                   #### But it doesn't handle 1995-06-22.
                                 */
                                int n1, n2, n3;
                                const char *s;

                                if (month != TT_UNKNOWN)
                                  /* if we saw a month name, this can't be. */
                                  break;

                                s = rest;

                                n1 = (*s++ - '0');                                /* first 1 or 2 digits */
                                if (*s >= '0' && *s <= '9')
                                  n1 = n1*10 + (*s++ - '0');

                                if (*s != '/' && *s != '-')                /* slash */
                                  break;
                                s++;

                                if (*s < '0' || *s > '9')                /* second 1 or 2 digits */
                                  break;
                                n2 = (*s++ - '0');
                                if (*s >= '0' && *s <= '9')
                                  n2 = n2*10 + (*s++ - '0');

                                if (*s != '/' && *s != '-')                /* slash */
                                  break;
                                s++;

                                if (*s < '0' || *s > '9')                /* third 1, 2, or 4 digits */
                                  break;
                                n3 = (*s++ - '0');
                                if (*s >= '0' && *s <= '9')
                                  n3 = n3*10 + (*s++ - '0');

                                if (*s >= '0' && *s <= '9')                /* optional digits 3 and 4 */
                                  {
                                        n3 = n3*10 + (*s++ - '0');
                                        if (*s < '0' || *s > '9')
                                          break;
                                        n3 = n3*10 + (*s++ - '0');
                                  }

                                if ((*s >= '0' && *s <= '9') ||        /* followed by non-alphanum */
                                        (*s >= 'A' && *s <= 'Z') ||
                                        (*s >= 'a' && *s <= 'z'))
                                  break;

                                /* Ok, we parsed three 1-2 digit numbers, with / or -
                                   between them.  Now decide what the hell they are
                                   (DD/MM/YY or MM/DD/YY or YY/MM/DD.)
                                 */

                                if (n1 > 31 || n1 == 0)  /* must be YY/MM/DD */
                                  {
                                        if (n2 > 12) break;
                                        if (n3 > 31) break;
                                        year = n1;
                                        if (year < 70)
                                            year += 2000;
                                        else if (year < 100)
                                            year += 1900;
                                        month = (TIME_TOKEN)(n2 + ((int)TT_JAN) - 1);
                                        date = n3;
                                        rest = s;
                                        break;
                                  }

                                if (n1 > 12 && n2 > 12)  /* illegal */
                                  {
                                        rest = s;
                                        break;
                                  }

                                if (n3 < 70)
                                    n3 += 2000;
                                else if (n3 < 100)
                                    n3 += 1900;

                                if (n1 > 12)  /* must be DD/MM/YY */
                                  {
                                        date = n1;
                                        month = (TIME_TOKEN)(n2 + ((int)TT_JAN) - 1);
                                        year = n3;
                                  }
                                else                  /* assume MM/DD/YY */
                                  {
                                        /* #### In the ambiguous case, should we consult the
                                           locale to find out the local default? */
                                        month = (TIME_TOKEN)(n1 + ((int)TT_JAN) - 1);
                                        date = n2;
                                        year = n3;
                                  }
                                rest = s;
                          }
                        else if ((*end >= 'A' && *end <= 'Z') ||
                                         (*end >= 'a' && *end <= 'z'))
                          /* Digits followed by non-punctuation - what's that? */
                          ;
                        else if ((end - rest) == 4)                /* four digits is a year */
                          year = (year < 0
                                          ? ((rest[0]-'0')*1000L +
                                                 (rest[1]-'0')*100L +
                                                 (rest[2]-'0')*10L +
                                                 (rest[3]-'0'))
                                          : year);
                        else if ((end - rest) == 2)                /* two digits - date or year */
                          {
                                int n = ((rest[0]-'0')*10 +
                                                 (rest[1]-'0'));
                                /* If we don't have a date (day of the month) and we see a number
                                     less than 32, then assume that is the date.

                                         Otherwise, if we have a date and not a year, assume this is the
                                         year.  If it is less than 70, then assume it refers to the 21st
                                         century.  If it is two digits (>= 70), assume it refers to this
                                         century.  Otherwise, assume it refers to an unambiguous year.

                                         The world will surely end soon.
                                   */
                                if (date < 0 && n < 32)
                                  date = n;
                                else if (year < 0)
                                  {
                                        if (n < 70)
                                          year = 2000 + n;
                                        else if (n < 100)
                                          year = 1900 + n;
                                        else
                                          year = n;
                                  }
                                /* else what the hell is this. */
                          }
                        else if ((end - rest) == 1)                /* one digit - date */
                          date = (date < 0 ? (rest[0]-'0') : date);
                        /* else, three or more than four digits - what's that? */

                        break;
                  }
                }

          /* Skip to the end of this token, whether we parsed it or not.
                 Tokens are delimited by whitespace, or ,;-/
                 But explicitly not :+-.
           */
          while (*rest &&
                         *rest != ' ' && *rest != '\t' &&
                         *rest != ',' && *rest != ';' &&
                         *rest != '-' && *rest != '+' &&
                         *rest != '/' &&
                         *rest != '(' && *rest != ')' && *rest != '[' && *rest != ']')
                rest++;
          /* skip over uninteresting chars. */
        SKIP_MORE:
          while (*rest &&
                         (*rest == ' ' || *rest == '\t' ||
                          *rest == ',' || *rest == ';' || *rest == '/' ||
                          *rest == '(' || *rest == ')' || *rest == '[' || *rest == ']'))
                rest++;

          /* "-" is ignored at the beginning of a token if we have not yet
                 parsed a year (e.g., the second "-" in "30-AUG-1966"), or if
                 the character after the dash is not a digit. */
          if (*rest == '-' && ((rest > string && isalpha(rest[-1]) && year < 0)
              || rest[1] < '0' || rest[1] > '9'))
                {
                  rest++;
                  goto SKIP_MORE;
                }

        }

  if (zone != TT_UNKNOWN && zone_offset == -1)
        {
          switch (zone)
                {
                case TT_PST: zone_offset = -8 * 60; break;
                case TT_PDT: zone_offset = -7 * 60; break;
                case TT_MST: zone_offset = -7 * 60; break;
                case TT_MDT: zone_offset = -6 * 60; break;
                case TT_CST: zone_offset = -6 * 60; break;
                case TT_CDT: zone_offset = -5 * 60; break;
                case TT_EST: zone_offset = -5 * 60; break;
                case TT_EDT: zone_offset = -4 * 60; break;
                case TT_AST: zone_offset = -4 * 60; break;
                case TT_NST: zone_offset = -3 * 60 - 30; break;
                case TT_GMT: zone_offset =  0 * 60; break;
                case TT_BST: zone_offset =  1 * 60; break;
                case TT_MET: zone_offset =  1 * 60; break;
                case TT_EET: zone_offset =  2 * 60; break;
                case TT_JST: zone_offset =  9 * 60; break;
                default:
                  PR_ASSERT (0);
                  break;
                }
        }

  /* If we didn't find a year, month, or day-of-the-month, we can't
         possibly parse this, and in fact, mktime() will do something random
         (I'm seeing it return "Tue Feb  5 06:28:16 2036", which is no doubt
         a numerologically significant date... */
  if (month == TT_UNKNOWN || date == -1 || year == -1)
      return PR_FAILURE;

  memset(&tm, 0, sizeof(tm));
  if (sec != -1)
        tm.tm_sec = sec;
  if (min != -1)
  tm.tm_min = min;
  if (hour != -1)
        tm.tm_hour = hour;
  if (date != -1)
        tm.tm_mday = date;
  if (month != TT_UNKNOWN)
        tm.tm_month = (((int)month) - ((int)TT_JAN));
  if (year != -1)
        tm.tm_year = year;
  if (dotw != TT_UNKNOWN)
        tm.tm_wday = (((int)dotw) - ((int)TT_SUN));

  if (zone == TT_UNKNOWN && default_to_gmt)
        {
          /* No zone was specified, so pretend the zone was GMT. */
          zone = TT_GMT;
          zone_offset = 0;
        }

  if (zone_offset == -1)
         {
           /* no zone was specified, and we're to assume that everything
             is local. */
          struct tm localTime;
          time_t secs;

          PR_ASSERT(tm.tm_month > -1
                                   && tm.tm_mday > 0
                                   && tm.tm_hour > -1
                                   && tm.tm_min > -1
                                   && tm.tm_sec > -1);

            /*
             * To obtain time_t from a tm structure representing the local
             * time, we call mktime().  However, we need to see if we are
             * on 1-Jan-1970 or before.  If we are, we can't call mktime()
             * because mktime() will crash on win16. In that case, we
             * calculate zone_offset based on the zone offset at
             * 00:00:00, 2 Jan 1970 GMT, and subtract zone_offset from the
             * date we are parsing to transform the date to GMT.  We also
             * do so if mktime() returns (time_t) -1 (time out of range).
           */

          /* month, day, hours, mins and secs are always non-negative
             so we dont need to worry about them. */
            if(tm.tm_year >= 1970)
                {
                  PRInt64 usec_per_sec;

                  localTime.tm_sec = tm.tm_sec;
                  localTime.tm_min = tm.tm_min;
                  localTime.tm_hour = tm.tm_hour;
                  localTime.tm_mday = tm.tm_mday;
                  localTime.tm_mon = tm.tm_month;
                  localTime.tm_year = tm.tm_year - 1900;
                  /* Set this to -1 to tell mktime "I don't care".  If you set
                     it to 0 or 1, you are making assertions about whether the
                     date you are handing it is in daylight savings mode or not;
                     and if you're wrong, it will "fix" it for you. */
                  localTime.tm_isdst = -1;
                  secs = mktime(&localTime);
                  if (secs != (time_t) -1)
                    {
#if defined(XP_MAC) && (__MSL__ < 0x6000)
                      /*
                       * The mktime() routine in MetroWerks MSL C
                       * Runtime library returns seconds since midnight,
                       * 1 Jan. 1900, not 1970 - in versions of MSL (Metrowerks Standard
                       * Library) prior to version 6.  Only for older versions of
                       * MSL do we adjust the value of secs to the NSPR epoch
                       */
                      secs -= ((365 * 70UL) + 17) * 24 * 60 * 60;
#endif
                      LL_I2L(*result, secs);
                      LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
                      LL_MUL(*result, *result, usec_per_sec);
                      return PR_SUCCESS;
                    }
                }

                /* So mktime() can't handle this case.  We assume the
                   zone_offset for the date we are parsing is the same as
                   the zone offset on 00:00:00 2 Jan 1970 GMT. */
                secs = 86400;
                (void) localtime_s(&localTime, &secs);
                zone_offset = localTime.tm_min
                              + 60 * localTime.tm_hour
                              + 1440 * (localTime.tm_mday - 2);
        }

        tm.tm_params.tp_gmt_offset = zone_offset * 60;

  *result = PR_ImplodeTime(&tm);

  return PR_SUCCESS;
}