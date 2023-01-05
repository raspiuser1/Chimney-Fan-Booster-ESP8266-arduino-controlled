/*********
automatisch regelen van radiator ventilators voor het boosten van de warmte in de ruimte
via een slider op de webserver kan de snelheid van de fan worden ingesteld


om te activeren of er warmte vraag is:
http://ip/get?fanon
voor geen warmte vraag
http://ip/get?fanoff


aanpassen van snelheid
http://ip/slider?value=100
of open het ip van de webserver en pas de slider aan
handmatig 0 tot 255 (of via openremote)

zomer modus, zorg ervoor dat de fans altijd kunnen draaien (override)
starttemp word ingesteld op 0 graden
http://ip/summeron

zomer modus uit en gaat dan pas aan op 27 graden
starttemperatuur word dan aangepast
http://ip/summeroff?value=27
standaard is 30 graden:
http://ip/summeroff

stel eindtemperatuur in op standaardwaarde dus 60 graden
 http://ip/eindtemp
 of eigen waarde invullen bijvoorbeeld 80 graden
 http://ip/eindtemp?value=80
 

voor het tweaken van de gelezen temperatuur (voor testdoeleinden) bijv 80 graden:
http://ip/tweak?value=80



*********/

// Import required libraries
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266HTTPClient.h>
//#include "DHTesp.h" // voor dht22

//voor dht21------------------------------------------
#include <DHT.h>
#define DHTTYPE DHT21 // DHT 21, AM2301
#define DHTPIN D3 
DHT dht(DHTPIN, DHTTYPE);

//pin van de temp sensor dht22
  

//DHTesp dht;  // voor dht22

//wit = vcc 3,3v
//rood = gnd
//geel = data

//wifi gegevens
char* ssid = "hulse";
char* password = "tc2012meta1";

//===============voor spanningsgestuurde fan================
//int fanPin = 5;
#define fanPin D1
long maxpwm = 1023; // is bij esp 10 bit dus max 1023, bij normale arduino is dit 255

long updatefreq = 5000;
//vertraging tussen iedere nieuwe lezing (iniden correct)
long intervalnu = 5000;
//vertraging (indien fout of disabled)
long delayerr = 10000;

//minimale temperatuur waarbij de ventilatoren aan gaan
int starttemp = 30;
//startup van de fan %
int startperc = 10;
int eindtemp = 40;//standaard is 60

//begin waarde slider (webserver)
String sliderValue = "100";

// ip instellingen
IPAddress local_IP(192, 168, 1, 154);
IPAddress gateway(192, 168, 1, 254);
//IPAddress subnet(255, 255, 255, 0);
//IPAddress primaryDNS(8, 8, 8, 8);   //optional
//IPAddress secondaryDNS(8, 8, 4, 4); //optional

//-------------emoncms info-----------------------------------
//server ip of dns
//const char host[] = "192.168.1.10";
const char host[] = "dekemp.synology.me";
//moet 80 zijn bij local 3333 voor dns
//#define poort 80 
#define poort 3333
//defineer subdirectory (indien nodig) "/emoncms3"
char basedir[] = "/emoncms";
//apikey 
char privateKey[] = "b190a228431eeb68408be26380dca449";
//naam van deze node/esp dit word weergegeven in emoncms logger
#define node5 "radiator-fan-kachel"

//==================================================================hieronder niets meer aanpassen=================================================================================================

//nog wat variabelen

