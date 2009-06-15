// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains a very small number of declarations that should be
// included in most programs.
//
// Sets the usage message to "*usage", parses the command-line flags,
// and initializes various other pieces of global state.  If running
// as root, this routine attempts to setuid to FLAGS_uid, which defaults
// to "nobody".
//
// It also logs all the program's command-line flags to the INFO log file.
//
// If the global variable FLAGS_uid is not equal to the empty string,
// then InitGoogle also does an 'su' to the user specified in
// FLAGS_uid, and sets the group equal to FLAGS_gid.
//
// The functions in here are thread-safe unless specified otherwise,
// but they must be called after InitGoogle() (or the
// InitGoogleExceptChangeRootAndUser/ChangeRootAndUser pair).

#ifndef _GOOGLE_H_
#define _GOOGLE_H_

#include <iosfwd>               // to forward declare ostream
#include "base/basictypes.h"
#include "third_party/cld/base/closure.h"
#include "third_party/cld/base/googleinit.h"

class Closure;

// A more-convenient way to access gethostname()
const char* Hostname();
// Return kernel version string in /proc/version
const char* GetKernelVersionString();
// Return true and set kernel version (x.y.z), patch level and revision (#p.r),
// false, if not known.
struct KernelVersion {
  int major;                          // Major release
  int minor;                          // Minor release
  int micro;                          // Whatever the third no. is called ...
  int patch;                          // Patch level
  int revision;                       // Patch revision
};
bool GetKernelVersion(KernelVersion* kv);
// A string saying when InitGoogle() was called -- probably program start
const char* GetStartTime();
// time in ms since InitGoogle() was called --
int64 GetUptime();
// the pid for the startup thread
int32 GetMainThreadPid();

// the resource limit for core size when InitGoogle() was called.
const struct rlimit* GetSavedCoreLimit();

// Restore the core size limit saved in InitGoogle(). This is a no-op if
// FLAGS_allow_kernel_coredumps is true.
int32 RestoreSavedCoreLimit();

// Return true if we have determined that all CPUs have the same timing
// (same model, clock rate, stepping).  Returns true if there is only one
// CPU.  Returns false if we cannot read or parse /proc/cpuinfo.
bool CPUsHaveSameTiming(
    const char *cpuinfo          = "/proc/cpuinfo",
    const char *cpuinfo_max_freq = "/sys/devices/system/cpu/cpu%d/"
                                   "cpufreq/cpuinfo_max_freq");

// FlagsParsed is called once for every run VLOG() call site.
// Returns true if command line flags have been parsed
bool FlagsParsed();

// A place-holder module initializer to declare initialization ordering
// with respect to it to make chosen initalizers run before command line flag
// parsing (see googleinit.h for more details).
DECLARE_MODULE_INITIALIZER(command_line_flags_parsing);

// Checks (only in debug mode) if main() has been started and crashes if not
// i.e. makes sure that we are out of the global constructor execution stage.
// Intended to for checking that some code should not be executed during
// global object construction (only specially crafted code might be safe
// to execute at that time).
void AssertMainHasStarted();

// Call this from main() if AssertMainHasStarted() is incorrectly failing
// for your code (its current implmentation relies on a call to InitGoogle()
// as the signal that we have reached main(), hence it is not 100% accurate).
void SetMainHasStarted();

// Checks (only in debug mode) if InitGoogle() has been fully executed
// and crashes if it has not been.
// Indtended for checking that code that depends on complete execution
// of InitGoogle() for its proper functioning is safe to execute.
void AssertInitGoogleIsDone();

// Initializes misc google-related things in the binary.
// In particular it does REQUIRE_MODULE_INITIALIZED(command_line_flags_parsing)
// parses command line flags and does RUN_MODULE_INITIALIZERS() (in that order).
// If a flag is defined more than once in the command line or flag
// file, the last definition is used.
// Typically called early on in main() and must be called before other
// threads start using functions from this file.
//
// 'usage' provides a short usage message passed to SetUsageMessage().
// 'argc' and 'argv' are the command line flags to parse.
// If 'remove_flags' then parsed flags are removed.
void InitGoogle(const char* usage, int* argc, char*** argv, bool remove_flags);

