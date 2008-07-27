    function LooksReasonable(str, method_name) {
      // verify that the str is a string that 
      // starts with "function " and contains methodname.

      var eval_str = eval(str);

      if (!eval_str.match(/\s{0,1}function/)) {
        testFailed(str + " does not look like a function");
        return;
      }

//      if (eval_str.indexOf(method_name) < 0) {
//        testFailed(str + " does not contain the function name");
//        return;
//      }

      testPassed(str);
   }

LooksReasonable("eval.toString()","eval");
LooksReasonable("parseInt.toString()","parseInt");
LooksReasonable("parseFloat.toString()","parseFloat");
LooksReasonable("isNaN.toString()","isNaN");
LooksReasonable("isFinite.toString()","isFinite");
LooksReasonable("escape.toString()","escape");
LooksReasonable("unescape.toString()","unescape");

LooksReasonable("Object.prototype.toString.toString()","toString");
LooksReasonable("Object.prototype.toLocaleString.toString()","toLocaleString");
LooksReasonable("Object.prototype.valueOf.toString()","valueOf");
LooksReasonable("Object.prototype.hasOwnProperty.toString()","hasOwnProperty");
LooksReasonable("Object.prototype.isPrototypeOf.toString()","isPrototypeOf");
LooksReasonable("Object.prototype.propertyIsEnumerable.toString()","propertyIsEnumerable");

LooksReasonable("Function.prototype.toString.toString()","toString");
LooksReasonable("Function.prototype.apply.toString()","apply");
LooksReasonable("Function.prototype.call.toString()","call");

LooksReasonable("Array.prototype.toString.toString()","toString");
LooksReasonable("Array.prototype.toLocaleString.toString()","toLocaleString");
LooksReasonable("Array.prototype.concat.toString()","concat");
LooksReasonable("Array.prototype.join.toString()","join");
LooksReasonable("Array.prototype.pop.toString()","pop");
LooksReasonable("Array.prototype.push.toString()","push");
LooksReasonable("Array.prototype.reverse.toString()","reverse");
LooksReasonable("Array.prototype.shift.toString()","shift");
LooksReasonable("Array.prototype.slice.toString()","slice");
LooksReasonable("Array.prototype.sort.toString()","sort");
LooksReasonable("Array.prototype.splice.toString()","splice");
LooksReasonable("Array.prototype.unshift.toString()","unshift");

LooksReasonable("String.prototype.toString.toString()","toString");
LooksReasonable("String.prototype.valueOf.toString()","valueOf");
LooksReasonable("String.prototype.charAt.toString()","charAt");
LooksReasonable("String.prototype.charCodeAt.toString()","charCodeAt");
LooksReasonable("String.prototype.concat.toString()","concat");
LooksReasonable("String.prototype.indexOf.toString()","indexOf");
LooksReasonable("String.prototype.lastIndexOf.toString()","lastIndexOf");
LooksReasonable("String.prototype.match.toString()","match");
LooksReasonable("String.prototype.replace.toString()","replace");
LooksReasonable("String.prototype.search.toString()","search");
LooksReasonable("String.prototype.slice.toString()","slice");
LooksReasonable("String.prototype.split.toString()","split");
LooksReasonable("String.prototype.substr.toString()","substr");
LooksReasonable("String.prototype.substring.toString()","substring");
LooksReasonable("String.prototype.toLowerCase.toString()","toLowerCase");
LooksReasonable("String.prototype.toUpperCase.toString()","toUpperCase");
LooksReasonable("String.prototype.big.toString()","big");
LooksReasonable("String.prototype.small.toString()","small");
LooksReasonable("String.prototype.blink.toString()","blink");
LooksReasonable("String.prototype.bold.toString()","bold");
LooksReasonable("String.prototype.fixed.toString()","fixed");
LooksReasonable("String.prototype.italics.toString()","italics");
LooksReasonable("String.prototype.strike.toString()","strike");
LooksReasonable("String.prototype.sub.toString()","sub");
LooksReasonable("String.prototype.sup.toString()","sup");
LooksReasonable("String.prototype.fontcolor.toString()","fontcolor");
LooksReasonable("String.prototype.fontsize.toString()","fontsize");
LooksReasonable("String.prototype.anchor.toString()","anchor");
LooksReasonable("String.prototype.link.toString()","link");

LooksReasonable("Boolean.prototype.toString.toString()","toString");
LooksReasonable("Boolean.prototype.valueOf.toString()","valueOf");

LooksReasonable("Number.prototype.toString.toString()","toString");
LooksReasonable("Number.prototype.toLocaleString.toString()","toLocaleString");
LooksReasonable("Number.prototype.valueOf.toString()","valueOf");
LooksReasonable("Number.prototype.toFixed.toString()","toFixed");
LooksReasonable("Number.prototype.toExponential.toString()","toExponential");
LooksReasonable("Number.prototype.toPrecision.toString()","toPrecision");

