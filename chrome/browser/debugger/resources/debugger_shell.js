/**
 * @fileoverview Shell objects and global helper functions for Chrome 
 * automation shell / debugger.  This file is loaded into the global namespace
 * of the interactive shell, so users can simply call global functions 
 * directly.
 */

// TODO(erikkay): look into how this can be split up into multiple files
// It's currently loaded explicitly by Chrome, so maybe I need an "include"
// or "source" builtin to allow a core source file to reference multiple
// sub-files.

/**
 * Sequence number of the DebugCommand.
 */
DebugCommand.next_seq_ = 0;

/**
 * Command messages to be sent to the debugger.
 * @constructor
 */
function DebugCommand(str) {
  this.command = undefined;
  // first, strip off of the leading word as the command
  var argv = str.split(' ');
  this.user_command = argv.shift();
  // the rest of the string is argv to the command
  str = argv.join(' ');
  if (DebugCommand.aliases[this.user_command])
    this.user_command = DebugCommand.aliases[this.user_command];
 if (this.parseArgs_(str) == 1)
   this.type = "request";	
 if (this.command == undefined)	
   this.command = this.user_command;
};

// Mapping of some control characters to avoid the \uXXXX syntax for most
// commonly used control cahracters.
const ctrlCharMap_ = {
  '\b': '\\b',
  '\t': '\\t',
  '\n': '\\n',
  '\f': '\\f',
  '\r': '\\r',
  '"' : '\\"',
  '\\': '\\\\'
};

