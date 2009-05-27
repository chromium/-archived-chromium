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
 * @fileoverview This file is the master javascript file for the interactive
 * editor/runner for all the simple samples.
 *
 */
// For browserFun
// TODO:
// Make it so that you can make the code editing window wider...
// Make it so that you can click links to the documentation of each object
// Add Ping Pong
// Fix the spacing in the editor (when u hit tab)
// Try to see if for the javascript part you can eval line by line so that you
// can output the line number

var fileTypes = {
  'js' : 'javascript',
  'html' : 'html',
  'php' : 'php'
};

function InteractiveSample() {
  this.categories = [];
  this.codeTitles = [];
  this.adjustCodeBoxAmount = 50;
  this.selectCode;
  this.codeDiv;
  this.codeLIs = [];
  this.currentCode = new Object();
  this.curI = '';
  this.cleanWindowObj;

  // Assume we're offline until we know that file reading doesn't work
  this.online = false;
}

function sortByCategory(a, b) {
  return a.category > b.category;
}

function sortByName(a, b) {
  return a.sampleName > b.sampleName;
}

function nameToHashName(name) {
  var hashName = name.toLowerCase();
  hashName = hashName.replace(/ /g, '_');
  return hashName;
}

InteractiveSample.prototype.init = function(codeDiv) {
  this.codeDiv = codeDiv;
  this.createCategories();
  this.addShowHideClicks();

    // TODO: Sort samples
    // codeArray.sort(sortByCategory);
    //
    // for (var i=0; i < codeArray.length; i++) {
    //   codeArray[i].samples.sort(sortByName);
    // }

  // Gotta set this for the iFrame to know it and change it
    // This is specific to O3D because a transparent overlay that is
    // under the <object> tag will actually hide the <object> tag
    // unless we muck with the zIndex after the <object> loads
  window.iframeZindex = '4999';

    // This is so that we can restore the first level of the window object
    // after we run a sample... so samples can get run a lot and not clutter
    // the window object.
  this.cleanWindowObj = singleLevelKeyCopy(window);
};

InteractiveSample.prototype.createCategories = function() {
  // codeArray is from interactive_samples.js
  this.selectCode = _gel('selectCode');

  for (var i = 0; i < codeArray.length; i++) {
    var container = _cel('span');
    container.className = 'category';

    var catName = _cel('span');

    var img = _cel('img');
    img.className = 'collapse';
    img.src = 'interactive_sampler_assets/images/cleardot.gif';
    // addEvent(img, 'click', this.toggleExpand(img), false);

    catName.appendChild(img);
    catName.innerHTML += codeArray[i].category;

    container.appendChild(catName);

    var ul = _cel('ul');
    ul.className = 'categoryItems';

    container.appendChild(ul);

    for (var j = 0; j < codeArray[i].samples.length; j++) {
      var item = codeArray[i].samples[j];
      var li = _cel('li');

      li.innerHTML = item.sampleName;

      this.codeTitles.push(li);
      var files = []
      for (var index = 0; index < codeArray[i].samples[j].files.length;
           ++index) {
        var dirName = '';
        var baseName = codeArray[i].samples[j].files[index];
        var slashIndex = baseName.lastIndexOf('/');
        if (slashIndex >= 0) {
          dirName = baseName.substring(0, slashIndex + 1);
          baseName = baseName.substring(slashIndex + 1);
        }
        files[index] = dirName + 'sampler_' + baseName;
      }
      //var files = codeArray[i].samples[j].files;
      addEvent(li, 'click', this.showSample(this, item, files, li));

      if (i == 0 && j == 0) {
        this.showSample(this, item, files, li, true)();
      }

      if (window.location.hash.length > 0) {
        var hashName = nameToHashName(item.sampleName);
        if (window.location.hash.substring(1) == hashName) {
          this.showSample(this, item, files, li)();
        }
      }

      this.codeLIs.push(li);
      ul.appendChild(li);
    }

    this.selectCode.appendChild(container);
    this.categories.push(container);
  }
};

InteractiveSample.prototype.toggleShowHide = function(category,
                                                      interactiveSample) {
  return function() {
    var li = category.nextSibling;
    var el = category.childNodes[0];
    if (el.className == 'expand')
      el.className = 'collapse';
    else
      el.className = 'expand';

    if (li.style.display == 'none') {
      li.style.display = 'block';
    } else {
      li.style.display = 'none';
    }
  };
};

