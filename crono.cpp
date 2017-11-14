#include <Arduino.h>
#include "FS.h"
#include <ArduinoJson.h>
#include "time_ntp.h"

float setPoint[5] = { 15.00, 18.00, 20.00, 21.50, 23.00};                             // Setpoint NoIce,Eco,Normal,Comfort,Comfort+
char* descPoint[5] = {"NoIce","Eco", "Normal", "Comfort", "Comfort+"};             // Nomi NoIce Setpoint Eco,Normal,Comfort,Comfort+
byte cronoPoint[8][48] = {1} ;                                              // matrice del crono


void SaveCronoMatrixSPIFFS () {
  SPIFFS.begin();
  DynamicJsonBuffer jsonBuffer;

  JsonObject& savePoint = jsonBuffer.createObject();
  savePoint["Sp0"] = setPoint[0];
  savePoint["Sp1"] = setPoint[1];
  savePoint["Sp2"] = setPoint[2];
  savePoint["Sp3"] = setPoint[3];

  JsonArray& cronomatrix = savePoint.createNestedArray("cronomatrix");
  //JsonArray& cronomatrix = jsonBuffer.createArray();
  byte dS = 0;
  byte gS = 0;
  byte i = 0;
  for (byte dS = 1; dS < 8; dS++) {
    for (byte gS = 0; gS < 48; gS++) {
      cronomatrix.add(cronoPoint[dS][gS]);
      byte pS = cronoPoint[dS][gS];
      Serial.print("Saving in SPIFFS : "); Serial.print(i); Serial.print(" day "); Serial.print(dS); Serial.print(" hour/2 "); Serial.print(gS); Serial.print(" value "); Serial.println(pS);
      //testati delay fino a 100 !
      delay(1);
      i++;
    }
    gS = 0;
  }
  dS = 0;
  i = 0;

  Serial.print("Ecco i dati in json: ");
  //cronomatrix.printTo(Serial);
  savePoint.printTo(Serial);
  char buffer[1000];
  savePoint.printTo(buffer, sizeof(buffer));
  Serial.println();

  // open file for writing
  File spiffs = SPIFFS.open("/crono.json", "w");
  if (!spiffs) {
    Serial.println("Failed to open crono.json");
    return;
  }
  //qui salvo il buffer su file
  spiffs.println(buffer);
  Serial.println("Salvo in SPIFFS il buffer con i settings :"); Serial.println(buffer);
  delay(1);
  //chiudo il file
  spiffs.close();
}

void ReadCronoMatrixSPIFFS() {

  Serial.println("Read Crono Settings from SPIFFS...");
  File  cronomatrix_inlettura = SPIFFS.open("/crono.json", "r");
  if (!cronomatrix_inlettura) {
    Serial.println("Failed to open sst_crono_matrix.json");
    return;
  }


  String risultatocronomatrix = cronomatrix_inlettura.readStringUntil('\n');
  //Serial.print("Ho letto dal file : ");Serial.println(risultatocronomatrix);
  char jsoncronomatrix[1000];
  risultatocronomatrix.toCharArray(jsoncronomatrix, 1000);
  Serial.print("Imposto i setpoint del crono: "); Serial.println(jsoncronomatrix);
  //StaticJsonBuffer<200> jsonBuffer_cronomatrix_inlettura;
  DynamicJsonBuffer jsonBuffer_cronomatrix_inlettura;

  JsonObject& rootcronomatrix_inlettura = jsonBuffer_cronomatrix_inlettura.parseObject(jsoncronomatrix);
  if (!rootcronomatrix_inlettura.success()) {
    Serial.println("parseObject() failed");
  }
  setPoint[1] = rootcronomatrix_inlettura["Sp0"];
  setPoint[2] = rootcronomatrix_inlettura["Sp1"];
  setPoint[3] = rootcronomatrix_inlettura["Sp2"];
  setPoint[4] = rootcronomatrix_inlettura["Sp3"];

  byte dS = 0;
  byte gS = 0;
  int ii = 0;
  for (byte dS = 1; dS < 8; dS++) {
    for (byte gS = 0; gS < 48; gS++) {
      cronoPoint[dS][gS] = rootcronomatrix_inlettura["cronomatrix"][ii];
      byte pS =cronoPoint[dS][gS];
      //Serial.print("Reading from SPIFFS : "); Serial.print(" day "); Serial.print(dS); Serial.print(" hour/2 "); Serial.print(gS); Serial.print(" value "); Serial.println(pS);
      delay(1);
      ii++;
    }
    gS = 0;
  }
  dS = 0;
  ii = 0;
  cronomatrix_inlettura.close();
}

float cronoSwitch0(int today,int ore){
  float tempCrono;
  tempCrono = setPoint[cronoPoint[today][ore*2]];
  Serial.print("setTemp 0 :" +String(tempCrono)+" giorno: "+String(today) + " ore: "+String(ore)+" / "+String(cronoPoint[today][ore*2])+"\n");
  return(tempCrono);
}
float cronoSwitch30(int today,int  ore){
   float tempCrono;
   tempCrono= setPoint[cronoPoint[today][ore*2+1]];
   Serial.print("setTemp 30 :" +String(tempCrono)+" giorno: "+String(today) + " ore: "+String(ore)+" / "+String(cronoPoint[today][ore*2])+"\n");
   return(tempCrono);
}

