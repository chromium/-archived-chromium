/*
** 2003 October 31
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains the C functions that implement date and time
** functions for SQLite.  
**
** There is only one exported symbol in this file - the function
** sqlite3RegisterDateTimeFunctions() found at the bottom of the file.
** All other code has file scope.
**
** $Id: date.c,v 1.87 2008/07/28 19:34:53 drh Exp $
**
** SQLite processes all times and dates as Julian Day numbers.  The
** dates and times are stored as the number of days since noon
** in Greenwich on November 24, 4714 B.C. according to the Gregorian
** calendar system. 
**
** 1970-01-01 00:00:00 is JD 2440587.5
** 2000-01-01 00:00:00 is JD 2451544.5
**
** This implemention requires years to be expressed as a 4-digit number
** which means that only dates between 0000-01-01 and 9999-12-31 can
** be represented, even though julian day numbers allow a much wider
** range of dates.
**
** The Gregorian calendar system is used for all dates and times,
** even those that predate the Gregorian calendar.  Historians usually
** use the Julian calendar for dates prior to 1582-10-15 and for some
** dates afterwards, depending on locale.  Beware of this difference.
**
** The conversion algorithms are implemented based on descriptions
** in the following text:
**
**      Jean Meeus
**      Astronomical Algorithms, 2nd Edition, 1998
**      ISBM 0-943396-61-1
**      Willmann-Bell, Inc
**      Richmond, Virginia (USA)
*/
#include "sqliteInt.h"
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#ifndef SQLITE_OMIT_DATETIME_FUNCS

/*
** On recent Windows platforms, the localtime_s() function is available
** as part of the "Secure CRT". It is essentially equivalent to 
** localtime_r() available under most POSIX platforms, except that the 
** order of the parameters is reversed.
**
** See http://msdn.microsoft.com/en-us/library/a442x3ye(VS.80).aspx.
**
** If the user has not indicated to use localtime_r() or localtime_s()
** already, check for an MSVC build environment that provides 
** localtime_s().
*/
#if !defined(HAVE_LOCALTIME_R) && !defined(HAVE_LOCALTIME_S) && \
     defined(_MSC_VER) && defined(_CRT_INSECURE_DEPRECATE)
#define HAVE_LOCALTIME_S 1
#endif

/*
** A structure for holding a single date and time.
*/
typedef struct DateTime DateTime;
struct DateTime {
  sqlite3_int64 iJD; /* The julian day number times 86400000 */
  int Y, M, D;       /* Year, month, and day */
  int h, m;          /* Hour and minutes */
  int tz;            /* Timezone offset in minutes */
  double s;          /* Seconds */
  char validYMD;     /* True if Y,M,D are valid */
  char validHMS;     /* True if h,m,s are valid */
  char validJD;      /* True if iJD is valid */
  char validTZ;      /* True if tz is valid */
};


/*
** Convert zDate into one or more integers.  Additional arguments
** come in groups of 5 as follows:
**
**       N       number of digits in the integer
**       min     minimum allowed value of the integer
**       max     maximum allowed value of the integer
**       nextC   first character after the integer
**       pVal    where to write the integers value.
**
** Conversions continue until one with nextC==0 is encountered.
** The function returns the number of successful conversions.
*/
static int getDigits(const char *zDate, ...){
  va_list ap;
  int val;
  int N;
  int min;
  int max;
  int nextC;
  int *pVal;
  int cnt = 0;
  va_start(ap, zDate);
  do{
    N = va_arg(ap, int);
    min = va_arg(ap, int);
    max = va_arg(ap, int);
    nextC = va_arg(ap, int);
    pVal = va_arg(ap, int*);
    val = 0;
    while( N-- ){
      if( !isdigit(*(u8*)zDate) ){
        goto end_getDigits;
      }
      val = val*10 + *zDate - '0';
      zDate++;
    }
    if( val<min || val>max || (nextC!=0 && nextC!=*zDate) ){
      goto end_getDigits;
    }
    *pVal = val;
    zDate++;
    cnt++;
  }while( nextC );
end_getDigits:
  va_end(ap);
  return cnt;
}

