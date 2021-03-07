<?php
/****************/
/* By Michael Scheffler, inspired by Tim Greve */
/****************/
$validVoice['Marlene'] = 'bits1-hsmm';
$validVoice['Hans'] = 'bits3-hsmm';
$validLanguage['de-DE'] = 'de';
$validLanguage['de_DE'] = 'de';
$validLanguage['en-US'] = 'de';
$validLanguage['en_US'] = 'de';
$validLanguage['en-GB'] = 'de';
$validLanguage['en_GB'] = 'de';
$validLanguage['fr-FR'] = 'de';
$validLanguage['fr_FR'] = 'de';
$validLanguage['es-ES'] = 'de';
$validLanguage['es_ES'] = 'de';

if($argc < 4) die("Wrong parameter count. Please provide the language as first, the voice as second and the string to say as third parameter. E. g.: EasyTTS.php de bits1-hsmm \"Hello World\"");

$language = $validLanguage[$argv[1]];
$voice = $validVoice[ucfirst($argv[2])];
$path = \Homegear\Homegear::TEMP_PATH."Bose/";
if ($language == '') { $language = 'de'; }
if($language == 'de' && $voice != 'bits1-hsmm' && $voice != 'bits3-hsmm') { $voice = 'bits1-hsmm'; }

if(!file_exists($path))
{
	if(!mkdir($path, 0775, true)) die("Could not create directory $path");
}

EASY_TTS::$cmID = \Homegear\Homegear::getFamilySetting(6, 'ttskey');
EASY_TTS::$cmUsername = \Homegear\Homegear::getFamilySetting(6, 'ttsusername');
if(!EASY_TTS::$cmID || !EASY_TTS::$cmUsername) die("Please specify \"TtsKey\" and \"TtsUsername\" in \"Bose.conf\".");

$easyTTS = new EASY_TTS();

$words = $argv[3]; //escapeshellarg($argv[2]);
$filename = $path.md5($words)."-".$language."-".$voice.".wav";
if(file_exists($filename) && filesize($filename) > 1024) touch($filename);
else $easyTTS->save_mp3($words, $filename, $language, $voice);

echo $filename;

exit(0);


///////////////////////////////////////////////////////////////////////////////////////
//   EASY_TTS             /////////////////////////////////////////////////////////////
//    by mscheffler, 2017/03/17, original IVONA_TTS by Titus 15.10.2015  //////////////
///////////////////////////////////////////////////////////////////////////////////////

class EASY_TTS
{
  static $cmID = '';
  static $cmUsername = '';
    function save_mp3($text, $filename, $language="de", $voice="bits1-hsmm", $rate="default", $volume="default") {
         $payload['Input'] = array();
            $payload['Input']['Data'] = urlencode($text); //utf8_encode($text);//{"Input":{"Data":"Hello world"}}'
            $payload['Voice']['Name'] = $voice;
            $payload['Voice']['Language'] = $language;

            #$payload = json_encode($payload);
            $mp3 = $this->get_mp3($payload);
            file_put_contents($filename, $mp3);
     }

    function get_mp3( $payload )
    {
        /*
         * Execute Crafted Request
         */
        $url    = "https://www.meine-homematic.de/tts.php?id=".self::$cmID."&username=".self::$cmUsername."&engine=mary&locale=".$payload['Voice']['Language']."&voice=".$payload['Voice']['Name']."&text=".$payload['Input']['Data'];
        $ch     = curl_init();
        $curl_headers = array();
	if(file_exists("/usr/local/etc/config/addons/www/homegear/data/www/rpc/hgui/ca-bundle.pem"))
	{
		curl_setopt($ch, CURLOPT_CAINFO, "/usr/local/etc/config/addons/www/homegear/data/www/rpc/hgui/ca-bundle.pem");
	}
        curl_setopt($ch, CURLOPT_URL,$url);
        curl_setopt($ch, CURLOPT_HTTPHEADER, $curl_headers);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER,1);
        curl_setopt($ch, CURLOPT_TCP_NODELAY, true);
        curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, 2 );
        curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, true );
        // debug opts
        {
            curl_setopt($ch, CURLOPT_VERBOSE, true);
            $verbose = fopen('php://temp', 'rw+');
            curl_setopt($ch, CURLOPT_STDERR, $verbose);
            $result = curl_exec($ch); // raw result
            rewind($verbose);
            $verboseLog = stream_get_contents($verbose);
            #echo "Verbose information:\n<pre>", htmlspecialchars($verboseLog), "</pre>\n";
        }
        return $result;
    }
}
