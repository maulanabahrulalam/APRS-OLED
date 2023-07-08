#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Wire.h>
#include "RemoteDebug.h"        //https://github.com/JoaoLopesF/RemoteDebug
#define SEALEVELPRESSURE_HPA (1013.25)
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
static const int RXPin = 14, TXPin = 12;
static const uint32_t GPSBaud = 9600;
TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);
String aprs_lat="";
String aprs_long="";
RemoteDebug Debug;
#include "SH1106Wire.h"   // legacy: #include "SH1106.h"
SH1106Wire display(0x3c, SDA, SCL);     // ADDRESS, SDA, SCL
int counter = 1;
#define DEMO_DURATION 3000
#define MAX_SRV_CLIENTS 1
#define MAX_TIME_INACTIVE 60000000
Adafruit_BME280 bmp; // I2C
const char* ssid     = "YD2AXX"; //write your wifi connection name
const char* password = "12345679"; // write your wifi password
float tempC, tempF, hum, pres, alt = -99;
String TitlePage = "PP5ERE"; //write your html title
String msg = "";
int led = D3;
int indi = D4;
unsigned long ElapsedTime = 0;

const String USER    = "YD2AXX"; //write your aprs callsign
const String PAS     = "yd2axxaprs"; // write your aprs password
//const String LAT     = "0659.17S"; //write your latitude
//const String LON     = "11029.97E"; //write your longitute
const String COMMENT = "Literasi Ngasal on BDG"; //write some comment
const String SERVER  = "cwop.aprs.net"; // write the address of the aprs server
//const String SERVER  = "indiana.aprs2.net";
const int    PORT    = 14580; //write the aprs server port
long timeSinceLastModeSwitch = 0;
ESP8266WebServer server(80);
String v_signal = "";
String v_user = "YD2AXX";
String v_temp = "";
String v_hum = "";
String v_lat = "";
String v_long = "";
String v_state = "No";
void(* resetFunc) (void) = 0; //restart

void drawProgressBarDemo() {
  int progress = (counter / 5) % 100;
  // draw the progress bar
  display.drawProgressBar(0, 32, 120, 10, progress);

  // draw the percentage as String
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 15, String(progress) + "%");
}
//The setup function is called once at startup of the sketch
void setup()
{
  Serial.begin(115200);
  pinMode(led,OUTPUT);
  pinMode(indi,OUTPUT);
  digitalWrite(led,LOW);
  digitalWrite(indi,HIGH);
    ss.begin(GPSBaud);
    Serial.println("Start setup()"); 
      display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10); 
  
    startConnection();
    startBMP();
    delay(10);
    Debug.begin("PP5ERE"); // Initiaze the telnet server
    Debug.setResetCmdEnabled(true); // Enable the reset command
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED){
    Debug.println("Start WiFi.status() = !WL_CONNECTED");  
    startConnection();
  }
  server.handleClient();
  Debug.handle();
  getDataFromBMP();
  gpsRead();
  display.clear();
  oledView();
  display.display();
  sendAPRSPacketEvery(60000); //run every 1 minutes
  yield();
}

