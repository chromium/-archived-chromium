onmessage = function(evt) {
  if (evt.data == "ping")
    postMessage("pong");
  else if (/eval.+/.test(evt.data)) {
    try {
      postMessage(eval(evt.data.substr(5)));
    } catch (ex) {
      postMessage(ex);
    }
  }
}
