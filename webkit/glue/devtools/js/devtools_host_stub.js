// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview DevToolsHost Stub emulates backend functionality and allows
 * DevTools frontend to function as a standalone web app.
 */
goog.provide('devtools.DevToolsHostStub');

/**
 * @constructor
 */
devtools.DevToolsHostStub = function() {
};


devtools.DevToolsHostStub.prototype.getDocumentElement = function(callback) {
  setTimeout(function() {
    callback(goog.json.serialize([
      1,       // id
      1,       // type = Node.ELEMENT_NODE,
      "HTML",  // nodeName
      "",      // nodeValue 
      ["foo","bar"],  // attributes
      2,       // childNodeCount
    ]))
  }, 0);
};


devtools.DevToolsHostStub.prototype.getChildNodes = function(id, callback) {
  if (id == 1) {
    setTimeout(function() {
      callback(goog.json.serialize(
        [
          [
           2,       // id
           1,       // type = Node.ELEMENT_NODE,
           "DIV",   // nodeName
           "",      // nodeValue 
           ["foo","bar"],  // attributes
           1,       // childNodeCount
          ],
          [
           3,  // id
           3,  // type = Node.TEXT_NODE,
           "", // nodeName
           "Text", // nodeValue 
          ]
        ]));
      }, 0);
  } else if (id == 2) {
    setTimeout(function() {
      callback(goog.json.serialize(
        [
          [
          4,       // id
          1,       // type = Node.ELEMENT_NODE,
          "span",   // nodeName
          "",      // nodeValue 
          ["foo","bar"],  // attributes
          0,       // childNodeCount
        ]
      ]));
    }, 0);
  }
};


devtools.DevToolsHostStub.prototype.attach = function() {
};


devtools.DevToolsHostStub.prototype.evaluate = function(str) {
};


devtools.DevToolsHostStub.prototype.setAttribute = function() {
};


devtools.DevToolsHostStub.prototype.removeAttribute = function() {
};


devtools.DevToolsHostStub.prototype.setTextNodeValue = function() {
};


devtools.DevToolsHostStub.prototype.hideDOMNodeHighlight = function() {
};


devtools.DevToolsHostStub.prototype.highlighDOMNode = function() {
};


devtools.DevToolsHostStub.prototype.debuggerSendMessage = function() {
};

if (!window['DevToolsHost']) {
  window['DevToolsHost'] = new devtools.DevToolsHostStub();
}