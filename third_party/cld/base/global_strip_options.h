// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Macros for stripping unnecessary string literals from binaries
// (especially for shipping outside of Google).

#ifndef BASE_GLOBAL_STRIP_OPTIONS_H_
#define BASE_GLOBAL_STRIP_OPTIONS_H_

// The global value of STRIP_LOG. All the messages logged to LOG(XXX)
// with severity less than STRIP_LOG will not be displayed. If it can
// be determined at compile time that the message will not be printed,
// the statement will be compiled out.  STRIP_LOG can be set to a value
// between 0 and 4 where 0 logs all messages and 4 logs no messages.
//
// Example: to strip out all INFO and WARNING messages, use the value
// of 2 below. To make an exception for WARNING messages from a single
// file, add "#define STRIP_LOG 1" to that file _before_ including
// base/logging.h
//
// Example: In addition it's possible to remove the dependency on the base
// library if an executable or library currently only depends upon logging.
//
// # A library that only includes "base/basictypes.h" and "base/logging.h".
// cc_library(name = "mylibrary",
//            srcs = [ "mymodule_that_logs.cc" ],
//            deps = [ "//base" ])
//
// The build rule for mylibrary can be modified as shown in the following...
//
// # A library with logging disabled.
// cc_library(name = "mylibrary_no_logging",
//            srcs = [ "mymodule_that_logs.cc",
//                     "/base:logging.h" ],
//            deps = [ "//third_party/stl" ],
//            copts = [ "-DSTRIP_LOG=4" ] )
//
// Finally if it's desirable to strip all logging from an executable build
// using...
//
// blaze build --copts="-DSTRIP_LOG=4" //mypackage:mylabel


#ifndef STRIP_LOG
#define STRIP_LOG 0
#endif

// By setting STRIP_FLAG_HELP to 1, we will replace the usage messages
// for command-line flags with "" (thus taking those string literals
// out of the binary). To make an exception for flags DEFINE'd in a
// certain file, add "#define STRIP_FLAG_HELP 0" to that file _before_
// including base/commandlineflags.h

#ifndef STRIP_FLAG_HELP
#define STRIP_FLAG_HELP 0
#endif

#endif  // BASE_GLOBAL_STRIP_OPTIONS_H_
