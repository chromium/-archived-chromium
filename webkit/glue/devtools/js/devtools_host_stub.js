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
  this.isProfiling_ = false;
  this.profileLogPos_ = 0;
};


RemoteDebuggerAgentStub.prototype.DebugBreak = function() {
};


RemoteDebuggerAgentStub.prototype.GetContextId = function() {
  RemoteDebuggerAgent.DidGetContextId(3);
};


RemoteDebuggerAgentStub.prototype.StopProfiling = function() {
  this.isProfiling_ = false;
};


RemoteDebuggerAgentStub.prototype.StartProfiling = function() {
  this.isProfiling_ = true;
};


RemoteDebuggerAgentStub.prototype.IsProfilingStarted = function() {
  var self = this;
  setTimeout(function() {
      RemoteDebuggerAgent.DidIsProfilingStarted(self.isProfiling_);
  }, 100);
};


RemoteDebuggerAgentStub.prototype.GetNextLogLines = function() {
  if (this.profileLogPos_ < RemoteDebuggerAgentStub.ProfilerLogBuffer.length) {
    this.profileLogPos_ += RemoteDebuggerAgentStub.ProfilerLogBuffer.length;
    setTimeout(function() {
        RemoteDebuggerAgent.DidGetNextLogLines(
            RemoteDebuggerAgentStub.ProfilerLogBuffer);
        },
        100);
  } else {
    setTimeout(function() { RemoteDebuggerAgent.DidGetNextLogLines(''); }, 100);
  }
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
    'HTML',  // nodeName
    '',      // nodeValue
    ['foo','bar'],  // attributes
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
         'DIV',   // nodeName
         '',      // nodeValue
         ['foo','bar'],  // attributes
         1,       // childNodeCount
        ],
        [
         3,  // id
         3,  // type = Node.TEXT_NODE,
         '', // nodeName
         'Text', // nodeValue
        ]
      ]);
  } else if (id == 2) {
    RemoteDomAgent.SetChildNodes(id,
      [
        [
        4,       // id
        1,       // type = Node.ELEMENT_NODE,
        'span',   // nodeName
        '',      // nodeValue
        ['foo','bar'],  // attributes
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
    functionName, args) {
  setTimeout(function() {
    var result = [];
    if (functionName == 'getProperties') {
      result = [
        'undefined', 'undefined_key', undefined,
        'string', 'string_key', 'value',
        'function', 'func', undefined,
        'array', 'array_key', [10],
        'object', 'object_key', undefined,
        'boolean', 'boolean_key', true,
        'number', 'num_key', 911,
        'date', 'date_key', new Date() ];
    } else if (functionName == 'getPrototypes') {
      result = ['Proto1', 'Proto2', 'Proto3'];
    } else if (functionName == 'getStyles') {
      result = {
        'computedStyle' : [0, '0px', '0px', null, null, null, ['display', false, false, '', 'none']],
        'inlineStyle' : [1, '0px', '0px', null, null, null, ['display', false, false, '', 'none']],
        'styleAttributes' : {
           attr: [2, '0px', '0px', null, null, null, ['display', false, false, '', 'none']]
        },
        'matchedCSSRules' : [
          { 'selector' : 'S',
            'style' : [3, '0px', '0px', null, null, null, ['display', false, false, '', 'none']],
            'parentStyleSheet' : { 'href' : 'http://localhost',
                                   'ownerNodeName' : 'DIV' }
          }
        ]
      };
    } else if (functionName == 'toggleNodeStyle' ||
        functionName == 'applyStyleText' ||
        functionName == 'setStyleProperty') {
      alert(functionName + '(' + args + ')');
    } else if (functionName == 'evaluate') {
      try {
        result = [ window.eval(JSON.parse(args)[0]), false ];
      } catch (e) {
        result = [ e.toString(), true ];
      }
    } else {
      alert('Unexpected utility function:' + functionName);
    }
    RemoteToolsAgent.DidExecuteUtilityFunction(callId,
        JSON.stringify(result), '');
  }, 0);
};


RemoteToolsAgentStub.prototype.GetNodePrototypes = function(callId, nodeId) {
  setTimeout(function() {
    RemoteToolsAgent.DidGetNodePrototypes(callId,
        JSON.stringify());
  }, 0);
};


RemoteToolsAgentStub.prototype.ClearConsoleMessages = function() {
};


RemoteToolsAgentStub.prototype.SetResourceTrackingEnabled = function(enabled, always) {
  RemoteToolsAgent.SetResourcesPanelEnabled(enabled);
  if (enabled) {
    WebInspector.resourceTrackingWasEnabled();
  } else {
    WebInspector.resourceTrackingWasDisabled();
  }
  addDummyResource();
};


RemoteDebuggerAgentStub.ProfilerLogBuffer =
  'profiler,begin,1\n' +
  'profiler,resume\n' +
  'code-creation,LazyCompile,0x1000,256,"test1 http://aaa.js:1"\n' +
  'code-creation,LazyCompile,0x2000,256,"test2 http://bbb.js:2"\n' +
  'code-creation,LazyCompile,0x3000,256,"test3 http://ccc.js:3"\n' +
  'tick,0x1010,0x0,3\n' +
  'tick,0x2020,0x0,3,0x1010\n' +
  'tick,0x2020,0x0,3,0x1010\n' +
  'tick,0x3010,0x0,3,0x2020, 0x1010\n' +
  'tick,0x2020,0x0,3,0x1010\n' +
  'tick,0x2030,0x0,3,0x2020, 0x1010\n' +
  'tick,0x2020,0x0,3,0x1010\n' +
  'tick,0x1010,0x0,3\n' +
  'profiler,pause\n';


/**
 * @constructor
 */
RemoteDebuggerCommandExecutorStub = function() {
};


RemoteDebuggerCommandExecutorStub.prototype.DebuggerCommand = function(cmd) {
  if ('{"seq":2,"type":"request","command":"scripts","arguments":{"' +
      'includeSource":false}}' == cmd) {
    var response1 =
        '{"seq":5,"request_seq":2,"type":"response","command":"scripts","' +
        'success":true,"body":[{"handle":61,"type":"script","name":"' +
        'http://www/~test/t.js","id":59,"lineOffset":0,"columnOffset":0,' +
        '"lineCount":1,"sourceStart":"function fib(n) {","sourceLength":300,' +
        '"scriptType":2,"compilationType":0,"context":{"ref":60}}],"refs":[{' +
        '"handle":60,"type":"context","data":{"type":"page","value":3}}],' +
        '"running":false}';
    this.sendResponse_(response1);
  } else if ('{"seq":3,"type":"request","command":"scripts","arguments":{' +
             '"ids":[59],"includeSource":true}}' == cmd) {
    this.sendResponse_(
        '{"seq":8,"request_seq":3,"type":"response","command":"scripts",' +
        '"success":true,"body":[{"handle":1,"type":"script","name":' +
        '"http://www/~test/t.js","id":59,"lineOffset":0,"columnOffset":0,' +
        '"lineCount":1,"source":"function fib(n) {return n+1;}",' +
        '"sourceLength":244,"scriptType":2,"compilationType":0,"context":{' +
        '"ref":0}}],"refs":[{"handle":0,"type":"context","data":{"type":' +
        '"page","value":3}}],"running":false}');
  } else {
    debugPrint('Unexpected command: ' + cmd);
  }
};


RemoteDebuggerCommandExecutorStub.prototype.sendResponse_ = function(response) {
  setTimeout(function() {
    RemoteDebuggerAgent.DebuggerOutput(response);
  }, 0);
};


/**
 * @constructor
 */
DevToolsHostStub = function() {
  this.isStub = true;
  window.domAutomationController = {
    send: function(text) {
        debugPrint(text);
    }
  };
};


function addDummyResource() {
  var payload = {
    requestHeaders : {},
    requestURL: 'http://google.com/simple_page.html',
    host: 'google.com',
    path: 'simple_page.html',
    lastPathComponent: 'simple_page.html',
    isMainResource: true,
    cached: false,
    mimeType: 'text/html',
    suggestedFilename: 'simple_page.html',
    expectedContentLength: 10000,
    statusCode: 200,
    contentLength: 10000,
    responseHeaders: {},
    type: WebInspector.Resource.Type.Document,
    finished: true,
    startTime: new Date(),

    didResponseChange: true,
    didCompletionChange: true,
    didTypeChange: true
  };

  WebInspector.addResource(1, payload);
  WebInspector.updateResource(1, payload);
}


DevToolsHostStub.prototype.loaded = function() {
  RemoteDomAgentStub.sendDocumentElement_();
  RemoteDomAgentStub.sendChildNodes_(1);
  RemoteDomAgentStub.sendChildNodes_(2);
  devtools.tools.updateFocusedNode_(4);
  addDummyResource();

  uiTests.runAllTests();
};


DevToolsHostStub.prototype.reset = function() {
};


DevToolsHostStub.prototype.getPlatform = function() {
  return "windows";
};


DevToolsHostStub.prototype.addResourceSourceToFrame = function(
    identifier, mimeType, element) {
};


if (!window['DevToolsHost']) {
  window['RemoteDebuggerAgent'] = new RemoteDebuggerAgentStub();
  window['RemoteDebuggerCommandExecutor'] =
      new RemoteDebuggerCommandExecutorStub();
  window['RemoteDomAgent'] = new RemoteDomAgentStub();
  window['RemoteToolsAgent'] = new RemoteToolsAgentStub();
  window['DevToolsHost'] = new DevToolsHostStub();
}