// Regular expression matching ", \ and control characters (0x00 - 0x1F)
// globally.
const ctrlCharMatch_ = /["\\\\\x00-\x1F]/g;

/**
 * Convert a String to its JSON representation.
 * @param {String} value - String to be converted
 * @return {String} JSON formatted String
 */
DebugCommand.stringToJSON = function(value) {
  // Check for" , \ and control characters (0x00 - 0x1F).
  if (ctrlCharMatch_.test(value)) {
    // Replace ", \ and control characters (0x00 - 0x1F).
    return '"' + value.replace(ctrlCharMatch_, function (char) {
        // Use charmap if possible.
        var mapped = ctrlCharMap_[char];
        if (mapped) return mapped;
        mapped = char.charCodeAt();
        // Convert control character to unicode escape sequence.
        var dig1 = (Math.floor(mapped / 16));
        var dig2 = (mapped % 16)
        return '\\u00' + dig1.toString(16) + dig2.toString(16);
      })
    + '"';
  }

  // Simple string with no special characters.
  return '"' + value + '"';
};

/**
 * @return {bool} True if x is an integer.
 */
DebugCommand.isInt = function(x) { 
   var y = parseInt(x); 
   if (isNaN(y))
     return false; 
   return x == y && x.toString() == y.toString(); 
};

/**
 * @return {float} log base 10 of num
 */
DebugCommand.log10 = function(num) {
  return Math.log(num)/Math.log(10);
};

/**
 * Take an object and encode it (non-recursively) as a JSON dict.
 * @param {Object} obj - object to encode
 */
DebugCommand.toJSON = function(obj) {
  // TODO(erikkay): use a real JSON library
  var json = '{';
  for (var key in obj) {
    if (json.length > 1)
      json += ",";
    var val = obj[key];
    if (!DebugCommand.isInt(val)) {
      val = DebugCommand.stringToJSON(val.toString());
    }
    json += '"' + key + '":' + val;
  }
  json += '}';
  return json;
};

/**
 * Encode the DebugCommand object into the V8 debugger JSON protocol format.
 * @see http://wiki/Main/V8Debugger
 */
DebugCommand.prototype.toJSONProtocol = function() {
  // TODO(erikkay): use a real JSON library
  var json = '{';
  json += '"seq":"' + this.seq;
  json += '","type":"' + this.type;
  json += '","command":"' + this.command + '"';
  if (this.arguments) {
    json += ',"arguments":' + DebugCommand.toJSON(this.arguments);
  }
  json += '}'
  return json;
}

/**
 * Encode the contents of this message and send it to the debugger.
 * @param {Object} tab - tab being debugged.  This is an internal
 *     Chrome object.
 */
DebugCommand.prototype.sendToDebugger = function(tab) {
  this.seq = DebugCommand.next_seq_++;
  str = this.toJSONProtocol();
  dprint("sending: " + str);
  tab.sendToDebugger(str);
};

DebugCommand.trim = function(str) {
  return str.replace(/^\s*/, '').replace(/\s*$/, '');
};

/**
 * Strip off a trailing parameter after a ':'. As the identifier for the
 * source can contain ':' characters (e.g. 'http://www....) something after
 * a ':' is only considered a parameter if it is numeric. 
 * @return {Array} two element array, the trimmed string and the parameter,
 *   or -1 if no parameter
 */
DebugCommand.stripTrailingParameter = function(str, opt_separator) {
  var sep = opt_separator || ':';
  var index = str.lastIndexOf(sep);
  // If a separator character if found strip if numeric.
  if (index != -1) {
    var value = parseInt(str.substring(index + 1, str.length), 10);
    if (isNaN(value) || value < 0) {
      return [str, -1];
    }
    str = str.substring(0, index);
    return [str, value];
  }
  return [str, -1];
};

/**
 * Format source and location strings based on source location input data.
 * @param {Object} script - script information object
 * @param {String} source - source code for the current location
 * @param {int} line - line number (0-based)
 * @param {String} func - function name
 * @return {array} [location(string), source line(string), line number(int)]
 */
DebugCommand.getSourceLocation = function(script, source, line, func) {
  // source line is 0-based, we present as 1-based
  line++;

  // TODO(erikkay): take column into account as well
  if (source)
    source = "" + line + ": " + source;
  var location = '';
  if (func) {
    location = func + ", ";
  }
  location += script ? script.name : '[no source]';
  return [location, source, line];
};

/**
 * Aliases for debugger commands.
 */
DebugCommand.aliases = {
  'b': 'break',
  'bi': 'break_info',
  'br': 'break',
  'bt': 'backtrace',
  'c': 'continue',
  'f': 'frame',
  'h': 'help',
  '?': 'help',
  'ls': 'source',
  'n': 'next',
  'p': 'print',
  's': 'step',
  'so': 'stepout',
};

/**
 * Parses arguments to "args" and "locals" command, and initializes
 * the underlying DebugCommand (which is a frame request).
 * @see DebugCommand.commands
 * @param {string} str The arguments to be parsed.
 * @return -1 for usage error, 1 for success
 */
DebugCommand.prototype.parseArgsAndLocals_ = function(str) {
  this.command = "frame";
  return str.length ? -1 : 1;
};

/**
 * Parses arguments to "break_info" command, and executes it.
 * "break_info" has an optional argument, which is the breakpoint
 * identifier.
 * @see DebugCommand.commands
 * @param {string} str - The arguments to be parsed.
 * @return -1 for usage error, 0 for success
 */
DebugCommand.prototype.parseBreakInfo_ = function(str) {
  this.type = "shell";

  // Array of breakpoints to be printed by this command
  // (default to all breakpoints)
  var breakpointsToPrint = shell_.breakpoints;

  if (str.length > 0) {
    // User specified an invalid breakpoint (not a number)
    if (!str.match(/^\s*\d+\s*$/))  
      return -1; // invalid usage

    // Check that the specified breakpoint identifier exists
    var id = parseInt(str);
    var info = shell_.breakpoints[id];
    if (!info) {
      print("Error: Invalid breakpoint");
      return 0; // success (of sorts)
    }
    breakpointsToPrint = [info];
  } else {
    // breakpointsToPrint.length isn't accurate, because of
    // deletions
    var num_breakpoints = 0;
    for (var i in breakpointsToPrint) num_breakpoints++;
    
    print("Num breakpoints: " + num_breakpoints);
  }

  DebugShell.printBreakpoints_(breakpointsToPrint);

  return 0; // success
}

/**
 * Parses arguments to "step" command. 
 * @see DebugCommand.commands
 * @param {string} str The arguments to be parsed.
 * @return -1 for usage error, 1 for success
 */
DebugCommand.prototype.parseStep_ = function(str, opt_stepaction) {
  this.command = "continue";
  action = opt_stepaction || "in";
  this.arguments = {"stepaction" : action}
  if (str.length) {
    count = parseInt(str);
    if (count > 0) {
      this.arguments["stepcount"] = count;
    } else {
      return -1;
    }
  }
  return 1;
};

/**
 * Parses arguments to "step" command. 
 * @see DebugCommand.commands
 * @param {string} str The arguments to be parsed.
 * @return -1 for usage error, 1 for success
 */
DebugCommand.prototype.parseStepOut_ = function(str) {
  return this.parseStep_(str, "out");
};

/**
 * Parses arguments to "next" command. 
 * @see DebugCommand.commands
 * @param {string} str The arguments to be parsed.
 * @return -1 for usage error, 1 for success
 */
DebugCommand.prototype.parseNext_ = function(str) {
  return this.parseStep_(str, "next");
};

/**
 * Parse the arguments to "print" command.
 * @see DebugCommand.commands
 * @param {string} str The arguments to be parsed.
 * @return 1 - always succeeds
 */
DebugCommand.prototype.parsePrint_ = function(str) {
  this.command = "evaluate";
  this.arguments = { "expression" : str };
  // If the page is in the running state, then we force the expression to
  // evaluate in the global context to avoid evaluating in a random context.
  if (shell_.running)
    this.arguments["global"] = true;
  return 1;
};

/**
 * Handle the response to a "print" command and display output to user.
 * @see http://wiki/Main/V8Debugger
 * @param {ProtocolPacket} evaluate_response - the V8 debugger response object
 */
DebugCommand.responsePrint_ = function(evaluate_response) {
  body = evaluate_response.body();
  if (body['text'] != undefined) {
    print(body['text']);
  } else {
    // TODO(erikkay): is "text" ever not set?
    print("can't print response");
  }
};

/**
 * Parses arguments to "break" command.  See DebugCommand.commands below
 * for syntax details.
 * @see DebugCommand.commands
 * @param {string} str The arguments to be parsed.
 * @return -1 for usage error, 1 for success, 0 for handled internally
 */
DebugCommand.prototype.parseBreak_ = function(str) {
  function stripTrailingParameter() {
    var ret = DebugCommand.stripTrailingParameter(str, ':');
    str = ret[0];
    return ret[1];
  }
  
  if (str.length == 0) {
    this.command = "break";
    return 1;
  } else {
    var parts = str.split(/\s+/);
    var condition = null;
    if (parts.length > 1) {
      str = parts.shift();
      condition = parts.join(" ");
    }
  
    this.command = "setbreakpoint";

    // Locate ...[:line[:column]] if present.
    var line = -1;
    var column = -1;
    line = stripTrailingParameter();
    if (line != -1) {
      line -= 1;
      var l = stripTrailingParameter();
      if (l != -1) {
        column = line;
        line = l - 1;
      }
    }

    if (line == -1 && column == -1) {
      this.arguments = { 'type' : 'function',
                         'target' : str };
    } else {
      var script = shell_.matchScript(str, line);
      if (script) {
        this.arguments = { 'type' : 'script',
                           'target' : script.name };
      } else {
        this.arguments = { 'type' : 'function',
                           'target' : str };
      }
      this.arguments.line = line;
      if (column != -1)
        this.arguments.position = column;
    }
    if (condition)
      this.arguments.condition = condition;
  }
  return 1;
};

/**
 * Handle the response to a "break" command and display output to user.
 * @see http://wiki/Main/V8Debugger
 * @param {ResponsePacket} setbreakpoint_response - the V8 debugger response
 *     object
 */
DebugCommand.responseBreak_ = function(setbreakpoint_response) {
  var body = setbreakpoint_response.body();
  var info = new BreakpointInfo(
      parseInt(body.breakpoint),
      msg.command.arguments.type,
      msg.command.arguments.target,
      msg.command.arguments.line,
      msg.command.arguments.position,
      msg.command.arguments.condition);
  shell_.addedBreakpoint(info);
};

/**
 * Parses arguments to "backtrace" command.  See DebugCommand.commands below
 * for syntax details.
 * @see DebugCommand.commands
 * @param {string} str The arguments to be parsed.
 * @return -1 for usage error, 1 for success
 */
 DebugCommand.prototype.parseBacktrace_ = function(str) {
  if (str.length > 0) {
    var parts = str.split(/\s+/);
    var non_empty_parts = parts.filter(function(s) { return s.length > 0; });
    // We need exactly two arguments.
    if (non_empty_parts.length != 2) {
      return -1;
    }
    var from = parseInt(non_empty_parts[0], 10);
    var to = parseInt(non_empty_parts[1], 10);
    // The two arguments have to be integers.
    if (from != non_empty_parts[0] || to != non_empty_parts[1]) {
      return -1;
    }
    this.arguments = { 'fromFrame': from, 'toFrame': to + 1 };
  } else {
    // Default to fetching the first 10 frames.
    this.arguments = { 'fromFrame': 0, 'toFrame': 10 };
  }
  return 1;
};

/**
 * Handle the response to a "backtrace" command and display output to user.
 * @see http://wiki/Main/V8Debugger
 * @param {ResponsePacket} backtrace_response - the V8 debugger response object
 */
DebugCommand.responseBacktrace_ = function(backtrace_response) {
  body = backtrace_response.body();
  if (body && body.totalFrames) {
    print('Frames #' + body.fromFrame + ' to #' + (body.toFrame - 1) +
	  ' of ' + body.totalFrames + ":");
    for (var i = 0; i < body.frames.length; i++) {
      print(body.frames[i].text);
    }
  } else {
    print("unimplemented (sorry)");
  }
};


/**
 * Parses arguments to "clear" command.  See DebugCommand.commands below
 * for syntax details.
 * @see DebugCommand.commands
 * @param {string} str The arguments to be parsed.
 * @return -1 for usage error, 1 for success
 */
DebugCommand.prototype.parseClearCommand_ = function(str) {
  this.command = "clearbreakpoint";
  if (str.length > 0) {
    var i = parseInt(str, 10);
    if (i != str) {
      return -1;
    }
    this.arguments = { 'breakpoint': i };
  }
  return 1;
}

/**
 * Handle the response to a "clear" command and display output to user.
 * @see http://wiki/Main/V8Debugger
 * @param {ResponsePacket} clearbreakpoint_response - the V8 debugger response
 *     object
 */
DebugCommand.responseClear_ = function(clearbreakpoint_response) {
  var body = clearbreakpoint_response.body();
  shell_.clearedBreakpoint(parseInt(msg.command.arguments.breakpoint));
}


/**
 * Parses arguments to "continue" command.  See DebugCommand.commands below
 * for syntax details.
 * @see DebugCommand.commands
 * @param {string} str The arguments to be parsed.
 * @return -1 for usage error, 1 for success
 */
DebugCommand.prototype.parseContinueCommand_ = function(str) {
  this.command = "continue";
  if (str.length > 0) {
    return -1;
  }
  return 1;
}

/**
 * Parses arguments to "frame" command.  See DebugCommand.commands below
 * for syntax details.
 * @see DebugCommand.commands
 * @param {string} str The arguments to be parsed.
 * @return -1 for usage error, 1 for success
 */
DebugCommand.prototype.parseFrame_ = function(str) {
  if (str.length > 0) {
    var i = parseInt(str, 10);
    if (i != str) {
      return -1;
    }
    this.arguments = { 'number': i };
  }
  return 1;
};

/**
 * Handle the response to a "frame" command and display output to user.
 * @see http://wiki/Main/V8Debugger
 * @param {ResponsePacket} frame_response - the V8 debugger response object
 */
DebugCommand.responseFrame_ = function(frame_response) {
  var body = frame_response.body();
  var func = frame_response.lookup(body.func.ref);
  loc = DebugCommand.getSourceLocation(func.script, 
      body.sourceLineText, body.line, func.name);
  print("#" + (body.index <= 9 ? '0' : '') + body.index + " " + loc[0]);
  print(loc[1]);
  shell_.current_frame = body.index;
  shell_.current_line = loc[2];
  shell_.current_script = func.script;
};

/**
 * Handle the response to a "args" command and display output to user.
 * @see http://wiki/Main/V8Debugger
 * @param {ProtocolPacket} frame_response - the V8 debugger response object (for
 *     "frame" command)
 */
DebugCommand.responseArgs_ = function(frame_response) {
  var body = frame_response.body();
  DebugCommand.printVariables_(body.arguments, frame_response);
}

/**
 * Handle the response to a "locals" command and display output to user.
 * @see http://wiki/Main/V8Debugger
 * @param {Object} msg - the V8 debugger response object (for "frame" command)
 */
DebugCommand.responseLocals_ = function(frame_response) {
  var body = frame_response.body();
  DebugCommand.printVariables_(body.locals, frame_response);
}

DebugCommand.printVariables_ = function(variables, protocol_packet) {
  for (var i = 0; i < variables.length; i++) {
    print(variables[i].name + " = " +
        DebugCommand.toPreviewString_(protocol_packet.lookup(variables[i].value.ref)));
  }
}

DebugCommand.toPreviewString_ = function(value) {
  // TODO(ericroman): pretty print arrays and objects, recursively.
  // TODO(ericroman): truncate length of preview if too long?
  if (value.type == "string") {
    // Wrap the string in quote marks and JS-escape
    return DebugCommand.stringToJSON(value.text);
  }
  return value.text;
}

/**
 * Parses arguments to "scripts" command.
 * @see DebugCommand.commands
 * @param {string} str - The arguments to be parsed.
 * @return -1 for usage error, 1 for success
 */
DebugCommand.prototype.parseScripts_ = function(str) {
  return 1
};

/**
 * Handle the response to a "scripts" command and display output to user.
 * @see http://wiki/Main/V8Debugger
 * @param {ResponsePacket} scripts_response - the V8 debugger response object
 */
DebugCommand.responseScripts_ = function(scripts_response) {
  scripts = scripts_response.body();
  shell_.scripts = [];
  for (var i in scripts) {
    var script = scripts[i];

    // Add this script to the internal list of scripts.
    shell_.scripts.push(script);
    
    // Print result if this response was the result of a user command.
    if (scripts_response.command.from_user) {
      var name = script.name;
      if (name) {
        if (script.lineOffset > 0) {
          print(name + " (lines " + script.lineOffset + "-" +
                (script.lineOffset + script.lineCount - 1) + ")");
        } else {
          print(name + " (lines " + script.lineCount + ")");
        }
      } else {
        // For unnamed scripts (typically eval) display some source.
        var sourceStart = script.sourceStart;
        if (sourceStart.length > 40)
          sourceStart = sourceStart.substring(0, 37) + '...';
        print("[unnamed] (source:\"" + sourceStart + "\")");
      }
    }
  }
};

/**
 * Parses arguments to "source" command.
 * @see DebugCommand.commands
 * @param {string} str - The arguments to be parsed.
 * @return -1 for usage error, 1 for success
 */
DebugCommand.prototype.parseSource_ = function(str) {
  this.arguments = {};
  if (this.current_frame > 0)
    this.arguments.frame = this.current_frame;
  if (str.length) {
    var args = str.split(" ");
    if (args.length == 1) {
      // with 1 argument n, we print 10 lines starting at n
      var num = parseInt(args[0]);
      if (num > 0) {
        this.arguments.fromLine = num - 1;
        this.arguments.toLine = this.arguments.fromLine + 10;
      } else {
        return -1;
      }
    } else if (args.length == 2) {
      // with 2 arguments x and y, we print from line x to line x + y
      var from = parseInt(args[0]);
      var len = parseInt(args[1]);
      if (from > 0 && len > 0) {
        this.arguments.fromLine = from - 1;
        this.arguments.toLine = this.arguments.fromLine + len;
      } else {
        return -1;
      }
    } else {
      return -1;
    }
    if (this.arguments.fromLine < 0)
      return -1;
    if (this.arguments.toLine <= this.arguments.fromLine)
      return -1;
  } else if (shell_.current_line > 0) {
    // with no arguments, we print 11 lines with the current line as the center
    this.arguments.fromLine =
        Math.max(0, shell_.current_line - 6);
    this.arguments.toLine = this.arguments.fromLine + 11;
  }
  return 1;
};

/**
 * Handle the response to a "source" command and display output to user.
 * @see http://wiki/Main/V8Debugger
 * @param {ProtocolPacket} source_response - the V8 debugger response object
 */
DebugCommand.responseSource_ = function(source_response) {
  var body = source_response.body();
  var from_line = parseInt(body.fromLine) + 1;
  var source = body.source;
  var lines = source.split('\n');
  var maxdigits = 1 + Math.floor(DebugCommand.log10(from_line + lines.length))
  for (var num in lines) {
    // there's an extra newline at the end
    if (num >= (lines.length - 1) && lines[num].length == 0)
      break;
    spacer = maxdigits - (1 + Math.floor(DebugCommand.log10(from_line)))
    var line = "";
    if (from_line == shell_.current_line) {
      for (var i = 0; i < (maxdigits + 2); i++)
        line += ">";
    } else {
      for (var i = 0; i < spacer; i++)
        line += " ";
      line += from_line + ": ";
    }
    line += lines[num];
    print(line);
    from_line++;
  }
};

/**
 * Parses arguments to "help" command.  See DebugCommand.commands below
 * for syntax details.
 * @see DebugCommand.commands
 * @param {string} str The arguments to be parsed.
 * @return 0 for handled internally
 */
DebugCommand.parseHelp_ = function(str) {
  DebugCommand.help(str);
  return 0;
};

/**
 * Takes argument and evaluates it in the context of the shell to allow commands
 * to be escaped to the outer shell.  Used primarily for development purposes.
 * @see DebugCommand.commands
 * @param {string} str The expression to be evaluated
 * @return 0 for handled internally
 */
DebugCommand.parseShell_ = function(str) {
  print(eval(str));
  return 0;
}

DebugCommand.parseShellDebug_ = function(str) {
  shell_.debug = !shell_.debug;
  if (shell_.debug) {
    print("shell debugging enabled");
  } else {
    print("shell debugging disabled");
  }
  return 0;
}

/**
 * Parses a user-entered command string.
 * @param {string} str The arguments to be parsed.
 */
DebugCommand.prototype.parseArgs_ = function(str) {
  if (str.length)
    str = DebugCommand.trim(str); 
  var cmd = DebugCommand.commands[this.user_command];
  if (cmd) {
    var parse = cmd['parse'];
    if (parse == undefined) {
      print('>>>can\'t find parse func for ' + this.user_command);
      this.type = "error";
    } else {
      var ret = parse.call(this, str);
      if (ret > 0) {
        // Command gererated a debugger request.
        this.type = "request";
      } else if (ret == 0) {
        // Command handeled internally.
        this.type = "handled";
      } else if (ret < 0) {
        // Command error.
        this.type = "handled";
        DebugCommand.help(this.user_command);
      }
    }
  } else {
    this.type = "handled";
    print('unknown command: ' + this.user_command);
    DebugCommand.help();
  }
};

/**
 * Displays command help or all help.
 * @param {string} opt_str Which command to print help for.
 */
DebugCommand.help = function(opt_str) {
  if (opt_str) {
    var cmd = DebugCommand.commands[opt_str];
    var usage = cmd.usage;
    print('usage: ' + usage);
    // Print additional details for the command.
    if (cmd.help) {
      print(cmd.help);
    }
  } else {
    if (shell_.running) {
      print('Status: page is running');
    } else {
      print('Status: page is paused');
    }
    print('Available commands:');
    for (var key in DebugCommand.commands) {
      var cmd = DebugCommand.commands[key];
      if (!cmd['hidden'] && (!shell_.running || cmd['while_running'])) {
        var usage = cmd.usage;
        print('   ' + usage);
      }
    }
  }
};

/**
 * Valid commands, their argument parser and their associated usage text.
 */
DebugCommand.commands = {
  'args': { 'parse': DebugCommand.prototype.parseArgsAndLocals_, 
            'usage': 'args',
            'help': 'summarize the arguments to the current function.',
            'response': DebugCommand.responseArgs_ },
  'break': { 'parse': DebugCommand.prototype.parseBreak_, 
             'response': DebugCommand.responseBreak_,
             'usage': 'break [location] <condition>',
             'help': 'location is one of <function> | <script:function> | <script:line> | <script:line:pos>',
             'while_running': true },
  'break_info': { 'parse': DebugCommand.prototype.parseBreakInfo_, 
                  'usage': 'break_info [breakpoint #]',
                  'help': 'list the current breakpoints, or the details on a single one',
                  'while_running': true },
  'backtrace': { 'parse': DebugCommand.prototype.parseBacktrace_,
                 'response': DebugCommand.responseBacktrace_,
                 'usage': 'backtrace [from frame #] [to frame #]' },  
  'clear': { 'parse': DebugCommand.prototype.parseClearCommand_, 
             'response': DebugCommand.responseClear_,
             'usage': 'clear <breakpoint #>',
             'while_running': true },
  'continue': { 'parse': DebugCommand.prototype.parseContinueCommand_, 
                'usage': 'continue' },
  'frame': { 'parse': DebugCommand.prototype.parseFrame_, 
             'response': DebugCommand.responseFrame_,
             'usage': 'frame <frame #>' },
  'help': { 'parse': DebugCommand.parseHelp_, 
            'usage': 'help [command]',
            'while_running': true },
  'locals': { 'parse': DebugCommand.prototype.parseArgsAndLocals_, 
              'usage': 'locals',
              'help': 'summarize the local variables for current frame',
              'response': DebugCommand.responseLocals_ },
  'next': { 'parse': DebugCommand.prototype.parseNext_, 
            'usage': 'next' } ,
  'print': { 'parse': DebugCommand.prototype.parsePrint_,
             'response': DebugCommand.responsePrint_,
             'usage': 'print <expression>',
             'while_running': true },
  'scripts': { 'parse': DebugCommand.prototype.parseScripts_,
              'response': DebugCommand.responseScripts_,
              'usage': 'scripts',
              'while_running': true },
  'source': { 'parse': DebugCommand.prototype.parseSource_,
               'response': DebugCommand.responseSource_,
               'usage': 'source [from line] | [<from line> <num lines>]' },
  'step': { 'parse': DebugCommand.prototype.parseStep_, 
            'usage': 'step' },
  'stepout': { 'parse': DebugCommand.prototype.parseStepOut_,
               'usage': 'stepout' },
  // local eval for debugging - remove this later
  'shell': { 'parse': DebugCommand.parseShell_,
             'usage': 'shell <expression>',
             'while_running': true,
             'hidden': true },
  'shelldebug': { 'parse': DebugCommand.parseShellDebug_,
                  'usage': 'shelldebug',
                  'while_running': true,
                  'hidden': true },
};


/**
 * Debug shell using the new JSON protocol
 * @param {Object} tab - which tab is to be debugged.  This is an internal
 *     Chrome object.
 * @constructor
 */
function DebugShell(tab) {
  this.tab = tab;
  this.tab.attach();
  this.ready = true;
  this.running = true;
  this.current_command = undefined;
  this.pending_commands = [];
  // The auto continue flag is used to indicate whether the JavaScript execution
  // should automatically continue after a break event and the processing of
  // pending commands. This is used to make it possible for the user to issue
  // commands, e.g. setting break points, without making an explicit break. In
  // this case the debugger will silently issue a forced break issue the command
  // and silently continue afterwards.
  this.auto_continue = false;
  this.debug = false;
  this.current_line = -1;
  this.current_pos = -1;
  this.current_frame = 0;
  this.current_script = undefined;
  this.scripts = [];

  // Mapping of breakpoints id --> info.
  // Must use numeric keys.
  this.breakpoints = [];
};

DebugShell.prototype.set_ready = function(ready) {
  if (ready != this.ready) {
    this.ready = ready;
    chrome.setDebuggerReady(this.ready);
  }
};

DebugShell.prototype.set_running = function(running) {
  if (running != this.running) {
    this.running = running;
    chrome.setDebuggerBreak(!this.running);
  }
};

/**
 * Execute a constructed DebugCommand object if possible, otherwise pend.
 * @param cmd {DebugCommand} - command to execute
 */
DebugShell.prototype.process_command = function(cmd) {
  dprint("Running: " + (this.running ? "yes" : "no"));

  // The "break" commands needs to be handled seperatly
  if (cmd.command == "break") {
    if (this.running) {
      // Schedule a break.
      print("Stopping JavaScript execution...");
      this.tab.debugBreak(false);
    } else {
      print("JavaScript execution already stopped.");
    }
    return;
  }
  
  // If page is running an break needs to be issued.
  if (this.running) {
    // Some requests are not valid when the page is running.
    var cmd_info = DebugCommand.commands[cmd.user_command];
    if (!cmd_info['while_running']) {
      print(cmd.user_command + " can only be run while paused");
      return;
    }

    // Add the command as pending before scheduling a break.    
    this.pending_commands.push(cmd);
    dprint("pending command: " + cmd.toJSONProtocol());

    // Schedule a forced break and enable auto continue.
    this.tab.debugBreak(true);
    this.auto_continue = true;
    this.set_ready(false);
    return;
  }
  
  // If waiting for a response add command as pending otherwise send the
  // command.
  if (this.current_command) {
    this.pending_commands.push(cmd);
    dprint("pending command: " + cmd.toJSONProtocol());
  } else {
    this.current_command = cmd;
    cmd.sendToDebugger(this.tab);
    this.set_ready(false);
  }
};

/**
 * Handle a break event from the debugger.
 * @param msg {Object} - event protocol message to handle
 */
DebugShell.prototype.event_break = function(break_event) {
  this.current_frame = 0;
  this.set_running(false);
  var body = break_event.body();
  if (body) {
    this.current_script = body.script;
    var loc = DebugCommand.getSourceLocation(body.script,
        body.sourceLineText, body.sourceLine, body.invocationText);
    var location = loc[0];
    var source = loc[1];
    this.current_line = loc[2];
    if (body.breakpoints) {
      // Always disable auto continue if a real break point is hit.
      this.auto_continue = false;
      var breakpoints = body.breakpoints;
      print("paused at breakpoint " + breakpoints.join(",") + ": " + 
            location);
      for (var i = 0; i < breakpoints.length; i++)
        this.didHitBreakpoint(parseInt(breakpoints[i]));
    } else if (body.scriptData == "") {
      print("paused");
    } else {
      // step, stepout, next, "break" and a "debugger" line in the code
      // are all treated the same (they're not really distinguishable anyway)
      if (location != this.last_break_location) {
        // We only print the location (function + script) when it changes, 
        // so as we step, you only see the source line when you transition
        // to a new script and/or function. Also if auto continue is enables
        // don't print the break location.
        if (!this.auto_continue)
          print(location);
      }
    }
    // Print th current source line unless auto continue is enabled.
    if (source && !this.auto_continue)
      print(source);
    this.last_break_location = location;
  }
  if (!this.auto_continue)
    this.set_ready(true);
};

/**
 * Handle an exception event from the debugger.
 * @param msg {ResponsePacket} - exception_event protocol message to handle
 */
DebugShell.prototype.event_exception = function(exception_event) {
  this.set_running(false);
  this.set_ready(true);
  var body = exception_event.body();
  if (body) {
    if (body["uncaught"]) {
      print("uncaught exception " + body["exception"].text);
    } else {
      print("paused at exception " + body["exception"].text);
    }
  }
};

DebugShell.prototype.matchScript = function(script_match, line) {
  var script = null;
  // In the v8 debugger, all scripts have a name, line offset and line count
  // Script names are usually URLs which are a pain to have to type again and
  // again, so we match the tail end of the script name.  This makes it easy
  // to type break foo.js:23 rather than
  // http://www.foo.com/bar/baz/quux/test/foo.js:23.  In addition to the tail
  // of the name we also look at the lines the script cover.  If there are
  // several scripts with the same tail including the requested line we match
  // the first one encountered.
  // TODO(sgjesse) Find how to handle several matching scripts.
  var candidate_scripts = [];
  for (var i in this.scripts) {
    if (this.scripts[i].name &&
        this.scripts[i].name.indexOf(script_match) >= 0) {
      candidate_scripts.push(this.scripts[i]);
    }
  }
  for (var i in candidate_scripts) {
    var s = candidate_scripts[i];
    var from = s.lineOffset;
    var to = from + s.lineCount;
    if (from <= line && line < to) {
      script = s;
      break;
    }
  }
  if (script)
    return script;
  else
    return null;
}

// The Chrome Subshell interface requires:
// prompt(), command(), response(), exit() and on_disconnect()

/** 
 * Called by Chrome Shell to get a prompt string to display.
 */
DebugShell.prototype.prompt = function() {
  if (this.current_command)
    return '';
  if (!this.running)
    return 'v8(paused)> ';
  else
    return 'v8(running)> ';
};

/**
 * Called by Chrome Shell when command input has been received from the user.
 */
DebugShell.prototype.command = function(str) {
  if (this.tab) {
    str = DebugCommand.trim(str);
    if (str.length) {
      var cmd = new DebugCommand(str);
      cmd.from_user = true;
      if (cmd.type == "request")
        this.process_command(cmd);
    }
  } else {
    print(">>not connected to a tab");
  }
};

/**
 * Called by Chrome Shell when a response to a previous command has been
 * received.
 */
DebugShell.prototype.response = function(str) {
  var response;
  try {
    dprint("received: " + str);
    response = new ProtocolPackage(str);
  } catch (error) {
    print(error.toString(), str);
    return;
  }
  if (response.type() == "event") {
    ev = response.event();
    if (ev == "break") {
      this.event_break(response);
    } else if (ev == "exception") {
      this.event_exception(response);
    }
  } else if (response.type() == "response") {
    if (response.requestSeq() != undefined) {
      if (!this.current_command || this.current_command.seq != response.requestSeq()){
        throw("received response to unknown command " + str);
      }
    } else {
      // TODO(erikkay): should we reject these when they happen?
      print(">>no request_seq in response " + str);
    }
    var cmd = DebugCommand.commands[this.current_command.user_command]
    response.command = this.current_command;
    this.current_command = null
    this.set_running(response.running());
    if (!response.success()) {
      print(response.message());
    } else {
      var handler = cmd['response'];
      if (handler != undefined) {
        handler.call(this, response);
      }
    }
    this.set_ready(true);
  }

  // Process next pending command if any.
  if (this.pending_commands.length) {
    this.process_command(this.pending_commands.shift());
  } else if (this.auto_continue) {
    // If no more pending commands and auto continue is active issue a continue command.
    this.auto_continue = false;
    this.process_command(new DebugCommand("continue"));
  }
};

/**
 * Called when a breakpoint has been set.
 * @param {BreakpointInfo} info - details of breakpoint set.
 */
DebugShell.prototype.addedBreakpoint = function(info) {
  print("set breakpoint #" + info.id);
  this.breakpoints[info.id] = info;
}

/**
 * Called when a breakpoint has been cleared.
 * @param {int} id - the breakpoint number that was cleared.
 */
DebugShell.prototype.clearedBreakpoint = function(id) {
  assertIsNumberType(id, "clearedBreakpoint called with invalid id");

  print("cleared breakpoint #" + id);
  delete this.breakpoints[id];
}

/**
 * Called when a breakpoint has been reached.
 * @param {int} id - the breakpoint number that was hit.
 */
DebugShell.prototype.didHitBreakpoint = function(id) {
  assertIsNumberType(id, "didHitBreakpoint called with invalid id");

  var info = this.breakpoints[id];
  if (!info)
    throw "Could not find breakpoint #" + id;

  info.hit_count ++;
}

/**
 * Print a summary of the specified breakpoints.
 *
 * @param {Array<BreakpointInfo>} breakpointsToPrint - List of breakpoints. The
 *     index is unused (id is determined from the info).
 */
DebugShell.printBreakpoints_ = function(breakpoints) {
  // TODO(ericroman): this would look much nicer if we could output as an HTML
  // table. I tried outputting as formatted text table, but this looks aweful
  // once it triggers wrapping (which is very likely if the target is a script)

  // Output as a comma separated list of key=value
  for (var i in breakpoints) {
    var b = breakpoints[i];
    var props = ["id", "hit_count", "type", "target", "line", "position",
        "condition"];
    var propertyList = [];
    for (var i = 0; i < props.length; i++) {
      var prop = props[i];
      var val = b[prop];
      if (val != undefined)
        propertyList.push(prop + "=" + val);
    }
    print(propertyList.join(", "));
  }
}

/**
 * Called by Chrome Shell when the outer shell is detaching from debugging
 * this tab.
 */
DebugShell.prototype.exit = function() {
  if (this.tab) {
    this.tab.detach();
    this.tab = null;
  }
};

/**
 * Called by the Chrome Shell when the tab that the shell is debugging
 * have attached.
 */
DebugShell.prototype.on_attach = function() {
  var title = this.tab.title;
  if (!title)
    title = "Untitled";
  print('attached to ' + title);
  // on attach, we update our current script list
  var cmd = new DebugCommand("scripts");
  cmd.from_user = false;
  this.process_command(cmd);
};


/**
 * Called by the Chrome Shell when the tab that the shell is debugging
 * went away.
 */
DebugShell.prototype.on_disconnect = function() {
  print(">>lost connection to tab");
  this.tab = null;
};


/**
 * Protocol packages send from the debugger.
 * @param {string} json - raw protocol packet as JSON string.
 * @constructor
 */
function ProtocolPackage(json) {
  this.packet_ = eval('(' + json + ')');
  this.refs_ = [];
  if (this.packet_.refs) {
    for (var i = 0; i < this.packet_.refs.length; i++) {
      this.refs_[this.packet_.refs[i].handle] = this.packet_.refs[i];
    }
  }
}


/**
 * Get the packet type.
 * @return {String} the packet type
 */
ProtocolPackage.prototype.type = function() {
  return this.packet_.type;
}


/**
 * Get the packet event.
 * @return {Object} the packet event
 */
ProtocolPackage.prototype.event = function() {
  return this.packet_.event;
}


/**
 * Get the packet request sequence.
 * @return {number} the packet request sequence
 */
ProtocolPackage.prototype.requestSeq = function() {
  return this.packet_.request_seq;
}


/**
 * Get the packet request sequence.
 * @return {number} the packet request sequence
 */
ProtocolPackage.prototype.running = function() {
  return this.packet_.running ? true : false;
}


ProtocolPackage.prototype.success = function() {
  return this.packet_.success ? true : false;
}


ProtocolPackage.prototype.message = function() {
  return this.packet_.message;
}


ProtocolPackage.prototype.body = function() {
  return this.packet_.body;
}


ProtocolPackage.prototype.lookup = function(handle) {
  return this.refs_[handle];
}


/**
 * Structure that holds the details about a breakpoint.
 * @constructor
 *
 * @param {int} id - breakpoint number
 * @param {string} type - "script" or "function"
 * @param {string} target - either a function name, or script url
 * @param {int} line - line number in the script, or undefined
 * @param {int} position - column in the script, or undefined
 * @param {string} condition - boolean expression, or undefined
 */
function BreakpointInfo(id, type, target, line, position, condition) {
  this.id = id;
  this.type = type;
  this.target = target;

  if (line != undefined)
    this.line = line;
  if (position != undefined)
    this.position = position;
  if (condition != undefined)
    this.condition = condition;

  this.hit_count = 0;

  // Check that the id is numeric, otherwise will run into problems later
  assertIsNumberType(this.id, "id is not a number");
}

/**
 * Global function to enter the debugger using DebugShell.
 * User can access this in the external shell by simply typing "debug()".
 * This is called by the Chrome Shell when the shell attaches to a tab.
 * @param {Object} opt_tab - which tab is to be debugged.  This is an internal
 *     Chrome object.
 */
function debug(opt_tab) {
  shell(new DebugShell(opt_tab || chrome.browser[0].tab[0]));
};

/**
 * Print debugging message when DebugShell's debug flag is true.
 */
function dprint(str) {
  if (shell_ && shell_.debug) {
    print(str);  
  }
};

/**
 * Helper that throws error if x is not a number
 * @param x {object} - object to test type of
 * @param error_message {string} - error to throw on failure
 */
function assertIsNumberType(x, error_message) {
  if (typeof x != "number")
    throw error_message;
}