/*
** Read text from z[] and convert into a floating point number.  Return
** the number of digits converted.
*/
#define getValue sqlite3AtoF

/*
** Parse a timezone extension on the end of a date-time.
** The extension is of the form:
**
**        (+/-)HH:MM
**
** Or the "zulu" notation:
**
**        Z
**
** If the parse is successful, write the number of minutes
** of change in p->tz and return 0.  If a parser error occurs,
** return non-zero.
**
** A missing specifier is not considered an error.
*/
static int parseTimezone(const char *zDate, DateTime *p){
  int sgn = 0;
  int nHr, nMn;
  int c;
  while( isspace(*(u8*)zDate) ){ zDate++; }
  p->tz = 0;
  c = *zDate;
  if( c=='-' ){
    sgn = -1;
  }else if( c=='+' ){
    sgn = +1;
  }else if( c=='Z' || c=='z' ){
    zDate++;
    goto zulu_time;
  }else{
    return c!=0;
  }
  zDate++;
  if( getDigits(zDate, 2, 0, 14, ':', &nHr, 2, 0, 59, 0, &nMn)!=2 ){
    return 1;
  }
  zDate += 5;
  p->tz = sgn*(nMn + nHr*60);
zulu_time:
  while( isspace(*(u8*)zDate) ){ zDate++; }
  return *zDate!=0;
}

/*
** Parse times of the form HH:MM or HH:MM:SS or HH:MM:SS.FFFF.
** The HH, MM, and SS must each be exactly 2 digits.  The
** fractional seconds FFFF can be one or more digits.
**
** Return 1 if there is a parsing error and 0 on success.
*/
static int parseHhMmSs(const char *zDate, DateTime *p){
  int h, m, s;
  double ms = 0.0;
  if( getDigits(zDate, 2, 0, 24, ':', &h, 2, 0, 59, 0, &m)!=2 ){
    return 1;
  }
  zDate += 5;
  if( *zDate==':' ){
    zDate++;
    if( getDigits(zDate, 2, 0, 59, 0, &s)!=1 ){
      return 1;
    }
    zDate += 2;
    if( *zDate=='.' && isdigit((u8)zDate[1]) ){
      double rScale = 1.0;
      zDate++;
      while( isdigit(*(u8*)zDate) ){
        ms = ms*10.0 + *zDate - '0';
        rScale *= 10.0;
        zDate++;
      }
      ms /= rScale;
    }
  }else{
    s = 0;
  }
  p->validJD = 0;
  p->validHMS = 1;
  p->h = h;
  p->m = m;
  p->s = s + ms;
  if( parseTimezone(zDate, p) ) return 1;
  p->validTZ = p->tz!=0;
  return 0;
}

/*
** Convert from YYYY-MM-DD HH:MM:SS to julian day.  We always assume
** that the YYYY-MM-DD is according to the Gregorian calendar.
**
** Reference:  Meeus page 61
*/
static void computeJD(DateTime *p){
  int Y, M, D, A, B, X1, X2;

  if( p->validJD ) return;
  if( p->validYMD ){
    Y = p->Y;
    M = p->M;
    D = p->D;
  }else{
    Y = 2000;  /* If no YMD specified, assume 2000-Jan-01 */
    M = 1;
    D = 1;
  }
  if( M<=2 ){
    Y--;
    M += 12;
  }
  A = Y/100;
  B = 2 - A + (A/4);
  X1 = 365.25*(Y+4716);
  X2 = 30.6001*(M+1);
  p->iJD = (X1 + X2 + D + B - 1524.5)*86400000;
  p->validJD = 1;
  if( p->validHMS ){
    p->iJD += p->h*3600000 + p->m*60000 + p->s*1000;
    if( p->validTZ ){
      p->iJD -= p->tz*60000;
      p->validYMD = 0;
      p->validHMS = 0;
      p->validTZ = 0;
    }
  }
}

