// Implementation of the Greasemonkey API, see:
// http://wiki.greasespot.net/Greasemonkey_Manual:APIs

const MIN_INT_32 = -0x80000000;
const MAX_INT_32 = 0x7FFFFFFF;

// Prefix for user script values that are stored in localStorage.
const STORAGE_NS = "__userscript__.";

function GM_getValue(name, defaultValue) {
  var value = localStorage.getItem(STORAGE_NS + name);
  return value ? value : defaultValue;
}

function GM_setValue(name, value) {
  // The values for GM_getValue() and GM_setValue() can only be boolean,
  // strings, or 32 bit integers.  See the setPrefs function in:
  // http://greasemonkey.devjavu.com/browser/trunk/src/chrome/chromeFiles/content/prefmanager.js
  var goodType = false;
  switch (typeof(value)) {
    case "string":
    case "boolean":
      goodType = true;
      break;
    case "number":
      // Note that "value % 1 == 0" checks that the number is not a float.
      if (value % 1 == 0 && value >= MIN_INT_32 && value <= MAX_INT_32) {
        goodType = true;
      }
      break;
  }

  if (!goodType) {
    throw new Error("Unsupported type for GM_setValue. Supported types " +
                    "are: string, bool, and 32 bit integers.");
  }

  localStorage.setItem(STORAGE_NS + name, value);
}

function GM_deleteValue(name) {
  localStorage.removeItem(STORAGE_NS + name);
}

function GM_listValues() {
  var values = [];
  for (var i = 0; i < localStorage.length; i++) {
    var key = localStorage.key(i);
    if (key.indexOf(STORAGE_NS) == 0) {
      key = key.substring(STORAGE_NS.length);
      values.push(key);
    }
  }
  return values;
}

function GM_getResourceURL(resourceName) {
  throw new Error("not implemented.");
}

function GM_getResourceText(resourceName) {
  throw new Error("not implemented.");
}

function GM_addStyle(css) {
  var head = document.getElementsByTagName("head")[0];
  if (!head) {
    return;
  }
  var style = document.createElement("style");
  style.type = "text/css";
  style.innerHTML = css;
  head.appendChild(style);
}

function GM_xmlhttpRequest(details) {
  throw new Error("not implemented.");
}

function GM_registerMenuCommand(commandName, commandFunc, accelKey,
                                accelModifiers, accessKey) {
  throw new Error("not implemented.");
}

function GM_openInTab(url) {
  window.open(url, "");
}

function GM_log(message) {
  window.console.log(message);
}
