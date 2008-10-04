#!/usr/bin/bash -x
#
# In order to build KJS or V8 versions of Chrome, we need to create
# a custom configuration header.  This script creates it.
#
# Input
#   create-config.sh <OutputDir> <kjs|v8>
#
# Output
#   in the $Output\WebCore directory, creates a config.h
#   custom to the desired build setup
#
set -ex
#
# Step 1: Create the webkit config.h which is appropriate for our 
#         JavaScript engine.
#
if [[ "${OS}" != "Windows_NT" ]]
then
    WebCoreObjDir="$1/WebCore"
    JSHeadersDir="$1/WebCore/JavaScriptHeaders"
    CP="cp -p"
else
    WebCoreObjDir="$1\obj\WebCore"
    JSHeadersDir="$1\obj\WebCore\JavaScriptHeaders"
    CP="cp"
fi
mkdir -p "$WebCoreObjDir"
rm -f $WebCoreObjDir/definitions.h 2> /dev/null


if [[ "$2" = "kjs" ]]
then
  SubDir=/kjs
  cat > $WebCoreObjDir/definitions.h << -=EOF=-
#define WTF_USE_JAVASCRIPTCORE_BINDINGS 1
#define WTF_USE_NPOBJECT 1
-=EOF=-
else 
  SubDir=/v8
  cat > $WebCoreObjDir/definitions.h << -=EOF=-
#define WTF_USE_V8_BINDING 1
#define WTF_USE_NPOBJECT 1
-=EOF=-
fi

if [[ "${OS}" = "Windows_NT" ]]
then
  SubDir=
fi

mkdir -p "${WebCoreObjDir}${SubDir}"

pwd
cat ../../config.h.in $WebCoreObjDir/definitions.h > $WebCoreObjDir$SubDir/config.h.new
if [[ "${OS}" = "Windows_NT" ]] || \
   ! diff -q $WebCoreObjDir$SubDir/config.h.new $WebCoreObjDir$SubDir/config.h >& /dev/null
then
  mv $WebCoreObjDir$SubDir/config.h.new $WebCoreObjDir$SubDir/config.h
else
  rm $WebCoreObjDir$SubDir/config.h.new
fi

rm -f "${WebCoreObjDir}/definitions.h"

#
# Step 2: Populate the JavaScriptHeaders based on the selected
#         JavaScript engine.
#
JSHeadersDir="${JSHeadersDir}${SubDir}"
mkdir -p $JSHeadersDir
JavaScriptCoreSrcDir="../../../third_party/WebKit/JavaScriptCore"
WebCoreSrcDir="../../../third_party/WebKit/WebCore"
if [[ "$2" = "kjs" ]]
then
  mkdir -p $JSHeadersDir/JavaScriptCore
  # Note, we'd like to copy dir/*.h here, but the directories
  # are poluted with files like Node.h.  Copying the Node.h
  # from these directories is fatal.  So, we have to hand copy just
  # the right list of files.  Hooray!
  $CP $JavaScriptCoreSrcDir/API/APICast.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/API/JSBase.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/API/JSValueRef.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/API/JSObjectRef.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/API/JSContextRef.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/API/JSStringRef.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/API/WebKitAvailability.h $JSHeadersDir/JavaScriptCore

  $CP $JavaScriptCoreSrcDir/kjs/JSImmediate.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/kjs/JSLock.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/kjs/JSType.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/kjs/collector.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/kjs/interpreter.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/kjs/protect.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/kjs/ustring.h $JSHeadersDir/JavaScriptCore
  $CP $JavaScriptCoreSrcDir/kjs/JSValue.h $JSHeadersDir/JavaScriptCore

  $CP $JavaScriptCoreSrcDir/wtf/HashCountedSet.h $JSHeadersDir/JavaScriptCore
else 
  $CP $WebCoreSrcDir/bridge/npapi.h $JSHeadersDir
  $CP $WebCoreSrcDir/bridge/npruntime.h $JSHeadersDir
  $CP ../../../webkit/port/bindings/v8/npruntime_priv.h $JSHeadersDir
fi

if [[ "${OS}" = "Windows_NT" ]]
then
  $CP $JavaScriptCoreSrcDir/os-win32/stdint.h $JSHeadersDir
fi
