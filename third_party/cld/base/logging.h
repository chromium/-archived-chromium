// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <errno.h>
#include <string.h>
#include <time.h>
#include <string>
#include <strstream>
#include <vector>

#ifndef COMPILER_MSVC
#include <unistd.h>   // for _exit()
#endif

#include "base/port.h"
#include "base/basictypes.h"
#include "third_party/cld/base/commandlineflags.h"
#include "third_party/cld/base/crash.h"
#include "third_party/cld/base/dynamic_annotations.h"
#include "third_party/cld/base/macros.h"
#include "third_party/cld/base/scoped_ptr.h"
#include "third_party/cld/base/stl_decl.h"
#include "third_party/cld/base/log_severity.h"
#include "third_party/cld/base/vlog_is_on.h"
#include "third_party/cld/base/global_strip_options.h"

// Make a bunch of macros for logging.  The way to log things is to stream
// things to LOG(<a particular severity level>).  E.g.,
//
//   LOG(INFO) << "Found " << num_cookies << " cookies";
//
// You can capture log messages in a string, rather than reporting them
// immediately:
//
//   vector<string> errors;
//   LOG_STRING(ERROR, &errors) << "Couldn't parse cookie #" << cookie_num;
//
// This pushes back the new error onto 'errors'; if given a NULL pointer,
// it reports the error via LOG(ERROR).
//
// You can also do conditional logging:
//
//   LOG_IF(INFO, num_cookies > 10) << "Got lots of cookies";
//
// You can also do occasional logging (log every n'th occurrence of an
// event):
//
//   LOG_EVERY_N(INFO, 10) << "Got the " << COUNTER << "th cookie";
//
// The above will cause log messages to be output on the 1st, 11th, 21st, ...
// times it is executed.  Note that the special COUNTER value is used to
// identify which repetition is happening.
//
// You can also do occasional conditional logging (log every n'th
// occurrence of an event, when condition is satisfied):
//
//   LOG_IF_EVERY_N(INFO, (size > 1024), 10) << "Got the " << COUNTER
//                                           << "th big cookie";
//
// You can log messages the first N times your code executes a line. E.g.
//
//   LOG_FIRST_N(INFO, 20) << "Got the " << COUNTER << "th cookie";
//
// Outputs log messages for the first 20 times it is executed.
//
// Analogous SYSLOG, SYSLOG_IF, and SYSLOG_EVERY_N macros are available.
// These log to syslog as well as to the normal logs.  If you use these at
// all, you need to be aware that syslog can drastically reduce performance,
// especially if it is configured for remote logging!  Don't use these
// unless you fully understand this and have a concrete need to use them.
// Even then, try to minimize your use of them.
//
// There are also "debug mode" logging macros like the ones above:
//
//   DLOG(INFO) << "Found cookies";
//
//   DLOG_IF(INFO, num_cookies > 10) << "Got lots of cookies";
//
//   DLOG_EVERY_N(INFO, 10) << "Got the " << COUNTER << "th cookie";
//
// All "debug mode" logging is compiled away to nothing for non-debug mode
// compiles.
//
// We also have
//
//   LOG_ASSERT(assertion);
//   DLOG_ASSERT(assertion);
//
// which is syntactic sugar for {,D}LOG_IF(FATAL, assert fails) << assertion;
//
// We also override the standard 'assert' to use 'DLOG_ASSERT'.
//
// There are "verbose level" logging macros.  They look like
//
//   VLOG(1) << "I'm printed when you run the program with --v=1 or more";
//   VLOG(2) << "I'm printed when you run the program with --v=2 or more";
//
// These always log at the INFO log level (when they log at all).
// The verbose logging can also be turned on module-by-module.  For instance,
//    --vmodule=recordio=2,file=1,gfs*=3 --v=0
// will cause:
//   a. VLOG(2) and lower messages to be printed from recordio.{h,cc}
//   b. VLOG(1) and lower messages to be printed from google2file
//   c. VLOG(3) and lower messages to be printed from files prefixed with "gfs"
//   d. VLOG(0) and lower messages to be printed from elsewhere
//
// The wildcarding functionality shown by (c) supports both '*' (match
// 0 or more characters) and '?' (match any single character) wildcards.
//
// There's also VLOG_IS_ON(n) "verbose level" condition macro. To be used as
//
//   if (VLOG_IS_ON(2)) {
//     // do some logging preparation and logging
//     // that can't be accomplished with just VLOG(2) << ...;
//   }
//
// There are also VLOG_IF, VLOG_EVERY_N and VLOG_IF_EVERY_N "verbose level"
// condition macros for sample cases, when some extra computation and
// preparation for logs is not needed.
//   VLOG_IF(1, (size > 1024))
//      << "I'm printed when size is more than 1024 and when you run the "
//         "program with --v=1 or more";
//   VLOG_EVERY_N(1, 10)
//      << "I'm printed every 10th occurrence, and when you run the program "
//         "with --v=1 or more. Present occurence is " << COUNTER;
//   VLOG_IF_EVERY_N(1, (size > 1024), 10)
//      << "I'm printed on every 10th occurence of case when size is more "
//         " than 1024, when you run the program with --v=1 or more. ";
//         "Present occurence is " << COUNTER;
//
// [MLOG is OBSOLETE - use the more convenient VLOG(n) macros]
// There is also an MLOG option that enables module-level logging.  MLOG
// is associated with a specific flag by defining a MODULE_FLAG macro.
// Other than this, it behaves like VLOG.  Example:
//    DEFINE_int32(dnsverbose, 0, "Verbose level for DNS module");
//    #define MODULE_FLAG FLAGS_dnsverbose
//    MLOG(1) << "I'm printed when you run with --dnsverbose=1 or more";
//
// The supported severity levels for macros that allow you to specify one
// are (in increasing order of severity) INFO, WARNING, ERROR, and FATAL.
// Note that messages of a given severity are logged not only in the
// logfile for that severity, but also in all logfiles of lower severity.
// E.g., a message of severity FATAL will be logged to the logfiles of
// severity FATAL, ERROR, WARNING, and INFO.
//
// There is also the special severity of DFATAL, which logs FATAL in
// debug mode, ERROR in normal mode.
//
// Very important: logging a message at the FATAL severity level causes
// the program to terminate (after the message is logged).
//
// Unless otherwise specified, logs will be written to the filename
// "<program name>.<hostname>.<user name>.log.<severity level>.", followed
// by the date, time, and pid (you can't prevent the date, time, and pid
// from being in the filename).
//
// The logging code takes two flags:
//     --v=#           set the verbose level
//     --logtostderr   log all the messages to stderr instead of to logfiles

// LOG LINE PREFIX FORMAT
//
// Log lines have this form:
//
//     Lmmdd hh:mm:ss.uuuuuu threadid file:line] msg...
//
// where the fields are defined as follows:
//
//   L                A single character, representing the log level
//                    (eg 'I' for INFO)
//   mm               The month (zero padded; ie May is '05')
//   dd               The day (zero padded)
//   hh:mm:ss.uuuuuu  Time in hours, minutes and fractional seconds
//   threadid         The space-padded thread ID as returned by GetTID()
//                    (this matches the PID on Linux)
//   file             The file name
//   line             The line number
//   msg              The user-supplied message
//
// Example:
//
//   I1103 11:57:31.739339 24395 google.cc:2341] Command line: ./some_prog
//   I1103 11:57:31.739403 24395 google.cc:2342] Process id 24395
//
// NOTE: although the microseconds are useful for comparing events on
// a single machine, clocks on different machines may not be well
// synchronized.  Hence, use caution when comparing the low bits of
// timestamps from different machines.

