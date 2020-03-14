#include "JeeUI2.h"

AsyncWebServer server(80);
bool __shouldReboot; // OTA update reboot flag

void jeeui2::var(String key, String value) 
{ 
    if(pub_enable){
        JsonVariant pub_key = pub_transport[key];
        if (!pub_key.isNull()) {
            pub_transport[key] = value;
            if(dbg)Serial.printf_P(PSTR("Pub: [%s - %s]\n"), key.c_str(), value.c_str());
            pub_mqtt(key, value);
            String tmp;
            serializeJson(pub_transport, tmp);
            deserializeJson(pub_transport, tmp);
            tmp = F("");
            return;
        }
    }
    if(rc)publish(String(F("jee/set/")) + key, value, true);
    if(dbg)Serial.print(F("WRITE: "));
    if(dbg)Serial.printf_P(PSTR("key (%s) value (%s) RAM: %d\n"), key.c_str(), value.substring(0, 15).c_str(), ESP.getFreeHeap());
    cfg[key] = value;
} 

String jeeui2::param(String key) 
{ 
    String value = cfg[key];
    if(dbg)Serial.print(F("READ: "));
    if(dbg)Serial.println(String(F("key (")) + key + String(F(") value (")) + value + String(F(")")));
    return value;
} 

String jeeui2::deb() 
{ 
    String cfg_str;
    serializeJson(cfg, cfg_str);
    return cfg_str;  
}

void jeeui2::begin(bool debug) {
    dbg = debug;
    nonWifiVar();
    load();
    if(dbg)Serial.println(String(F("CONFIG: ")) + deb());
    begin();
    if(dbg)Serial.println(String(F("RAM: ")) + String(ESP.getFreeHeap()));
    if(dbg)Serial.println(String(F("MAC: ")) + mac);
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, F("text/plain"), F("Not found"));
}

