// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview These stubs emulate backend functionality and allows
 * DevTools frontend to function as a standalone web app.
 */

/**
 * @constructor
 */
RemoteDomAgentStub = function() {
};


RemoteDomAgentStub.prototype.GetDocumentElement = function(callId) {
  setTimeout(function() {
    RemoteDomAgent.GetDocumentElementResult(callId, [
      1,       // id
      1,       // type = Node.ELEMENT_NODE,
      "HTML",  // nodeName
      "",      // nodeValue 
      ["foo","bar"],  // attributes
      2,       // childNodeCount
    ]);
  }, 0);
};


RemoteDomAgentStub.prototype.GetChildNodes = function(callId, id) {
  if (id == 1) {
    setTimeout(function() {
      RemoteDomAgent.GetChildNodesResult(callId, 
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
        ]);
      }, 0);
  } else if (id == 2) {
    setTimeout(function() {
      RemoteDomAgent.GetChildNodesResult(callId, 
        [
          [
          4,       // id
          1,       // type = Node.ELEMENT_NODE,
          "span",   // nodeName
          "",      // nodeValue 
          ["foo","bar"],  // attributes
          0,       // childNodeCount
        ]
      ]);
    }, 0);
  }
};


RemoteDomAgentStub.prototype.SetAttribute = function() {
};


RemoteDomAgentStub.prototype.RemoveAttribute = function() {
};


RemoteDomAgentStub.prototype.SetTextNodeValue = function() {
};


/**
 * @constructor
 */
RemoteToolsAgentStub = function() {
};


RemoteToolsAgentStub.prototype.HideDOMNodeHighlight = function() {
};


RemoteToolsAgentStub.prototype.HighlightDOMNode = function() {
};


RemoteToolsAgentStub.prototype.SetDomAgentEnabled = function() {
};


RemoteToolsAgentStub.prototype.SetNetAgentEnabled = function() {
};


/**
 * @constructor
 */
RemoteNetAgentStub = function() {
};


/**
 * @constructor
 */
DevToolsHostStub = function() {
};


if (!window['DevToolsHost']) {
  window['RemoteDomAgent'] = new RemoteDomAgentStub();
  window['RemoteNetAgent'] = new RemoteNetAgentStub();
  window['RemoteToolsAgent'] = new RemoteToolsAgentStub();
  window['DevToolsHost'] = new DevToolsHostStub();
}