InteractiveSample.prototype.addShowHideClicks = function() {
  for (var i = 0; i < this.categories.length; i++) {
    categoryName = this.categories[i].childNodes[0];
    addEvent(categoryName, 'click', this.toggleShowHide(categoryName, this));
  }
};

InteractiveSample.prototype.loadLocally = function(relativeUrl, filename,
                                                   fileType,
                                                   opt_changeCodeMirror) {
  // readFile is in utils.js
  var data = readFile(relativeUrl);
  if (data == null) {
    this.online = true;
    return false;
  }
  if (fileType == 'javascript') {
    data = this.replaceV8Call(data);
  }
  if (opt_changeCodeMirror == true) {
    this.changeCodeMirror(data, fileType);
  }
  is_instance.currentCode[filename] = {
    code : data
  };
  console.log(relativeUrl + ': loaded locally.');

  return true;
};

InteractiveSample.prototype.loadRemotely = function(relativeUrl, filename,
                                                    fileType,
                                                    opt_changeCodeMirror) {
  is_instance = this;
  downloadUrl(relativeUrl, function(data, status) {
    if (fileType == 'javascript') {
      data = this.replaceV8Call(data);
    }
    if (opt_changeCodeMirror == true) {
      is_instance.changeCodeMirror(data, fileType);
    }
    is_instance.currentCode[filename] = {
      code : data
    };
  });
};

InteractiveSample.prototype.loadCode = function(filename,
                                                opt_changeCodeMirror) {
  // If the code is in the currentCode buffer, then grab it there
  // otherwise, load it via XHR
  // If opt_changeCodeMirror is specified, load it into the window

  // Get filetype
  var filenameSplit = filename.split('.');
  var extension = filenameSplit[filenameSplit.length - 1];
  var fileType = fileTypes[extension.toLowerCase()];
  var inBuffer = (this.currentCode[filename] &&
                  this.currentCode[filename].code) ? true : false;
  if (inBuffer && opt_changeCodeMirror == true) {
    this.changeCodeMirror(this.currentCode[filename].code, fileType);
  } else {
    var relativeUrl = filename;
    is_instance = this;

    if (!this.online) {
      this.loadLocally(relativeUrl, filename, fileType, opt_changeCodeMirror);
    }

    if (this.online) {
      this.loadRemotely(relativeUrl, filename, fileType, opt_changeCodeMirror);
    }
  }
};

// TODO: can is_instance just be set as is_instance = this above return
//     function()
InteractiveSample.prototype.showSample = function(is_instance, codeItem,
                                                  files, thisLI, def) {
  return function() {
    is_instance.refreshInlinePlugin();

    var codeDiv = is_instance.codeDiv;
    var codeLIs = is_instance.codeLIs;
    for (var i = 0; i < codeLIs.length; i++) {
      codeLIs[i].className = '';
    }

    // turn off the "Run Here" button if this sample should be run in its own
    // window.
    var runHere = document.getElementById('runHere');
    runHere.style.visibility = codeItem.ownWindow ? 'hidden' : 'visible';

    // For linking purposes
    if (!def) {
      window.location.hash = nameToHashName(thisLI.innerHTML);
    }

    // Make code selected designate this as selected
    thisLI.className = 'selected';

    is_instance.currentCode = new Object();


    // add file names at top
    var tab_bar = _gel('tab_bar');
    tab_bar.innerHTML = '';

    // prototype syntax
    files.each(function(file, index) {

      var tabClass = 'lb';
      if (index == 0) {
        tabClass = 'db';
        is_instance.loadCode(file, true);
      } else {
        is_instance.loadCode(file, false);
      }


      var containerDiv = _cel('div');
      containerDiv.className = 'roundedcornr_box';
      addEvent(containerDiv, 'click', is_instance.changeTab(file, is_instance));

      var html = '<div class="' + tabClass + '_top" ><div></div></div>';
      html += '<div class="' + tabClass + '_roundedcornr_content" >';
      html += file;
      html += '</div>';

      containerDiv.innerHTML = html;

      tab_bar.appendChild(containerDiv);
    });

    // is_instance.loadCode(files[0], textArea);
    is_instance.curI = files[0];
  };
};

