<?php

if($argc != 4) die("Wrong parameter count. Please provice the language as first, the voice as second and the string to say as thrid parameter. E. g.: IvonaTTS.php de-DE Marlene \"Hello World\"");

require(__DIR__.'/vendor/autoload.php');
use Aws\Polly\PollyClient;

$language = $argv[1];
if($language == 'de') $language = 'de-DE';
else if($language == 'fr') $language = 'fr-FR';
else if($language == 'en') $language = 'en-US';
$voice = $argv[2];
$words = $argv[3];
$path = \Homegear\Homegear::TEMP_PATH."Bose/";

if($language == 'de-DE' && $voice != 'Vicki' && $voice != 'Marlene' && $voice != 'Hans') $voice = 'Vicki';
else if($language == 'en-US' && $voice != 'Justin' && $voice != 'Salli' && $voice != 'Joey' && $voice != 'Kimberly' && $voice != 'Kendra' && $voice != 'Ivy' && $voice != 'Matthew' && $voice != 'Joanna') $voice = 'Justin';
else if($language == 'en-GB' && $voice != 'Amy' && $voice != 'Brian' && $voice != 'Emma') $voice = 'Amy';
else if($language == 'fr-FR' && $voice != 'Celine' && $voice != 'Mathieu' && $voice != 'Lea') $voice = 'Celine';

if(!file_exists($path))
{
        if(!mkdir($path, 0775, true)) die("Could not create directory $path");
}

$hg = new \Homegear\Homegear();

$speech = [
  'Text' => '',
  'OutputFormat' => 'mp3',
  'TextType' => 'text',
  'VoiceId' => ''
];

$config = [
  'version' => 'latest',
  'region' => 'eu-central-1',
  'credentials' => [
    'key' => '',
    'secret' => '',
  ]
];

$speech['Text'] = $words;
$speech['VoiceId'] = $voice;

$config['credentials']['key'] = $hg->getFamilySetting(6, 'ttsusername');
$config['credentials']['secret'] = $hg->getFamilySetting(6, 'ttskey');

// get service handle
try {$client = new PollyClient($config);}
catch(Exception $e) {print_r($e); exit;}

$filename = $path.md5($words)."-".$language."-".$voice.".mp3";

if(file_exists($filename) && filesize($filename) > 1024) touch($filename);
else
{
    $response = $client->synthesizeSpeech($speech);
    file_put_contents($filename, $response['AudioStream']);
}

echo $filename;

//print_r($response);

exit(0);
