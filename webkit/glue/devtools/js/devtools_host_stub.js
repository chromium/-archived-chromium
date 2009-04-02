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


RemoteDomAgentStub.prototype.SetAttribute = function(callId) {
  setTimeout(function() {
    RemoteDomAgent.DidApplyDomChange(callId, true);
  }, 0);
};


RemoteDomAgentStub.prototype.RemoveAttribute = function(callId) {
  setTimeout(function() {
    RemoteDomAgent.DidApplyDomChange(callId, true);
  }, 0);
};


RemoteDomAgentStub.prototype.SetTextNodeValue = function(callId) {
  setTimeout(function() {
    RemoteDomAgent.DidApplyDomChange(callId, true);
  }, 0);
};


RemoteDomAgentStub.prototype.PerformSearch = function(callId, query) {
  setTimeout(function() {
    RemoteDomAgent.DidPerformSearch(callId, [1]);
  }, 0);
};


RemoteDomAgentStub.prototype.DiscardBindings = function() {
};


RemoteDomAgentStub.prototype.GetNodeStyles = function(callId, id, authorOnly) {
  var styles = {
      computedStyle: "display: none",
      inlineStyle: "display: none",
      styleAttributes: {attr: "display: none"},
      matchedCSSRules: [{selector: "S", cssText: "display: none",
                         parentStyleSheetHref: "http://localhost",
                         parentStyleSheetOwnerNodeName: "DIV"}]
  };
  setTimeout(function() {
    RemoteDomAgent.DidGetNodeStyles(callId, styles);
  }, 0);
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


RemoteToolsAgentStub.prototype.evaluate = function(expr) {
  window.eval(expr);
};

RemoteToolsAgentStub.prototype.EvaluateJavaScript = function(callId, script) {
  setTimeout(function() {
    var result = eval(script);
    RemoteToolsAgent.DidEvaluateJavaScript(callId, result);
  }, 0);
};


RemoteToolsAgentStub.prototype.ExecuteUtilityFunction = function(callId, 
    functionName, nodeId, args) {
  setTimeout(function() {
    var result = [];
    if (functionName == 'devtools$$getProperties') {
      result = [
        'undefined', 'undefined_key', undefined,
        'string', 'string_key', 'value',
        'function', 'func', undefined,
        'array', 'array_key', [10],
        'object', 'object_key', undefined,
        'boolean', 'boolean_key', true,
        'number', 'num_key', 911,
        'date', 'date_key', new Date() ];
    } else if (functionName == 'devtools$$getPrototypes') {
      result = ['Proto1', 'Proto2', 'Proto3'];
    } else {
      alert('Unexpected utility function:' + functionName);
    }
    RemoteToolsAgent.DidExecuteUtilityFunction(callId,
        goog.json.serialize(result));
  }, 0);
};


RemoteToolsAgentStub.prototype.GetNodePrototypes = function(callId, nodeId) {
  setTimeout(function() {
    RemoteToolsAgent.DidGetNodePrototypes(callId,
        goog.json.serialize());
  }, 0);
};


/**
 * @constructor
 */
RemoteDebuggerCommandExecutorStub = function() {
};


RemoteDebuggerCommandExecutorStub.prototype.DebuggerCommand = function() {
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
  window['RemoteDebuggerCommandExecutor'] =
      new RemoteDebuggerCommandExecutorStub();
  window['RemoteDomAgent'] = new RemoteDomAgentStub();
  window['RemoteNetAgent'] = new RemoteNetAgentStub();
  window['RemoteToolsAgent'] = new RemoteToolsAgentStub();
  window['DevToolsHost'] = new DevToolsHostStub();
}