void startConnection(){
  int count = 0;
  Debug.println("Try to connect(). Tentativa: "+String(count));
  WiFi.mode(WIFI_STA);  
  WiFi.begin(ssid, password);
  Debug.println("Run: WiFi.begin(ssid, password);");  
  while(WiFi.status() != WL_CONNECTED && count < 100) {
    Serial.println("Run: while(WiFi.status() != WL_CONNECTED && count < 100)");
    delay(500);
    Serial.println("WiFi.status() = "+ String(WiFi.status())+"\nTry = "+String(count)+"\n");
    Debug.println("Try Connecting... ");
    if (WiFi.status() == WL_NO_SSID_AVAIL){
      Serial.println("WiFi.status() == WL_NO_SSID_AVAIL"+ String(WiFi.status())+ "\nReset\n");
      delay(5000);
      resetFunc();
    }
    count++;
  }
  if (count == 100 && WiFi.status() != WL_CONNECTED){
    Debug.println("try == 100 && WiFi.status() != WL_CONNECTED\nReset...");
      delay(5000);
    resetFunc();
  }
  Serial.println("Connected!");
  IPAddress ip(192,168,43,41);
  Serial.println("Set IP = 192.168.43.41");
  IPAddress gate(192,168,43,203);//192.168.127.228
  Serial.println("Set Gateway = 192.168.43.203");
  IPAddress sub(255,255,255,0);
  Debug.println("Set mask = 255.255.255.0");
  WiFi.config(ip,gate,sub);//,dns1,dns2);
  Serial.println("Config the values: Wifi.config(ip,gate,sub)");
  server.on("/weather", HTTP_GET, getJson);
  Debug.println("Set server.on(/weather)");
  server.on("/", HTTP_GET, getPage);
  Debug.println("Set server.on(/)");
  Debug.println("Set server.on(log)");
  server.onNotFound(onNotFound);
  Debug.println("Set server.onNoFound");
  if ( MDNS.begin ( "esp8266" ) ) {
    Debug.println ( "MDNS started" );
  }
  server.begin();
  Serial.println("Run server.begin()");
  
}

void sendAPRSPacketEvery(unsigned long t){
  unsigned long currentTime;
  currentTime = millis();
  if (currentTime < ElapsedTime){
    Serial.println("Run: (currentTime = millis()) < ElapsedTime.\ncurrentTime ="+String(currentTime)+"\nElapsedTime="+String(ElapsedTime));
    ElapsedTime = 0;    
    Serial.println("Set ElapsedTime=0");
  }
  if ((currentTime - ElapsedTime) >= t){
    Serial.println("Tried : (currentTime - ElapsedTime) >= t.\ncurrentTime ="+String(currentTime)+"\nElapsedTime="+String(ElapsedTime));
    clientConnectTelNet();
    ElapsedTime = currentTime;  
    Serial.println("Set ElapsedTime = currentTime");
  }
}

void clientConnectTelNet(){
  WiFiClient client;
  int count = 0;
  String packet, aprsauth, tempStr, humStr, presStr;
  Serial.println("Run clientConnectTelNet()");
  getDataFromBMP();
  const String LAT=aprs_lat.c_str();
  const String LON=aprs_long.c_str();
  while (!client.connect(SERVER.c_str(), PORT) && count <20){
    digitalWrite(indi,LOW);
    Serial.println("Didn't connect with server: "+String(SERVER)+" Port: "+String(PORT));
    delay (1000);
    client.stop();
    client.flush();
    Serial.println("Run client.stop");
    Serial.println("Trying to connect with server: "+String(SERVER)+" Port: "+String(PORT));
    digitalWrite(indi,HIGH);
    count++;
    Serial.println("Try: "+String(count));
    v_state = "No";
  }
  if (count == 20){
    Serial.println("Tried: "+String(count)+" don't send the packet!");
    v_state = "No";
  }else{
    v_state = "Yes";
    digitalWrite(led,HIGH);
    Serial.println("Connected with server: "+String(SERVER)+" Port: "+String(PORT));
    tempStr = getTemp(tempF);
    humStr = getHum(hum);
    presStr = getPres(pres);
    Serial.println("Leu tempStr="+tempStr+" humStr="+humStr+" presStr="+presStr);
    while (client.connected()){ //there is some problem with the original code from WiFiClient.cpp on procedure uint8_t WiFiClient::connected()
      // it don't check if the connection was close, so you need to locate and change the line below:
      //if (!_client ) to: 
      //if (!_client || _client->state() == CLOSED)
      delay(1000);
      Serial.println("Run client.connected()");
      if(tempStr != "-999" || presStr != "-99999" || humStr != "-99"){
          aprsauth = "user " + USER + " pass " + PAS + "\n";
          client.write(aprsauth.c_str());
          delay(500);
          Serial.println("Send client.write="+aprsauth);
                  
          packet = USER + ">APRMCU,TCPIP*,qAC,T2BRAZIL:=" + LAT + "/" + LON +
               "_.../...g...t" + tempStr +
               "r...p...P...h" + humStr +
               "b" + presStr + COMMENT + "\n";
        
          client.write(packet.c_str());
          delay(500);
          Serial.println("Send client.write="+packet);
       
          client.stop();
          client.flush();
          Serial.println("Telnet client disconnect ");
      }
    }
  }
  
}

