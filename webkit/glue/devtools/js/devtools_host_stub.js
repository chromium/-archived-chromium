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
RemoteDebuggerAgentStub = function() {
};

RemoteDebuggerAgentStub.prototype.DebugAttach = function() {
};

RemoteDebuggerAgentStub.prototype.DebugDetach = function() {
};

RemoteDebuggerAgentStub.prototype.DebugCommand = function() {
};

RemoteDebuggerAgentStub.prototype.DebugBreak = function() {
};


/**
 * @constructor
 */
RemoteDomAgentStub = function() {
};


RemoteDomAgentStub.sendDocumentElement_ = function() {
  RemoteDomAgent.SetDocumentElement([
    1,       // id
    1,       // type = Node.ELEMENT_NODE,
    "HTML",  // nodeName
    "",      // nodeValue 
    ["foo","bar"],  // attributes
    2,       // childNodeCount
  ]);
};


RemoteDomAgentStub.sendChildNodes_ = function(id) {
  if (id == 1) {
    RemoteDomAgent.SetChildNodes(id,
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
  } else if (id == 2) {
    RemoteDomAgent.SetChildNodes(id, 
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
  }
};


RemoteDomAgentStub.prototype.GetDocumentElement = function(callId) {
  setTimeout(function() {
    RemoteDomAgentStub.sendDocumentElement_();
  }, 0);
};


RemoteDomAgentStub.prototype.GetChildNodes = function(callId, id) {
  setTimeout(function() {
    RemoteDomAgentStub.sendChildNodes_(id);
    RemoteDomAgent.DidGetChildNodes(callId);
  }, 0);
};


RemoteDomAgentStub.prototype.SetAttribute = function() {
};


RemoteDomAgentStub.prototype.RemoveAttribute = function() {
};


RemoteDomAgentStub.prototype.SetTextNodeValue = function() {
};


RemoteDomAgentStub.prototype.PerformSearch = function(callId, query) {
  setTimeout(function() {
    RemoteDomAgent.DidPerformSearch(callId, [1]);
  }, 0);
};


RemoteDomAgentStub.prototype.DiscardBindings = function() {
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


RemoteToolsAgentStub.prototype.SetEnabled = function() {
};


RemoteToolsAgentStub.prototype.evaluate = function(expr) {
  window.eval(expr);
};

RemoteToolsAgentStub.prototype.EvaluateJavaSctipt = function(callId, script) {
  setTimeout(function() {
    var result = eval(script);
    RemoteToolsAgent.DidEvaluateJavaSctipt(callId, result);
  }, 0);
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


DevToolsHostStub.prototype.loaded = function() {
  RemoteDomAgentStub.sendDocumentElement_();
  RemoteDomAgentStub.sendChildNodes_(1);
  RemoteDomAgentStub.sendChildNodes_(2);
  devtools.tools.updateFocusedNode(4);
};


if (!window['DevToolsHost']) {
  window['RemoteDebuggerAgent'] = new RemoteDebuggerAgentStub();
  window['RemoteDomAgent'] = new RemoteDomAgentStub();
  window['RemoteNetAgent'] = new RemoteNetAgentStub();
  window['RemoteToolsAgent'] = new RemoteToolsAgentStub();
  window['DevToolsHost'] = new DevToolsHostStub();
}