void jeeui2::begin() { 
    wifi_connect();

    /*use mdns for host name resolution*/
    char tmpbuf[32];
    sprintf_P(tmpbuf,PSTR("%s%s"),__IDPREFIX, mc.c_str());    
    if (!MDNS.begin(tmpbuf)) {
        Serial.println(F("Error setting up MDNS responder!"));
        while (1) {
        delay(1000);
        }
    }
    MDNS.addService(F("http"), F("tcp"), 80);
    Serial.printf_P(PSTR("mDNS responder started: %s.local\n"),tmpbuf);

    server.on(PSTR("/post"), HTTP_ANY, [this](AsyncWebServerRequest *request) {
        uint8_t params = request->params();
        AsyncWebParameter *p;
        for (uint8_t i = 0; i < params; i++)
        {
          p = request->getParam(i);
          if (p->name().indexOf(F("BTN_")) != -1){
                btnui = p->name().substring(4, p->name().length());
                if(btnui == F("bWF")){
                    var(F("wifi"), F("STA"));
                    save();
                    ESP.restart();
                }
          } 
          else {
            var(p->name(), p->value());
            as();
          }
        }
        request->send(200, F("text/plain"), F("OK"));
    });

    server.on(PSTR("/pub"), HTTP_ANY, [this](AsyncWebServerRequest *request) {
        AsyncWebParameter *p;
        String value = F("");
        p = request->getParam(0);
        JsonVariant pub_key = pub_transport[p->name()];
        if (!pub_key.isNull()) {
            value = pub_transport[p->name()].as<String>();
            if(dbg)Serial.printf_P(PSTR("pub: [%s - %s - %s]\n"),p->name().c_str(), p->value().c_str(), value.c_str());
        }
        request->send(200, F("text/plain"), value);//p->value());
    });

    server.on(PSTR("/echo"), HTTP_ANY, [this](AsyncWebServerRequest *request) { 
        foo();
        request->send(200, F("text/plain"), buf);
        buf = F("");
    });

    server.on(PSTR("/_refresh"), HTTP_ANY, [this](AsyncWebServerRequest *request) {
        static unsigned long echoTm; // сброс только через секунду
        
        if(!_refresh)
            {echoTm = millis();}
        else if(echoTm+1500<millis()){ // 1.5 секунды при цикле опроса в 1 секунду
            _refresh = false;
            echoTm = millis();
        }

        request->send(200, F("text/plain"), String(F("{\"_refresh\":\"")) + String(_refresh) + F("\"}"));
    });

    server.on(PSTR("/config"), HTTP_ANY, [this](AsyncWebServerRequest *request) { 
        String config = deb();
        request->send(200, F("text/plain"), config);
        config = F("");
    });

    server.on(PSTR("/js/maker.js"), HTTP_ANY, [this](AsyncWebServerRequest *request) {
        AsyncWebServerResponse* response = request->beginResponse(SPIFFS, F("/js/maker.js.gz"), F("application/javascript"));
        response->addHeader(F("Content-Encoding"), F("gzip"));
        request->send(response);
    });

    server.on(PSTR("/"), HTTP_ANY, [this](AsyncWebServerRequest *request) {
        AsyncWebServerResponse* response = request->beginResponse(SPIFFS, F("/index.html.gz"), F("text/html"));
        response->addHeader(F("Content-Encoding"), F("gzip"));
        request->send(response);
    });

    server.on(PSTR("/css/pure-min.css"), HTTP_ANY, [this](AsyncWebServerRequest *request) {
        AsyncWebServerResponse* response = request->beginResponse(SPIFFS, F("/css/pure-min.css.gz"), F("text/css"));
        response->addHeader(F("Content-Encoding"), F("gzip"));
        request->send(response);
    });

    server.on(PSTR("/css/side-menu.css"), HTTP_ANY, [this](AsyncWebServerRequest *request) {
        AsyncWebServerResponse* response = request->beginResponse(SPIFFS, F("/css/side-menu.css.gz"), F("text/css"));
        response->addHeader(F("Content-Encoding"), F("gzip"));
        request->send(response);
    });

    server.on(PSTR("/css/range.css"), HTTP_ANY, [this](AsyncWebServerRequest *request) {
        AsyncWebServerResponse* response = request->beginResponse(SPIFFS, F("/css/range.css.gz"), F("text/css"));
        response->addHeader(F("Content-Encoding"), F("gzip"));
        request->send(response);
    });

    server.on(PSTR("/css/grids.css"), HTTP_ANY, [this](AsyncWebServerRequest *request) {
        AsyncWebServerResponse* response = request->beginResponse(SPIFFS, F("/css/grids.css.gz"), F("text/css"));
        response->addHeader(F("Content-Encoding"), F("gzip"));
        request->send(response);
    });

    server.on(PSTR("/css/chk.css"), HTTP_ANY, [this](AsyncWebServerRequest *request) {
        AsyncWebServerResponse* response = request->beginResponse(SPIFFS, F("/css/chk.css.gz"), F("text/css"));
        response->addHeader(F("Content-Encoding"), F("gzip"));
        request->send(response);
    });

    server.on(PSTR("/js/ui.js"), HTTP_ANY, [this](AsyncWebServerRequest *request) {
        AsyncWebServerResponse* response = request->beginResponse(SPIFFS, F("/js/ui.js.gz"), F("application/javascript"));
        response->addHeader(F("Content-Encoding"), F("gzip"));
        request->send(response);
    });

    server.on(PSTR("/favicon.ico"), HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse* response = request->beginResponse(SPIFFS, F("/favicon.ico.gz"), F("image/x-icon"));
        response->addHeader(F("Content-Encoding"), F("gzip"));
        //AsyncWebServerResponse *response = request->beginResponse_P(200, F("image/x-icon"), favicon_ico_gz, favicon_ico_gz_len);
        request->send(response);
    });

    server.on(PSTR("/heap"), HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, F("text/plain"), String(ESP.getFreeHeap()));
    });

    // Simple Firmware Update Form
    server.on(PSTR("/update"), HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, F("text/html"), F("<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>"));
    });

    server.on(PSTR("/update"), HTTP_POST, [](AsyncWebServerRequest *request){
        __shouldReboot = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, F("text/plain"), (__shouldReboot?F("OK"):F("FAIL")));
        response->addHeader(F("Connection"), F("close"));
        request->send(response);
    },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if(!index){
            Serial.printf_P(PSTR("Update Start: %s\n"), filename.c_str());
#ifndef ESP32
            Update.runAsync(true);
#endif
            if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
                Update.printError(Serial);
            }
        }
        if(!Update.hasError()){
            if(Update.write(data, len) != len){
                Update.printError(Serial);
            }
        }
        if(final){
            if(Update.end(true)){
                Serial.printf_P(PSTR("Update Success: %uB\n"), index+len);
            } else {
                Update.printError(Serial);
            }
        }
    });
    
    server.onNotFound(notFound);

    server.begin();
    foo();
    upd();
    mqtt_update();
}

void jeeui2::led(uint8_t pin, bool invert)
{
    if (pin == -1) return;
    LED_PIN = pin;
    LED_INVERT = invert;
    pinMode(LED_PIN, OUTPUT);
}

void jeeui2::handle()
{
    if(__shouldReboot){
        Serial.println(F("Rebooting..."));
        delay(100);
        ESP.restart();
    }
    MDNS.update();
    _connected();
    mqtt_handle();
    udpLoop();
    static unsigned long timer;
    unsigned int interval = 300;
    if (timer + interval > millis())
        return;
    timer = millis();
    btn();
    led_handle();
    autosave();
}

void jeeui2::nonWifiVar(){
    getAPmac();
    String wifi = param(F("wifi"));
    String ssid = param(F("ssid"));
    String pass = param(F("pass"));
    String ap_ssid = param(F("ap_ssid"));
    String ap_pass = param(F("ap_pass"));
    if(wifi == F("null")) var(F("wifi"), F("AP"));
    if(ssid == F("null")) var(F("ssid"), F("JeeUI2"));
    if(pass == F("null")) var(F("pass"), "");
    if(ap_ssid == F("null")) var(F("ap_ssid"), String(__IDPREFIX) + mc);
    if(ap_pass == F("null")) var(F("ap_pass"), "");
}

void jeeui2::getAPmac(){
    if(mc != F("")) return;
    #ifdef ESP32
    WiFi.mode(WIFI_MODE_AP);
    #else
    WiFi.mode(WIFI_AP);
    #endif
    mc = String(WiFi.softAPmacAddress());
    mc.replace(F(":"), F(""));
}
