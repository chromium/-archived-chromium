// The port for communicating back to the extension.
var benchmarkExtensionPort = chrome.extension.connect();

// The url is what this page is known to the benchmark as.
// The benchmark uses this id to differentiate the benchmark's
// results from random pages being browsed.

// TODO(mbelshe): If the page redirects, the location changed and the
// benchmark stalls.
var benchmarkExtensionUrl = window.location.toString();

function sendTimesToExtension() {
  var times = window.chromium.GetLoadTimes();
  if (times.finishLoadTime != 0) {
    benchmarkExtensionPort.postMessage({message: "load", url: benchmarkExtensionUrl, values: times});
  } else {
    window.setTimeout(sendTimesToExtension, 100);
  }
}

function loadComplete() {
  // Only trigger for top-level frames (e.g. the one we benchmarked)
  if (window.parent == window) {
    sendTimesToExtension();
  }
}

window.addEventListener("load", loadComplete);