// Normally, InitGoogle will chroot (if requested with the --chroot flag)
// and setuid to --uid and --gid (default nobody).
// This version will not, and you will be responsible for calling
// ChangeRootAndUser
// This option is provided for applications that need to read files outside
// the chroot before chrooting.
void InitGoogleExceptChangeRootAndUser(const char* usage, int* argc,
                                       char*** argv, bool remove_flags);
// Thread-hostile.
void ChangeRootAndUser();

// if you need to flush InitGoogle's resources from a sighandler
void SigHandlerFlushInitGoogleResources();

// Alter behavior of error to not dump core on an error.
// Simply cleanup and exit.  Thread-hostile.
void SetNoCoreOnError();

// limit the amount of physical memory used by this process to a
// fraction of the available physical memory. The process is killed if
// it tries to go beyond this limit. If randomize is set, we reduce
// the fraction a little in a sort-of-random way. randomize is meant
// to be used for applications which run many copies -- by randomizing
// the limit, we can avoid having all copies of the application hit
// the limit (and die) at the same time.
void LimitPhysicalMemory(double fraction, bool randomize);

// Return the limit set on physical memory, zero if error or no limit set.
uint64 GetPhysicalMemoryLimit();

// Add specified closure to the set of closures which are executed
// when the program dies a horrible death (signal, etc.)
//
// Note: These are not particularly efficient.  Use sparingly.
// Note: you can't just use atexit() because functions registered with
// atexit() are supposedly only called on normal program exit, and we
// want to do things like flush logs on failures.
void RunOnFailure(Closure* closure);

// Remove specified closure references from the set created by RunOnFailure.
void CancelRunOnFailure(Closure* closure);

// Adds specified Callback2 instances to a set of callbacks that are
// executed when the program crashes. Two values: signo and ucontext_t*
// will be passed into these callback functions. We use void* to avoid the
// use of ucontext_t on non-POSIX systems.
//
// Note: it is recommended that these callbacks are signal-handler
// safe. Also, the calls of these callbacks are not protected by
// a mutex, so they are better to be multithread-safe.
void RunOnFailureCallback2(Callback2<int, void*>* callback);
void CancelRunOnFailureCallback2(Callback2<int, void*>* callback);

// Return true if the google default signal handler is running, false
// otherwise.  Sometimes callbacks specified with
// RunOnFailure{,Callback2} are not called because the process hangs
// or takes too long to symbolize callstacks. Users may want to
// augment the RunOnFailure mechanism with a dedicated thread which
// polls the below function periodically (say, every second) and runs
// their failure closures when it returns true.
bool IsFailureSignalHandlerRunning();

// Type of function used for printing in stack trace dumping, etc.
// We avoid closures to keep things simple.
typedef void DebugWriter(const char*, void*);

// A few useful DebugWriters
DebugWriter DebugWriteToStderr;
DebugWriter DebugWriteToStream;
DebugWriter DebugWriteToFile;
DebugWriter DebugWriteToString;

// Dump current stack trace omitting the topmost 'skip_count' stack frames.
void DumpStackTrace(int skip_count, DebugWriter *w, void* arg);

// Dump given pc and stack trace.
void DumpPCAndStackTrace(void *pc, void *stack[], int depth,
                         DebugWriter *writerfn, void *arg);

// Returns the program counter from signal context, NULL if unknown.
// vuc is a ucontext_t *.  We use void* to avoid the use
// of ucontext_t on non-POSIX systems.
void* GetPC(void* vuc);

// Dump current address map.
void DumpAddressMap(DebugWriter *w, void* arg);

// Dump information about currently allocated memory.
void DumpMallocStats(DebugWriter *w, void* arg);

// Return true if currently executing in the google failure signal
// handler. If this returns true you should:
//
// - avoid allocating anything via malloc/new
// - assume that your stack limit is SIGSTKSZ
// - assume that no other thread can be executing in the failure handler
bool InFailureSignalHandler();

// Return the alternate signal stack size (in bytes) needed in order to
// safely run the failure signal handlers.  The returned value will
// always be a multiple of the system page size.
int32 GetRequiredAlternateSignalStackSize();

#endif // _GOOGLE_H_
