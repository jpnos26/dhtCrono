/*---------------------------------------------------
HTTP 1.1 Temperature & Humidity Webserver for ESP8266 
and ext value for Raspberry Thermostat

by Jpnos 2017  - free for everyone

Connect DHT21 / AMS2301 at GPIO2
---------------------------------------------------*/
#include <WiFiUdp.h>
#include "DHT.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include "time_ntp.h"
#include "datalogger.h"
#include "preferences.h"
#include "crono.h"
#include <TimeLib.h>
#include <SoftwareSerial.h>
#include <Nextion.h>

// SKETCH BEGIN
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");


// storage for Measurements; keep some mem free; allocate remainder

unsigned long ulMeasCount=0;        // values already measured
unsigned long ulNoMeasValues=0;     // size of array
unsigned long ulMeasDelta_ms=5000; // distance to next meas time
unsigned long ulNextMeas_ms;        // next meas time
unsigned long ulNextgraph;          //prossimo salvataggio grafico
unsigned long ulGraphDelta=600000;  // delta salvataggio grafico 
int checkEnable = 0;                // Controllo Manuale/auto
int RELEPIN = 13;                   // pin per RELE
uint8_t ONRele = 0;                  // Configurazione per Rele ON : 1 HIGH e 0 LOW
uint8_t OFFRele = 1;                // Configurazione per Rele OFF : 0 Low e 1 HIGH Ricordare di inverire logica con ONRele
float setStep = 0.5;                // step Incremento tasti
float tempHist=0.2;                 // temp histeresis
float dhtTemp;			                // dht read temperature
float dhtUm;			                  // dht read umidity
float dhtTemp0;                     // dht read temperature
float dhtUm0;                       // dht read umidity
float dhtIce = 15;                  // temperatura in off no ice
int dhtIceCom;
int dhtTempCom0;
int dhtTempCom1;
int setTempCom;
int set_rele;


unsigned long dhtTime;              //  time of measurements
float setTemp=16;                   // set Temp to check
unsigned long ulNextntp;            // next ntp check
unsigned long timezoneRead();       // read time from internet db
String stato= "OFF";                //stato sistema
unsigned long ulSecs2000_timer;     // orario in microsecondi
unsigned long ulSecsRtc_timer;      //orario rtc in microsecondi
byte checkTemp0 = 0;                //check Temperature
byte checkTemp30 = 0;               //check Temperature
unsigned long next_down = 20000;     // display down dimmer tempo in millisecondi per diminuire luminosita
unsigned long ulNextDown;           //
unsigned int cycle_down = 0;        // 
unsigned int num_cycle_down = 3;    // numero di cicli di next_down per lo spegnimento 0 non spegne mai 
unsigned int max_lum = 80;          // luminosita alta
unsigned int min_lum = 20;          // luminosita bassa
String stato_sys = "Caldaia OFF";

File fsUploadFile;

IPAddress ip(STATIC_IP);
IPAddress gateway(STATIC_IP_GW);
IPAddress subnet(STATIC_SUBNET);

SoftwareSerial nextion(12, 14);// Nextion TX to pin 4 and RX to pin 5 of Arduino

Nextion myNextion(nextion, 9600); //create a Nextion object named myNextion using the nextion serial port @ 9600bps

