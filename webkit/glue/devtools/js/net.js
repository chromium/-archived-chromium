// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'Net' manages resources along with the corresponding
 * HTTP requests and responses.
 * web inspector.
 */
goog.provide('devtools.Net');

devtools.Net = function() {
  this.resources_ = {};
  this.id_for_url_ = {};
};


devtools.Net.prototype.willSendRequest = function(identifier, request) {
  var mainResource = false;
  var cached = false;
  var resource = new WebInspector.Resource(request.requestHeaders, 
      request.url, request.domain, request.path, request.lastPathComponent,
      identifier, mainResource, cached);
  resource.startTime = request.startTime;
  WebInspector.addResource(resource);
  this.resources_[identifier] = resource;
  this.id_for_url_[request.url] = identifier;
};


devtools.Net.prototype.didReceiveResponse = function(identifier, response) {
  var resource = this.resources_[identifier];
  if (!resource) {
    return;
  }
  resource.expectedContentLength = response.expectedContentLength;
  resource.responseStatusCode = response.responseStatusCode;
  resource.mimeType = response.mimeType;
  resource.suggestedFilename = response.suggestedFilename;
  var mimeType = response.mimeType;
  if (mimeType.indexOf("image/") == 0) {
    resource.type = WebInspector.Resource.Type.Image;
  } else if (mimeType.indexOf("text/html") == 0) {
    resource.type = WebInspector.Resource.Type.Document;
  } else if (mimeType.indexOf("script") != -1 ||
      response.url.indexOf(".js") != -1) {
    resource.type = WebInspector.Resource.Type.Script;
  } else {
    resource.type = WebInspector.Resource.Type.Other;
  } 
  resource.responseReceivedTime = response.responseReceivedTime;
};


devtools.Net.prototype.didFinishLoading = function(identifier, value) {
  var resource = this.resources_[identifier];
  if (!resource) {
    return;
  }
  resource.endTime = value.endTime;
  resource.finished = true;
  resource.failed = false;
};


devtools.Net.prototype.didFailLoading = function(identifier, value) {
  var resource = this.resources_[identifier];
  if (!resource) {
    return;
  }
  resource.endTime = value.endTime;
  resource.finished = false;
  resource.failed = true;
};


devtools.Net.prototype.setResourceContent = function(identifier,
    content) {
};