/*
** Parse dates of the form
**
**     YYYY-MM-DD HH:MM:SS.FFF
**     YYYY-MM-DD HH:MM:SS
**     YYYY-MM-DD HH:MM
**     YYYY-MM-DD
**
** Write the result into the DateTime structure and return 0
** on success and 1 if the input string is not a well-formed
** date.
*/
static int parseYyyyMmDd(const char *zDate, DateTime *p){
  int Y, M, D, neg;

  if( zDate[0]=='-' ){
    zDate++;
    neg = 1;
  }else{
    neg = 0;
  }
  if( getDigits(zDate,4,0,9999,'-',&Y,2,1,12,'-',&M,2,1,31,0,&D)!=3 ){
    return 1;
  }
  zDate += 10;
  while( isspace(*(u8*)zDate) || 'T'==*(u8*)zDate ){ zDate++; }
  if( parseHhMmSs(zDate, p)==0 ){
    /* We got the time */
  }else if( *zDate==0 ){
    p->validHMS = 0;
  }else{
    return 1;
  }
  p->validJD = 0;
  p->validYMD = 1;
  p->Y = neg ? -Y : Y;
  p->M = M;
  p->D = D;
  if( p->validTZ ){
    computeJD(p);
  }
  return 0;
}

/*
** Set the time to the current time reported by the VFS
*/
static void setDateTimeToCurrent(sqlite3_context *context, DateTime *p){
  double r;
  sqlite3 *db = sqlite3_context_db_handle(context);
  sqlite3OsCurrentTime(db->pVfs, &r);
  p->iJD = (sqlite3_int64)(r*86400000.0 + 0.5);
  p->validJD = 1;
}

/*
** Attempt to parse the given string into a Julian Day Number.  Return
** the number of errors.
**
** The following are acceptable forms for the input string:
**
**      YYYY-MM-DD HH:MM:SS.FFF  +/-HH:MM
**      DDDD.DD 
**      now
**
** In the first form, the +/-HH:MM is always optional.  The fractional
** seconds extension (the ".FFF") is optional.  The seconds portion
** (":SS.FFF") is option.  The year and date can be omitted as long
** as there is a time string.  The time string can be omitted as long
** as there is a year and date.
*/
static int parseDateOrTime(
  sqlite3_context *context, 
  const char *zDate, 
  DateTime *p
){
  if( parseYyyyMmDd(zDate,p)==0 ){
    return 0;
  }else if( parseHhMmSs(zDate, p)==0 ){
    return 0;
  }else if( sqlite3StrICmp(zDate,"now")==0){
    setDateTimeToCurrent(context, p);
    return 0;
  }else if( sqlite3IsNumber(zDate, 0, SQLITE_UTF8) ){
    double r;
    getValue(zDate, &r);
    p->iJD = (sqlite3_int64)(r*86400000.0 + 0.5);
    p->validJD = 1;
    return 0;
  }
  return 1;
}

/*
** Compute the Year, Month, and Day from the julian day number.
*/
static void computeYMD(DateTime *p){
  int Z, A, B, C, D, E, X1;
  if( p->validYMD ) return;
  if( !p->validJD ){
    p->Y = 2000;
    p->M = 1;
    p->D = 1;
  }else{
    Z = (p->iJD + 43200000)/86400000;
    A = (Z - 1867216.25)/36524.25;
    A = Z + 1 + A - (A/4);
    B = A + 1524;
    C = (B - 122.1)/365.25;
    D = 365.25*C;
    E = (B-D)/30.6001;
    X1 = 30.6001*E;
    p->D = B - D - X1;
    p->M = E<14 ? E-1 : E-13;
    p->Y = p->M>2 ? C - 4716 : C - 4715;
  }
  p->validYMD = 1;
}

/*
** Compute the Hour, Minute, and Seconds from the julian day number.
*/
static void computeHMS(DateTime *p){
  int s;
  if( p->validHMS ) return;
  computeJD(p);
  s = (p->iJD + 43200000) % 86400000;
  p->s = s/1000.0;
  s = p->s;
  p->s -= s;
  p->h = s/3600;
  s -= p->h*3600;
  p->m = s/60;
  p->s += s - p->m*60;
  p->validHMS = 1;
}

