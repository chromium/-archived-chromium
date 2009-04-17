// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBAPPCACHECONTEXT_H_
#define WEBKIT_GLUE_WEBAPPCACHECONTEXT_H_

#include "base/basictypes.h"

class GURL;

// This class is used in child processes, renderers and workers.
//
// An AppCacheContext corresponds with what html5 refers to as a
// "browsing context". Conceptually, each frame or worker represents
// a unique context. This class is used in child processes (renderers
// and workers) to inform the browser process of new frames and workers, and
// to keep track of which appcache is selected for each context. Resource
// requests contain the context id so the browser process can identify
// which context a request came from. As new documents are committed into a
// frame, the cache selection algorithm is initiated by calling one of the
// SelectAppCache methods.
//
// Each WebAppCacheContext is assigned a unique id within its child process.
// These ids are made globally unique by pairing them with a child process
// id within the browser process.
//
// WebFrameImpl has a scoped ptr to one of these as a data member.
// TODO(michaeln): integrate with WebWorkers
class WebAppCacheContext {
 public:
  enum ContextType {
    MAIN_FRAME = 0,
    CHILD_FRAME,
    DEDICATED_WORKER
  };

  static const int kNoAppCacheContextId;  // = 0;
  static const int64 kNoAppCacheId;       // = 0;
  static const int64 kUnknownAppCacheId;  // = -1;

  // Factory method called internally by webkit_glue to create a concrete
  // instance of this class. If SetFactory has been called, the factory
  // function provided there is used to create a new instance, otherwise
  // a noop implementation is returned.
  static WebAppCacheContext* Create();

  typedef WebAppCacheContext* (*WebAppCacheFactoryProc)(void);
  static void SetFactory(WebAppCacheFactoryProc factory_proc);

  // Unique id within the child process housing this context
  virtual int GetContextID() = 0;

  // Which appcache is associated with the context. There are windows of
  // time where the appcache is not yet known, the return value is
  // kUnknownAppCacheId in that case.
  virtual int64 GetAppCacheID() = 0;

  // The following methods result in async messages being sent to the
  // browser process. The initialize method tells the browser process about
  // the existance of this context, its type and its id. The select methods
  // tell the browser process to initiate the cache selection algorithm for
  // the context.
  virtual void Initialize(ContextType context_type,
                          WebAppCacheContext* opt_parent) = 0;
  virtual void SelectAppCacheWithoutManifest(
                                     const GURL& document_url,
                                     int64 cache_document_was_loaded_from) = 0;
  virtual void SelectAppCacheWithManifest(
                                     const GURL& document_url,
                                     int64 cache_document_was_loaded_from,
                                     const GURL& manifest_url) = 0;

  virtual ~WebAppCacheContext() {}
};

#endif  // WEBKIT_GLUE_WEBAPPCACHECONTEXT_H_
