// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the VLOG_IS_ON macro that controls the variable-verbosity
// conditional logging.
//
// It's used by VLOG and VLOG_IF in logging.h
// and by RAW_VLOG in raw_logging.h to trigger the logging.
//
// It can also be used directly e.g. like this:
//   if (VLOG_IS_ON(2)) {
//     // do some logging preparation and logging
//     // that can't be accomplished e.g. via just VLOG(2) << ...;
//   }
//
// The truth value that VLOG_IS_ON(level) returns is determined by
// the three verbosity level flags:
//   --v=<n>  Gives the default maximal active V-logging level;
//            0 is the default.
//            Normally positive values are used for V-logging levels.
//   --vmodule=<str>  Gives the per-module maximal V-logging levels to override
//                    the value given by --v.
//                    E.g. "my_module=2,foo*=3" would change the logging level
//                    for all code in source files "my_module.*" and "foo*.*"
//                    ("-inl" suffixes are also disregarded for this matching).
//   --silent_init  When true has the effect of increasing
//                  the argument of VLOG_IS_ON by 1,
//                  thus suppressing one more level of verbose logging.
//
// SetVLOGLevel helper function is provided to do limited dynamic control over
// V-logging by overriding the per-module settings given via --vmodule flag.
//
// CAVEAT: --vmodule functionality is not available in non gcc compilers.
//

#ifndef BASE_VLOG_IS_ON_H_
#define BASE_VLOG_IS_ON_H_

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/port.h"
#include "third_party/cld/base/commandlineflags.h"
#include "third_party/cld/base/log_severity.h"

DECLARE_int32(v);  // in vlog_is_on.cc
DECLARE_bool(silent_init);  // in google.cc

#if defined(__GNUC__)
// We pack an int16 verbosity level and an int16 epoch into an
// Atomic32 at every VLOG_IS_ON() call site.  The level determines
// whether the site should log, and the epoch determines whether the
// site is stale and should be reinitialized.  A verbosity level of
// kUseFlag (-1) indicates that the value of FLAGS_v should be used as
// the verbosity level.  When the site is (re)initialized, a verbosity
// level for the current source file is retrieved from an internal
// list.  This list is mutated through calls to SetVLOGLevel() and
// mutations to the --vmodule flag.  New log sites are initialized
// with a stale epoch and a verbosity level of kUseFlag.
//
// TODO(llansing): Investigate using GCC's __builtin_constant_p() to
// generate less code at call sites where verbositylevel is known to
// be a compile-time constant.
#define VLOG_IS_ON(verboselevel)                                               \
  ({ static Atomic32 site__ = ::base::internal::kDefaultSite;                  \
     ::base::internal::VLogEnabled(&site__, (verboselevel), __FILE__); })
#else
// GNU extensions not available, so we do not support --vmodule.
// Dynamic value of FLAGS_v always controls the logging level.
//
// TODO(llansing): Investigate supporting --vmodule on other platforms.
#define VLOG_IS_ON(verboselevel)                                               \
  (FLAGS_v >= (verboselevel) + FLAGS_silent_init)
#endif

// Set VLOG(_IS_ON) level for module_pattern to log_level.
// This lets us dynamically control what is normally set by the --vmodule flag.
// Returns the level that previously applied to module_pattern.
// NOTE: To change the log level for VLOG(_IS_ON) sites
//       that have already executed after/during InitGoogle,
//       one needs to supply the exact --vmodule pattern that applied to them.
//       (If no --vmodule pattern applied to them
//       the value of FLAGS_v will continue to control them.)
int SetVLOGLevel(const char* module_pattern, int log_level);

// Private implementation details.  No user-serviceable parts inside.
namespace base {
namespace internal {

// Each log site determines whether its log level is up to date by
// comparing its epoch to this global epoch.  Whenever the program's
// vmodule configuration changes (ex: SetVLOGLevel is called), the
// global epoch is advanced, invalidating all site epochs.
extern Atomic32 vlog_epoch;

// A log level of kUseFlag means "read the logging level from FLAGS_v."
const int kUseFlag = -1;

// Log sites use FLAGS_v by default, and have an initial epoch of 0.
const Atomic32 kDefaultSite = kUseFlag << 16;

// The global epoch is the least significant half of an Atomic32, and
// may only be accessed through atomic operations.
inline Atomic32 GlobalEpoch() { return Acquire_Load(&vlog_epoch) & 0x0000FFFF; }

// The least significant half of a site is the epoch.
inline int SiteEpoch(Atomic32 site) { return site & 0x0000FFFF; }

// The most significant half of a site is the logging level.
inline int SiteLevel(Atomic32 site) { return site >> 16; }

// Construct a logging site from a logging level and epoch.
inline Atomic32 Site(int level, int epoch) {
  return ((level & 0x0000FFFF) << 16) | (epoch & 0x0000FFFF);
}

// Attempt to initialize or reinitialize a VLOG site.  Returns the
// level of the log site, regardless of whether the attempt succeeds
// or fails.
//   site: The address of the log site's state.
//   fname: The filename of the current source file.
int InitVLOG(Atomic32* site, const char* fname);

// Determine whether verbose logging should occur at a given log site.
//
// TODO(llansing): Find a way to eliminate FLAGS_silent_init from this
// function while preserving the silent initialization behavior.  The
// common-case code path shouldn't pay for silent initialization.
inline bool VLogEnabled(Atomic32* site, int32 level, const char* const file) {
  const Atomic32 site_copy = Acquire_Load(site);
  const int32 site_level =
      PREDICT_TRUE(SiteEpoch(site_copy) == GlobalEpoch()) ?
      SiteLevel(site_copy) : InitVLOG(site, file);
  return (site_level == kUseFlag ? FLAGS_v : site_level) >=
      (level + FLAGS_silent_init);
}

}  // namespace internal
}  // namespace base

#endif  // BASE_VLOG_IS_ON_H_
