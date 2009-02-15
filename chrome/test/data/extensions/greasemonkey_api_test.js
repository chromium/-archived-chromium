var localStorage;
var document;
var window;

function testGetSetValue() {
  localStorage = new LocalStorageFake();
  GM_setValue('string', 'value');
  assert('value', GM_getValue('string'));
  GM_setValue('integer', 123);
  assert(123, GM_getValue('integer'));
  GM_setValue('boolean', true);
  assert(true, GM_getValue('boolean'));

  assert(undefined, GM_getValue('notset'));
  assert('default', GM_getValue('notset', 'default'));

  // illegal values
  assertThrow(function() {
    GM_setValue('illegal value', null);
  });
  assertThrow(function() {
    GM_setValue('illegal value', 1.5);
  });
}

function testDeleteValue() {
  localStorage = new LocalStorageFake();
  GM_setValue('delete', 'value');
  assert('value', GM_getValue('delete'));
  GM_deleteValue('delete');
  assert(undefined, GM_getValue('delete'));
  GM_deleteValue('notset');
}

function testListValues() {
  localStorage = new LocalStorageFake();
  var a = GM_listValues();
  assert(0, a.length);

  GM_setValue('one', 'first');
  a = GM_listValues();
  assert(1, a.length);
  assert('one', a[0]);

  GM_setValue('two', 'second');
  a = GM_listValues();

  // Test that keys that are not namespaced can't be read.
  localStorage.setItem('three', 'third');
  assert(2, a.length);
  assert(true, a.indexOf('one') >= 0);
  assert(true, a.indexOf('two') >= 0);
}

function testGetResourceURL() {
  assertThrow(function() {
    var resource = GM_getResourceURL('urlResourceName');
  });
}

function testGetResourceText() {
  assertThrow(function() {
    var resource = GM_getResourceText('textResourceName');
  });
}

function testAddStyle() {
  var head = new ElementFake('head');
  document = new DocumentFake(head);
  var css = 'color: #decaff;';
  GM_addStyle(css);
  assert(1, head.childNodes.length);
  var style = head.childNodes[0];
  assert("style", style.tagName);
  assert("text/css", style.type);
  assert(css, style.innerHTML);
}

function testXmlhttpRequest() {
  var xhr = GM_xmlhttpRequest({});
}

function testRegisterMenuCommand() {
  assertThrow(function() {
    GM_registerMenuCommand('name', function() {}, 'a', '', 'a');
  });
}

function testOpenInTab() {
  var mockWindow = {
    open: function(url, name, opt_options) {
      this.openedUrl = url;
      this.openedName = name;
      this.openedOptions = opt_options;
    }
  };
  window = mockWindow;
  var url = 'http://example.com';
  GM_openInTab(url);
  assert(mockWindow.openedUrl, url);
}

function testLog() {
  var mockWindow = {
    console: {
      message: null,
      log: function(message) {
        this.message = message;
      }
    }
  };
  window = mockWindow;
  var message = 'hello world';
  GM_log(message);
  assert(message, mockWindow.console.message);
}

function LocalStorageFake() {
  this.map_ = {};
  this.keys_ = [];
}
LocalStorageFake.prototype = {
  length: 0,
  key: function(index) {
    if (index >= this.length) {
      throw new Error('INDEX_SIZE_ERR');
    }
    return this.keys_[index];
  },
  getItem: function(key) {
    if (key in this.map_) {
      return this.map_[key];
    }
    return null;
  },
  setItem: function(key, data) {
    this.map_[key] = data;
    this.updateKeys_();
  },
  removeItem: function(key) {
    delete this.map_[key];
    this.updateKeys_();
  },
  clear: function() {
    this.map_ = {};
    this.updateKeys_();
  },
  updateKeys_: function() {
    var keys = [];
    for (var key in this.map_) {
      keys.push(key);
    }
    this.keys_ = keys;
    this.length = keys.length;
  }
}

function DocumentFake(head) {
  this.head_ = head;
}
DocumentFake.prototype = {
  getElementsByTagName: function(tagName) {
    if (tagName == 'head' && this.head_) {
      return [this.head_];
    }
    return [];
  },
  createElement: function(tagName) {
    return new ElementFake(tagName);
  }
}

function ElementFake(tagName) {
  this.tagName = tagName;
}
ElementFake.prototype = {
  childNodes: [],
  appendChild: function(e) {
    this.childNodes.push(e);
    return e;
  },
}

function assert(expected, actual) {
  if (expected !== actual) {
    throw new Error('Assert failed: "' + expected + '" !== "' + actual + '"');
  }
}

function fail() {
  throw new Error('Fail');
}

function assertThrow(f) {
  var threw = false;
  try {
    f();
  } catch(e) {
    threw = true;
  }
  if (!threw) {
    throw new Error('Assert failed, expression did not throw.');
  }
}
var localStorage;
var document;
var window;