void getDataFromBMP(){
  int count = 0;
   tempC = bmp.readTemperature();
    //(0 °C × 9/5) + 32 = 32 °F
    tempF = (tempC*9/5)+32;
    hum = bmp.readHumidity();
    pres = bmp.readPressure();
    alt = bmp.readAltitude(SEALEVELPRESSURE_HPA);  Debug.println("Read tempC="+String(tempC)+" tempF="+String(tempF)+" hum="+String(hum)+" pres="+String(pres)+" alt="+String(alt));
  while ((isnan(tempC) || isnan(tempF) || isnan(pres) || isnan(hum) || isnan(alt) || hum == 0) && count < 1000){
    Serial.println("Read (isnan(tempC) || isnan(tempF) || isnan(pres) || isnan(hum) || isnan(alt) || hum == 0) && count < 1000");
    tempC = bmp.readTemperature();
    tempF = (tempC*9/5)+32;
    hum = bmp.readHumidity();
    pres = bmp.readPressure();
    alt = bmp.readAltitude(SEALEVELPRESSURE_HPA);
    Serial.println("Trying read again tempC="+String(tempC)+" tempF="+String(tempF)+" hum="+String(hum)+" pres="+String(pres)+" alt="+String(alt)+" count="+String(count));
    
    delay(2);
    count++;
  }
  v_temp=String(tempC);
  v_hum=String(hum);
  
}
void getJson(){
  getDataFromBMP();
  
  String json = "{\"TempC\":" + String(tempC) + 
                ",\"TempF\":" + String(tempF) + 
                ",\"Hum\":" + String(hum) + 
                ",\"Pres\":" + String(pres/100) + 
                ",\"Alt\":" + String(alt) + 
                "}";
  Debug.println("Create json="+json);
  server.send (200, "application/json", json);
  Debug.println("Run server.send (200, \"application/json\", json);");
}