/*
** Compute both YMD and HMS
*/
static void computeYMD_HMS(DateTime *p){
  computeYMD(p);
  computeHMS(p);
}

/*
** Clear the YMD and HMS and the TZ
*/
static void clearYMD_HMS_TZ(DateTime *p){
  p->validYMD = 0;
  p->validHMS = 0;
  p->validTZ = 0;
}

#ifndef SQLITE_OMIT_LOCALTIME
/*
** Compute the difference (in milliseconds)
** between localtime and UTC (a.k.a. GMT)
** for the time value p where p is in UTC.
*/
static int localtimeOffset(DateTime *p){
  DateTime x, y;
  time_t t;
  x = *p;
  computeYMD_HMS(&x);
  if( x.Y<1971 || x.Y>=2038 ){
    x.Y = 2000;
    x.M = 1;
    x.D = 1;
    x.h = 0;
    x.m = 0;
    x.s = 0.0;
  } else {
    int s = x.s + 0.5;
    x.s = s;
  }
  x.tz = 0;
  x.validJD = 0;
  computeJD(&x);
  t = x.iJD/1000 - 2440587.5*86400.0;
#ifdef HAVE_LOCALTIME_R
  {
    struct tm sLocal;
    localtime_r(&t, &sLocal);
    y.Y = sLocal.tm_year + 1900;
    y.M = sLocal.tm_mon + 1;
    y.D = sLocal.tm_mday;
    y.h = sLocal.tm_hour;
    y.m = sLocal.tm_min;
    y.s = sLocal.tm_sec;
  }
#elif defined(HAVE_LOCALTIME_S)
  {
    struct tm sLocal;
    localtime_s(&sLocal, &t);
    y.Y = sLocal.tm_year + 1900;
    y.M = sLocal.tm_mon + 1;
    y.D = sLocal.tm_mday;
    y.h = sLocal.tm_hour;
    y.m = sLocal.tm_min;
    y.s = sLocal.tm_sec;
  }
#else
  {
    struct tm *pTm;
    sqlite3_mutex_enter(sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER));
    pTm = localtime(&t);
    y.Y = pTm->tm_year + 1900;
    y.M = pTm->tm_mon + 1;
    y.D = pTm->tm_mday;
    y.h = pTm->tm_hour;
    y.m = pTm->tm_min;
    y.s = pTm->tm_sec;
    sqlite3_mutex_leave(sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER));
  }
#endif
  y.validYMD = 1;
  y.validHMS = 1;
  y.validJD = 0;
  y.validTZ = 0;
  computeJD(&y);
  return y.iJD - x.iJD;
}
#endif /* SQLITE_OMIT_LOCALTIME */