function testGetSetValue() {
  localStorage = new LocalStorageFake();
  GM_setValue('string', 'value');
  assert('value', GM_getValue('string'));
  GM_setValue('integer', 123);
  assert(123, GM_getValue('integer'));
  GM_setValue('boolean', true);
  assert(true, GM_getValue('boolean'));

  assert(undefined, GM_getValue('notset'));
  assert('default', GM_getValue('notset', 'default'));

  // illegal values
  assertThrow(function() {
    GM_setValue('illegal value', null);
  });
  assertThrow(function() {
    GM_setValue('illegal value', 1.5);
  });
}

function testDeleteValue() {
  localStorage = new LocalStorageFake();
  GM_setValue('delete', 'value');
  assert('value', GM_getValue('delete'));
  GM_deleteValue('delete');
  assert(undefined, GM_getValue('delete'));
  GM_deleteValue('notset');
}

function testListValues() {
  localStorage = new LocalStorageFake();
  var a = GM_listValues();
  assert(0, a.length);

  GM_setValue('one', 'first');
  a = GM_listValues();
  assert(1, a.length);
  assert('one', a[0]);

  GM_setValue('two', 'second');
  a = GM_listValues();

  // Test that keys that are not namespaced can't be read.
  localStorage.setItem('three', 'third');
  assert(2, a.length);
  assert(true, a.indexOf('one') >= 0);
  assert(true, a.indexOf('two') >= 0);
}

function testGetResourceURL() {
  assertThrow(function() {
    var resource = GM_getResourceURL('urlResourceName');
  });
}

function testGetResourceText() {
  assertThrow(function() {
    var resource = GM_getResourceText('textResourceName');
  });
}

function testAddStyle() {
  var head = new ElementFake('head');
  document = new DocumentFake(head);
  var css = 'color: #decaff;';
  GM_addStyle(css);
  assert(1, head.childNodes.length);
  var style = head.childNodes[0];
  assert("style", style.tagName);
  assert("text/css", style.type);
  assert(css, style.innerHTML);
}

function testXmlhttpRequest() {
  var xhr = GM_xmlhttpRequest({});
}

function testRegisterMenuCommand() {
  assertThrow(function() {
    GM_registerMenuCommand('name', function() {}, 'a', '', 'a');
  });
}

function testOpenInTab() {
  var mockWindow = {
    open: function(url, name, opt_options) {
      this.openedUrl = url;
      this.openedName = name;
      this.openedOptions = opt_options;
    }
  };
  window = mockWindow;
  var url = 'http://example.com';
  GM_openInTab(url);
  assert(mockWindow.openedUrl, url);
}

function testLog() {
  var mockWindow = {
    console: {
      message: null,
      log: function(message) {
        this.message = message;
      }
    }
  };
  window = mockWindow;
  var message = 'hello world';
  GM_log(message);
  assert(message, mockWindow.console.message);
}

function LocalStorageFake() {
  this.map_ = {};
  this.keys_ = [];
}
LocalStorageFake.prototype = {
  length: 0,
  key: function(index) {
    if (index >= this.length) {
      throw new Error('INDEX_SIZE_ERR');
    }
    return this.keys_[index];
  },
  getItem: function(key) {
    if (key in this.map_) {
      return this.map_[key];
    }
    return null;
  },
  setItem: function(key, data) {
    this.map_[key] = data;
    this.updateKeys_();
  },
  removeItem: function(key) {
    delete this.map_[key];
    this.updateKeys_();
  },
  clear: function() {
    this.map_ = {};
    this.updateKeys_();
  },
  updateKeys_: function() {
    var keys = [];
    for (var key in this.map_) {
      keys.push(key);
    }
    this.keys_ = keys;
    this.length = keys.length;
  }
}

function DocumentFake(head) {
  this.head_ = head;
}
DocumentFake.prototype = {
  getElementsByTagName: function(tagName) {
    if (tagName == 'head' && this.head_) {
      return [this.head_];
    }
    return [];
  },
  createElement: function(tagName) {
    return new ElementFake(tagName);
  }
}

function ElementFake(tagName) {
  this.tagName = tagName;
}
ElementFake.prototype = {
  childNodes: [],
  appendChild: function(e) {
    this.childNodes.push(e);
    return e;
  },
}

function assert(expected, actual) {
  if (expected !== actual) {
    throw new Error('Assert failed: "' + expected + '" !== "' + actual + '"');
  }
}

function fail() {
  throw new Error('Fail');
}

function assertThrow(f) {
  var threw = false;
  try {
    f();
  } catch(e) {
    threw = true;
  }
  if (!threw) {
    throw new Error('Assert failed, expression did not throw.');
  }
}