void getPage(){
  String html = "";
  html += "<!doctype html> \n";
  html += "<html>\n";
  html += "<head>\n";
  html += "    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n";
  html += "    <title>"+TitlePage+"</title>\n";
  html += "   <!--<link rel=\"stylesheet\" href=\"fonts.css\">-->\n";
  html += "    <script src=\"https://canvas-gauges.com/download/latest/all/gauge.min.js\"></script>\n";
  html += "    <script src=\"https://code.jquery.com/jquery-3.3.1.min.js\"></script>\n";
  html += "</head>\n";
  html += "<body style=\"background: #222\" onload=animateGauges()>\n";
  html += "<canvas id=\"tempC\"\n";
  html += "    data-type=\"radial-gauge\"\n";
  html += "        data-width=\"300\"\n";
  html += "        data-height=\"300\"\n";
  html += "        data-units=\"°C\"\n";
  html += "        data-title=\"Temperature\"\n";
  html += "        data-value=\"0\"\n";
  html += "        data-min-value=\"-50\"\n";
  html += "        data-max-value=\"50\"\n";
  html += "        data-major-ticks=\"[-50,-40,-30,-20,-10,0,10,20,30,40,50]\"\n";
  html += "        data-minor-ticks=\"2\"\n";
  html += "        data-stroke-ticks=\"true\"\n";
  html += "        data-highlights=\'[\n";
  html += "                    {\"from\": -50, \"to\": 0, \"color\": \"rgba(0,0, 255, .3)\"},\n";
  html += "                    {\"from\": 0, \"to\": 50, \"color\": \"rgba(255, 0, 0, .3)\"}\n";
  html += "                ]\'\n";
  html += "        data-ticks-angle=\"225\"\n";
  html += "        data-start-angle=\"67.5\"\n";
  html += "        data-color-major-ticks=\"#ddd\"\n";
  html += "        data-color-minor-ticks=\"#ddd\"\n";
  html += "        data-color-title=\"#eee\"\n";
  html += "        data-color-units=\"#ccc\"\n";
  html += "        data-color-numbers=\"#eee\"\n";
  html += "        data-color-plate=\"#222\"\n";
  html += "        data-border-shadow-width=\"0\"\n";
  html += "        data-borders=\"true\"\n";
  html += "        data-needle-type=\"arrow\"\n";
  html += "        data-needle-width=\"2\"\n";
  html += "        data-needle-circle-size=\"7\"\n";
  html += "        data-needle-circle-outer=\"true\"\n";
  html += "        data-needle-circle-inner=\"false\"\n";
  html += "        data-animated-value=\"true\"\n";
  html += "        data-animation-duration=\"1500\"\n";
  html += "        data-animation-rule=\"linear\"\n";
  html += "        data-color-border-outer=\"#333\"\n";
  html += "        data-color-border-outer-end=\"#111\"\n";
  html += "        data-color-border-middle=\"#222\"\n";
  html += "        data-color-border-middle-end=\"#111\"\n";
  html += "        data-color-border-inner=\"#111\"\n";
  html += "        data-color-border-inner-end=\"#333\"\n";
  html += "        data-color-needle-shadow-down=\"#333\"\n";
  html += "        data-color-needle-circle-outer=\"#333\"\n";
  html += "        data-color-needle-circle-outer-end=\"#111\"\n";
  html += "        data-color-needle-circle-inner=\"#111\"\n";
  html += "        data-color-needle-circle-inner-end=\"#222\"\n";
  html += "        data-color-value-box-rect=\"#222\"\n";
  html += "        data-color-value-box-rect-end=\"#333\"\n";
  html += "        data-font-value=\"Led\"\n";
  html += "        data-font-numbers=\"Led\"\n";
  html += "        data-font-title=\"Led\"\n";
  html += "        data-font-units=\"Led\"\n";
  html += "></canvas>\n";
  html += "<canvas id=\"hum\"\n";
  html += "    data-type=\"radial-gauge\"\n";
  html += "        data-width=\"300\"\n";
  html += "        data-height=\"300\"\n";
  html += "        data-units=\"%\"\n";
  html += "        data-title=\"Humidity\"\n";
  html += "        data-value=\"10\"\n";
  html += "        data-min-value=\"10\"\n";
  html += "        data-max-value=\"100\"\n";
  html += "        data-major-ticks=\"10,20,30,40,50,60,70,80,90,100\"\n";
  html += "        data-minor-ticks=\"2\"\n";
  html += "        data-stroke-ticks=\"true\"\n";
  html += "        data-highlights=\'[{\"from\": 90, \"to\": 100, \"color\": \"rgba(200, 50, 50, .75)\"}]\'\n";
  html += "        data-color-plate=\"#222\"\n";
  html += "        data-color-major-ticks=\"#f5f5f5\"\n";
  html += "        data-color-minor-ticks=\"#ddd\"\n";
  html += "        data-color-title=\"#fff\"\n";
  html += "        data-color-units=\"#ccc\"\n";
  html += "        data-color-numbers=\"#eee\"\n";
  html += "        data-color-needle-start=\"rgba(240, 128, 128, 1)\"\n";
  html += "        data-color-needle-end=\"rgba(255, 160, 122, .9)\"\n";
  html += "        data-value-box=\"true\"\n";
  html += "        data-font-value=\"Repetition\"\n";
  html += "        data-font-numbers=\"Repetition\"\n";
  html += "        data-animated-value=\"true\"\n";
  html += "        data-animation-duration=\"1500\"\n";
  html += "        data-animation-rule=\"linear\"\n";
  html += "        data-borders=\"false\"\n";
  html += "        data-border-shadow-width=\"0\"\n";
  html += "        data-needle-type=\"arrow\"\n";
  html += "        data-needle-width=\"2\"\n";
  html += "        data-needle-circle-size=\"7\"\n";
  html += "        data-needle-circle-outer=\"true\"\n";
  html += "        data-needle-circle-inner=\"false\"\n";
  html += "        data-ticks-angle=\"250\"\n";
  html += "        data-start-angle=\"55\"\n";
  html += "        data-color-needle-shadow-down=\"#333\"\n";
  html += "        data-value-box-width=\"45\"\n";
  html += "></canvas>\n";
  html += "<canvas id=\"pres\"\n";
  html += "    data-type=\"radial-gauge\"\n";
  html += "        data-width=\"300\"\n";
  html += "        data-height=\"300\"\n";
  html += "        data-units=\"hPa\"\n";
  html += "        data-title=\"Pressure\"\n";
  html += "        data-value=\"960\"\n";
  html += "        data-min-value=\"960\"\n";
  html += "        data-max-value=\"1060\"\n";
  html += "        data-major-ticks=\"[960,970,980,990,1000,1010,1020,1030,1040,1050,1060]\"\n";
  html += "        data-minor-ticks=\"10\"\n";
  html += "        data-stroke-ticks=\"true\"\n";
  html += "        data-highlights=\'[\n";
  html += "        {\"from\": 960, \"to\": 990, \"color\": \"rgba(0, 0, 255, .3)\"},\n";
  html += "        {\"from\": 990, \"to\": 1030, \"color\": \"rgba(0, 255, 0, .3)\"},\n";
  html += "        {\"from\": 1030, \"to\": 1060, \"color\": \"rgba(255, 0, 0, .3)\"}\n";
  html += "                ]\'\n";
  html += "        data-ticks-angle=\"225\"\n";
  html += "        data-start-angle=\"67.5\"\n";
  html += "        data-color-major-ticks=\"#ddd\"\n";
  html += "        data-color-minor-ticks=\"#ddd\"\n";
  html += "        data-color-title=\"#eee\"\n";
  html += "        data-color-units=\"#ccc\"\n";
  html += "        data-color-numbers=\"#eee\"\n";
  html += "        data-color-plate=\"#222\"\n";
  html += "        data-border-shadow-width=\"0\"\n";
  html += "        data-borders=\"true\"\n";
  html += "        data-font-Numbers-Size=\"14\"\n";
  html += "        data-needle-type=\"arrow\"\n";
  html += "        data-needle-width=\"2\"\n";
  html += "        data-needle-circle-size=\"7\"\n";
  html += "        data-needle-circle-outer=\"true\"\n";
  html += "        data-needle-circle-inner=\"false\"\n";
  html += "        data-animated-value=\"true\"\n";
  html += "        data-animation-duration=\"1500\"\n";
  html += "        data-animation-rule=\"linear\"\n";
  html += "        data-color-border-outer=\"#333\"\n";
  html += "        data-color-border-outer-end=\"#111\"\n";
  html += "        data-color-border-middle=\"#222\"\n";
  html += "        data-color-border-middle-end=\"#111\"\n";
  html += "        data-color-border-inner=\"#111\"\n";
  html += "        data-color-border-inner-end=\"#333\"\n";
  html += "        data-color-needle-shadow-down=\"#333\"\n";
  html += "        data-color-needle-circle-outer=\"#333\"\n";
  html += "        data-color-needle-circle-outer-end=\"#111\"\n";
  html += "        data-color-needle-circle-inner=\"#111\"\n";
  html += "        data-color-needle-circle-inner-end=\"#222\"\n";
  html += "        data-color-value-box-rect=\"#222\"\n";
  html += "        data-color-value-box-rect-end=\"#333\"\n";
  html += "        data-font-value=\"Led\"\n";
  html += "        data-font-numbers=\"Led\"\n";
  html += "        data-font-title=\"Led\"\n";
  html += "        data-font-units=\"Led\"\n";
  html += "></canvas>\n";
  html += "<script>\n";
  html += "var timers = [];\n";
  html += "function animateGauges() {\n";
  html += "  var url = window.location.origin+\"/weather\";\n";
  html += "  document.getElementById(\"tempC\").setAttribute(\"data-value\", 0);\n";
  html += "    document.getElementById(\"hum\").setAttribute(\"data-value\", 10);\n";
  html += "    document.getElementById(\"pres\").setAttribute(\"data-value\", 960);\n";
  html += "    document.gauges.forEach(function(gauge) {\n";
  html += "        timers.push(setInterval(function() {\n";
  html += "           $.getJSON(url, function(data) {    \n";
  html += "                if (gauge.options.maxValue == 50){\n";
  html += "                    //document.getElementById(\"tempC\").setAttribute(\"data-value\",  data.TempC);\n";
  html += "                    gauge.value = data.TempC;\n";
  html += "               }\n";
  html += "               if (gauge.options.maxValue == 100){\n";
  html += "                    //document.getElementById(\"hum\").setAttribute(\"data-value\", data.Hum);        \n";
  html += "                    gauge.value = data.Hum;\n";
  html += "               }\n";
  html += "               if (gauge.options.maxValue == 1060){\n";
  html += "                   gauge.value = data.Pres;\n";
  html += "               }\n";
  html += "           });   \n"; 
              
  html += "        }, gauge.animation.duration + 50));\n";
  html += "    });\n";
  html += "}\n";
  html += "</script>\n";
  html += "</body>\n";
  html += "</html>\n";
  Debug.println("Write html page");

  server.send (200, "text/html", html);
  Serial.println("Run server.send (200, \"text/html\", html);");
}