/*
** Process a modifier to a date-time stamp.  The modifiers are
** as follows:
**
**     NNN days
**     NNN hours
**     NNN minutes
**     NNN.NNNN seconds
**     NNN months
**     NNN years
**     start of month
**     start of year
**     start of week
**     start of day
**     weekday N
**     unixepoch
**     localtime
**     utc
**
** Return 0 on success and 1 if there is any kind of error.
*/
static int parseModifier(const char *zMod, DateTime *p){
  int rc = 1;
  int n;
  double r;
  char *z, zBuf[30];
  z = zBuf;
  for(n=0; n<sizeof(zBuf)-1 && zMod[n]; n++){
    z[n] = tolower(zMod[n]);
  }
  z[n] = 0;
  switch( z[0] ){
#ifndef SQLITE_OMIT_LOCALTIME
    case 'l': {
      /*    localtime
      **
      ** Assuming the current time value is UTC (a.k.a. GMT), shift it to
      ** show local time.
      */
      if( strcmp(z, "localtime")==0 ){
        computeJD(p);
        p->iJD += localtimeOffset(p);
        clearYMD_HMS_TZ(p);
        rc = 0;
      }
      break;
    }
#endif
    case 'u': {
      /*
      **    unixepoch
      **
      ** Treat the current value of p->iJD as the number of
      ** seconds since 1970.  Convert to a real julian day number.
      */
      if( strcmp(z, "unixepoch")==0 && p->validJD ){
        p->iJD = p->iJD/86400.0 + 2440587.5*86400000.0;
        clearYMD_HMS_TZ(p);
        rc = 0;
      }
#ifndef SQLITE_OMIT_LOCALTIME
      else if( strcmp(z, "utc")==0 ){
        double c1;
        computeJD(p);
        c1 = localtimeOffset(p);
        p->iJD -= c1;
        clearYMD_HMS_TZ(p);
        p->iJD += c1 - localtimeOffset(p);
        rc = 0;
      }
#endif
      break;
    }
    case 'w': {
      /*
      **    weekday N
      **
      ** Move the date to the same time on the next occurrence of
      ** weekday N where 0==Sunday, 1==Monday, and so forth.  If the
      ** date is already on the appropriate weekday, this is a no-op.
      */
      if( strncmp(z, "weekday ", 8)==0 && getValue(&z[8],&r)>0
                 && (n=r)==r && n>=0 && r<7 ){
        sqlite3_int64 Z;
        computeYMD_HMS(p);
        p->validTZ = 0;
        p->validJD = 0;
        computeJD(p);
        Z = ((p->iJD + 129600000)/86400000) % 7;
        if( Z>n ) Z -= 7;
        p->iJD += (n - Z)*86400000;
        clearYMD_HMS_TZ(p);
        rc = 0;
      }
      break;
    }
    case 's': {
      /*
      **    start of TTTTT
      **
      ** Move the date backwards to the beginning of the current day,
      ** or month or year.
      */
      if( strncmp(z, "start of ", 9)!=0 ) break;
      z += 9;
      computeYMD(p);
      p->validHMS = 1;
      p->h = p->m = 0;
      p->s = 0.0;
      p->validTZ = 0;
      p->validJD = 0;
      if( strcmp(z,"month")==0 ){
        p->D = 1;
        rc = 0;
      }else if( strcmp(z,"year")==0 ){
        computeYMD(p);
        p->M = 1;
        p->D = 1;
        rc = 0;
      }else if( strcmp(z,"day")==0 ){
        rc = 0;
      }
      break;
    }
    case '+':
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
      n = getValue(z, &r);
      assert( n>=1 );
      if( z[n]==':' ){
        /* A modifier of the form (+|-)HH:MM:SS.FFF adds (or subtracts) the
        ** specified number of hours, minutes, seconds, and fractional seconds
        ** to the time.  The ".FFF" may be omitted.  The ":SS.FFF" may be
        ** omitted.
        */
        const char *z2 = z;
        DateTime tx;
        sqlite3_int64 day;
        if( !isdigit(*(u8*)z2) ) z2++;
        memset(&tx, 0, sizeof(tx));
        if( parseHhMmSs(z2, &tx) ) break;
        computeJD(&tx);
        tx.iJD -= 43200000;
        day = tx.iJD/86400000;
        tx.iJD -= day*86400000;
        if( z[0]=='-' ) tx.iJD = -tx.iJD;
        computeJD(p);
        clearYMD_HMS_TZ(p);
        p->iJD += tx.iJD;
        rc = 0;
        break;
      }
      z += n;
      while( isspace(*(u8*)z) ) z++;
      n = strlen(z);
      if( n>10 || n<3 ) break;
      if( z[n-1]=='s' ){ z[n-1] = 0; n--; }
      computeJD(p);
      rc = 0;
      if( n==3 && strcmp(z,"day")==0 ){
        p->iJD += r*86400000.0 + 0.5;
      }else if( n==4 && strcmp(z,"hour")==0 ){
        p->iJD += r*(86400000.0/24.0) + 0.5;
      }else if( n==6 && strcmp(z,"minute")==0 ){
        p->iJD += r*(86400000.0/(24.0*60.0)) + 0.5;
      }else if( n==6 && strcmp(z,"second")==0 ){
        p->iJD += r*(86400000.0/(24.0*60.0*60.0)) + 0.5;
      }else if( n==5 && strcmp(z,"month")==0 ){
        int x, y;
        computeYMD_HMS(p);
        p->M += r;
        x = p->M>0 ? (p->M-1)/12 : (p->M-12)/12;
        p->Y += x;
        p->M -= x*12;
        p->validJD = 0;
        computeJD(p);
        y = r;
        if( y!=r ){
          p->iJD += (r - y)*30.0*86400000.0 + 0.5;
        }
      }else if( n==4 && strcmp(z,"year")==0 ){
        computeYMD_HMS(p);
        p->Y += r;
        p->validJD = 0;
        computeJD(p);
      }else{
        rc = 1;
      }
      clearYMD_HMS_TZ(p);
      break;
    }
    default: {
      break;
    }
  }
  return rc;
}