// Set whether log messages go to stderr instead of logfiles
DECLARE_bool(logtostderr);

// Set whether log messages go to stderr in addition to logfiles.
DECLARE_bool(alsologtostderr);

// Log messages at a level >= this flag are automatically sent to
// stderr in addition to log files.
DECLARE_int32(stderrthreshold);

// Set whether the log prefix should be prepended to each line of output.
DECLARE_bool(log_prefix);

// Log messages at a level <= this flag are buffered.
// Log messages at a higher level are flushed immediately.
DECLARE_int32(logbuflevel);

// Sets the maximum number of seconds which logs may be buffered for.
DECLARE_int32(logbufsecs);

// Should Google1 logging be turned on?
DECLARE_bool(logging);

// Log suppression level: messages logged at a lower level than this
// are suppressed.
DECLARE_int32(minloglevel);

// If specified, logfiles are written into this directory instead of the
// default logging directory.
DECLARE_string(log_dir);

// Sets the path of the directory into which to put additional links
// to the log files.
DECLARE_string(log_link);

// Sets the maximum log file size (in MB).
DECLARE_int32(max_log_size);

// Should log IO be directed to a background thread?  This flag has no
// effect unless //thread/logger:logger is linked into the binary.
DECLARE_bool(threaded_logging);

// Set to cause StatusMessage() to write status to ./STATUS file.
DECLARE_bool(status_messages_to_status_file);

// Sets whether to avoid logging to the disk if the disk is full.
DECLARE_bool(stop_logging_if_full_disk);

// Log messages below the STRIP_LOG level will be compiled away for
// security reasons. See LOG(severtiy) below. STRIP_LOG is defined in
// //base/global_strip_log.h

// A few definitions of macros that don't generate much code.  Since
// LOG(INFO) and its ilk are used all over our code, it's
// better to have compact code for these operations.

#if STRIP_LOG == 0
#define COMPACT_GOOGLE_LOG_INFO LogMessage(__FILE__, __LINE__)
#define LOG_TO_STRING_INFO(message) LogMessage(__FILE__, __LINE__, INFO, \
                                               message)
#else
#define COMPACT_GOOGLE_LOG_INFO NullStream()
#define LOG_TO_STRING_INFO(message) NullStream()
#endif

#if STRIP_LOG <= 1
#define COMPACT_GOOGLE_LOG_WARNING LogMessage(__FILE__, __LINE__, WARNING)
#define LOG_TO_STRING_WARNING(message) LogMessage(__FILE__, __LINE__, \
                                                  WARNING, message)
#else
#define COMPACT_GOOGLE_LOG_WARNING NullStream()
#define LOG_TO_STRING_WARNING(message) NullStream()
#endif

#if STRIP_LOG <= 2
#define COMPACT_GOOGLE_LOG_ERROR LogMessage(__FILE__, __LINE__, ERROR)
#define LOG_TO_STRING_ERROR(message) LogMessage(__FILE__, __LINE__, ERROR, \
                                                message)
#else
#define COMPACT_GOOGLE_LOG_ERROR NullStream()
#define LOG_TO_STRING_ERROR(message) NullStream()
#endif

#if STRIP_LOG <= 3
#define COMPACT_GOOGLE_LOG_FATAL LogMessageFatal(__FILE__, __LINE__)
#define COMPACT_GOOGLE_LOG_QFATAL LogMessageQuietlyFatal(__FILE__, __LINE__)
#define LOG_TO_STRING_FATAL(message) LogMessage(__FILE__, __LINE__, FATAL, \
                                                message)
#else
#define COMPACT_GOOGLE_LOG_FATAL NullStreamFatal()
#define COMPACT_GOOGLE_LOG_QFATAL NullStreamFatal()
#define LOG_TO_STRING_FATAL(message) NullStreamFatal()
#endif

// For DFATAL, we want to use LogMessage (as opposed to
// LogMessageFatal), to be consistent with the original behavior.
#ifdef NDEBUG
#define COMPACT_GOOGLE_LOG_DFATAL COMPACT_GOOGLE_LOG_ERROR
#elif STRIP_LOG <= 3
#define COMPACT_GOOGLE_LOG_DFATAL LogMessage(__FILE__, __LINE__, FATAL)
#else
#define COMPACT_GOOGLE_LOG_DFATAL NullStreamFatal()
#endif

#define GOOGLE_LOG_INFO(counter) \
  LogMessage(__FILE__, __LINE__, INFO, counter, &LogMessage::SendToLog)
#define SYSLOG_INFO(counter) \
  LogMessage(__FILE__, __LINE__, INFO, counter, \
  &LogMessage::SendToSyslogAndLog)
#define GOOGLE_LOG_WARNING(counter)  \
  LogMessage(__FILE__, __LINE__, WARNING, counter, &LogMessage::SendToLog)
#define SYSLOG_WARNING(counter)  \
  LogMessage(__FILE__, __LINE__, WARNING, counter, \
  &LogMessage::SendToSyslogAndLog)
#define GOOGLE_LOG_ERROR(counter)  \
  LogMessage(__FILE__, __LINE__, ERROR, counter, &LogMessage::SendToLog)
#define SYSLOG_ERROR(counter)  \
  LogMessage(__FILE__, __LINE__, ERROR, counter, \
  &LogMessage::SendToSyslogAndLog)
#define GOOGLE_LOG_FATAL(counter) \
  LogMessage(__FILE__, __LINE__, FATAL, counter, &LogMessage::SendToLog)
#define SYSLOG_FATAL(counter) \
  LogMessage(__FILE__, __LINE__, FATAL, counter, \
  &LogMessage::SendToSyslogAndLog)
#define GOOGLE_LOG_DFATAL(counter) \
  LogMessage(__FILE__, __LINE__, DFATAL_LEVEL, counter, &LogMessage::SendToLog)
#define SYSLOG_DFATAL(counter) \
  LogMessage(__FILE__, __LINE__, DFATAL_LEVEL, counter, \
  &LogMessage::SendToSyslogAndLog)

#ifdef OS_WINDOWS
// A very useful logging macro to log windows errors:
#define LOG_SYSRESULT(result) \
  if (FAILED(result)) { \
    LPTSTR message = NULL; \
    LPTSTR msg = reinterpret_cast<LPTSTR>(&message); \
    DWORD message_length = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | \
                         FORMAT_MESSAGE_FROM_SYSTEM, \
                         0, result, 0, msg, 100, NULL); \
    if (message_length > 0) { \
      LogMessage(__FILE__, __LINE__, ERROR, 0, \
                 &LogMessage::SendToLog).stream() << message; \
      LocalFree(message); \
    } \
  }
#endif

// We use the preprocessor's merging operator, "##", so that, e.g.,
// LOG(INFO) becomes the token GOOGLE_LOG_INFO.  There's some funny
// subtle difference between ostream member streaming functions (e.g.,
// ostream::operator<<(int) and ostream non-member streaming functions
// (e.g., ::operator<<(ostream&, string&): it turns out that it's
// impossible to stream something like a string directly to an unnamed
// ostream. We employ a neat hack by calling the stream() member
// function of LogMessage which seems to avoid the problem.
#define LOG(severity) COMPACT_GOOGLE_LOG_ ## severity.stream()
#define SYSLOG(severity) SYSLOG_ ## severity(0).stream()

