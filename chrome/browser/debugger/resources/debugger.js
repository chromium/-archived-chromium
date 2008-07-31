/**
 * @fileoverview Helper functions and objects for the JS debugger UI.
 * @see debugger.html
 */

/**
 * Called at the end of <body>.
 */
function loaded() {
  focusOnCommandLine();
};

/**
 * Sets focus to command-line-text element.
 */
function focusOnCommandLine() {
  var input = document.getElementById('command-line-text');
  input.focus();
};

/**
 * Called by chrome code when there's output to display.
 */
function appendText(txt) {
  var output = document.getElementById('output');
  output.appendChild(document.createTextNode(txt));
  output.appendChild(document.createElement('br'));
  document.body.scrollTop = document.body.scrollHeight;
};

// command object stores command-line history state.
var command = {
  history: [],
  history_index: 0,
  pending: null
};

/**
 * Execute a debugger command, add it to the command history and display it in
 * the output window.
 */
function executeCommand(str) {
  appendText("$ " + str);
  // Sends field.value to DebuggerContents.HandleCommand.
  chrome.send("command", [str]);
  command.history.push(str);
  command.history_index = command.history.length;
  command.pending = null;
};

/**
 * Display the previous history item in the given text field.
 */
function selectPreviousCommand(field) {
  if (command.history_index > 0) {
    // Remember the current field value as a pending command if we're at the
    // end (it's something the user typed in).
    if (command.history_index == command.history.length)
      command.pending = field.value;
    command.history_index--;
    field.value = command.history[command.history_index];
    field.select();
  }
};

/**
 * Display the next history item in the given text field.
 */
function selectNextCommand(field) {
  if (command.history_index < command.history.length) {
    command.history_index++;
    if (command.history_index == command.history.length) {
      field.value = command.pending || "";
    } else {
      field.value = command.history[command.history_index];
    }
    field.select();
  }
};


/**
 * command-line-text's onkeypress handler
 */
function keypress(e) {
  var field = e.target;
  var key = e.keyCode;
  if (key == 13) { // enter
    executeCommand(field.value);
    field.value = "";
    return false;
  }
  return true;
};

/**
 * command-line-text's onkeydown handler
 */
function keydown(e) {
  var field = e.target;
  var key = e.keyCode;
  if (key == 38) { // up arrow
    selectPreviousCommand(field);
    return false;
  } else if (key == 40) { // down arrow
    selectNextCommand(field);
    return false;
  }
  return true;
};

/**
 * Called by chrome code to set the current state as to whether the debugger
 * is stopped at a breakpoint or is running.
 */
function setDebuggerBreak(is_broken) {
  var out = document.getElementById('output');
  if (is_broken) {
    out.style.color = "black";
    focusOnCommandLine();
  } else {
    out.style.color = "gray";
  }
};