const char* PARAM_INPUT = "value";
//zet fans automatisch uit bij opstarten van de esp
int statusfan = 0;
String json;
int out;
int tweak = 0; 
float t;
int summermode = 0 ;
int fanSpeedPercentstart = 0;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP Web Server</title>
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 2.3rem;}
    p {font-size: 1.9rem;}
    body {max-width: 400px; margin:0px auto; padding-bottom: 25px;}
    .slider { -webkit-appearance: none; margin: 14px; width: 360px; height: 25px; background: #FFD65C;
      outline: none; -webkit-transition: .2s; transition: opacity .2s;}
    .slider::-webkit-slider-thumb {-webkit-appearance: none; appearance: none; width: 35px; height: 35px; background: #003249; cursor: pointer;}
    .slider::-moz-range-thumb { width: 35px; height: 35px; background: #003249; cursor: pointer; } 
  </style>
</head>
<body>
  <h2>Kachel booster</h2>
  <p><span id="textSliderValue">%SLIDERVALUE%</span></p>
  <p><input type="range" onchange="updateSliderPWM(this)" id="pwmSlider" min="0" max="100" value="%SLIDERVALUE%" step="1" class="slider"></p>
  <h1>Snelheid ventilatoren %</h1>
<script>
function updateSliderPWM(element) {
  var sliderValue = document.getElementById("pwmSlider").value;
  document.getElementById("textSliderValue").innerHTML = sliderValue;
  console.log(sliderValue);
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/slider?value="+sliderValue, true);
  xhr.send();
}
</script>
</body>
</html>
)rawliteral";

// Replaces placeholder with button section in your web page
String processor(const String& var){
  //Serial.println(var);
  if (var == "SLIDERVALUE"){
    return sliderValue;
  }
  return String();
}

unsigned long TimeStamp=0;              // Type needs to match millis() function
volatile unsigned int  NumberOfPulses;  // Used in interrupt function so make volatile
int LastReading;
int rpmnu;

void ICACHE_RAM_ATTR PulseCount();

void setup(){
  //============================================voor rpm te lezen===================
  // Set up interrupt pin with pull-up so that it reliably detects FALLING

  //dht.setup(DHTPIN, DHTesp::DHT22);//voor dht22
  dht.begin();
  // Set an interrupt to trigger with a falling edge

  statusfan = 1;
  // Serial port for debugging purposes
  Serial.begin(115200);

  //voor spanningsgestuurde fan:
  // analogWrite(fanPin, 0); // set the fan speed
  //analogWriteRange(100); // to have a range 1 - 100 for the fan
  //analogWriteFreq(10000); 
  
  
  //=====================================================================================================
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/fanon", HTTP_GET, [](AsyncWebServerRequest *request){
    statusfan = 1;
    Serial.println("Programma actief");
    request->send(200, "text/plain", "OK");      
  });
  server.on("/fanoff", HTTP_GET, [](AsyncWebServerRequest *request){
    statusfan = 0;
    Serial.println("Programma uitgeschakeld");
    request->send(200, "text/plain", "OK");   
  });
  //zomer modus voor koude lucht, temperatuur limiet word dan 0, met max snelheid
  server.on("/summeron", HTTP_GET, [](AsyncWebServerRequest *request){
    starttemp = 0;
    summermode = 1;
    controlFanSpeed (100);
    Serial.println("Zomer modus aan");
    request->send(200, "text/plain", "OK");   
  });
  server.on("/summeroff", HTTP_GET, [](AsyncWebServerRequest *request){
    summermode = 0;
    String inputMessage2;
    // GET input1 value on <ESP_IP>/summeroff?value=<inputMessage2>
    if (request->hasParam(PARAM_INPUT)) {
      inputMessage2 = request->getParam(PARAM_INPUT)->value();
      starttemp = inputMessage2.toInt();  
    } else { //als we geen inputwaarde vinden stel dan 30 graden in
      starttemp = 30;
    }
    Serial.println("Start Temperatuur ingesteld op: " + String(starttemp));
    request->send(200, "text/plain", "OK");
  });
    server.on("/eindtemp", HTTP_GET, [](AsyncWebServerRequest *request){
    String inputMessage5;
    // GET input1 value on <ESP_IP>/summeroff?value=<inputMessage2>
    if (request->hasParam(PARAM_INPUT)) {
      inputMessage5 = request->getParam(PARAM_INPUT)->value();
      eindtemp = inputMessage5.toInt();  
    } else { //als we geen inputwaarde vinden stel dan 60 graden in
      eindtemp = 60;
    }
    Serial.println("Eind Temperatuur ingesteld op: " + String(starttemp));
    request->send(200, "text/plain", "OK");
  });
    server.on("/tweak", HTTP_GET, [](AsyncWebServerRequest *request){
    String inputMessage3;
    if (request->hasParam(PARAM_INPUT)) {
      inputMessage3 = request->getParam(PARAM_INPUT)->value();
      tweak = inputMessage3.toInt();   
      Serial.println("Tweak temperatuur ingesteld op : " + String(tweak));   
    } else {
        tweak = 0;
        Serial.println("Tweaken van temperatuur uitgeschakeld");
    }  
    request->send(200, "text/plain", "OK");
  });
  // Send a GET request to <ESP_IP>/slider?value=<inputMessage>
  server.on("/slider", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    // GET input1 value on <ESP_IP>/slider?value=<inputMessage>
    if (request->hasParam(PARAM_INPUT)) {
      inputMessage = request->getParam(PARAM_INPUT)->value();
      sliderValue = inputMessage;
    }
    else {
      inputMessage = "No message sent";
    }
    Serial.println("Slider aangepast: " + String(inputMessage) + "%");
    request->send(200, "text/plain", "OK");
  });
  // Start server
  server.begin();
  //hieronder niets meer zetten
}


void controlFanSpeed (int fanSpeedPercent) {
  // bij welke waarde gaat de fan draaien
  int minout = 320;
  //tijdelijke boost
  int minspeed = 600;
  out = map(fanSpeedPercent,10,100,minout,maxpwm);
  if (out > maxpwm){
    out = maxpwm;
    fanSpeedPercent = 100;
    Serial.print("Max ");
  }
  // zorg voor een startup bij lage toeren (tijdelijk boosten voor 3 seconden als fans uit zijn)
  if (fanSpeedPercentstart == 0 && out >= minout && out < minspeed){
    fanSpeedPercentstart = fanSpeedPercent;
    analogWrite(fanPin,minspeed);
    Serial.print("Boosten Fan 3sec, ");
    delay(3000);
    } 
  if (fanSpeedPercent == 0){   
    fanSpeedPercentstart = 0;// als 0% is moeten we gaan boosten
    out =0;
    Serial.print("Uitgeschakeld: ");
    }else{
      fanSpeedPercentstart = fanSpeedPercent;
    }
  analogWrite(fanPin,out);
  Serial.print("Fan Speed: ");
  Serial.print(fanSpeedPercent);
  Serial.print("%  ");  
  Serial.println(out); 
}

