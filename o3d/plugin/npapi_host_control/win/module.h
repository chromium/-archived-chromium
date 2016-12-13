/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the ATL module class used by the O3D host ActiveX control.

#ifndef O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_MODULE_H_
#define O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_MODULE_H_

#include "npapi_host_control.h"

class NPAPIHostControlModule : public CAtlDllModuleT<NPAPIHostControlModule> {
 public:
  NPAPIHostControlModule() { InitializeCriticalSection(&cs_); }
  virtual ~NPAPIHostControlModule() { DeleteCriticalSection(&cs_); }

  // Routine used to serialize threads executing within the control.  Enters
  // a critical section shared for the process hosting the control.
  static void LockModule() {
    EnterCriticalSection(&GetGlobalInstance()->cs_);
  }

  // Routine used to serialize threads executing within the control.  Leaves
  // the critical section entered in lock_module().
  static void UnlockModule() {
    LeaveCriticalSection(&GetGlobalInstance()->cs_);
  }

  // Accessor routine for the global pointer _pAtlModule maintained by ATL.
  static NPAPIHostControlModule* GetGlobalInstance() {
    return static_cast<NPAPIHostControlModule*>(_pAtlModule);
  }

  DECLARE_LIBID(LIBID_npapi_host_controlLib)
 private:
  CRITICAL_SECTION cs_;
  DISALLOW_COPY_AND_ASSIGN(NPAPIHostControlModule);
};

#endif  // O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_MODULE_H_
