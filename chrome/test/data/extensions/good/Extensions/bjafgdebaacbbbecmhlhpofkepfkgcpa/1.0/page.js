var win = window;
if (typeof(contentWindow) != 'undefined') {
  win = contentWindow;
}

chrome.extension.onConnect.addListener(function(port) {
  console.log('connected');
  port.onMessage.addListener(function(msg) {
    console.log('got ' + msg);
    if (msg.testPostMessage) {
      port.postMessage({success: true});
    } else if (msg.testDisconnect) {
      port.disconnect();
    } else if (msg.testDisconnectOnClose) {
      win.location = "about:blank";
    }
    // Ignore other messages since they are from us.
  });
});

// Tests that postMessage to the extension and its response works.
win.testPostMessageFromTab = function() {
  var port = chrome.extension.connect();
  port.postMessage({testPostMessageFromTab: true});
  port.onMessage.addListener(function(msg) {
    win.domAutomationController.send(msg.success);
    console.log('sent ' + msg.success);
    port.disconnect();
  });
  console.log('posted message');
}