InteractiveSample.prototype.changeTab = function(i, is_instance) {
  return function() {
    var siblings = this.parentNode.childNodes;
    is_instance.currentCode[is_instance.curI].code = is_instance.getCode();

    // Swap the colors of the tabs
    for (var z = 0; z < siblings.length; z++) {
      if (siblings[z].childNodes[1].innerHTML == i) {
        siblings[z].childNodes[0].className = 'db_top';
        siblings[z].childNodes[1].className = 'db_roundedcornr_content';
      } else {
        siblings[z].childNodes[0].className = 'lb_top';
        siblings[z].childNodes[1].className = 'lb_roundedcornr_content';
      }
    }

    is_instance.loadCode(i, true);
    is_instance.curI = i;
  };
};

InteractiveSample.prototype.increaseCodeBoxHeight = function() {
  var curHeight = this.textArea.style.height;
  curHeight = curHeight.substr(0, curHeight.indexOf('px'));
  var newHeight = parseInt(curHeight) + this.adjustCodeBoxAmount;
  newHeight += 'px';
  this.textArea.style.height = newHeight;
};

InteractiveSample.prototype.decreaseCodeBoxHeight = function() {
  var curHeight = this.textArea.style.height;
  curHeight = curHeight.substr(0, curHeight.indexOf('px'));
  var newHeight = parseInt(curHeight) - this.adjustCodeBoxAmount;
  newHeight += 'px';
  this.textArea.style.height = newHeight;
};

InteractiveSample.prototype.replaceV8Call = function(code) {
  return code.replace(
    /(o3djs.util.setMainEngine[^;\n]+)[;\n]/,
    "\n   // Can't use V8 in the interactive sampler yet.\n   //$1");
}

InteractiveSample.prototype.prepareAllCodeRun = function() {
  // TODO: Change this so it doesn't rely on the first file being HTML
  // TODO: Change this to use REGEX to replace
  this.deleteOldWindowStuff();

  this.refreshInlinePlugin();
  this.currentCode[this.curI].code = this.getCode();

  var html = '';
  for (var i in this.currentCode) {
    if (i.indexOf('.html') != -1)
      html = this.currentCode[i].code;
  }
  var pattern = /<script\s*(type\s*=\s*"[^"]*"\s*)?src\s*=\s*"/g;
  var result;
  while ((result = pattern.exec(html)) != null) {
    var scriptSrc = pattern.lastIndex;
    var scriptSrcEnd = html.indexOf('"', scriptSrc);
    var script = html.substring(scriptSrc, scriptSrcEnd);
    var endScriptLoc = html.indexOf('</script>', scriptSrcEnd) + 9;
    var found = false;
    for (var z in this.currentCode) {
      if (z == script) {
        var data = this.replaceV8Call(this.currentCode[z].code);
        script = '<script type="text/javascript" charset="utf-8">' + data +
            '</script>';
        found = true;
        break;
      }
    }
    if (found) {
      html = html.substring(0, result.index) + script +
             html.substring(endScriptLoc);
    }
  }

  // console.log(html);
  window.codeToRun = html;

  // console.log(html.slice(scriptLoc, endScriptLoc));
  // console.log(html);
};

InteractiveSample.prototype.refreshInlinePlugin = function() {
  var o3dobject = _gel('o3d');
  o3dparent = o3dobject.parentNode;
  o3dparent.removeChild(o3dobject);

  var nO3D = _cel('div');
  nO3D.id = 'o3d';
  nO3D.style.width = '300px';
  nO3D.style.height = '300px';
  nO3D.style.backgroundColor = 'gray';

  o3dparent.appendChild(nO3D);
};

InteractiveSample.prototype.runJS = function() {
  // TODO don't assume that we run javascript in any order.  Make it so that
  // it checks the HTML for which order JS goes in
  this.refreshInlinePlugin();

  // console.log(this.currentCode[0].code.split('\n'));
  window.is.startJS();
};

InteractiveSample.prototype.deleteOldWindowStuff = function() {
  for (var i in window) {
    if (typeof this.cleanWindowObj[i] == 'boolean' &&
        this.cleanWindowObj[i] == true) {
    } else {
      window[i] = null;  // Delete doesn't remove properties created with var.
      delete window[i];  // But just in case we can, we do.
    }
  }
};

