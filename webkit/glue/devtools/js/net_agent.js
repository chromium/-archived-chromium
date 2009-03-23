// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'Net' manages resources along with the corresponding
 * HTTP requests and responses.
 * web inspector.
 */
goog.provide('devtools.NetAgent');

devtools.NetAgent = function() {
  this.resources_ = {};
  this.id_for_url_ = {};

  RemoteNetAgent.GetResourceContentResult =
      devtools.Callback.processCallback;
  RemoteNetAgent.WillSendRequest =
      goog.bind(this.willSendRequest, this);
  RemoteNetAgent.DidReceiveResponse =
      goog.bind(this.didReceiveResponse, this);
  RemoteNetAgent.DidFinishLoading =
      goog.bind(this.didFinishLoading, this);
  RemoteNetAgent.DidFailLoading =
      goog.bind(this.didFailLoading, this);
};


/**
 * Returns resource object for given identifier.
 * @param {number} identifier Identifier to get resource for.
 * @return {WebInspector.Resouce} Resulting resource.
 */
devtools.NetAgent.prototype.getResource = function(identifier) {
  return this.resources_[identifier];
};


/**
 * Asynchronously queries for the resource content.
 * @param {number} identifier Resource identifier.
 * @param {function(string):undefined} opt_callback Callback to call when 
 *     result is available.
 */
devtools.NetAgent.prototype.getResourceContentAsync = function(identifier, 
    opt_callback) {
  var resource = this.resources_[identifier];
  if (!resource) {
    return;
  }
  var mycallback = function(content) {
    if (opt_callback) {
      opt_callback(content);
    }
  };
  RemoteNetAgent.GetResourceContent(
      devtools.Callback.wrap(mycallback), identifier, resource.url);
};


/**
 * @see NetAgentDelegate.
 * {@inheritDoc}.
 */
devtools.NetAgent.prototype.willSendRequest = function(identifier, request) {
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


/**
 * @see NetAgentDelegate.
 * {@inheritDoc}.
 */
devtools.NetAgent.prototype.didReceiveResponse = function(identifier, response) {
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


/**
 * @see NetAgentDelegate.
 * {@inheritDoc}.
 */
devtools.NetAgent.prototype.didFinishLoading = function(identifier, value) {
  var resource = this.resources_[identifier];
  if (!resource) {
    return;
  }
  resource.endTime = value.endTime;
  resource.finished = true;
  resource.failed = false;
};


/**
 * @see NetAgentDelegate.
 * {@inheritDoc}.
 */
devtools.NetAgent.prototype.didFailLoading = function(identifier, value) {
  var resource = this.resources_[identifier];
  if (!resource) {
    return;
  }
  resource.endTime = value.endTime;
  resource.finished = false;
  resource.failed = true;
};
