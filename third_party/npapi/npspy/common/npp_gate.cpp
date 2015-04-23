/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "xp.h"

#include "epmanager.h"
#include "plugload.h"
#include "logger.h"

extern NPNetscapeFuncs NPNFuncs;
extern Logger * logger;

NPNetscapeFuncs fakeNPNFuncs;

extern NPPEntryPointManager * epManager;

jref NPP_GetJavaClass (void)
{
  if(logger)
    logger->logCall(action_npp_get_java_class);

  if(logger)
    logger->logReturn(action_npp_get_java_class);
  return NULL;
}

NPError NPP_New(NPMIMEType pluginType,
                NPP instance,
                uint16 mode,
                int16 argc,
                char* argn[],
                char* argv[],
                NPSavedData* saved)
{   
  if(epManager == NULL)
    return NPERR_GENERIC_ERROR;
  
  if(instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(logger)
    logger->logCall(action_npp_new, (DWORD)pluginType, (DWORD)instance, (DWORD)mode, (DWORD)argc, (DWORD)argn, (DWORD)argv, (DWORD)saved);

/* now action begins */

  if(NULL == epManager->findEntryPointsForPlugin(pluginType))
  {
    // if it is first time in, we don't have it yet
    // scan plugins dir for available plugins to see if we have anything 
    // for the given mimetype
    XP_HLIB hLib = LoadRealPlugin(pluginType);
    if(!hLib)
    {
      // what do we do if we don't?
      return NPERR_GENERIC_ERROR;
    }

    NP_GETENTRYPOINTS real_NP_GetEntryPoints = (NP_GETENTRYPOINTS)XP_GetSymbol(hLib, "NP_GetEntryPoints");
    if(!real_NP_GetEntryPoints)
      return NPERR_GENERIC_ERROR;

    NP_INITIALIZE real_NP_Initialize = (NP_INITIALIZE)XP_GetSymbol(hLib, "NP_Initialize");
    if(!real_NP_Initialize)
      return NPERR_GENERIC_ERROR;

    NP_SHUTDOWN real_NP_Shutdown = (NP_SHUTDOWN)XP_GetSymbol(hLib, "NP_Shutdown");
    if(!real_NP_Shutdown)
      return NPERR_GENERIC_ERROR;

    // fill callbacks structs
    NPPluginFuncs realNPPFuncs;
    memset(&realNPPFuncs, 0, sizeof(NPPluginFuncs));
    realNPPFuncs.size = sizeof(NPPluginFuncs);

    real_NP_GetEntryPoints(&realNPPFuncs);

    if(logger)
      logger->logSPY_NP_GetEntryPoints(&realNPPFuncs);

    // store the table with the entry point manager
    epManager->createEntryPointsForPlugin(pluginType, &realNPPFuncs, real_NP_Shutdown, hLib);
    
    // inform the plugin about our entry point it should call
    memset((void *)&fakeNPNFuncs, 0, sizeof(fakeNPNFuncs));

    fakeNPNFuncs.size             = sizeof(fakeNPNFuncs);
    fakeNPNFuncs.version          = NPNFuncs.version;
    fakeNPNFuncs.geturlnotify     = NPN_GetURLNotify;
    fakeNPNFuncs.geturl           = NPN_GetURL;
    fakeNPNFuncs.posturlnotify    = NPN_PostURLNotify;
    fakeNPNFuncs.posturl          = NPN_PostURL;
    fakeNPNFuncs.requestread      = NPN_RequestRead;
    fakeNPNFuncs.newstream        = NPN_NewStream;
    fakeNPNFuncs.write            = NPN_Write;
    fakeNPNFuncs.destroystream    = NPN_DestroyStream;
    fakeNPNFuncs.status           = NPN_Status;
    fakeNPNFuncs.uagent           = NPN_UserAgent;
    fakeNPNFuncs.memalloc         = NPN_MemAlloc;
    fakeNPNFuncs.memfree          = NPN_MemFree;
    fakeNPNFuncs.memflush         = NPN_MemFlush;
    fakeNPNFuncs.reloadplugins    = NPN_ReloadPlugins;
    fakeNPNFuncs.getJavaEnv       = NPN_GetJavaEnv;
    fakeNPNFuncs.getJavaPeer      = NPN_GetJavaPeer;
    fakeNPNFuncs.getvalue         = NPN_GetValue;
    fakeNPNFuncs.setvalue         = NPN_SetValue;
    fakeNPNFuncs.invalidaterect   = NPN_InvalidateRect;
    fakeNPNFuncs.invalidateregion = NPN_InvalidateRegion;
    fakeNPNFuncs.forceredraw      = NPN_ForceRedraw;
    fakeNPNFuncs.getstringidentifier    = NPN_GetStringIdentifier;
    fakeNPNFuncs.getstringidentifiers   = NPN_GetStringIdentifiers;
    fakeNPNFuncs.getintidentifier       = NPN_GetIntIdentifier;
    fakeNPNFuncs.identifierisstring     = NPN_IdentifierIsString;
    fakeNPNFuncs.utf8fromidentifier     = NPN_UTF8FromIdentifier;
    fakeNPNFuncs.intfromidentifier      = NPN_IntFromIdentifier;
    fakeNPNFuncs.createobject           = NPN_CreateObject;
    fakeNPNFuncs.retainobject           = NPN_RetainObject;
    fakeNPNFuncs.releaseobject          = NPN_ReleaseObject;
    fakeNPNFuncs.invoke                 = NPN_Invoke;
    fakeNPNFuncs.invokeDefault          = NPN_InvokeDefault;
    fakeNPNFuncs.evaluate               = NPN_Evaluate;
    fakeNPNFuncs.getproperty            = NPN_GetProperty;
    fakeNPNFuncs.setproperty            = NPN_SetProperty;
    fakeNPNFuncs.removeproperty         = NPN_RemoveProperty;
    fakeNPNFuncs.hasproperty            = NPN_HasProperty;
    fakeNPNFuncs.hasmethod              = NPN_HasMethod;
    fakeNPNFuncs.releasevariantvalue    = NPN_ReleaseVariantValue;
    fakeNPNFuncs.setexception           = NPN_SetException;
    fakeNPNFuncs.pushpopupsenabledstate = NPN_PushPopupsEnabledState;
    fakeNPNFuncs.poppopupsenabledstate  = NPN_PopPopupsEnabledState;
    fakeNPNFuncs.enumerate              = NPN_Enumerate;

    if(logger)
      logger->logSPY_NP_Initialize();

    real_NP_Initialize(&fakeNPNFuncs);
  }
  
  NPError rv = epManager->callNPP_New(pluginType, instance, mode, argc, argn, argv, saved);
  
  if(logger)
    logger->logReturn(action_npp_new);

  return rv;
}

NPError NPP_Destroy (NPP instance, NPSavedData** save)
{
  if(epManager == NULL)
    return NPERR_GENERIC_ERROR;

  if(instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  BOOL last = FALSE;

  if(logger)
    logger->logCall(action_npp_destroy, (DWORD)instance, (DWORD)save);

  NPError rv = epManager->callNPP_Destroy(instance, save, &last);

  if(logger)
    logger->logReturn(action_npp_destroy, rv);

  if(last && logger->bSPALID)
  {
    // this will log it
    epManager->callNP_Shutdown(instance);

    XP_HLIB hLib = NULL;

    epManager->removeEntryPointsForPlugin(instance, &hLib);
  
    UnloadRealPlugin(hLib);
  }
  return rv;
}

NPError NPP_SetWindow (NPP instance, NPWindow* pNPWindow)
{    
  if(epManager == NULL)
    return NPERR_GENERIC_ERROR;

  if(instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(logger)
    logger->logCall(action_npp_set_window, (DWORD)instance, (DWORD)pNPWindow);

  NPError rv = epManager->callNPP_SetWindow(instance, pNPWindow);

  if(logger)
    logger->logReturn(action_npp_set_window, rv);

  return rv;
}

NPError NPP_NewStream(NPP instance,
                      NPMIMEType type,
                      NPStream* stream, 
                      NPBool seekable,
                      uint16* stype)
{
  if(epManager == NULL)
    return NPERR_GENERIC_ERROR;

  if(instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(logger)
    logger->logCall(action_npp_new_stream, (DWORD)instance, (DWORD)type, (DWORD)stream, (DWORD)seekable, (DWORD)stype);

  NPError rv = epManager->callNPP_NewStream(instance, type, stream, seekable, stype);

  if(logger)
    logger->logReturn(action_npp_new_stream, rv);

  return rv;
}

int32 NPP_WriteReady (NPP instance, NPStream *stream)
{
  if(epManager == NULL)
    return NPERR_GENERIC_ERROR;

  if(instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(logger)
    logger->logCall(action_npp_write_ready, (DWORD)instance, (DWORD)stream);

  int32 rv = epManager->callNPP_WriteReady(instance, stream);

  if(logger)
    logger->logReturn(action_npp_write_ready, rv);

  return rv;
}

int32 NPP_Write (NPP instance, NPStream *stream, int32 offset, int32 len, void *buffer)
{   
  if(epManager == NULL)
    return NPERR_GENERIC_ERROR;

  if(instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(logger)
    logger->logCall(action_npp_write, (DWORD)instance, (DWORD)stream, (DWORD)offset, (DWORD)len, (DWORD)buffer);

  int32 rv = epManager->callNPP_Write(instance, stream, offset, len, buffer);

  if(logger)
    logger->logReturn(action_npp_write, rv);

  return rv;
}

NPError NPP_DestroyStream (NPP instance, NPStream *stream, NPError reason)
{
  if(epManager == NULL)
    return NPERR_GENERIC_ERROR;

  if(instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(logger)
    logger->logCall(action_npp_destroy_stream, (DWORD)instance, (DWORD)stream, (DWORD)reason);

  NPError rv = epManager->callNPP_DestroyStream(instance, stream, reason);

  if(logger)
    logger->logReturn(action_npp_destroy_stream, rv);

  return rv;
}

void NPP_StreamAsFile (NPP instance, NPStream* stream, const char* fname)
{
  if(epManager == NULL)
    return;

  if(instance == NULL)
    return;

  if(logger)
    logger->logCall(action_npp_stream_as_file, (DWORD)instance, (DWORD)stream, (DWORD)fname);

  epManager->callNPP_StreamAsFile(instance, stream, fname);
}

void NPP_Print (NPP instance, NPPrint* printInfo)
{
  if(epManager == NULL)
    return;

  if(logger)
    logger->logCall(action_npp_print, (DWORD)instance, (DWORD)printInfo);

  epManager->callNPP_Print(instance, printInfo);
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
  if(epManager == NULL)
    return;

  if(instance == NULL)
    return;

  if(logger)
    logger->logCall(action_npp_url_notify, (DWORD)instance, (DWORD)url, (DWORD)reason, (DWORD)notifyData);

  epManager->callNPP_URLNotify(instance, url, reason, notifyData);
}

NPError	NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
  if(epManager == NULL)
    return NPERR_GENERIC_ERROR;

  if(instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(logger)
    logger->logCall(action_npp_get_value, (DWORD)instance, (DWORD)variable, (DWORD)value);

  NPError rv = epManager->callNPP_GetValue(instance, variable, value);

  if(logger)
    logger->logReturn(action_npp_get_value, rv);

  return rv;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
  if(epManager == NULL)
    return NPERR_GENERIC_ERROR;

  if(instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(logger)
    logger->logCall(action_npp_set_value, (DWORD)instance, (DWORD)variable, (DWORD)value);

  NPError rv = epManager->callNPP_SetValue(instance, variable, value);

  if(logger)
    logger->logReturn(action_npp_set_value, rv);

  return rv;
}

int16	NPP_HandleEvent(NPP instance, void* event)
{
  if(epManager == NULL)
    return 0;

  if(instance == NULL)
    return 0;

  if(logger)
    logger->logCall(action_npp_handle_event, (DWORD)instance, (DWORD)event);

  int16 rv = epManager->callNPP_HandleEvent(instance, event);

  if(logger)
    logger->logReturn(action_npp_handle_event, rv);

  return rv;
}
