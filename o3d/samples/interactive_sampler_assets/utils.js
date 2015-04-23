/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/**
 * @fileoverview This file contains utility functions for the interactive
 * sample runner.
 *
 */

var ie;
var opera;
var safari;
var gecko;


if (typeof console == 'undefined') {
  var console = {
    log : function() {}
  };
}

function readFile(relativePathToFile) {
  var curPath = location.href;
  var fileLoc = curPath.indexOf('file:///');
  var lastSlash = curPath.lastIndexOf('/');
  curPath = curPath.substring(0, lastSlash + 1);
  if (fileLoc != -1) {
    curPath = curPath.substring(fileLoc + 8);
  }
  var filepath = curPath + relativePathToFile;
  filepath = filepath.replace(/\//g, '\\');
  filepath = filepath.replace(/%20/g, ' ');

  try {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  } catch (e) {
    return null;
  }
  var file = Components.classes["@mozilla.org/file/local;1"].createInstance(
      Components.interfaces.nsILocalFile);
  file.initWithPath( filepath );
  if ( file.exists() == false ) {
    alert("File does not exist");
  }
  var is = Components.classes["@mozilla.org/network/file-input-stream;1"].
      createInstance(Components.interfaces.nsIFileInputStream);
  is.init( file,0x01, 00004, null);
  var sis = Components.classes["@mozilla.org/scriptableinputstream;1"].
      createInstance(Components.interfaces.nsIScriptableInputStream);
  sis.init(is);
  var output = sis.read( sis.available() );
  return output;
}

/**
* Returns an XMLHttp instance to use for asynchronous
* downloading. This method will never throw an exception, but will
* return NULL if the browser does not support XmlHttp for any reason.
* @return {XMLHttpRequest|Null}
*/
function createXmlHttpRequest() {
  try {
    if (typeof ActiveXObject != 'undefined') {
      return new ActiveXObject('Microsoft.XMLHTTP');
    } else if (window["XMLHttpRequest"]) {
      return new XMLHttpRequest();
    }
  } catch (e) {
    changeStatus(e);
  }
  return null;
};

/**
* This functions wraps XMLHttpRequest open/send function.
* It lets you specify a URL and will call the callback if
* it gets a status code of 200.
* @param {String} url The URL to retrieve
* @param {Function} callback The function to call once retrieved.
*/
function downloadUrl(url, callback) {
  var status = -1;
  var request = createXmlHttpRequest();
  if (!request) {
    return false;
  }

  request.onreadystatechange = function() {
    if (request.readyState == 4) {
      try {
        status = request.status;
      } catch (e) {
        // Usually indicates request timed out in FF.
      }
      if (status == 200) {
        callback(request.responseText, request.status);
        request.onreadystatechange = function() {};
      }
    }
  };

  request.open('GET', url, true);
  try {
    request.send(null);
  } catch (e) {
    changeStatus(e);
  }
}


function deepObjCopy(dupeObj) {
    var retObj = new Object();
    if (typeof(dupeObj) == 'object') {
        if (typeof(dupeObj.length) != 'undefined')
            var retObj = new Array();
        for (var objInd in dupeObj) {
            if (typeof(dupeObj[objInd]) == 'object') {
                retObj[objInd] = deepObjCopy(dupeObj[objInd]);
            } else if (typeof(dupeObj[objInd]) == 'string') {
                retObj[objInd] = dupeObj[objInd];
            } else if (typeof(dupeObj[objInd]) == 'number') {
                retObj[objInd] = dupeObj[objInd];
            } else if (typeof(dupeObj[objInd]) == 'boolean') {
                ((dupeObj[objInd] == true) ?
                     retObj[objInd] = true : retObj[objInd] = false);
            }
        }
    }
    return retObj;
}

function singleLevelKeyCopy(dupeObj) {
	var retObj = new Object();
	for (var objInd in dupeObj) {
		retObj[objInd] = true;
	}
	return retObj;
}

function browserFun() {
  // Browser fun.
  if (window.ActiveXObject) {
    ie = this[window.XMLHttpRequest ? 'ie7' : 'ie6'] = true;
  } else if (window.opera) {
    opera = true;
  } else if (document.childNodes && !document.all && !navigator.taintEnabled) {
    safari = true;
  } else if (document.getBoxObjectFor != null) {
    gecko = true;
  }
}

function _cel(name) {
  return document.createElement(name);
}

function _gel(name) {
  return document.getElementById(name);
}

function addEvent(a,b,c,d) {
  if (a.addEventListener) {
    a.addEventListener(b,c,d?true:false);
  } else if(a.attachEvent) {
    a.attachEvent('on'+b,c);
  } else{
    a['on'+b]=c;
  }
}

browserFun();