/*
** Process time function arguments.  argv[0] is a date-time stamp.
** argv[1] and following are modifiers.  Parse them all and write
** the resulting time into the DateTime structure p.  Return 0
** on success and 1 if there are any errors.
**
** If there are zero parameters (if even argv[0] is undefined)
** then assume a default value of "now" for argv[0].
*/
static int isDate(
  sqlite3_context *context, 
  int argc, 
  sqlite3_value **argv, 
  DateTime *p
){
  int i;
  const unsigned char *z;
  int eType;
  memset(p, 0, sizeof(*p));
  if( argc==0 ){
    setDateTimeToCurrent(context, p);
  }else if( (eType = sqlite3_value_type(argv[0]))==SQLITE_FLOAT
                   || eType==SQLITE_INTEGER ){
    p->iJD = sqlite3_value_double(argv[0])*86400000.0 + 0.5;
    p->validJD = 1;
  }else{
    z = sqlite3_value_text(argv[0]);
    if( !z || parseDateOrTime(context, (char*)z, p) ){
      return 1;
    }
  }
  for(i=1; i<argc; i++){
    if( (z = sqlite3_value_text(argv[i]))==0 || parseModifier((char*)z, p) ){
      return 1;
    }
  }
  return 0;
}


/*
** The following routines implement the various date and time functions
** of SQLite.
*/

/*
**    julianday( TIMESTRING, MOD, MOD, ...)
**
** Return the julian day number of the date specified in the arguments
*/
static void juliandayFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  DateTime x;
  if( isDate(context, argc, argv, &x)==0 ){
    computeJD(&x);
    sqlite3_result_double(context, x.iJD/86400000.0);
  }
}

/*
**    datetime( TIMESTRING, MOD, MOD, ...)
**
** Return YYYY-MM-DD HH:MM:SS
*/
static void datetimeFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  DateTime x;
  if( isDate(context, argc, argv, &x)==0 ){
    char zBuf[100];
    computeYMD_HMS(&x);
    sqlite3_snprintf(sizeof(zBuf), zBuf, "%04d-%02d-%02d %02d:%02d:%02d",
                     x.Y, x.M, x.D, x.h, x.m, (int)(x.s));
    sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
  }
}

/*
**    time( TIMESTRING, MOD, MOD, ...)
**
** Return HH:MM:SS
*/
static void timeFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  DateTime x;
  if( isDate(context, argc, argv, &x)==0 ){
    char zBuf[100];
    computeHMS(&x);
    sqlite3_snprintf(sizeof(zBuf), zBuf, "%02d:%02d:%02d", x.h, x.m, (int)x.s);
    sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
  }
}

/*
**    date( TIMESTRING, MOD, MOD, ...)
**
** Return YYYY-MM-DD
*/
static void dateFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  DateTime x;
  if( isDate(context, argc, argv, &x)==0 ){
    char zBuf[100];
    computeYMD(&x);
    sqlite3_snprintf(sizeof(zBuf), zBuf, "%04d-%02d-%02d", x.Y, x.M, x.D);
    sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
  }
}

