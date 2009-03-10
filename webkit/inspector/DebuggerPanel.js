// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview WebInspector panel representing command line debugger.
 */

/**
 * Command line debugger panel.
 * @constructor
 * @extends {WebInspector.Panel}
 */
WebInspector.DebuggerPanel = function()
{
    WebInspector.Panel.call(this);

    this.contentElement = document.createElement("div");
    this.contentElement.id = "debugger-content";
        
    var table = document.createElement("table");
    table.id = 'outer';
    var tr = document.createElement("tr");
    this._output = document.createElement("td");
    this._output.id = "output";
    this._output.valign = "bottom";
    this.appendText("Chrome JavaScript Debugger");
    this.appendText("Type 'help' for a list of commands.");
    
    tr.appendChild(this._output);
    table.appendChild(tr);
    this.contentElement.appendChild(table);
    
        
    var commandLine = document.createElement("div");
    commandLine.id = "command-line";
    var input = document.createElement("input");
    input.id = "command-line-text";
    input.addEventListener('keydown', this._onInputKeyDown.bind(this), true);
    input.addEventListener('keypress', this._onInputKeyPress.bind(this), true);
    input.type = "text";
    this.commandLineInput_ = input;
    
    commandLine.appendChild(input);
    this.contentElement.appendChild(commandLine);
    
    // command object stores command-line history state.
    this._command = {
        history: [],
        history_index: 0,
        pending: null
    };
    
    this.element.appendChild(this.contentElement);    
    p('DebuggerPanel created');
};

WebInspector.DebuggerPanel.prototype = {
    toolbarItemClass: "debugger",

    get toolbarItemLabel()
    {
        return "Debugger"; //WebInspector.UIString("Debugger");
    },

    show: function()
    {
        WebInspector.Panel.prototype.show.call(this);
        this.focusOnCommandLine();
    },
    
    /**
     * Sets focus to command-line-text element.
     */
    focusOnCommandLine: function()
    {
        if (this.visible) {
            this.commandLineInput_.focus();
        }
    },

    /**
     * Called by chrome code when there's output to display.
     * @param {string} txt
     */
    appendText: function(txt)
    {
        this._output.appendChild(document.createTextNode(txt));
        this._output.appendChild(document.createElement('br'));
    },

    /**
     * Called by chrome code to set the current state as to whether the debugger
     * is stopped at a breakpoint or is running.
     * @param {boolean} isBroken
     */
    setDebuggerBreak: function(isBroken)
    {
        var out = this._output;
        if (isBroken) {
            out.style.color = "black";
            this.focusOnCommandLine();
        } else {
            out.style.color = "gray";
        }
    },

    /**
     * Execute a debugger command, add it to the command history and display 
     * it in the output window.
     * @param {string} str
     */
    executeCommand: function(str)
    {
        this.appendText("$ " + str);
        // Sends message to DebuggerContents.HandleCommand.
        if (DebugShell.singleton) {
            DebugShell.singleton.command(str);
        } else {
            this.appendText("FAILED to send the command as DebugShell is null");
        }
        
        this._command.history.push(str);
        this._command.history_index = this._command.history.length;
        this._command.pending = null;
    },

    /**
     * Display the previous history item in the given text field.
     * @param {HTMLInputElement} field
     */
    _selectPreviousCommand: function(field)
    {
        var command = this._command;
        if (command.history_index > 0) {
            // Remember the current field value as a pending command if we're at the
            // end (it's something the user typed in).
            if (command.history_index == command.history.length)
                command.pending = field.value;
            command.history_index--;
            field.value = command.history[command.history_index];
            field.select();
        }
    },

    /**
     * Display the next history item in the given text field.
     * @param {HTMLInputElement} field
     */
    _selectNextCommand: function(field)
    {
        var command = this._command;
        if (command.history_index < command.history.length) {
            command.history_index++;
            if (command.history_index == command.history.length) {
                field.value = command.pending || "";
            } else {
                field.value = command.history[command.history_index];
            }
            field.select();
        }
    },
    
    /**
     * command-line-text's onkeydown handler.
     * @param {KeyboardEvent} e
     * @return {boolean}
     */
    _onInputKeyDown: function (e)
    {
        var field = e.target;
        var key = e.keyCode;
        if (key == 38) { // up arrow
            this._selectPreviousCommand(field);
            return false;
        } else if (key == 40) { // down arrow
            this._selectNextCommand(field);
            return false;
        }
        return true;
    },

    /**
     * command-line-text's onkeypress handler.
     * @param {KeyboardEvent} e
     * @return {boolean}
     */
    _onInputKeyPress: function (e)
    {
        var field = e.target;
        var key = e.keyCode;
        if (key == 13) { // enter
            this.executeCommand(field.value);
            field.value = "";
            return false;
        }
        return true;
    }
};

WebInspector.DebuggerPanel.prototype.__proto__ = WebInspector.Panel.prototype;