// needed to avoid link error on ram check
extern "C" 
{
#include "user_interface.h"
}
//////////////////////////////
// DHT21 / AMS2301 pin
//////////////////////////////
#define DHTPIN 0

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11 
#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// init DHT; 3rd parameter = 16 works for ESP8266@80MHz
DHT dht(DHTPIN, DHTTYPE,16); 

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if(info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        if(info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if((info->index + len) == info->len){
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}




void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("Esp8266 WI-FI Temp/Umidita Jpnos-2017 v1.0");
   //initialize display
    
    myNextion.init();
   
    nextion_initial();
  
    
    Serial.print ("Inizializzo Wifi .........\n");
  ///start wifi
  WiFiStart();
  delay(500);
   
   
  
  //ESP.eraseConfig();
   /////Inizializzo PIN

  pinMode(RELEPIN,OUTPUT);
  digitalWrite ( RELEPIN,OFFRele);
  
  Serial.print ("Inizializzo SPIFFS .........\n");
   SPIFFS.begin();
   delay(500);
   
  Serial.print ("Inizializzo ntp .........\n");
  ntpacquire();
   delay(500);

  Serial.print ("Inizializzo Server web .........\n");
  myNextion.setComponentText("sistema","Start web..");
  server_web();
  server.begin();
  delay(500);
  
 

  ulNextMeas_ms = millis()+ulMeasDelta_ms;
  Serial.print("leggiamo il Crono \n");
  ReadCronoMatrixSPIFFS();
  
  Serial.print("Leggiamo il DHT \n");
  ReadDht();

  ulNextDown = millis() + next_down;
  myNextion.setComponentText("sistema",stato_sys);

}

void loop(){

  

    ///////////////////////////
    ///check NTP
   //////////////////////////
    if (millis()>=ulNextntp){
          ntpacquire();     
    }  
   ///////////////////
    // do data logging
   ///////////////////
  
   if (millis()>=ulNextMeas_ms) {
   
   ReadDht();
   
    ///////////////////////////
  // check crono
  ///////////////////////////
  //Serial.println("Minuto per Crono : "+String(getNTPminute()));
  if  (getNTPminute() <= 29 && checkTemp0 == 0 && checkEnable == 0){
    setTemp = cronoSwitch0(getNTPday(),(getNTPhour()));
    checkTemp0 = 1;
    checkTemp30 = 0;
  }
  if (getNTPminute() >= 30 && checkTemp30 == 0 && checkEnable == 0){
    setTemp =cronoSwitch30(getNTPday(),getNTPhour());
    checkTemp0 = 0;
    checkTemp30 = 1;
  }

   
  
   dhtTempCom0 = int(dhtTemp*100);dhtTempCom1=int((dhtTemp*100)+(tempHist*100));setTempCom = int(setTemp*100);dhtIceCom=int(dhtIce*100);
   //Serial.print(String(setTempCom)+" - "+String(dhtTempCom0)+" - "+String(dhtTempCom1)+" - "+String(dhtIceCom)+" - "+stato +"\n");
   delay(10);
   
   switch (checkEnable){ 
    case 2:
      if (dhtIceCom > dhtTempCom1){
          if (stato =="OFF"){
            digitalWrite ( RELEPIN,ONRele);
            myNextion.setComponentText("sistema","Caldaia ON");
            myNextion.sendCommand("temp.pco=63488");
            myNextion.sendCommand("ref sistema");
            stato_sys = "Caldaia ON";
            stato="ON";
            ulNextgraph = millis();
            delay(10);
          }
        }
      else if ( dhtIceCom <=  dhtTempCom0){
          if (stato =="ON"){
            digitalWrite ( RELEPIN ,OFFRele) ;
            myNextion.setComponentText("sistema","Caldaia OFF");
            myNextion.sendCommand("sistema.pco=31");
            myNextion.sendCommand("temp.pco=31");
            myNextion.sendCommand("ref sistema");
            stato_sys = "Caldaia OFF";
            stato="OFF" ;
            ulNextgraph = millis();
            delay(10);
          }
      }
      break;
    default:
      if (setTempCom > dhtTempCom1){
           if (stato =="OFF"){
            digitalWrite ( RELEPIN,ONRele);
            myNextion.setComponentText("sistema","Caldaia ON");
            myNextion.sendCommand("temp.pco=63488");
            myNextion.sendCommand("ref sistema");
            stato_sys = "Caldaia ON";
            stato="ON";
            ulNextgraph = millis();
            delay(10);
           }
        }
      else if ( setTempCom <=  dhtTempCom0){
         if (stato =="ON"){
            digitalWrite ( RELEPIN , OFFRele) ;
            myNextion.setComponentText("sistema","Caldaia OFF");
            myNextion.sendCommand("temp.pco=31");
            myNextion.sendCommand("ref sistema");
            stato_sys = "Caldaia OFF";
            stato="OFF" ;
            ulNextgraph = millis();
            delay(10);
          }
      }
      break;
    }
   }
   
  
 
////salvo grafico
  if (millis()>= ulNextgraph)
      {     
       bool check_rele = digitalRead(RELEPIN); //se pin acceso alto
       if(ONRele == 0){
        check_rele = !check_rele;
       }
       if (check_rele == 1){
        set_rele = 5;
       }
       else{
        set_rele = 0;
       }
       save_datalogger(setTemp,dhtTemp,dhtUm,set_rele,ulSecs2000_timer,getNTPday());
       delay(10);
       ulNextgraph = millis()+ulGraphDelta;
      }
//// controllo pressione tasti nextion
  String message = myNextion.listen(); //check for message
  if(message != ""){ // if a message is received...
    Serial.println(message); //...print it out
    ulNextDown = millis() + next_down;
    if (message == "65 0 3 0 ff ff ff"){
      setTemp = setTemp + setStep;
    }
    else if (message =="65 0 2 0 ff ff ff"){
      setTemp = setTemp - setStep;
    }
    else if (message =="65 0 6 0 ff ff ff"){
      checkEnable++;
      if (checkEnable == 3){
        checkEnable = 0; 
      } 
    }
      
    if (setTemp <=15){
        setTemp = 15;
      }
    else if(setTemp >=25){
        setTemp=25;
      }
    String send_dim = "dim="+String(max_lum);
    myNextion.sendCommand(send_dim.c_str());
    myNextion.sendCommand("wup=255"); 
    send_nextion();
    cycle_down = 0;
    ulNextDown = millis()+ next_down;
    }
  
   if (millis()>=ulNextDown) {
    if (num_cycle_down != 0){
      if (cycle_down == num_cycle_down){
        myNextion.sendCommand("dim=0");
        }
      else {
        String send_dim = "dim="+String(min_lum);
        myNextion.sendCommand(send_dim.c_str());
        ulNextDown = millis()+ next_down;
        cycle_down++;      
      }
    }
   }
}

 
  void ntpacquire() 
  {
  ///////////////////////////////
  // connect to NTP and get time
  ///////////////////////////////
 
    myNextion.sendCommand("sistema.txt=\"check ntp..\""); 
    delay(500);
  //Leggo Time da NTP
  //unsigned long timeNtp=getNTPTimestamp();
  //if (!timeNtp == 0){
  //  ulSecs2000_timer=timeNtp;
  //  Serial.print("Ora Corrente da server NTP : " );
  //  Serial.println(epoch_to_string(ulSecs2000_timer).c_str());
  //  ulSecs2000_timer -= millis()/1000;  // keep distance to millis() counter
  //  ulNextntp=millis()+3600000;
  //}
  //delay(100);
  //Leggo Time da TimezoneDb
  if (WiFi.status() == WL_CONNECTED){
    unsigned long timeInternet=timezoneRead();
    if (!timeInternet == 0)
      {
        ulSecs2000_timer=timeInternet;
        Serial.print("Ora Corrente da server Internet: " );
        Serial.println(epoch_to_string(ulSecs2000_timer).c_str());
           
        ulSecs2000_timer -= millis()/1000;  // keep distance to millis() counter
        setTime( ulSecs2000_timer+946684800UL+(millis()/1000));
        time_t timeNow = now();
        delay(100);
        String time_invia = "rtc0="+String(year(timeNow));
        myNextion.sendCommand(time_invia.c_str());
        time_invia = "rtc1="+String(month(timeNow));
        myNextion.sendCommand(time_invia.c_str());
        time_invia = "rtc2="+String(day(timeNow));
        myNextion.sendCommand(time_invia.c_str());
        time_invia = "rtc3="+String(hour(timeNow));
        myNextion.sendCommand(time_invia.c_str());
        time_invia = "rtc4="+String(minute(timeNow));
        myNextion.sendCommand(time_invia.c_str());
        time_invia = "rtc5="+String(second(timeNow));
        myNextion.sendCommand(time_invia.c_str());
        myNextion.sendCommand("wup=255");
        time_invia="";
        Serial.print("Ora Set :"+ String(hour(timeNow))+":"+String(minute(timeNow))+":"+String(second(timeNow))+"-"+String(day(timeNow))+"/"+String(month(timeNow))+"/"+String(year(timeNow))+"\n");
      }
      else{
        unsigned int  test[5]={0} ;
        test[0] = myNextion.getRtcValue("rtc0");
        test[1] = myNextion.getRtcValue("rtc1"); 
        test[2] = myNextion.getRtcValue("rtc2");
        test[3] = myNextion.getRtcValue("rtc3"); 
        test[4] = myNextion.getRtcValue("rtc4");
        test[5] = myNextion.getRtcValue("rtc5");
        test[0]-=2000;
        ulSecsRtc_timer = date_to_epoch(test[5],test[4],test[3],test[2],test[1],test[0]);
        ulSecs2000_timer=ulSecsRtc_timer;
        Serial.print("Ora Corrente da RTC: " );
        Serial.println(epoch_to_string(ulSecs2000_timer).c_str());   
        ulSecs2000_timer -= millis()/1000;  // keep distance to millis() counter
        setTime( ulSecs2000_timer+946684800UL+(millis()/1000));
        time_t timeNow = now();
        delay(100);
      }
  }  
  ulNextntp=millis()+3600000;
  myNextion.setComponentText("sistema",stato_sys);
 }


void nextion_initial(){
  myNextion.sendCommand("baud=115200");
  Nextion myNextion(nextion, 115200);
  myNextion.init();
  myNextion.sendCommand("dim=80");
  myNextion.sendCommand("sistema.txt=\"System\"");
  myNextion.sendCommand("check.txt=\"\"");
  myNextion.sendCommand("sistema.pco=31");
  myNextion.sendCommand("ref sistema");
  myNextion.sendCommand("doevents");
}

void ReadDht(){
    dht.begin();
    delay(50);
    dhtTemp0 = dht.readTemperature();
    dhtUm0 = dht.readHumidity();
    //Serial.println("Leggo : "+String(dhtTemp0)+" , "+String(dhtUm0));
      if (isnan(dhtTemp0) || isnan(dhtUm0)) 
      {
          dhtTime = millis()/1000+1000;
          Serial.println("Failed to read from DHT sensor!"); 
          ulNextMeas_ms = millis()+ulMeasDelta_ms;
      }   
      else
        {
        dhtTime = millis()/1000+ulSecs2000_timer;
        dhtTemp=dhtTemp0 + OFFSET_DHT;
        dhtUm=dhtUm0;
        ulNextMeas_ms = millis()+ulMeasDelta_ms;
        //Serial.print("Logging Temperature: "); 
        //Serial.print(dhtTemp);
        //Serial.print(" deg Celsius - Humidity: "); 
        //Serial.print(dhtUm);
        //Serial.print("% - Time: ");
        //Serial.println(dhtTime);
        ulMeasCount++;
      }
      send_nextion();
        
    }

