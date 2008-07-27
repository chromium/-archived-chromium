<?php
// This is a compat shim to make our php-cgi act more like apache mod_php.
// http://www.qijoo.com/fapm/PHP/en/function.getallheaders.html
// Well, sort of, lighttpd gives us headers like HTTP_UPPERCASE_WEE, and so
// we do some ugly php to make that Uppercase-Wee...
function getallheaders() {
  foreach($_SERVER as $name => $value) {
    if(substr($name, 0, 5) == 'HTTP_') {
      $name = strtolower(substr($name, 5));
      $name = join("-", array_map('ucwords', explode("_", $name)));
      $headers[$name] = $value;
    }
  }
  return $headers;
}
?>
