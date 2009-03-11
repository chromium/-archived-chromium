// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
