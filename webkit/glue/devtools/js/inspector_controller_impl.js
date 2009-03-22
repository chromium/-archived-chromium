// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview DevTools' implementation of the InspectorController API.
 */
goog.require('devtools.InspectorController');

goog.provide('devtools.InspectorControllerImpl');

devtools.InspectorControllerImpl = function() {
  devtools.InspectorController.call(this);
  this.frame_element_id_ = 1;

  this.window_ = {
      get document() {
        return dom;
      },
      get Node() {
        return devtools.DomNode;
      },
      get Element() {
        return devtools.DomNode;
      }
  };
};
goog.inherits(devtools.InspectorControllerImpl,
    devtools.InspectorController);


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.hiddenPanels = function() {
  return "scripts,profiles,databases";
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.addSourceToFrame =
    function(mimeType, source, element) {
  if (!element.id) {
    element.id = "f" + this.frame_element_id_++; 
  }
  DevToolsHost.addSourceToFrame(mimeType, source, element.id);
  return true;
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.hideDOMNodeHighlight = function() {
  DevToolsHost.hideDOMNodeHighlight();
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.highlightDOMNode =
    function(hoveredNode) {
  DevToolsHost.highlightDOMNode(hoveredNode.id);
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.inspectedWindow = function() {
  return this.window_;
};


var InspectorController = new devtools.InspectorControllerImpl();
