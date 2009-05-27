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


// File declaring helper functions for conversion between ActiveX and
// NPAPI variant types.

#ifndef O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_VARIANT_UTILS_H_
#define O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_VARIANT_UTILS_H_

#include "plugin/npapi_host_control/win/np_browser_proxy.h"
#include "plugin/npapi_host_control/win/np_object_proxy.h"

// Converts an ActiveX variant to an NPAPI variant.
// Parameters:
//    browser_proxy:  The emulated NPAPI browser environment, required for
//                    managing NPAPI string resource construction, etc.
//    source:  The source COM VARIANT.
//    destination: The NPAPI variant to receive the value stored in the source.
//                 On failure, the destination will be empty.
void VariantToNPVariant(NPBrowserProxy* browser_proxy,
                        const VARIANT* source,
                        NPVariant* destination);


// Converts a NPAPI variant to an ActiveX variant.
// Parameters:
//    browser_proxy:  The emulated NPAPI browser environment, required for
//                    managing NPAPI string resource construction, etc.
//    source:  The source NPAPI variant.
//    destination: The COM VARIANT to receive the value stored in the source.
//                 On failure, the destination will be empty.
void NPVariantToVariant(NPBrowserProxy* browser_proxy,
                        const NPVariant* source,
                        CComVariant* destination);

#endif  // O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_VARIANT_UTILS_H_