// A convenient shorthand
#define LG LOG(INFO)

class LogSink;  // defined below

// If a non-NULL sink pointer is given, we push this message to that sink.
// For LOG_TO_SINK we then do normal LOG(severity) logging as well.
// This is useful for capturing messages and passing/storing them
// somewhere more specific than the global log of the process.
// Argument types:
//   LogSink* sink;
//   LogSeverity severity;
// The cast is to disambiguate NULL arguments.
#define LOG_TO_SINK(sink, severity) \
  LogMessage(__FILE__, __LINE__, severity, \
             static_cast<LogSink*>(sink), true).stream()
#define LOG_TO_SINK_BUT_NOT_TO_LOGFILE(sink, severity) \
  LogMessage(__FILE__, __LINE__, severity, \
             static_cast<LogSink*>(sink), false).stream()

// If a non-NULL string pointer is given, we write this message to that string.
// We then do normal LOG(severity) logging as well.
// This is useful for capturing messages and storing them somewhere more
// specific than the global log of the process.
// Argument types:
//   string* message;
//   LogSeverity severity;
// The cast is to disambiguate NULL arguments.
// NOTE: LOG(severity) expands to LogMessage().stream() for the specified
// severity.
#define LOG_TO_STRING(severity, message) \
  LOG_TO_STRING_##severity(static_cast<string*>(message)).stream()

// If a non-NULL pointer is given, we push the message onto the end
// of a vector of strings; otherwise, we report it with LOG(severity).
// This is handy for capturing messages and perhaps passing them back
// to the caller, rather than reporting them immediately.
// Argument types:
//   LogSeverity severity;
//   vector<string> *outvec;
// The cast is to disambiguate NULL arguments.
#define LOG_STRING(severity, outvec) \
  LOG_TO_STRING_##severity(static_cast<vector<string>*>(outvec)).stream()

#define LOG_IF(severity, condition) \
  !(condition) ? (void) 0 : LogMessageVoidify() & LOG(severity)
#define SYSLOG_IF(severity, condition) \
  !(condition) ? (void) 0 : LogMessageVoidify() & SYSLOG(severity)

#define LOG_ASSERT(condition)  \
  LOG_IF(FATAL, !(condition)) << "Assert failed: " #condition
#define SYSLOG_ASSERT(condition) \
  SYSLOG_IF(FATAL, !(condition)) << "Assert failed: " #condition

// CHECK dies with a fatal error if condition is not true.  It is *not*
// controlled by NDEBUG, so the check will be executed regardless of
// compilation mode.  Therefore, it is safe to do things like:
//    CHECK(fp->Write(x) == 4)
#define CHECK(condition)  \
      LOG_IF(FATAL, PREDICT_FALSE(!(condition))) \
             << "Check failed: " #condition " "

// QCHECK is a quiet version of CHECK. It has all of the same properties,
// except that when it dies it simply prints out this message and doesn't
// dump a giant stack trace, etc. This is good for tests like sanity-checking
// user inputs, where your own failure message is really the only thing you
// need or want to display.
#define QCHECK(condition)  \
      LOG_IF(QFATAL, PREDICT_FALSE(!(condition))) \
             << "Check failed: " #condition " "

// A container for a string pointer which can be evaluated to a bool -
// true iff the pointer is NULL.
struct CheckOpString {
  CheckOpString(string* str) : str_(str) { }
  // No destructor: if str_ is non-NULL, we're about to LOG(FATAL),
  // so there's no point in cleaning up str_.
  operator bool() const { return PREDICT_FALSE(str_ != NULL); }
  string* str_;
};

// Function is overloaded for integral types to allow static const
// integrals declared in classes and not defined to be used as arguments to
// CHECK* macros. It's not encouraged though.
template <class T>
inline const T&       GetReferenceableValue(const T&           t) { return t; }
inline char           GetReferenceableValue(char               t) { return t; }
inline unsigned char  GetReferenceableValue(unsigned char      t) { return t; }
inline signed char    GetReferenceableValue(signed char        t) { return t; }
inline short          GetReferenceableValue(short              t) { return t; }
inline unsigned short GetReferenceableValue(unsigned short     t) { return t; }
inline int            GetReferenceableValue(int                t) { return t; }
inline unsigned int   GetReferenceableValue(unsigned int       t) { return t; }
inline long           GetReferenceableValue(long               t) { return t; }
inline unsigned long  GetReferenceableValue(unsigned long      t) { return t; }
inline long long      GetReferenceableValue(long long          t) { return t; }
inline unsigned long long GetReferenceableValue(unsigned long long t) {
  return t;
}

// Build the error message string.
template<class t1, class t2>
string* MakeCheckOpString(const t1& v1, const t2& v2, const char* names) {
  strstream ss;
  ss << names << " (" << v1 << " vs. " << v2 << ")";
  return new string(ss.str(), ss.pcount());
}

// Helper functions for CHECK_OP macro.
// The (int, int) specialization works around the issue that the compiler
// will not instantiate the template version of the function on values of
// unnamed enum type - see comment below.
#define DEFINE_CHECK_OP_IMPL(name, op) \
  template <class t1, class t2> \
  inline string* Check##name##Impl(const t1& v1, const t2& v2, \
                                   const char* names) { \
    if (v1 op v2) return NULL; \
    else return MakeCheckOpString(v1, v2, names); \
  } \
  inline string* Check##name##Impl(int v1, int v2, const char* names) { \
    return Check##name##Impl<int, int>(v1, v2, names); \
  }

// Use _EQ, _NE, _LE, etc. in case the file including base/logging.h
// provides its own #defines for the simpler names EQ, NE, LE, etc.
// This happens if, for example, those are used as token names in a
// yacc grammar.
DEFINE_CHECK_OP_IMPL(_EQ, ==)
DEFINE_CHECK_OP_IMPL(_NE, !=)
DEFINE_CHECK_OP_IMPL(_LE, <=)
DEFINE_CHECK_OP_IMPL(_LT, < )
DEFINE_CHECK_OP_IMPL(_GE, >=)
DEFINE_CHECK_OP_IMPL(_GT, > )
#undef DEFINE_CHECK_OP_IMPL

// Helper macro for binary operators.
// Don't use this macro directly in your code, use CHECK_EQ et al below.

#if defined(STATIC_ANALYSIS)
// Only for static analysis tool to know that it is equivalent to assert
#define CHECK_OP_LOG(name, op, val1, val2, log) CHECK((val1) op (val2))
#elif !defined(NDEBUG)
// In debug mode, avoid constructing CheckOpStrings if possible,
// to reduce the overhead of CHECK statments by 2x.
// Real DCHECK-heavy tests have seen 1.5x speedups.