void onNotFound()
{
  server.send(404, "text/plain", "Not Found service in /" );
  Serial.println("Run server.send(404, \"text/plain\", \"Not Found service in /\" );");
}

String getTemp(float pTemp){
  String strTemp;
  int intTemp;
  Debug.println("Run getTemp(float pTemp)");
  intTemp = (int)pTemp;
  strTemp = String(intTemp);
  //strTemp.replace(".", "");
  
  switch (strTemp.length()){
  case 1:
    return "00" + strTemp;
    break;
  case 2:
    return "0" + strTemp;
    break;
  case 3:
    return strTemp;
    break;
  default:
    return "-999";
  }

}

String getHum(float pHum){
  String strHum;
  int intHum;
  Serial.println("Run getHum(float pHum)");
  intHum = (int)pHum;
  strHum = String(intHum);
  
  switch (strHum.length()){
  case 1:
    return "0" + strHum;
    break;
  case 2:
    return strHum;
    break;
  case 3:
    if (intHum == 100){
       return "00";
    }else {
       return "-99";
    }
    break;
  default:
    return "-99";
  }
}

String getPres(float pPress){
  String strPress;
  int intPress = 0;
  intPress = (int)(pPress/10);
  strPress = String(intPress);
  Debug.println("Run getPres(float pPress)");
  switch (strPress.length()){
  case 1:
    return "0000" + strPress;
    break;
  case 2:
    return "000" + strPress;
    break;
  case 3:
    return "00" + strPress;
    break;
  case 4:
    return "0" + strPress;
    break;
  case 5:
    return strPress;
    break;
  default:
    return "-99999";
  }
}