InteractiveSample.prototype.startJS = function() {
  this.deleteOldWindowStuff();
  var topHolder = _gel('HTMLforInlineJavascript');
  topHolder.innerHTML = "";
  var scriptHolder = document.createElement('div');
  topHolder.appendChild(scriptHolder);
  var htmlHolder = document.createElement('div');
  topHolder.appendChild(htmlHolder);

  // NOTE: These are ordered such that no file depends on anything after it.
  var forced_includes = [
    "o3djs/base.js",
    "o3djs/error.js",
    "o3djs/dump.js",
    "o3djs/math.js",
    "o3djs/serialization.js",
    "o3djs/element.js",
    "o3djs/shape.js",
    "o3djs/rendergraph.js",
    "o3djs/test.js",
    "o3djs/particles.js",
    "o3djs/primitives.js",
    "o3djs/quaternions.js",
    "o3djs/arcball.js",
    "o3djs/event.js",
    "o3djs/debug.js",
    "o3djs/picking.js",
    "o3djs/io.js",
    "o3djs/util.js",
    "o3djs/effect.js",
    "o3djs/material.js",
    "o3djs/camera.js",
    "o3djs/canvas.js",
    "o3djs/pack.js",
    "o3djs/scene.js",
    "o3djs/loader.js",
    "o3djs/simple.js",
    "o3djs/fps.js"  // If fps isn't last on this list, update the check below.
    ];

  for (var i = 0; i < forced_includes.length; ++i) {
    var scr = document.createElement('script');
    scr.type = "text/javascript";
    scr.src = forced_includes[i];
    scriptHolder.appendChild(scr);
  }

  this.currentCode[this.curI].code = this.getCode();

  var sampleJS;
  var sampleHTML;
  for (var i in this.currentCode) {
    if (i.indexOf('.html') != -1) {
      // Change the name and id of the object tag before storing the HTML,
      // otherwise it will override the inline object we actually want the demo
      // to be run in.
      sampleHTML = this.currentCode[i].code;
      sampleHTML = sampleHTML.replace('id="o3d"', 'id="notouchy"');
      sampleHTML = sampleHTML.replace('id=\'o3d\'', 'id="notouchy"');
      sampleHTML = sampleHTML.replace('name="o3d"', 'name="notouchy"');
      sampleHTML = sampleHTML.replace('name=\'o3d\'', 'name="notouchy"');
    }
    if (i.indexOf('.js') != -1) {
      sampleJS = this.currentCode[i].code;
    }
  }

  var intervalId;
  var callback = function() {
    // fps is the last on the forced_includes list.
    if (o3djs != undefined && o3djs.fps) {
      clearInterval(intervalId);
      intervalId = null;
      // TODO Is there any chance that the onload could get called before
      // we're done loading the script tag?  If so, we'll need to add another
      // stage to the callback.
      htmlHolder.innerHTML = sampleHTML;
      var scr = document.createElement('script');
      scr.type = "text/javascript";
      scr.text = sampleJS;
      scriptHolder.appendChild(scr);
      try {
        if (typeof window.onload == 'function') {
          window.onload()
        } else if (typeof window.init == 'function') {
          window.init();
        } else if (typeof window.initClient == 'function') {
          window.initClient();
        } else if (typeof window.createClients == 'function') {
          window.createClients();
        }
      } catch(e) {
        alert(e.message);
      }

      _gel('o3d').focus();
    }
  }
  intervalId = setInterval(callback, 10);
};

InteractiveSample.prototype.changeCodeMirror = function(content, lang) {
  if (lang == 'javascript') {
    window.jsEditor.setCode(content);
    window.jsEditor.frame.style.display = 'inline';
    window.htmlEditor.frame.style.display = 'none';
  } else if (lang == 'html') {
    window.htmlEditor.setCode(content);
    window.htmlEditor.frame.style.display = 'inline';
    window.jsEditor.frame.style.display = 'none';
  }
};

InteractiveSample.prototype.getCode = function() {
  if (window.htmlEditor.frame.style.display != 'none') {
    return window.htmlEditor.getCode();
  } else if (window.jsEditor.frame.style.display != 'none') {
    return window.jsEditor.getCode();
  }
};

// Todo have the window automatically size to the size of the window

InteractiveSample.prototype.increaseWidth = function() {
  var container = document.getElementById('container');
  var curWidth = container.style.maxWidth = '1800px';
};
