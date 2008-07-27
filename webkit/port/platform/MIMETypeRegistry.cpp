/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CString.h"
#include "MIMETypeRegistry.h"
#include "StringHash.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_util.h"

namespace WebCore
{

// NOTE: the following methods are unimplemented
// (and will give linker error if used):
//   HashSet<String> &MIMETypeRegistry::getSupportedImageMIMETypes()
//   HashSet<String> &MIMETypeRegistry::getSupportedImageResourceMIMETypes()
//   HashSet<String> &MIMETypeRegistry::getSupportedNonImageMIMETypes()
// These methods are referenced by WebKit but not WebCore.
// Therefore defering their implementation until necessary.
// Since the returned HashSet is mutable, chrome would need to synchronize
// the mime type registry between renderer/browser.

// Checks if any of the plugins handle this extension, and if so returns the
// plugin's mime type for this extension.  Otherwise returns an empty string.
String GetPluginMimeTypeFromExtension(const String& extension);

String MIMETypeRegistry::getMIMETypeForPath(const String& path)
{
    int pos = path.reverseFind('.');
    if (pos < 0)
        return "application/octet-stream";
    String extension = path.substring(pos + 1);
    String mimeType = getMIMETypeForExtension(extension);
    if (mimeType.isEmpty()) {
        // If there's no mimetype registered for the extension, check to see
        // if a plugin can handle the extension.
        mimeType = GetPluginMimeTypeFromExtension(extension);
    }
    return mimeType;
}

bool MIMETypeRegistry::isSupportedImageMIMEType(const String& mimeType)
{ 
    return !mimeType.isEmpty()
        && mime_util::IsSupportedImageMimeType(mimeType.latin1().data()); 
}

bool MIMETypeRegistry::isSupportedJavaScriptMIMEType(const String& mimeType)
{
    return !mimeType.isEmpty()
        && mime_util::IsSupportedJavascriptMimeType(mimeType.latin1().data()); 
}

bool MIMETypeRegistry::isSupportedImageResourceMIMEType(const String& mimeType)
{ 
    return isSupportedImageMIMEType(mimeType); 
}
    
bool MIMETypeRegistry::isSupportedNonImageMIMEType(const String& mimeType)
{
   return !mimeType.isEmpty()
       && mime_util::IsSupportedNonImageMimeType(mimeType.latin1().data()); 
}

bool MIMETypeRegistry::isJavaAppletMIMEType(const String& mimeType)
{
    // Since this set is very limited and is likely to remain so we won't bother with the overhead
    // of using a hash set.
    // Any of the MIME types below may be followed by any number of specific versions of the JVM,
    // which is why we use startsWith()
    return mimeType.startsWith("application/x-java-applet", false) 
        || mimeType.startsWith("application/x-java-bean", false) 
        || mimeType.startsWith("application/x-java-vm", false);
}

}  // namespace WebCore