// Interrupt function for speed measurement
void ICACHE_RAM_ATTR PulseCount()
{
  NumberOfPulses++; // Just increment count
  return;
}

  
void loop() {
    WiFiClient client;
    // lees de temperatuur en vochtigheid
    //float h = dht.getHumidity();// dht22
    float h = dht.readHumidity();// dht21
    if (tweak > 0 ){// als er handmatig een temperatuur ingesteld word voor te testen lees dan niet de sensor uit
       t = tweak;
    } else {
       //t = dht.getTemperature();/dht22
       t = dht.readTemperature();
    }
    
    if (isnan(h) || isnan(t)) {
      Serial.println("Kan de temperatuur/vochtigheid niet uitlezen");
      delay(delayerr);
      return; // ga terug naar het begin van de loop als niets gevonden
    }
    //if (t < starttemp){
    //  Serial.println("De doel temperatuur van " + String(starttemp) + " is nog niet gehaald, Temp: " + String(t) + " Hum: " + String(h));
    //  delay(delayerr);
    //  return;
    //}

    
    //mapping
    int fanSpeedPercent1 = map(t, starttemp, eindtemp, startperc, 100);//temperatuur 20 tot 60 graden = 10 tot 100% > bijvoorbeeld 40 graden = 55%
    //int slidervaluemap = map(sliderValue.toInt(), 0, 255, 1, 100);// 0-255 waarde van de slider = 1 to 100% > voorbeeld 20%
    int slidervaluemap = sliderValue.toInt();//directe waarden van 0 to 100% > voorbeeld 20%
    int fanSpeedPercent = map(slidervaluemap, 1, 100, 10, fanSpeedPercent1);//hier nemen we dan 20% van de 45% fan-speed


    //als fanspeed onder nul? zet dan op 0, meestal als doeltemperatuur nog niet behaald is
    if (fanSpeedPercent < 0 ){
      fanSpeedPercent = 0;
      delay(delayerr);
    }
    
    if (client.connect(host, poort)) { //ga verder als er een connectie is naar de emoncms server
        //loggen op emoncms
       json =  "{temp:" + String(t) + ",hum:" + String(h) + ",speed:" + String(fanSpeedPercent) + ",slider:" + String(slidervaluemap) + ",active:" + String(statusfan) + ",tstart:" + String(starttemp) + ",teind:" + String(eindtemp) +"}"; 
       //verzenden json string naar emoncms server
        String url = "/emoncms/input/post.json?node=";
        url += node5;
        url += "&apikey=";
        url += privateKey;
        url += "&json=";
        url += json;
        Serial.println(" ");
        Serial.println("Emoncms data:");
        Serial.println(json);
        Serial.println(" ");    
         // This will send the request to the server
        client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                  "Host: " + host + "\r\n" +
                  "Connection: close\r\n\r\n");
    }
    else {
        Serial.println("Kan data niet loggen op emoncms server");
    }

 
    
  // als er warmtevraag is ga dan door   

  if (statusfan == 1) {
    if (summermode == 1){// als zomer module aan stop dan
        Serial.println("Zomer modus is actief, fans op 100%");
        delay(delayerr);
        return;
    }
    // als meer dan 10% fan speed dan ga door
    if (fanSpeedPercent1 >= startperc) { 
          //voor spanningsgestuurde fan:    
          controlFanSpeed (fanSpeedPercent); // Update fan speed    
          delay(intervalnu);
    } 
    else {
     if (t < starttemp){
        Serial.println("Starttemperatuur ligt hoger, Ventilator is uit");
        controlFanSpeed (0); 
        delay(delayerr);
     } else {    
     Serial.println("Ventilator snelheid word niet aangepast, Starttemperatuur " +  String(starttemp) + " graden"); 
     if (out == maxpwm){
      controlFanSpeed (0);
     }
    }
     Serial.println("");
     Serial.println("---=======INFO=======---");
     Serial.println("Temperatuur: " +  String(t) + "°C"); 
     Serial.println("Luchtvochtigheid: " +  String(h) + "%"); 
     Serial.println("Starttemperatuur: " +  String(starttemp) ); 
     delay(delayerr);
    } 
  }  
 else {
    Serial.println("Er is geen warmtevraag, Ventilators zijn uitgeschakeld");
    controlFanSpeed (0);
    delay(delayerr);
  } 
}
