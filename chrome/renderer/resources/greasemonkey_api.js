// Implementation of the Greasemonkey API, see:
// http://wiki.greasespot.net/Greasemonkey_Manual:APIs

function GM_getValue(name, defaultValue) {
  throw new Error("not implemented.");
}

function GM_setValue(name, value) {
  throw new Error("not implemented.");
}

function GM_deleteValue(name) {
  throw new Error("not implemented.");
}

function GM_listValues() {
  throw new Error("not implemented.");
}

function GM_getResourceURL(resourceName) {
  throw new Error("not implemented.");
}

function GM_getResourceText(resourceName) {
  throw new Error("not implemented.");
}

function GM_addStyle(css) {
  throw new Error("not implemented.");
}

function GM_xmlhttpRequest(details) {
  throw new Error("not implemented.");
}

function GM_registerMenuCommand(commandName, commandFunc, accelKey,
                                accelModifiers, accessKey) {
  throw new Error("not implemented.");
}

function GM_openInTab(url) {
  throw new Error("not implemented.");
}

function GM_log(message) {
  window.console.log(message);
}
