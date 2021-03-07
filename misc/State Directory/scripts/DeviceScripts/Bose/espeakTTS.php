<?php
function sliceString($string, $maxCharacters)
{
        $i = 0;
        while (strlen($string) > $maxCharacters)
        {
                $temp = substr($string, 0, $maxCharacters);
                $pos = strrpos($temp, "+");
                $strings[$i] = substr($string, 0, $pos);
                $string = substr($string, $pos);
                $i++;
        }
        $strings[$i] = $string;
        return $strings;
}

if($argc != 4) die("Wrong parameter count. Please provide the language as first, the voice as second and the string to say as third parameter. E. g.: espeakTTS.php de \"\" \"Hello World\"");

$language = $argv[1];
$path = "/var/lib/homegear/tmp/Bose/";
$speed_delta = 0;
$pitch_delta = 0;
$volume_delta = 0;
$speed = (int)(175 + 175 * $speed_delta / 100);
$pitch = (int)(50 + $pitch_delta / 2);
$volume = (int)(100 + $volume_delta);

if(!file_exists($path))
{
        if(!mkdir($path, 0777, true)) die("Could not create directory $path");
}

$words = escapeshellarg($argv[3]);
$filename = $path.md5($words)."-".$language.".mp3";
if(!file_exists($filename))
{
        $cmd = "espeak -s $speed -p $pitch -a $volume --stdout $words | lame --preset voice -q 9 --vbr-new - $filename";
        exec($cmd);
}

echo $filename;
?>