void startBMP(){
  int fail = 0;
  bool isBegin = false;
  bool status;
  status = bmp.begin(0x76);  
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

}
void gpsRead()
{
  while (ss.available() > 0)
    if (gps.encode(ss.read()))
      displayInfo();
  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
    while(true);
  }
 delay(100);
 Serial.println("Read GPS");
}

void displayInfo()
{
  Serial.println(F("Location: ")); 
  if (gps.location.isValid())
  {
    v_signal = "Ok";
    String latitude = String(gps.location.lat(), 6);
    v_lat = latitude;
    int titik = latitude.indexOf('.');
    String derajat = latitude.substring(0,titik);
    int int_derajat = derajat.toInt();
    String S_N ="N";
    String drj = "";
    if(int_derajat<=0){
      int_derajat=int_derajat*(-1);
      S_N="S";
    }
    if(int_derajat<10){
      drj+="0";
    }
    drj+=String(int_derajat);
    String menit = latitude.substring(titik+1);
    int int_menit = menit.toInt();
    int_menit = int_menit*0.00006;
    String det = String(int_menit*0.00006,6);
    int indet = det.indexOf('.');
    det = det.substring(indet+1);
    int int_det= det.toInt();
    int_det = int_det*60;
    String mnt = "";
    if(int_menit<10){
      mnt+="0";
    }
    mnt+=String(int_menit);
    aprs_lat=drj+mnt+'.'+String(int_det).substring(0,2)+S_N;
   Serial.println("Latitude :"+aprs_lat);
    String longi = String(gps.location.lng(), 6);
    v_long = longi;
    int titi = longi.indexOf('.');
    String derlong = longi.substring(0,titi);
    int int_derlong = derlong.toInt();
    String W_E ="E";
    String drjlong = "";
    if(int_derlong<=0){
      int_derlong=int_derlong*(-1);
      W_E="W";
    }
    if(int_derlong<10){
      drjlong+="00";
    }
     if(int_derlong<100 && int_derlong>10){
      drjlong+="0";
    }
    drjlong+=String(int_derlong);
    String menitlong = longi.substring(titi+1);
    int int_menitlong = menitlong.toInt();
    int_menitlong = int_menitlong*0.00006;
    String detlong = String(int_menitlong*0.00006,6);
    int indetlong = detlong.indexOf('.');
    detlong = detlong.substring(indetlong+1);
    int int_detlong= detlong.toInt();
    int_detlong = int_detlong*60;
    String mntlong = "";
    if(int_menitlong<10){
      mntlong+="0";
    }
    mntlong+=String(int_menitlong);
    aprs_long=drjlong+mntlong+'.'+String(int_detlong).substring(0,2)+W_E;
    Serial.println("Longitudinal :"+aprs_long);
  }
  else
  {
    Serial.print(F("INVALID"));
    v_signal = "Error";
  }

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    v_signal = "Ok";
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
  v_signal = "Error";
  }

  Serial.print(F(" "));
  if (gps.time.isValid())
  {
    v_signal = "Ok";
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  }
  else
  {
    Serial.print(F("INVALID"));
  v_signal = "Error";
  }
  Serial.println();
}
void oledView() {
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Signal");
  display.drawString(45, 0, ":");
  display.drawString(49, 0, v_signal);
  display.drawString(70, 0, "/");
  display.drawString(80, 0, v_state);
  display.drawString(0, 10,"User");
  display.drawString(45, 10, ":");
  display.drawString(49, 10, v_user);
  display.drawString(0, 20,"Temp.");
  display.drawString(45, 20, ":");
  display.drawString(49, 20, v_temp+" "+"°C");
  display.drawString(0, 30,"Hum.");
  display.drawString(45, 30, ":");
  display.drawString(49, 30, v_hum+" %");
  display.drawString(0, 40,"Lat.");
  display.drawString(45, 40, ":");
  display.drawString(49, 40, v_lat);
  display.drawString(0, 50,"Long.");
  display.drawString(45, 50, ":");
  display.drawString(49, 50, v_long);
}