/*
**    strftime( FORMAT, TIMESTRING, MOD, MOD, ...)
**
** Return a string described by FORMAT.  Conversions as follows:
**
**   %d  day of month
**   %f  ** fractional seconds  SS.SSS
**   %H  hour 00-24
**   %j  day of year 000-366
**   %J  ** Julian day number
**   %m  month 01-12
**   %M  minute 00-59
**   %s  seconds since 1970-01-01
**   %S  seconds 00-59
**   %w  day of week 0-6  sunday==0
**   %W  week of year 00-53
**   %Y  year 0000-9999
**   %%  %
*/
static void strftimeFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  DateTime x;
  u64 n;
  int i, j;
  char *z;
  sqlite3 *db;
  const char *zFmt = (const char*)sqlite3_value_text(argv[0]);
  char zBuf[100];
  if( zFmt==0 || isDate(context, argc-1, argv+1, &x) ) return;
  db = sqlite3_context_db_handle(context);
  for(i=0, n=1; zFmt[i]; i++, n++){
    if( zFmt[i]=='%' ){
      switch( zFmt[i+1] ){
        case 'd':
        case 'H':
        case 'm':
        case 'M':
        case 'S':
        case 'W':
          n++;
          /* fall thru */
        case 'w':
        case '%':
          break;
        case 'f':
          n += 8;
          break;
        case 'j':
          n += 3;
          break;
        case 'Y':
          n += 8;
          break;
        case 's':
        case 'J':
          n += 50;
          break;
        default:
          return;  /* ERROR.  return a NULL */
      }
      i++;
    }
  }
  if( n<sizeof(zBuf) ){
    z = zBuf;
  }else if( n>db->aLimit[SQLITE_LIMIT_LENGTH] ){
    sqlite3_result_error_toobig(context);
    return;
  }else{
    z = sqlite3DbMallocRaw(db, n);
    if( z==0 ){
      sqlite3_result_error_nomem(context);
      return;
    }
  }
  computeJD(&x);
  computeYMD_HMS(&x);
  for(i=j=0; zFmt[i]; i++){
    if( zFmt[i]!='%' ){
      z[j++] = zFmt[i];
    }else{
      i++;
      switch( zFmt[i] ){
        case 'd':  sqlite3_snprintf(3, &z[j],"%02d",x.D); j+=2; break;
        case 'f': {
          double s = x.s;
          if( s>59.999 ) s = 59.999;
          sqlite3_snprintf(7, &z[j],"%06.3f", s);
          j += strlen(&z[j]);
          break;
        }
        case 'H':  sqlite3_snprintf(3, &z[j],"%02d",x.h); j+=2; break;
        case 'W': /* Fall thru */
        case 'j': {
          int nDay;             /* Number of days since 1st day of year */
          DateTime y = x;
          y.validJD = 0;
          y.M = 1;
          y.D = 1;
          computeJD(&y);
          nDay = (x.iJD - y.iJD)/86400000.0 + 0.5;
          if( zFmt[i]=='W' ){
            int wd;   /* 0=Monday, 1=Tuesday, ... 6=Sunday */
            wd = ((x.iJD+43200000)/86400000) % 7;
            sqlite3_snprintf(3, &z[j],"%02d",(nDay+7-wd)/7);
            j += 2;
          }else{
            sqlite3_snprintf(4, &z[j],"%03d",nDay+1);
            j += 3;
          }
          break;
        }
        case 'J': {
          sqlite3_snprintf(20, &z[j],"%.16g",x.iJD/86400000.0);
          j+=strlen(&z[j]);
          break;
        }
        case 'm':  sqlite3_snprintf(3, &z[j],"%02d",x.M); j+=2; break;
        case 'M':  sqlite3_snprintf(3, &z[j],"%02d",x.m); j+=2; break;
        case 's': {
          sqlite3_snprintf(30,&z[j],"%d",
                           (int)(x.iJD/1000.0 - 210866760000.0));
          j += strlen(&z[j]);
          break;
        }
        case 'S':  sqlite3_snprintf(3,&z[j],"%02d",(int)x.s); j+=2; break;
        case 'w':  z[j++] = (((x.iJD+129600000)/86400000) % 7) + '0'; break;
        case 'Y':  sqlite3_snprintf(5,&z[j],"%04d",x.Y); j+=strlen(&z[j]);break;
        default:   z[j++] = '%'; break;
      }
    }
  }
  z[j] = 0;
  sqlite3_result_text(context, z, -1,
                      z==zBuf ? SQLITE_TRANSIENT : SQLITE_DYNAMIC);
}

