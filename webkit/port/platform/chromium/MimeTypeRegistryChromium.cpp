// Copyright (c) 2008, Google Inc.
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

#include "config.h"
#include "ChromiumBridge.h"
#include "MIMETypeRegistry.h"

namespace WebCore 
{

// From MIMETypeRegistryMac.mm.
#if PLATFORM(DARWIN) && PLATFORM(CG)
String getMIMETypeForUTI(const String & uti)
{
    CFStringRef utiref = uti.createCFString();
    CFStringRef mime = UTTypeCopyPreferredTagWithClass(utiref, kUTTagClassMIMEType);
    String mimeType = mime;
    if (mime)
        CFRelease(mime);
    CFRelease(utiref);
    return mimeType;
}
#endif

// Returns the file extension if one is found.  Does not include the dot in the
// filename.  E.g., 'html'.
// NOTE: This does not work in the sandbox because the renderer doesn't have
// access to the Windows Registry.
String MIMETypeRegistry::getPreferredExtensionForMIMEType(const String& type)
{
    // Prune out any parameters in case they happen to have snuck in there...
    // TODO(darin): Is this really necessary??
    String mimeType = type.substring(0, static_cast<unsigned>(type.find(';')));

    String ext = ChromiumBridge::preferredExtensionForMIMEType(type);
    if (!ext.isEmpty() && ext[0] == L'.')
        ext = ext.substring(1);

    return ext;
}

String MIMETypeRegistry::getMIMETypeForExtension(const String &ext)
{
    return ChromiumBridge::mimeTypeForExtension(ext);
}

}
