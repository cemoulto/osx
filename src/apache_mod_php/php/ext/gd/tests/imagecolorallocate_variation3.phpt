--TEST--
Test imagecolorallocate() function : usage variations  - passing different data types to third argument
--SKIPIF--
<?php
if(!extension_loaded('gd')) {
    die('skip gd extension is not loaded');
}
if(!function_exists('imagecreatetruecolor')) {
    die('skip imagecreatetruecolor function is not available');
}
?> 
--FILE--
<?php
/* Prototype  : imagecolorallocate(resource im, int red, int green, int blue)
 * Description: Allocate a color for an image
 * Source code: ext/gd/gd.c
 */

echo "*** Testing imagecolorallocate() : usage variations ***\n";

$im = imagecreatetruecolor(200, 200);
$red = 10;
$blue = 10;

$fp = tmpfile();

//get an unset variable
$unset_var = 10;
unset ($unset_var);
// define some classes
class classWithToString
{
        public function __toString() {
                return "Class A object";
        }
}


class classWithoutToString
{
}

// heredoc string
$heredoc = <<<EOT
hello world
EOT;

// add arrays
$index_array = array (1, 2, 3);
$assoc_array = array ('one' => 1, 'two' => 2);

//array of values to iterate over
$values = array(

      // float data
      'float 10.5' => 10.5,
      'float -10.5' => -10.5,
      'float 10.1234567e5' => 10.1234567e5,
      'float 10.7654321E-5' => 10.7654321E-5,
      'float .5' => .5,

      // array data
      'empty array' => array(),
      'int indexed array' => $index_array,
      'associative array' => $assoc_array,
	  'nested arrays' => array('foo', $index_array, $assoc_array),
      
      // null data
	  'uppercase NULL' => NULL,
      'lowercase null' => null,

      // boolean data
      'lowercase true' => true,
      'lowercase false' =>false,
      'uppercase TRUE' =>TRUE,
      'uppercase FALSE' =>FALSE,

      // empty data
      'empty string DQ' => "",
      'empty string SQ' => '',

      // string data
      'string DQ' => "string",
      'string SQ' => 'string',
      'mixed case string' => "sTrInG",
      'heredoc' => $heredoc,

      // object data
      'instance of classWithToString' => new classWithToString(),
      'instance of classWithoutToString' => new classWithoutToString(),

      // undefined data
      'undefined var' => @$undefined_var,

      // unset data
      'unset var' => @$unset_var,
      
      //resource 
      "file resource" => $fp
);
// loop through each element of the array for red
foreach($values as $key => $value) {
      echo "\n--$key--\n";
      var_dump( imagecolorallocate($im, $red, $value, $blue) );
};
?>
===DONE===
--EXPECTF--
*** Testing imagecolorallocate() : usage variations ***

--float 10.5--
int(657930)

--float -10.5--
int(652810)

--float 10.1234567e5--
int(259815690)

--float 10.7654321E-5--
int(655370)

--float .5--
int(655370)

--empty array--

Warning: imagecolorallocate() expects parameter 3 to be long, array given in %s on line %d
NULL

--int indexed array--

Warning: imagecolorallocate() expects parameter 3 to be long, array given in %s on line %d
NULL

--associative array--

Warning: imagecolorallocate() expects parameter 3 to be long, array given in %s on line %d
NULL

--nested arrays--

Warning: imagecolorallocate() expects parameter 3 to be long, array given in %s on line %d
NULL

--uppercase NULL--
int(655370)

--lowercase null--
int(655370)

--lowercase true--
int(655626)

--lowercase false--
int(655370)

--uppercase TRUE--
int(655626)

--uppercase FALSE--
int(655370)

--empty string DQ--

Warning: imagecolorallocate() expects parameter 3 to be long, string given in %s on line %d
NULL

--empty string SQ--

Warning: imagecolorallocate() expects parameter 3 to be long, string given in %s on line %d
NULL

--string DQ--

Warning: imagecolorallocate() expects parameter 3 to be long, string given in %s on line %d
NULL

--string SQ--

Warning: imagecolorallocate() expects parameter 3 to be long, string given in %s on line %d
NULL

--mixed case string--

Warning: imagecolorallocate() expects parameter 3 to be long, string given in %s on line %d
NULL

--heredoc--

Warning: imagecolorallocate() expects parameter 3 to be long, string given in %s on line %d
NULL

--instance of classWithToString--

Warning: imagecolorallocate() expects parameter 3 to be long, object given in %s on line %d
NULL

--instance of classWithoutToString--

Warning: imagecolorallocate() expects parameter 3 to be long, object given in %s on line %d
NULL

--undefined var--
int(655370)

--unset var--
int(655370)

--file resource--

Warning: imagecolorallocate() expects parameter 3 to be long, resource given in %s on line %d
NULL
===DONE===