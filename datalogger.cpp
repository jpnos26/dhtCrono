#include "preferences.h"
#include "datalogger.h"
#include "FS.h"
#include "time_ntp.h"
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "time_ntp.h"

/*getNTPday()
 * 1.Sunday 2.Monday 3.Tuesday 4.Wednesday 5.Thursday 6.Friday 7.Saturday
 * SPIFFS.exists(path)
 * SPIFFS.remove(path)
 */
int iDay_prev;

 

void save_datalogger(float setpoint,float temperature,float humidity,int relaystatus,unsigned long timing,int iDay) {
  String S_TimeDate = epoch_to_date_line(millis()/1000+timing)+" " +epoch_to_time(millis()/1000+timing) + ":00";
  String S_filename = "/datalog/datalogger" + String(iDay) + ".csv";
  

//  Serial.print("iDay. ");Serial.println(iDay);
//  Serial.print("iDay_prev. ");Serial.println(iDay_prev);


  if(iDay_prev > 0 && (iDay != iDay_prev)) {
    if(SPIFFS.exists(S_filename)==1)  {
      Serial.print("    Delete older file: ");Serial.println(S_filename);
      SPIFFS.remove(S_filename);
   }
  }
  


  File datalogger = SPIFFS.open(S_filename, "a");
  if (!datalogger) {
    Serial.print(S_filename);Serial.println(" open failed");
  }
  Serial.print(S_filename);Serial.print(" size: ");Serial.println(datalogger.size());
    datalogger.print(S_TimeDate);datalogger.print(",");datalogger.print(setpoint,1);datalogger.print(",");datalogger.print(temperature,1);datalogger.print(",");datalogger.print(humidity,1);datalogger.print(",");datalogger.println(relaystatus);
    Serial.print("Salvo linea datalogger -> ");Serial.print(S_TimeDate);Serial.print(",");Serial.print(setpoint,1);Serial.print(",");Serial.print(temperature,1);Serial.print(",");Serial.print(humidity,1);Serial.print(",");Serial.println(relaystatus);
  Serial.print(S_filename);Serial.print(" size: ");Serial.println(datalogger.size());
  datalogger.close();
  iDay_prev = getNTPday();
  ////////////////////////
 
}

