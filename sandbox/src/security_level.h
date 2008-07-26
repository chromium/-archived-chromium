// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
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

#ifndef SANDBOX_SRC_RESTRICTED_SECURITY_LEVEL_H__
#define SANDBOX_SRC_RESTRICTED_SECURITY_LEVEL_H__

namespace sandbox {

// List of all the integrity levels supported in the sandbox. This is used
// only on Windows Vista. You can't set the integrity level of the process
// in the sandbox to a level higher than yours.
enum IntegrityLevel {
  INTEGRITY_LEVEL_SYSTEM,
  INTEGRITY_LEVEL_HIGH,
  INTEGRITY_LEVEL_MEDIUM,
  INTEGRITY_LEVEL_MEDIUM_LOW,
  INTEGRITY_LEVEL_LOW,
  INTEGRITY_LEVEL_BELOW_LOW,
  INTEGRITY_LEVEL_LAST
};

// The Token level specifies a set of  security profiles designed to
// provide the bulk of the security of sandbox.
//
//  TokenLevel                 |Restricting   |Deny Only       |Privileges|
//                             |Sids          |Sids            |          |
// ----------------------------|--------------|----------------|----------|
// USER_LOCKDOWN               | Null Sid     | All            | None     |
// ----------------------------|--------------|----------------|----------|
// USER_RESTRICTED             | RESTRICTED   | All            | Traverse |
// ----------------------------|--------------|----------------|----------|
// USER_LIMITED                | Users        | All except:    | Traverse |
//                             | Everyone     | Users          |          |
//                             | RESTRICTED   | Everyone       |          |
//                             |              | Interactive    |          |
// ----------------------------|--------------|----------------|----------|
// USER_INTERACTIVE            | Users        | All except:    | Traverse |
//                             | Everyone     | Users          |          |
//                             | RESTRICTED   | Everyone       |          |
//                             | Owner        | Interactive    |          |
//                             |              | Local          |          |
//                             |              | Authent-users  |          |
//                             |              | User           |          |
// ----------------------------|--------------|----------------|----------|
// USER_NON_ADMIN              | None         | All except:    | Traverse |
//                             |              | Users          |          |
//                             |              | Everyone       |          |
//                             |              | Interactive    |          |
//                             |              | Local          |          |
//                             |              | Authent-users  |          |
//                             |              | User           |          |
// ----------------------------|--------------|----------------|----------|
// USER_RESTRICTED_SAME_ACCESS | All          | None           | All      |
// ----------------------------|--------------|----------------|----------|
// USER_UNPROTECTED            | None         | None           | All      |
// ----------------------------|--------------|----------------|----------|
//
// The above restrictions are actually a transformation that is applied to
// the existing broker process token. The resulting token that will be
// applied to the target process depends both on the token level selected
// and on the broker token itself.
//
//  The LOCKDOWN and RESTRICTED are designed to allow access to almost
//  nothing that has security associated with and they are the recommended
//  levels to run sandboxed code specially if there is a chance that the
//  broker is process might be started by a user that belongs to the Admins
//  or power users groups.
enum TokenLevel {
   USER_LOCKDOWN = 0,
   USER_RESTRICTED,
   USER_LIMITED,
   USER_INTERACTIVE,
   USER_NON_ADMIN,
   USER_RESTRICTED_SAME_ACCESS,
   USER_UNPROTECTED
};

// The Job level specifies a set of decreasing security profiles for the
// Job object that the target process will be placed into.
// This table summarizes the security associated with each level:
//
//  JobLevel        |General                            |Quota               |
//                  |restrictions                       |restrictions        |
// -----------------|---------------------------------- |--------------------|
// JOB_UNPROTECTED  | None                              | *Kill on Job close.|
// -----------------|---------------------------------- |--------------------|
// JOB_INTERACTIVE  | *Forbid system-wide changes using |                    |
//                  |  SystemParametersInfo().          | *Kill on Job close.|
//                  | *Forbid the creation/switch of    |                    |
//                  |  Desktops.                        |                    |
//                  | *Forbids calls to ExitWindows().  |                    |
// -----------------|---------------------------------- |--------------------|
// JOB_LIMITED_USER | Same as INTERACTIVE_USER plus:    | *One active process|
//                  | *Forbid changes to the display    |  limit.            |
//                  |  settings.                        | *Kill on Job close.|
// -----------------|---------------------------------- |--------------------|
// JOB_RESTRICTED   | Same as LIMITED_USER plus:        | *One active process|
//                  | * No read/write to the clipboard. |  limit.            |
//                  | * No access to User Handles that  | *Kill on Job close.|
//                  |   belong to other processes.      |                    |
//                  | * Forbid message broadcasts.      |                    |
//                  | * Forbid setting global hooks.    |                    |
//                  | * No access to the global atoms   |                    |
//                  |   table.                          |                    |
// -----------------|-----------------------------------|--------------------|
// JOB_LOCKDOWN     | Same as RESTRICTED                | *One active process|
//                  |                                   |  limit.            |
//                  |                                   | *Kill on Job close.|
//                  |                                   | *Kill on unhandled |
//                  |                                   |  exception.        |
//                  |                                   |                    |
// In the context of the above table, 'user handles' refers to the handles of
// windows, bitmaps, menus, etc. Files, treads and registry handles are kernel
// handles and are not affected by the job level settings.
enum JobLevel {
  JOB_LOCKDOWN = 0,
  JOB_RESTRICTED,
  JOB_LIMITED_USER,
  JOB_INTERACTIVE,
  JOB_UNPROTECTED
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_RESTRICTED_SECURITY_LEVEL_H__
