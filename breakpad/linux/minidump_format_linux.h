// Copyright (c) 2009, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CLIENT_LINUX_MINIDUMP_FORMAT_LINUX_H_
#define CLIENT_LINUX_MINIDUMP_FORMAT_LINUX_H_

// These are additional minidump stream values which are specific to the linux
// breakpad implementation.
enum {
  MD_LINUX_CPU_INFO              = 0x47670003,    /* /proc/cpuinfo    */
  MD_LINUX_PROC_STATUS           = 0x47670004,    /* /proc/$x/status  */
  MD_LINUX_LSB_RELEASE           = 0x47670005,    /* /etc/lsb-release */
  MD_LINUX_CMD_LINE              = 0x47670006,    /* /proc/$x/cmdline */
  MD_LINUX_ENVIRON               = 0x47670007,    /* /proc/$x/environ */
  MD_LINUX_AUXV                  = 0x47670008,    /* /proc/$x/auxv    */
};

#endif  // CLIENT_LINUX_MINIDUMP_FORMAT_LINUX_H_
