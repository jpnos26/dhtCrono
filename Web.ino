 
 void server_web(){
 //Send OTA events to the browser
  Serial.print ("Inizializzo OTA .........\n");
  ArduinoOTA.onStart([]() { events.send("Update Start", "ota"); });
  ArduinoOTA.onEnd([]() { events.send("Update End", "ota"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char p[32];
    sprintf(p, "Progress: %u%%\n", (progress/(total/100)));
    events.send(p, "ota");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    if(error == OTA_AUTH_ERROR) events.send("Auth Failed", "ota");
    else if(error == OTA_BEGIN_ERROR) events.send("Begin Failed", "ota");
    else if(error == OTA_CONNECT_ERROR) events.send("Connect Failed", "ota");
    else if(error == OTA_RECEIVE_ERROR) events.send("Recieve Failed", "ota");
    else if(error == OTA_END_ERROR) events.send("End Failed", "ota");
  });
  ArduinoOTA.setHostname(HostName);
  ArduinoOTA.begin();

  MDNS.addService("http","tcp",80);
  
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  events.onConnect([](AsyncEventSourceClient *client){
    client->send("hello!",NULL,millis(),1000);
  });
  server.addHandler(&events);

  server.addHandler(new SPIFFSEditor(Http_username,Http_password));

  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
   server.on("/all", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.printf("\nGET /all");
    String json = "{";
    json += "\"heap\":"+String(ESP.getFreeHeap());
    json += ", \"temperature\":"+String(dhtTemp);
    json += ", \"setpoint\":"+String(setTemp);
    json += ", \"humidity\":"+String(dhtUm);
    json += ", \"relestatus\":\"" +stato+"\"";
    json += ", \"checkEnable\":"+String(checkEnable);
    json += "}";
    Serial.printf("Json: \n");
    request->send(200, "application/json", json);
  });

  server.on("/setManuale", HTTP_GET, [](AsyncWebServerRequest *request){
   switch (checkEnable){
    case 0:
      Serial.printf("\nSET Manuale ON");
      checkEnable = 1;
    break;
    case 1:
      Serial.printf("\nSET OFF No Ice");
      checkEnable = 2;
      break;
    case 2:
      Serial.printf("\nSET Auto ON");
      checkEnable = 0;
      break;
   }
   request->redirect("/Salva.htm");
  });

  server.on("/setTemp", HTTP_GET, [](AsyncWebServerRequest *request){
    int params = request->params();
    int i = 0;
    AsyncWebParameter* p = request->getParam(i);
    setTemp = p->name().toFloat();
    Serial.printf("HEADER[%s]: %s\n", p->name().c_str(), p->value().c_str());
    request->redirect("/Salva.htm");
  });

  server.on("/setCrono", HTTP_GET, [](AsyncWebServerRequest *request){
    int params = request->params();
    int i = 0;
    AsyncWebParameter* p = request->getParam(i);
    Serial.printf("HEADER[%s]: %s\n", p->name().c_str(), p->value().c_str());
    String salva_crono = p->name();
    String S_filena_WBS = "/crono.json";
          fsUploadFile = SPIFFS.open(S_filena_WBS, "w");
          if (!fsUploadFile){ 
            Serial.println("file open failed");
            }
          else{
            fsUploadFile.println(salva_crono); 
            Serial.printf("BodyEnd: %s\n",salva_crono.c_str());
          }
          fsUploadFile.close();
          delay(20);
          ReadCronoMatrixSPIFFS();
    request->redirect("/Salva.htm");
  });
  
   server
    .serveStatic("/", SPIFFS, "/")
    .setDefaultFile("index.htm")
    .setCacheControl("max-age=300")
    .setAuthentication(Http_username, Http_password);

  server.onNotFound([](AsyncWebServerRequest *request){
    Serial.printf("NOT_FOUND: ");
    if(request->method() == HTTP_GET)
      Serial.printf("GET");
    else if(request->method() == HTTP_POST)
      Serial.printf("POST");
    else if(request->method() == HTTP_DELETE)
      Serial.printf("DELETE");
    else if(request->method() == HTTP_PUT)
      Serial.printf("PUT");
    else if(request->method() == HTTP_PATCH)
      Serial.printf("PATCH");
    else if(request->method() == HTTP_HEAD)
      Serial.printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
      Serial.printf("OPTIONS");
    else
      Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if(request->contentLength()){
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });
  server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
      Serial.printf("UploadStart: %s\n", filename.c_str());
    Serial.printf("%s", (const char*)data);
    if(final)
      Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
      Serial.printf("BodyStart: %u\n", total);
    Serial.printf("%s", (const char*)data);
    if(index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
});
 }
 ///////////////////
// (re-)start WiFi
///////////////////
    void WiFiStart()
    { 
      myNextion.sendCommand("sistema.txt=\"Start WiFi..\"");
      WiFi.mode(WIFI_AP_STA);
      int ret;
      bool autoConnect;
      autoConnect = WiFi.getAutoConnect();
      // Connect to WiFi network
      if (dhcp == 0){
        WiFi.config(ip, gateway, subnet);
        delay(100);
        }
      int timeout = 10;
      WiFi.softAP(ap_ssid, ap_password);
      WiFi.softAPIP();
      Serial.print("AP IP address "); 
      Serial.println(WiFi.softAPIP());
      WiFi.begin(Wifi_ssid, Wifi_password);
       while ( ((ret = WiFi.status()) != WL_CONNECTED) && timeout ) {
        delay(500);
        Serial.print(".");
        --timeout;
       }
      if ((ret = WiFi.status()) == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi connessa");
        // Print the IP address
        Serial.println(WiFi.localIP());
      }
     wifi_set_sleep_type(LIGHT_SLEEP_T); 
     myNextion.setComponentText("sistema",stato_sys);
  }

void send_nextion(){
   String send_set =String(setTemp,1);
   switch (checkEnable){
        case 0:
          myNextion.sendCommand("check.txt=\"Auto\"");
          myNextion.setComponentText("set",send_set);
          break;
        case 1:
           myNextion.sendCommand("check.txt=\"Manuale\"");
           myNextion.setComponentText("set",send_set);
          break;
        case 2:
           myNextion.sendCommand("check.txt=\"No Ice\"");
           send_set =String(dhtIce,1);
           myNextion.setComponentText("set",send_set);
          break;
        }
       String send_temp = String(dhtTemp,1)+char(176)+"c";
       String send_umidita =String(dhtUm,0)+"%";
       myNextion.setComponentText("temp",send_temp);
       myNextion.setComponentText("umidita",send_umidita);
       myNextion.sendCommand("wup=255");
      }

