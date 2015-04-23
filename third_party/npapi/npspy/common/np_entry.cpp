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

#include "npapi.h"
#include "npupp.h"
#include "epmanager.h"
#include "logger.h"

// we need to keep track of different plugins and different instances
// of the same plugin when we call NPP functions, so that we always
// call the right ones. Entry point manager will take care of it.
NPPEntryPointManager * epManager = NULL;

Logger * logger = NULL;

NPNetscapeFuncs NPNFuncs;

NPError WINAPI NP_GetEntryPoints(NPPluginFuncs* pFuncs)
{
  // create the logger
  if(!logger)
  {
    logger = NewLogger();
    if(logger)
    {
      logger->platformInit();
      logger->init();
    }
  }

  if(logger)
    logger->logNS_NP_GetEntryPoints();

  if(pFuncs == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  if(pFuncs->size < sizeof(NPPluginFuncs))
    return NPERR_INVALID_FUNCTABLE_ERROR;

  pFuncs->version       = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  pFuncs->newp          = NPP_New;
  pFuncs->destroy       = NPP_Destroy;
  pFuncs->setwindow     = NPP_SetWindow;
  pFuncs->newstream     = NPP_NewStream;
  pFuncs->destroystream = NPP_DestroyStream;
  pFuncs->asfile        = NPP_StreamAsFile;
  pFuncs->writeready    = NPP_WriteReady;
  pFuncs->write         = NPP_Write;
  pFuncs->print         = NPP_Print;
  pFuncs->event         = NPP_HandleEvent;
  pFuncs->urlnotify     = NPP_URLNotify;
  pFuncs->getvalue      = NPP_GetValue;
  pFuncs->setvalue      = NPP_SetValue;
  pFuncs->javaClass     = NULL;

  return NPERR_NO_ERROR;
}

NPError WINAPI NP_Initialize(NPNetscapeFuncs* pFuncs)
{
   // DebugBreak();

  // create the logger
  if(!logger)
  {
    logger = NewLogger();
    if(logger)
    {
      logger->platformInit();
      logger->init();
    }
  }

  if(logger)
    logger->logNS_NP_Initialize();

  if(pFuncs == NULL) {
    logger->logMessage("NP_Initialize: NULL functable!\r\n");
    return NPERR_INVALID_FUNCTABLE_ERROR;
  }

  if(HIBYTE(pFuncs->version) > NP_VERSION_MAJOR) {
    logger->logMessage("NP_Initialize: incompatible version!\r\n");
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }

  /*
   *
   * Removing this check so that we can work with older versions 
   * of servers (which may provide fewer functions)
   *
  if(pFuncs->size < sizeof NPNetscapeFuncs) {
    logger->logReturn(NPERR_INVALID_FUNCTABLE_ERROR);
    char msg[512];
    sprintf(msg, "functable is %d, expected %d", pFuncs->size, sizeof(NPNetscapeFuncs));
    logger->logMessage(msg);
    return NPERR_INVALID_FUNCTABLE_ERROR;
  }
   *
   */

  NPNFuncs.size             = pFuncs->size;
  NPNFuncs.version          = pFuncs->version;
  NPNFuncs.geturlnotify     = pFuncs->geturlnotify;
  NPNFuncs.geturl           = pFuncs->geturl;
  NPNFuncs.posturlnotify    = pFuncs->posturlnotify;
  NPNFuncs.posturl          = pFuncs->posturl;
  NPNFuncs.requestread      = pFuncs->requestread;
  NPNFuncs.newstream        = pFuncs->newstream;
  NPNFuncs.write            = pFuncs->write;
  NPNFuncs.destroystream    = pFuncs->destroystream;
  NPNFuncs.status           = pFuncs->status;
  NPNFuncs.uagent           = pFuncs->uagent;
  NPNFuncs.memalloc         = pFuncs->memalloc;
  NPNFuncs.memfree          = pFuncs->memfree;
  NPNFuncs.memflush         = pFuncs->memflush;
  NPNFuncs.reloadplugins    = pFuncs->reloadplugins;
  NPNFuncs.getJavaEnv       = pFuncs->getJavaEnv;
  NPNFuncs.getJavaPeer      = pFuncs->getJavaPeer;
  NPNFuncs.getvalue         = pFuncs->getvalue;
  NPNFuncs.setvalue         = pFuncs->setvalue;
  NPNFuncs.invalidaterect   = pFuncs->invalidaterect;
  NPNFuncs.invalidateregion = pFuncs->invalidateregion;
  NPNFuncs.forceredraw      = pFuncs->forceredraw;
  NPNFuncs.getstringidentifier      = pFuncs->getstringidentifier;
  NPNFuncs.getstringidentifiers      = pFuncs->getstringidentifiers;
  NPNFuncs.identifierisstring      = pFuncs->identifierisstring;
  NPNFuncs.utf8fromidentifier      = pFuncs->utf8fromidentifier;
  NPNFuncs.intfromidentifier      = pFuncs->intfromidentifier;
  NPNFuncs.createobject      = pFuncs->createobject;
  NPNFuncs.retainobject      = pFuncs->retainobject;
  NPNFuncs.releaseobject      = pFuncs->releaseobject;
  NPNFuncs.invoke      = pFuncs->invoke;
  NPNFuncs.invokeDefault      = pFuncs->invokeDefault;
  NPNFuncs.evaluate      = pFuncs->evaluate;
  NPNFuncs.getproperty      = pFuncs->getproperty;
  NPNFuncs.setproperty      = pFuncs->setproperty;
  NPNFuncs.removeproperty      = pFuncs->removeproperty;
  NPNFuncs.hasproperty      = pFuncs->hasproperty;
  NPNFuncs.hasmethod      = pFuncs->hasmethod;
  NPNFuncs.releasevariantvalue      = pFuncs->releasevariantvalue;
  NPNFuncs.setexception      = pFuncs->setexception;
  NPNFuncs.pushpopupsenabledstate      = pFuncs->pushpopupsenabledstate;
  NPNFuncs.poppopupsenabledstate      = pFuncs->poppopupsenabledstate;
  NPNFuncs.enumerate      = pFuncs->enumerate;

  // create entry point manager for real plugins
  epManager = new NPPEntryPointManager();
  if(!epManager) {
    logger->logMessage("NP_Initialize: could not create EntryPointManager\r\n");
    return NPERR_GENERIC_ERROR;
  }

  logger->logMessage("NP_Initialize: success\r\n");
  return NPERR_NO_ERROR;
}

NPError WINAPI NP_Shutdown()
{
  // should be safe because if they've already been called shutdown procs must be NULL
  if(epManager)
    epManager->callNP_ShutdownAll(); // this will log the action

  if(logger)
  {
    logger->shut();
    logger->platformShut();
    DeleteLogger(logger);
    logger = NULL;
  }

  delete epManager;

  return NPERR_NO_ERROR;
}