/*
** current_time()
**
** This function returns the same value as time('now').
*/
static void ctimeFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  timeFunc(context, 0, 0);
}

/*
** current_date()
**
** This function returns the same value as date('now').
*/
static void cdateFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  dateFunc(context, 0, 0);
}

/*
** current_timestamp()
**
** This function returns the same value as datetime('now').
*/
static void ctimestampFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  datetimeFunc(context, 0, 0);
}
#endif /* !defined(SQLITE_OMIT_DATETIME_FUNCS) */

#ifdef SQLITE_OMIT_DATETIME_FUNCS
/*
** If the library is compiled to omit the full-scale date and time
** handling (to get a smaller binary), the following minimal version
** of the functions current_time(), current_date() and current_timestamp()
** are included instead. This is to support column declarations that
** include "DEFAULT CURRENT_TIME" etc.
**
** This function uses the C-library functions time(), gmtime()
** and strftime(). The format string to pass to strftime() is supplied
** as the user-data for the function.
*/
static void currentTimeFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  time_t t;
  char *zFormat = (char *)sqlite3_user_data(context);
  sqlite3 *db;
  double rT;
  char zBuf[20];

  db = sqlite3_context_db_handle(context);
  sqlite3OsCurrentTime(db->pVfs, &rT);
  t = 86400.0*(rT - 2440587.5) + 0.5;
#ifdef HAVE_GMTIME_R
  {
    struct tm sNow;
    gmtime_r(&t, &sNow);
    strftime(zBuf, 20, zFormat, &sNow);
  }
#else
  {
    struct tm *pTm;
    sqlite3_mutex_enter(sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER));
    pTm = gmtime(&t);
    strftime(zBuf, 20, zFormat, pTm);
    sqlite3_mutex_leave(sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER));
  }
#endif

  sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
}
#endif

/*
** This function registered all of the above C functions as SQL
** functions.  This should be the only routine in this file with
** external linkage.
*/
void sqlite3RegisterDateTimeFunctions(sqlite3 *db){
#ifndef SQLITE_OMIT_DATETIME_FUNCS
  static const struct {
     char *zName;
     int nArg;
     void (*xFunc)(sqlite3_context*,int,sqlite3_value**);
  } aFuncs[] = {
    { "julianday", -1, juliandayFunc   },
    { "date",      -1, dateFunc        },
    { "time",      -1, timeFunc        },
    { "datetime",  -1, datetimeFunc    },
    { "strftime",  -1, strftimeFunc    },
    { "current_time",       0, ctimeFunc      },
    { "current_timestamp",  0, ctimestampFunc },
    { "current_date",       0, cdateFunc      },
  };
  int i;

  for(i=0; i<sizeof(aFuncs)/sizeof(aFuncs[0]); i++){
    sqlite3CreateFunc(db, aFuncs[i].zName, aFuncs[i].nArg,
        SQLITE_UTF8, 0, aFuncs[i].xFunc, 0, 0);
  }
#else
  static const struct {
     char *zName;
     char *zFormat;
  } aFuncs[] = {
    { "current_time", "%H:%M:%S" },
    { "current_date", "%Y-%m-%d" },
    { "current_timestamp", "%Y-%m-%d %H:%M:%S" }
  };
  int i;

  for(i=0; i<sizeof(aFuncs)/sizeof(aFuncs[0]); i++){
    sqlite3CreateFunc(db, aFuncs[i].zName, 0, SQLITE_UTF8, 
        aFuncs[i].zFormat, currentTimeFunc, 0, 0);
  }
#endif
}
