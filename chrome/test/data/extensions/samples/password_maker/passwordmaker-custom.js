// Custom hacks to tweak the javascript edition of PasswordMaker to work as a
// Chrome extension.

// Stores our bookmark data so "getCookie" can look it up synchronously.
var bookmarkCookies = {};

// Set this to false for older versions of Chrome that don't support cookies
// in extension URLs.
var cookiesEnabled = true;

// Try to auto-detect cookie support.
setCookie('cookie-test', '42');
if (getCookie('cookie-test') != '42')
  cookiesEnabled = false;

origOnload = window.onload;
window.onload = function() {
  origOnload();

  // Get the site's URL from the query param, and fill in the input field.
  preURL.value = window.contentURL || '';
  populateURL();

  passwdMaster.onkeypress = function(e) {
    preGeneratePassword();
    if (e.charCode == 13)
      sendPassword();
  };

  // Add a button to actually send the password to the page.
  var elemTable = document.getElementsByTagName('table')[0];
  var row = document.createElement('tr');
  row.innerHTML = 
     '<td>Fill in Password</td>' +
     '<td><button onclick="sendPassword()">Accept</button></td>';
  elemTable.insertBefore(row, elemTable.firstChild);

  if (!cookiesEnabled) {
    // Hide the "Save" checkbox row since it requires cookies.
    var row = document.getElementById('savePreferencesRow');
    row.style.display = "none";

    // Add a button to save the settings.  We can't do it on unload because
    // our APIs are asynchronous.
    var row = document.createElement('tr');
    row.innerHTML =
       '<td>Save Settings:</td>' +
       '<td><button onclick="saveSettings()">Save</button></td>';
    elemTable.appendChild(row);

    // Get the settings out of the bookmark if it exists.
    getSettingsBookmark("settings3", function(bookmark) {
      bookmarkCookies["settings3"] = bookmark;
      init();
    });
  }

  document.body.style.visibility = 'visible';
  passwdMaster.focus();
}

if (!cookiesEnabled) {
  function getSettingsUrl(name) {
    return location.href + "#" + name;
  }

  function getSettingsFolder(callback) {
    var kSettings = 'PasswordMaker Settings';
    chrome.bookmarks.search(kSettings, function(bookmarks) {
      if (bookmarks.length == 1) {
        callback(bookmarks[0]);
      } else {
        chrome.bookmarks.create({title: kSettings}, function(folder) {
          callback(folder);
        });
      }
    });
  }

  function getSettingsBookmark(name, callback) {
    url = getSettingsUrl(name);
    chrome.bookmarks.search(getSettingsUrl(name), function(bookmarks) {
      if (bookmarks.length == 1) {
        callback(bookmarks[0]);
      } else {
        getSettingsFolder(function(folder) {
          chrome.bookmarks.create(
            {parentId: folder.id, title: "", url: url},
            callback);
        });
      }
    });
  }

  // Cookies don't work for extensions, so we fake it.
  // TODO: use local storage to store this info.
  origGetCookie = window.getCookie;
  window.getCookie = function(name) {
    if (bookmarkCookies[name]) {
      return unescape(bookmarkCookies[name].title);
    }
    return null;
  }

  origSetCookie = window.setCookie;
  window.setCookie = function(name, value, expires) {
    var cookie = escape(value);
    getSettingsBookmark(name, function(bookmark) {
      chrome.bookmarks.setTitle({id: bookmark.id, title: cookie});
    });
  }

  // Saves settings
  function saveSettings() {
    setCookie("settings3", exportPreferences(), null);
  }
}

// Sends our generated password up to the extension, who routes it to the
// page.
function sendPassword() {
  contentPort.postMessage({password: passwdGenerated.value});
  window.close();
}
