port = null;
function getPort() {
  if (!port) {
    port = chrome.extension.connect();
    port.onMessage.addListener(function(msg, port) {
      fillPasswords(msg.password);
    });
  }
  return port;
}

function getPassword() {
  getPort().postMessage({queryPassword: true,
                        url: document.location.href});
}

function fillPasswords(password) {
  forEachPasswordField(function(form) {
    form.value = password;
  });
}

function forEachPasswordField(callback) {
  for (var i = 0; i < document.forms.length; ++i) {
    var form = document.forms[i];
    for (var j = 0; j < form.length; ++j) {
      if (form[j].type.toLowerCase() == 'password') {
        callback(form[j]);
      }
    }
  }
}

function findPasswordFields() {
  var found = false;
  forEachPasswordField(function(form) {found = true;});

  if (found)
    getPort().postMessage({canQueryPassword: found});
}

function registerKeybind() {
  // looking for alt+` which is 18, 192
  var lastKeyDown = 0;
  document.addEventListener(
    'keydown',
    function(e) {
      if (lastKeyDown == 18 && e.keyCode == 192) {
        e.stopPropagation();
        e.preventDefault();
        getPassword();
      }
      lastKeyDown = e.keyCode;
    },
    false
  );
  document.addEventListener(
    'keyup',
    function(e) { lastKeyDown = 0; },
    false
  );
}

if (window == top) {
  registerKeybind();
  findPasswordFields();
  window.addEventListener("focus", findPasswordFields);
}