// The meaning of "string" might be different between now and
// when this macro gets invoked (e.g., if someone is experimenting
// with other string implementations that get defined after this
// file is included).  Save the current meaning now and use it
// in the macro.
typedef string _Check_string;
#define CHECK_OP_LOG(name, op, val1, val2, log) \
  while (_Check_string* _result = \
         Check##name##Impl(GetReferenceableValue(val1), \
                           GetReferenceableValue(val2), \
                           #val1 " " #op " " #val2)) \
    log(__FILE__, __LINE__, CheckOpString(_result)).stream()
#else
// In optimized mode, use CheckOpString to hint to compiler that
// the while condition is unlikely.
#define CHECK_OP_LOG(name, op, val1, val2, log) \
  while (CheckOpString _result = \
         Check##name##Impl(GetReferenceableValue(val1), \
                           GetReferenceableValue(val2), \
                           #val1 " " #op " " #val2)) \
    log(__FILE__, __LINE__, _result).stream()
#endif  // STATIC_ANALYSIS, !NDEBUG

#if STRIP_LOG <= 3
#define CHECK_OP(name, op, val1, val2) \
  CHECK_OP_LOG(name, op, val1, val2, LogMessageFatal)
#else
#define CHECK_OP(name, op, val1, val2) \
  CHECK_OP_LOG(name, op, val1, val2, NullStreamFatal)
#endif // STRIP_LOG <= 3
#define QCHECK_OP(name, op, val1, val2) \
  CHECK_OP_LOG(name, op, val1, val2, LogMessageQuietlyFatal)

// Equality/Inequality checks - compare two values, and log a FATAL message
// including the two values when the result is not as expected.  The values
// must have operator<<(ostream, ...) defined.
//
// You may append to the error message like so:
//   CHECK_NE(1, 2) << ": The world must be ending!";
//
// We are very careful to ensure that each argument is evaluated exactly
// once, and that anything which is legal to pass as a function argument is
// legal here.  In particular, the arguments may be temporary expressions
// which will end up being destroyed at the end of the apparent statement,
// for example:
//   CHECK_EQ(string("abc")[1], 'b');
//
// WARNING: These don't compile correctly if one of the arguments is a pointer
// and the other is NULL. To work around this, simply static_cast NULL to the
// type of the desired pointer.

#define CHECK_EQ(val1, val2) CHECK_OP(_EQ, ==, val1, val2)
#define CHECK_NE(val1, val2) CHECK_OP(_NE, !=, val1, val2)
#define CHECK_LE(val1, val2) CHECK_OP(_LE, <=, val1, val2)
#define CHECK_LT(val1, val2) CHECK_OP(_LT, < , val1, val2)
#define CHECK_GE(val1, val2) CHECK_OP(_GE, >=, val1, val2)
#define CHECK_GT(val1, val2) CHECK_OP(_GT, > , val1, val2)

#define QCHECK_EQ(val1, val2) QCHECK_OP(_EQ, ==, val1, val2)
#define QCHECK_NE(val1, val2) QCHECK_OP(_NE, !=, val1, val2)
#define QCHECK_LE(val1, val2) QCHECK_OP(_LE, <=, val1, val2)
#define QCHECK_LT(val1, val2) QCHECK_OP(_LT, < , val1, val2)
#define QCHECK_GE(val1, val2) QCHECK_OP(_GE, >=, val1, val2)
#define QCHECK_GT(val1, val2) QCHECK_OP(_GT, > , val1, val2)


// Check that the input is non NULL.  This very useful in constructor
// initializer lists.

#define CHECK_NOTNULL(val) \
  CheckNotNull(__FILE__, __LINE__, "'" #val "' Must be non NULL", (val))

// Helper functions for string comparisons.
// To avoid bloat, the definitions are in logging.cc.
#define DECLARE_CHECK_STROP_IMPL(func, expected) \
  string* Check##func##expected##Impl(const char* s1, const char* s2, \
                                      const char* names);
DECLARE_CHECK_STROP_IMPL(strcmp, true)
DECLARE_CHECK_STROP_IMPL(strcmp, false)
DECLARE_CHECK_STROP_IMPL(strcasecmp, true)
DECLARE_CHECK_STROP_IMPL(strcasecmp, false)
#undef DECLARE_CHECK_STROP_IMPL

// Helper macro for string comparisons.
// Don't use this macro directly in your code, use CHECK_STREQ et al below.
#define CHECK_STROP(func, op, expected, s1, s2) \
  while (CheckOpString _result = \
         Check##func##expected##Impl((s1), (s2), \
                                     #s1 " " #op " " #s2)) \
    LOG(FATAL) << *_result.str_
#define QCHECK_STROP(func, op, expected, s1, s2) \
  while (CheckOpString _result = \
         Check##func##expected##Impl((s1), (s2), \
                                     #s1 " " #op " " #s2)) \
    LOG(QFATAL) << *_result.str_


// String (char*) equality/inequality checks.
// CASE versions are case-insensitive.
//
// Note that "s1" and "s2" may be temporary strings which are destroyed
// by the compiler at the end of the current "full expression"
// (e.g. CHECK_STREQ(Foo().c_str(), Bar().c_str())).

#define CHECK_STREQ(s1, s2) CHECK_STROP(strcmp, ==, true, s1, s2)
#define CHECK_STRNE(s1, s2) CHECK_STROP(strcmp, !=, false, s1, s2)
#define CHECK_STRCASEEQ(s1, s2) CHECK_STROP(strcasecmp, ==, true, s1, s2)
#define CHECK_STRCASENE(s1, s2) CHECK_STROP(strcasecmp, !=, false, s1, s2)

#define CHECK_INDEX(I,A) CHECK(I < (sizeof(A)/sizeof(A[0])))
#define CHECK_BOUND(B,A) CHECK(B <= (sizeof(A)/sizeof(A[0])))

#define QCHECK_STREQ(s1, s2) QCHECK_STROP(strcmp, ==, true, s1, s2)
#define QCHECK_STRNE(s1, s2) QCHECK_STROP(strcmp, !=, false, s1, s2)
#define QCHECK_STRCASEEQ(s1, s2) QCHECK_STROP(strcasecmp, ==, true, s1, s2)
#define QCHECK_STRCASENE(s1, s2) QCHECK_STROP(strcasecmp, !=, false, s1, s2)

#define QCHECK_INDEX(I,A) QCHECK(I < (sizeof(A)/sizeof(A[0])))
#define QCHECK_BOUND(B,A) QCHECK(B <= (sizeof(A)/sizeof(A[0])))

// Likely to be deprecated; instead use
//   CHECK(MathUtil::NearByMargin(x, y))
// (or another similar function from util/math/mathutil.h).
#define CHECK_DOUBLE_EQ(val1, val2)              \
  do {                                           \
    CHECK_LE((val1), (val2)+0.000000000000001L); \
    CHECK_GE((val1), (val2)-0.000000000000001L); \
  } while (0)

// Likely to be deprecated; instead use
//   CHECK(MathUtil::WithinMargin(x, y, margin))
// (or another similar function from util/math/mathutil.h).
#define CHECK_NEAR(val1, val2, margin)           \
  do {                                           \
    CHECK_LE((val1), (val2)+(margin));           \
    CHECK_GE((val1), (val2)-(margin));           \
  } while (0)

// perror()..googly style!
//
// PLOG() and PLOG_IF() and PCHECK() behave exactly like their LOG* and
// CHECK equivalents with the addition that they postpend a description
// of the current state of errno to their output lines.

#define PLOG(severity) GOOGLE_PLOG(severity, 0).stream()

#define GOOGLE_PLOG(severity, counter)  \
  ErrnoLogMessage(__FILE__, __LINE__, severity, counter, \
                  &LogMessage::SendToLog)

#define PLOG_IF(severity, condition) \
  !(condition) ? (void) 0 : LogMessageVoidify() & PLOG(severity)

// A CHECK() macro that postpends errno if the condition is false. E.g.
//
// if (poll(fds, nfds, timeout) == -1) { PCHECK(errno == EINTR); ... }
#define PCHECK(condition)  \
      PLOG_IF(FATAL, PREDICT_FALSE(!(condition))) \
              << "Check failed: " #condition " "

// A CHECK() macro that lets you assert the success of a function that
// returns -1 and sets errno in case of an error. E.g.
//
// CHECK_ERR(mkdir(path, 0700));
//
// or
//
// int fd = open(filename, flags); CHECK_ERR(fd) << ": open " << filename;
#define CHECK_ERR(invocation)                                          \
PLOG_IF(FATAL, PREDICT_FALSE((invocation) == -1)) << #invocation

// Use macro expansion to create, for each use of LOG_EVERY_N(), static
// variables with the __LINE__ expansion as part of the variable name.
#define LOG_EVERY_N_VARNAME(base, line) LOG_EVERY_N_VARNAME_CONCAT(base, line)
#define LOG_EVERY_N_VARNAME_CONCAT(base, line) base ## line

#define LOG_OCCURRENCES LOG_EVERY_N_VARNAME(occurrences_, __LINE__)
#define LOG_OCCURRENCES_MOD_N LOG_EVERY_N_VARNAME(occurrences_mod_n_, __LINE__)

#define SOME_KIND_OF_LOG_EVERY_N(severity, n, what_to_do) \
  static int LOG_OCCURRENCES = 0, LOG_OCCURRENCES_MOD_N = 0; \
  ++LOG_OCCURRENCES; \
  if (++LOG_OCCURRENCES_MOD_N > n) LOG_OCCURRENCES_MOD_N -= n; \
  if (LOG_OCCURRENCES_MOD_N == 1) \
    LogMessage(__FILE__, __LINE__, severity, LOG_OCCURRENCES, \
               &what_to_do).stream()

#define SOME_KIND_OF_LOG_IF_EVERY_N(severity, condition, n, what_to_do) \
  static int LOG_OCCURRENCES = 0, LOG_OCCURRENCES_MOD_N = 0; \
  ANNOTATE_BENIGN_RACE(&LOG_OCCURRENCES, "logging"); \
  ANNOTATE_BENIGN_RACE(&LOG_OCCURRENCES_MOD_N, "logging"); \
  ++LOG_OCCURRENCES; \
  if (condition && \
      ((LOG_OCCURRENCES_MOD_N=(LOG_OCCURRENCES_MOD_N + 1) % n) == (1 % n))) \
    LogMessage(__FILE__, __LINE__, severity, LOG_OCCURRENCES, \
                 &what_to_do).stream()

#define SOME_KIND_OF_PLOG_EVERY_N(severity, n, what_to_do) \
  static int LOG_OCCURRENCES = 0, LOG_OCCURRENCES_MOD_N = 0; \
  ANNOTATE_BENIGN_RACE(&LOG_OCCURRENCES, "logging"); \
  ANNOTATE_BENIGN_RACE(&LOG_OCCURRENCES_MOD_N, "logging"); \
  ++LOG_OCCURRENCES; \
  if (++LOG_OCCURRENCES_MOD_N > n) LOG_OCCURRENCES_MOD_N -= n; \
  if (LOG_OCCURRENCES_MOD_N == 1) \
    ErrnoLogMessage(__FILE__, __LINE__, severity, LOG_OCCURRENCES, \
                    &what_to_do).stream()

#define SOME_KIND_OF_LOG_FIRST_N(severity, n, what_to_do) \
  static int LOG_OCCURRENCES = 0; \
  ANNOTATE_BENIGN_RACE(&LOG_OCCURRENCES, "logging"); \
  if (LOG_OCCURRENCES <= n) \
    ++LOG_OCCURRENCES; \
  if (LOG_OCCURRENCES <= n) \
    LogMessage(__FILE__, __LINE__, severity, LOG_OCCURRENCES, \
               &what_to_do).stream()

#define LOG_EVERY_N(severity, n) \
  COMPILE_ASSERT(severity < NUM_SEVERITIES, \
                 INVALID_REQUESTED_LOG_SEVERITY); \
  SOME_KIND_OF_LOG_EVERY_N(severity, (n), LogMessage::SendToLog)

#define SYSLOG_EVERY_N(severity, n) \
  SOME_KIND_OF_LOG_EVERY_N(severity, (n), LogMessage::SendToSyslogAndLog)

#define PLOG_EVERY_N(severity, n) \
  SOME_KIND_OF_PLOG_EVERY_N(severity, (n), LogMessage::SendToLog)

#define LOG_FIRST_N(severity, n) \
  SOME_KIND_OF_LOG_FIRST_N(severity, (n), LogMessage::SendToLog)

#define LOG_IF_EVERY_N(severity, condition, n) \
  SOME_KIND_OF_LOG_IF_EVERY_N(severity, (condition), (n), LogMessage::SendToLog)

// We want the special COUNTER value available for LOG_EVERY_X()'ed messages
enum PRIVATE_Counter {COUNTER};


// Plus some debug-logging macros that get compiled to nothing for production

#ifndef NDEBUG

#define DLOG(severity) LOG(severity)
#define DVLOG(verboselevel) VLOG(verboselevel)
#define DLOG_IF(severity, condition) LOG_IF(severity, condition)
#define DLOG_EVERY_N(severity, n) LOG_EVERY_N(severity, n)
#define DLOG_IF_EVERY_N(severity, condition, n) \
  LOG_IF_EVERY_N(severity, condition, n)
#define DLOG_ASSERT(condition) LOG_ASSERT(condition)

// debug-only checking.  not executed in NDEBUG mode.
#define DCHECK(condition) CHECK(condition)
#define DCHECK_EQ(val1, val2) CHECK_EQ(val1, val2)
#define DCHECK_NE(val1, val2) CHECK_NE(val1, val2)
#define DCHECK_LE(val1, val2) CHECK_LE(val1, val2)
#define DCHECK_LT(val1, val2) CHECK_LT(val1, val2)
#define DCHECK_GE(val1, val2) CHECK_GE(val1, val2)
#define DCHECK_GT(val1, val2) CHECK_GT(val1, val2)
#define DCHECK_STREQ(str1, str2) CHECK_STREQ(str1, str2)
#define DCHECK_STRCASEEQ(str1, str2) CHECK_STRCASEEQ(str1, str2)
#define DCHECK_STRNE(str1, str2) CHECK_STRNE(str1, str2)
#define DCHECK_STRCASENE(str1, str2) CHECK_STRCASENE(str1, str2)

#else  // NDEBUG

#define DLOG(severity) \
  true ? (void) 0 : LogMessageVoidify() & LOG(severity)

#define DVLOG(verboselevel) \
  (true || !VLOG_IS_ON(verboselevel)) ?\
    (void) 0 : LogMessageVoidify() & LOG(INFO)

#define DLOG_IF(severity, condition) \
  (true || !(condition)) ? (void) 0 : LogMessageVoidify() & LOG(severity)

#define DLOG_EVERY_N(severity, n) \
  true ? (void) 0 : LogMessageVoidify() & LOG(severity)

#define DLOG_IF_EVERY_N(severity, condition, n) \
  (true || !(condition))? (void) 0 : LogMessageVoidify() & LOG(severity)

#define DLOG_ASSERT(condition) \
  true ? (void) 0 : LOG_ASSERT(condition)

#define DCHECK(condition) \
  while (false) \
    CHECK(condition)

#define DCHECK_EQ(val1, val2) \
  while (false) \
    CHECK_EQ(val1, val2)

#define DCHECK_NE(val1, val2) \
  while (false) \
    CHECK_NE(val1, val2)

#define DCHECK_LE(val1, val2) \
  while (false) \
    CHECK_LE(val1, val2)

#define DCHECK_LT(val1, val2) \
  while (false) \
    CHECK_LT(val1, val2)

#define DCHECK_GE(val1, val2) \
  while (false) \
    CHECK_GE(val1, val2)

#define DCHECK_GT(val1, val2) \
  while (false) \
    CHECK_GT(val1, val2)

#define DCHECK_STREQ(str1, str2) \
  while (false) \
    CHECK_STREQ(str1, str2)

#define DCHECK_STRCASEEQ(str1, str2) \
  while (false) \
    CHECK_STRCASEEQ(str1, str2)

#define DCHECK_STRNE(str1, str2) \
  while (false) \
    CHECK_STRNE(str1, str2)

#define DCHECK_STRCASENE(str1, str2) \
  while (false) \
    CHECK_STRCASENE(str1, str2)


#endif  // NDEBUG

// Log only in verbose mode.

#define VLOG(verboselevel) LOG_IF(INFO, VLOG_IS_ON(verboselevel))

#define VLOG_IF(verboselevel, condition) \
  LOG_IF(INFO, (condition) && VLOG_IS_ON(verboselevel))

#define VLOG_EVERY_N(verboselevel, n) \
  LOG_IF_EVERY_N(INFO, VLOG_IS_ON(verboselevel), n)

#define VLOG_IF_EVERY_N(verboselevel, condition, n) \
  LOG_IF_EVERY_N(INFO, (condition) && VLOG_IS_ON(verboselevel), n)


// [MLOG is OBSOLETE - use the more convenient VLOG(n) macros]
// Log only when a module-specific value (MODULE_FLAG) has a specific
// value.  MODULE_FLAG must be a macro that evaluates to the name of
// the flag that you wish to use.  You should '#define MODULE_FLAG
// <variable name>' before using this macro.  (For example:
//       #define MODULE_FLAG FLAGS_dnsverbose
#define MLOG(verboselevel) LOG_IF(INFO, MODULE_FLAG >= (verboselevel))

// Redefine the standard assert to use our nice log files
#undef assert
#define assert(x) DLOG_ASSERT(x)

//
// This class more or less represents a particular log message.  You
// create an instance of LogMessage and then stream stuff to it.
// When you finish streaming to it, ~LogMessage is called and the
// full message gets streamed to the appropriate destination.
//
// You shouldn't actually use LogMessage's constructor to log things,
// though.  You should use the LOG() macro (and variants thereof)
// above.
class LogMessage {
public:
  enum {
    // Passing kNoLogPrefix for the line number disables the
    // log-message prefix. Useful for using the LogMessage
    // infrastructure as a printing utility. See also the --log_prefix
    // flag for controlling the log-message prefix on an
    // application-wide basis.
    kNoLogPrefix = -1
  };

  class LogStream : public ostrstream {
  public:
    LogStream(char *buf, int len, int ctr)
      : ostrstream(buf, len),
        ctr_(ctr) {
      self_ = this;
    }

    int ctr() const { return ctr_; }
    void set_ctr(int ctr) { ctr_ = ctr; }
    LogStream* self() const { return self_; }

  private:
    int ctr_;  // Counter hack (for the LOG_EVERY_X() macro)
    LogStream *self_;  // Consistency check hack
  };

public:
  // icc 8 requires this typedef to avoid an internal compiler error.
  typedef void (LogMessage::*SendMethod)();

  LogMessage(const char* file, int line, LogSeverity severity, int ctr,
             SendMethod send_method);

  // Two special constructors that generate reduced amounts of code at
  // LOG call sites for common cases.

  // Used for LOG(INFO): Implied are:
  // severity = INFO, ctr = 0, send_method = &LogMessage::SendToLog.
  //
  // Using this constructor instead of the more complex constructor above
  // saves 19 bytes per call site.
  LogMessage(const char* file, int line);

  // Used for LOG(severity) where severity != INFO.  Implied
  // are: ctr = 0, send_method = &LogMessage::SendToLog
  //
  // Using this constructor instead of the more complex constructor above
  // saves 17 bytes per call site.
  LogMessage(const char* file, int line, LogSeverity severity);

  // Constructor to log this message to a specified sink (if not NULL).
  // Implied are: ctr = 0, send_method = &LogMessage::SendToSinkAndLog if
  // also_send_to_log is true, send_method = &LogMessage::SendToSink otherwise.
  LogMessage(const char* file, int line, LogSeverity severity, LogSink* sink,
             bool also_send_to_log);

  // Constructor where we also give a vector<string> pointer
  // for storing the messages (if the pointer is not NULL).
  // Implied are: ctr = 0, send_method = &LogMessage::SaveOrSendToLog.
  LogMessage(const char* file, int line, LogSeverity severity,
             vector<string>* outvec);

  // Constructor where we also give a string pointer for storing the
  // message (if the pointer is not NULL).  Implied are: ctr = 0,
  // send_method = &LogMessage::WriteToStringAndLog.
  LogMessage(const char* file, int line, LogSeverity severity,
             string* message);

  // A special constructor used for check failures
  LogMessage(const char* file, int line, const CheckOpString& result);

  ~LogMessage();

  // Flush a buffered message to the sink set in the constructor.  Always
  // called by the destructor, it may also be called from elsewhere if
  // needed.  Only the first call is actioned; any later ones are ignored.
  void Flush();

  // An arbitrary limit on the length of a single log message.  This
  // is so that streaming can be done more efficiently.
  static const size_t kMaxLogMessageLen;

  // Theses should not be called directly outside of logging.*,
  // only passed as SendMethod arguments to other LogMessage methods:
  void SendToLog();  // Actually dispatch to the logs
  void SendToSyslogAndLog();  // Actually dispatch to syslog and the logs

  // Call abort() or similar to perform LOG(FATAL) crash.
  // Writes current stack trace to stderr.
  static void Fail() ATTRIBUTE_NORETURN;

  // Same as Fail(), but without writing out the stack trace.
  // It is assumed that the caller has already generated and
  // written the trace as appropriate.
  static void FailWithoutStackTrace() ATTRIBUTE_NORETURN;

  // Similar to FailWithoutStackTrace(), but without abort()ing.
  // Terminates the process with error exit code.
  static void FailQuietly() ATTRIBUTE_NORETURN;

  ostream& stream() { return *(data_->stream_); }

  int preserved_errno() const { return data_->preserved_errno_; }

  // Must be called without the log_mutex held.  (L < log_mutex)
  static int64 num_messages(int severity);

private:
  // Fully internal SendMethod cases:
  void SendToSinkAndLog();  // Send to sink if provided and dispatch to the logs
  void SendToSink();  // Send to sink if provided, do nothing otherwise.

  // Write to string if provided and dispatch to the logs.
  void WriteToStringAndLog();

  void SaveOrSendToLog();  // Save to stringvec if provided, else to logs

  void Init(const char* file, int line, LogSeverity severity,
            void (LogMessage::*send_method)());

  // Used to fill in crash information during LOG(FATAL) failures.
  void RecordCrashReason(base::CrashReason* reason);

  // Counts of messages sent at each priority:
  static int64 num_messages_[NUM_SEVERITIES];  // under log_mutex

  // We keep the data in a separate struct so that each instance of
  // LogMessage uses less stack space.
  struct LogMessageData {
    LogMessageData() {};

    int preserved_errno_;         // errno at Init() time
    scoped_array<char> buf_;      // buffer space for non FATAL messages
    char* message_text_;          // Complete message text
    scoped_ptr<LogStream> stream_alloc_;
    LogStream* stream_;
    char severity_;               // level of LogMessage (ex. I, W, E, F)
    int line_;                    // line number of file that called LOG
    void (LogMessage::*send_method_)();  // Call this in destructor to send
    union {  // At most one of these is used: union to keep the size low.
      LogSink* sink_;             // NULL or sink to send message to
      vector<string>* outvec_;    // NULL or vector to push message onto
      string* message_;           // NULL or string to write message into
    };
    time_t timestamp_;            // Time of creation of LogMessage
    struct tm tm_time_;           // Time of creation of LogMessage
    size_t num_prefix_chars_;     // # of chars of prefix in this message
    size_t num_chars_to_log_;     // # of chars of msg to send to log
    size_t num_chars_to_syslog_;  // # of chars of msg to send to syslog
    const char* basename_;        // basename of file that called LOG
    const char* fullname_;        // fullname of file that called LOG
    bool has_been_flushed_;       // false => data has not been flushed
    bool first_fatal_;            // true => this was first fatal msg

   private:
    DISALLOW_EVIL_CONSTRUCTORS(LogMessageData);
  };

  static LogMessageData fatal_msg_data_exclusive_;
  static LogMessageData fatal_msg_data_shared_;

  scoped_ptr<LogMessageData> allocated_;
  LogMessageData* data_;

  friend class LogDestination;

  DISALLOW_EVIL_CONSTRUCTORS(LogMessage);

protected:
  // Default false; if true, all failures should be as quiet as possible. This
  // is stored in LogMessage, rather than LogMessageData, because all FATAL-
  // level handlers share the same LogMessageData for signal safety reasons.
  bool fail_quietly_;
};

// This class happens to be thread-hostile because all instances share
// a single data buffer, but since it can only be created just before
// the process dies, we don't worry so much.
class LogMessageFatal : public LogMessage {
 public:
  LogMessageFatal(const char* file, int line);
  LogMessageFatal(const char* file, int line, const CheckOpString& result);
  ~LogMessageFatal() ATTRIBUTE_NORETURN;
};

class LogMessageQuietlyFatal : public LogMessage {
 public:
  LogMessageQuietlyFatal(const char* file, int line);
  LogMessageQuietlyFatal(const char* file, int line,
                         const CheckOpString& result);
  ~LogMessageQuietlyFatal() ATTRIBUTE_NORETURN;
};

// A non-macro interface to the log facility; (useful
// when the logging level is not a compile-time constant).
inline void LogAtLevel(int const severity, string const &msg) {
  LogMessage(__FILE__, __LINE__, severity).stream() << msg;
}

// A macro alternative of LogAtLevel. New code may want to use this
// version since there are two advantages: 1. this version outputs the
// file name and the line number where this macro is put like other
// LOG macros, 2. this macro can be used as C++ stream.
#define LOG_AT_LEVEL(severity) LogMessage(__FILE__, __LINE__, severity).stream()

// Helpers for CHECK_NOTNULL(). Two are necessary to support both raw pointers
// and smart pointers.
template <typename T>
T* CheckNotNull(const char *file, int line, const char *names, T* t) {
  return CheckNotNullCommon(file, line, names, t);
}

template <typename T>
T& CheckNotNull(const char *file, int line, const char *names, T& t) {
  return CheckNotNullCommon(file, line, names, t);
}

template <typename T>
T& CheckNotNullCommon(const char *file, int line, const char *names, T& t) {
  if (t == NULL) {
    LogMessageFatal(file, line, new string(names));
  }
  return t;
}

// Allow folks to put a counter in the LOG_EVERY_X()'ed messages. This
// only works if ostream is a LogStream. If the ostream is not a
// LogStream you'll get an assert saying as much at runtime.
ostream& operator<<(ostream &os, const PRIVATE_Counter&);


// We need to be able to stream DocIds.  But if DocIds are the same as
// a built-in type, don't try to redefine things that are already
// defined!
#ifndef NDEBUG
inline ostream& operator<<(ostream& o, const DocId& d) {
  return (o << DocidForPrintf(d));
}

inline ostream& operator<<(ostream& o, const DocId32Bit& d) {
  return (o << Docid32BitForPrintf(d));
}
#endif  // NDEBUG


// Derived class for PLOG*() above.
class ErrnoLogMessage : public LogMessage {
 public:

  ErrnoLogMessage(const char* file, int line, LogSeverity severity, int ctr,
                  void (LogMessage::*send_method)());

  // Postpends ": strerror(errno) [errno]".
  ~ErrnoLogMessage();

 private:

  DISALLOW_EVIL_CONSTRUCTORS(ErrnoLogMessage);
};


// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".

class LogMessageVoidify {
 public:
  LogMessageVoidify() { }
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(ostream&) { }
};


// Flushes all log files that contains messages that are at least of
// the specified severity level.  Thread-safe.
void FlushLogFiles(LogSeverity min_severity);

// Flushes all log files that contains messages that are at least of
// the specified severity level. Thread-hostile because it ignores
// locking -- used for catastrophic failures.
void FlushLogFilesUnsafe(LogSeverity min_severity);

//
// Set the destination to which a particular severity level of log
// messages is sent.  If base_filename is "", it means "don't log this
// severity".  Thread-safe.
//
void SetLogDestination(LogSeverity severity, const char* base_filename);

//
// Set the basename of the symlink to the latest log file at a given
// severity.  If symlink_basename is empty, do not make a symlink.  If
// you don't call this function, the symlink basename is the
// invocation name of the program.  Thread-safe.
//
void SetLogSymlink(LogSeverity severity, const char* symlink_basename);

//
// Used to send logs to some other kind of destination
// Users should subclass LogSink and override send to do whatever they want.
// Implementations must be thread-safe because a shared instance will
// be called from whichever thread ran the LOG(XXX) line.
class LogSink {
 public:
  virtual ~LogSink();

  // Sink's logging logic (message_len is such as to exclude '\n' at the end).
  // This method can't use LOG() or CHECK() as logging system mutex(s) are held
  // during this call.
  virtual void send(LogSeverity severity, const char* full_filename,
                    const char* base_filename, int line,
                    const struct tm* tm_time,
                    const char* message, size_t message_len) = 0;

  // Redefine this to implement waiting for
  // the sink's logging logic to complete.
  // It will be called after each send() returns,
  // but before that LogMessage exits or crashes.
  // By default this function does nothing.
  // Using this function one can implement complex logic for send()
  // that itself involves logging; and do all this w/o causing deadlocks and
  // inconsistent rearrangement of log messages.
  // E.g. if a LogSink has thread-specific actions, the send() method
  // can simply add the message to a queue and wake up another thread that
  // handles real logging while itself making some LOG() calls;
  // WaitTillSent() can be implemented to wait for that logic to complete.
  // See our unittest for an example.
  virtual void WaitTillSent();

  // Returns the normal text output of the log message.
  // Can be useful to implement send().
  static string ToString(LogSeverity severity, const char* file, int line,
                         const struct tm* tm_time,
                         const char* message, size_t message_len);
};

// Add or remove a LogSink as a consumer of logging data.  Thread-safe.
void AddLogSink(LogSink *destination);
void RemoveLogSink(LogSink *destination);

//
// Specify an "extension" added to the filename specified via
// SetLogDestination.  This applies to all severity levels.  It's
// often used to append the port we're listening on to the logfile
// name.  Thread-safe.
//
void SetLogFilenameExtension(const char* filename_extension);

//
// Make it so that all log messages of at least a particular severity
// are logged to stderr (in addition to logging to the usual log
// file(s)).  Thread-safe.
//
void SetStderrLogging(LogSeverity min_severity);

//
// Make it so that all log messages go only to stderr.  Thread-safe.
//
void LogToStderr();

//
// Make it so that all log messages of at least a particular severity are
// logged via email to a list of addresses (in addition to logging to the
// usual log file(s)).  The list of addresses is just a string containing
// the email addresses to send to (separated by spaces, say).
//
// Beyond thread-hostile.  This function enables email logging,
// which calls popen() if any log messages are actually mailed.
// A multi-thread program which calls this function, even in a single thread,
// will randomly hang if it logs any messages which are mailed.
void SetEmailLogging(LogSeverity min_severity, const char* addresses);

//
// Generate a special "status" message.  This will be useful to
// monitoring scripts that want to know about the progress of
// a long-running program.  The two supplied arguments should have
// identical units.  The "done" argument says how much work has
// been completed, and the "total" argument says how much total
// work has to be done.  Thread-hostile if
// FLAGS_status_messages_to_status_file.  Thread-safe otherwise.
//
void StatusMessage(int64 done, int64 total);

// Like StatusMessage(), only writes the status to the file ./STATUS
// Intended to make life easier for processes running on the global
// work queue, where the standard status message file is ./STATUS.
// Thread-hostile.
void GWQStatusMessage(const char* msg);

// A simple function that sends email. dest is a comma-separated
// list of addressess.
//
// Beyond thread-hostile.  This function calls popen().
// A multi-thread program which calls this function, even in a single thread,
// will randomly hang.
bool SendEmail(const char*dest, const char *subject, const char*body);

// Return the set of directories to try generating a log file into.
// Thread-hostile, but expected to only be called from InitGoogle.
const vector<string>& GetLoggingDirectories();

// For tests only:  Clear the internal [cached] list of logging directories to
// force a refresh the next time GetLoggingDirectories is called.
// Thread-hostile.
void TestOnly_ClearLoggingDirectoriesList();

// Returns a set of existing temporary directories, which will be a
// subset of the directories returned by GetLogginDirectories().
// Thread-safe.
void GetExistingTempDirectories(vector<string>* list);

// Print any fatal message again -- useful to call from signal handler
// so that the last thing in the output is the fatal message.
// Thread-hostile, but a race is unlikely.
void ReprintFatalMessage();

// Truncate a log file that may be the append-only output of multiple
// processes and hence can't simply be renamed/reopened (typically a
// stdout/stderr).  If the file "path" is > "limit" bytes, copy the
// last "keep" bytes to offset 0 and truncate the rest. Since we could
// be racing with other writers, this approach has the potential to
// lose very small amounts of data. For security, only follow symlinks
// if the path is /proc/self/fd/*
void TruncateLogFile(const char *path, int64 limit, int64 keep);

// Truncate stdout and stderr if they are over the value specified by
// --max_log_size; keep the final 1MB.  This function has the same
// race condition as TruncateLogFile.
void TruncateStdoutStderr();

// Return the string representation of the provided LogSeverity level.
// Thread-safe.
const char* GetLogSeverityName(LogSeverity severity);

// ---------------------------------------------------------------------
// Implementation details that are not useful to most clients
// ---------------------------------------------------------------------

// A Logger is the interface used by logging modules (base/logging.cc
// and file/logging/blog.cc) to emit entries to a log.  A typical
// implementation will dump formatted data to a sequence of files.  We
// also provide interfaces that will forward the data to another
// thread so that the invoker never blocks.  Implementations should be
// thread-safe since the logging system will write to them from
// multiple threads.

namespace base {

class Logger {
 public:
  virtual ~Logger();

  // Writes "message[0,message_len-1]" corresponding to an event that
  // occurred at "timestamp".  If "force_flush" is true, the log file
  // is flushed immediately.
  //
  // The input message has already been formatted as deemed
  // appropriate by the higher level logging facility.  For example,
  // textual log messages already contain timestamps, and the
  // file:linenumber header.
  virtual void Write(bool force_flush,
                     time_t timestamp,
                     const char* message,
                     int message_len) = 0;

  // Flush any buffered messages
  virtual void Flush() = 0;

  // Get the current LOG file size.
  // The returned value is approximate since some
  // logged data may not have been flushed to disk yet.
  virtual uint32 LogSize() = 0;
};

// Get the logger for the specified severity level.  The logger
// remains the property of the logging module and should not be
// deleted by the caller.  Thread-safe.
extern Logger* GetLogger(LogSeverity level);

// Set the logger for the specified severity level.  The logger
// becomes the property of the logging module and should not
// be deleted by the caller.  Thread-safe.
extern void SetLogger(LogSeverity level, Logger* logger);

}

// glibc has traditionally implemented two incompatible versions of
// strerror_r(). There is a poorly defined convention for picking the
// version that we want, but it is not clear whether it even works with
// all versions of glibc.
// So, instead, we provide this wrapper that automatically detects the
// version that is in use, and then implements POSIX semantics.
// N.B. In addition to what POSIX says, we also guarantee that "buf" will
// be set to an empty string, if this function failed. This means, in most
// cases, you do not need to check the error code and you can directly
// use the value of "buf". It will never have an undefined value.
int posix_strerror_r(int err, char *buf, size_t len);


// A class for which we define operator<<, which does nothing.
class NullStream : public LogMessage::LogStream {
 public:
  // Initialize the LogStream so the messages can be written somewhere
  // (they'll never be actually displayed). This will be needed if a
  // NullStream& is implicitly converted to LogStream&, in which case
  // the overloaded NullStream::operator<< will not be invoked.
  NullStream() : LogMessage::LogStream(message_buffer_, 1, 0) { }
  NullStream(const char* /*file*/, int /*line*/,
             const CheckOpString& /*result*/) :
      LogMessage::LogStream(message_buffer_, 1, 0) { }
  NullStream &stream() { return *this; }
 private:
  // A very short buffer for messages (which we discard anyway). This
  // will be needed if NullStream& converted to LogStream& (e.g. as a
  // result of a conditional expression).
  char message_buffer_[2];
};

// Do nothing. This operator is inline, allowing the message to be
// compiled away. The message will not be compiled away if we do
// something like (flag ? LOG(INFO) : LOG(ERROR)) << message; when
// SKIP_LOG=WARNING. In those cases, NullStream will be implicitly
// converted to LogStream and the message will be computed and then
// quietly discarded.
template<class T>
inline NullStream& operator<<(NullStream &str, const T &value) { return str; }

// Similar to NullStream, but aborts the program (without stack
// trace), like LogMessageFatal.
class NullStreamFatal : public NullStream {
 public:
  NullStreamFatal() { }
  NullStreamFatal(const char* file, int line, const CheckOpString& result) :
      NullStream(file, line, result) { }
  ~NullStreamFatal() ATTRIBUTE_NORETURN { _exit(1); }
};

#endif // _LOGGING_H_