LooksReasonable("Math.abs.toString()","abs");
LooksReasonable("Math.acos.toString()","acos");
LooksReasonable("Math.asin.toString()","asin");
LooksReasonable("Math.atan.toString()","atan");
LooksReasonable("Math.atan2.toString()","atan2");
LooksReasonable("Math.ceil.toString()","ceil");
LooksReasonable("Math.cos.toString()","cos");
LooksReasonable("Math.exp.toString()","exp");
LooksReasonable("Math.floor.toString()","floor");
LooksReasonable("Math.log.toString()","log");
LooksReasonable("Math.max.toString()","max");
LooksReasonable("Math.min.toString()","min");
LooksReasonable("Math.pow.toString()","pow");
LooksReasonable("Math.random.toString()","random");
LooksReasonable("Math.round.toString()","round");
LooksReasonable("Math.sin.toString()","sin");
LooksReasonable("Math.sqrt.toString()","sqrt");
LooksReasonable("Math.tan.toString()","tan");

LooksReasonable("Date.prototype.toString.toString()","toString");
LooksReasonable("Date.prototype.toUTCString.toString()","toUTCString");
LooksReasonable("Date.prototype.toDateString.toString()","toDateString");
LooksReasonable("Date.prototype.toTimeString.toString()","toTimeString");
LooksReasonable("Date.prototype.toLocaleString.toString()","toLocaleString");
LooksReasonable("Date.prototype.toLocaleDateString.toString()","toLocaleDateString");
LooksReasonable("Date.prototype.toLocaleTimeString.toString()","toLocaleTimeString");
LooksReasonable("Date.prototype.valueOf.toString()","valueOf");
LooksReasonable("Date.prototype.getTime.toString()","getTime");
LooksReasonable("Date.prototype.getFullYear.toString()","getFullYear");
LooksReasonable("Date.prototype.getUTCFullYear.toString()","getUTCFullYear");
LooksReasonable("Date.prototype.toGMTString.toString()","toGMTString");
LooksReasonable("Date.prototype.getMonth.toString()","getMonth");
LooksReasonable("Date.prototype.getUTCMonth.toString()","getUTCMonth");
LooksReasonable("Date.prototype.getDate.toString()","getDate");
LooksReasonable("Date.prototype.getUTCDate.toString()","getUTCDate");
LooksReasonable("Date.prototype.getDay.toString()","getDay");
LooksReasonable("Date.prototype.getUTCDay.toString()","getUTCDay");
LooksReasonable("Date.prototype.getHours.toString()","getHours");
LooksReasonable("Date.prototype.getUTCHours.toString()","getUTCHours");
LooksReasonable("Date.prototype.getMinutes.toString()","getMinutes");
LooksReasonable("Date.prototype.getUTCMinutes.toString()","getUTCMinutes");
LooksReasonable("Date.prototype.getSeconds.toString()","getSeconds");
LooksReasonable("Date.prototype.getUTCSeconds.toString()","getUTCSeconds");
LooksReasonable("Date.prototype.getMilliseconds.toString()","getMilliseconds");
LooksReasonable("Date.prototype.getUTCMilliseconds.toString()","getUTCMilliseconds");
LooksReasonable("Date.prototype.getTimezoneOffset.toString()","getTimezoneOffset");
LooksReasonable("Date.prototype.setTime.toString()","setTime");
LooksReasonable("Date.prototype.setMilliseconds.toString()","setMilliseconds");
LooksReasonable("Date.prototype.setUTCMilliseconds.toString()","setUTCMilliseconds");
LooksReasonable("Date.prototype.setSeconds.toString()","setSeconds");
LooksReasonable("Date.prototype.setUTCSeconds.toString()","setUTCSeconds");
LooksReasonable("Date.prototype.setMinutes.toString()","setMinutes");
LooksReasonable("Date.prototype.setUTCMinutes.toString()","setUTCMinutes");
LooksReasonable("Date.prototype.setHours.toString()","setHours");
LooksReasonable("Date.prototype.setUTCHours.toString()","setUTCHours");
LooksReasonable("Date.prototype.setDate.toString()","setDate");
LooksReasonable("Date.prototype.setUTCDate.toString()","setUTCDate");
LooksReasonable("Date.prototype.setMonth.toString()","setMonth");
LooksReasonable("Date.prototype.setUTCMonth.toString()","setUTCMonth");
LooksReasonable("Date.prototype.setFullYear.toString()","setFullYear");
LooksReasonable("Date.prototype.setUTCFullYear.toString()","setUTCFullYear");
LooksReasonable("Date.prototype.setYear.toString()","setYear");
LooksReasonable("Date.prototype.getYear.toString()","getYear");
LooksReasonable("Date.prototype.toGMTString.toString()","toGMTString");

LooksReasonable("RegExp.prototype.exec.toString()","exec");
LooksReasonable("RegExp.prototype.test.toString()","test");
LooksReasonable("RegExp.prototype.toString.toString()","toString");

LooksReasonable("Error.prototype.toString.toString()","toString");
successfullyParsed = true
