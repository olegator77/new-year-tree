
#include "gfx.h"
#include "ESP8266WiFi.h"
#include "WiFiClient.h"
#include "ESP8266WebServer.h"
#include <ESP8266HTTPClient.h>
#include <WiievaPlayer.h>
#include <WiievaRecorder.h>
#include <SdFat.h>


// GPIO pins, where relays are connected
// https://www.seeedstudio.com/Relay-Shield-v3.0-p-2440.html
int relayGpios[] = {7,4, 5 ,6};

// Number of controlled relays
const int numRelays = sizeof (relayGpios) /sizeof (relayGpios[0]);
// Current state of relays
int relayStates[numRelays] = {0};

const char *ssid     = "...";
const char *password = "...";
const char *url      = "http://www.google.com/speech-api/v2/recognize?output=json&lang=ru&key=...";

ESP8266WebServer server(80);
WiievaRecorder recorder (2000*5);
SdFat SD;

unsigned long timeRecorderStart = 0;
unsigned long timeRecorderEnd=0;
unsigned long timeLastActivity = 0;
bool screenSaver = true;
bool musicStatus = false;
bool wasVAD = false;


// GUI part
// ********
GHandle ghContainerMain,ghButton1,ghButton2,ghButton3,ghButton4,ghButtonAll,ghButtonVoice,ghButtonTree;
gdispImage ballImg,bearImg,candleImg,microphoneImg,treeImg,bigTreeImg,lightsImg;

GListener glistener;
extern "C" void gwinButtonDraw_ImageText(GWidgetObject *gw, void *param);

// Load icons and images from spiffs, create controls
void guiCreate(void) {
    gfxInit();

    // Add GUI event listener for handling events
    geventListenerInit(&glistener);
    geventAttachSource(&glistener, ginputGetKeyboard(0), 0);
    gwinAttachListener(&glistener);

    // Setup defaults for GUI
    gwinSetDefaultFont(gdispOpenFont("DejaVuSans16"));
    gwinSetDefaultStyle(&WhiteWidgetStyle, FALSE);
    gwinSetDefaultColor(HTML2COLOR(0x000000));
    gwinSetDefaultBgColor(HTML2COLOR(0xFFFFFF));


    gdispImageOpenFile(&ballImg, "ball.bmp");
    gdispImageOpenFile(&bearImg, "bear.bmp");
    gdispImageOpenFile(&candleImg, "candle.bmp");
    gdispImageOpenFile(&microphoneImg, "music.bmp");
    gdispImageOpenFile(&treeImg, "tree.bmp");
    gdispImageOpenFile(&lightsImg, "lights.bmp");
    gdispImageOpenFile(&bigTreeImg, "bigtree.bmp");

    // Create GUI elements
    GWidgetInit wi; gwinWidgetClearInit(&wi);
    wi.g.x = 0; wi.g.y = 0; wi.g.width = 176; wi.g.height = 220; wi.g.show = TRUE;
    ghContainerMain = gwinContainerCreate(0, &wi, 0);
    wi.g.parent = ghContainerMain;
    wi.customDraw = gwinButtonDraw_ImageText;
    wi.customStyle = 0;

    wi.customParam = &bigTreeImg; wi.g.x = 0; wi.g.y = 0; wi.text = ""; ghButtonTree =  gwinButtonCreate(0, &wi);

    wi.g.show = FALSE;
    wi.customParam = &ballImg; wi.g.width = 88; wi.g.height = 73; wi.text = "Шарики"; ghButton1 = gwinButtonCreate(0, &wi);
    wi.customParam = &candleImg; wi.g.x = 88; wi.g.y = 0; wi.text = "Свечки"; ghButton2 = gwinButtonCreate(0, &wi);
    wi.customParam = &bearImg; wi.g.x = 0; wi.g.y = 73; wi.text = "Мишки"; ghButton3 = gwinButtonCreate(0, &wi);
    wi.customParam = &lightsImg; wi.g.x = 88; wi.g.y = 73; wi.text = "Огоньки"; ghButton4 = gwinButtonCreate(0, &wi);
    wi.customParam = &treeImg; wi.g.x = 0; wi.g.y = 146; wi.text = "Все"; ghButtonAll = gwinButtonCreate(0, &wi);
    wi.customParam = &microphoneImg; wi.g.x = 88; wi.g.y = 146; wi.text = "Музыка"; ghButtonVoice = gwinButtonCreate(0, &wi);

    geventEventWait(&glistener, 10);
}

// Switch screen between scrensaver and control modes
void switchScreen (bool flag) {
    gwinSetVisible (ghButton1,flag);
    gwinSetVisible (ghButton2,flag);
    gwinSetVisible (ghButton3,flag);
    gwinSetVisible (ghButton4,flag);
    gwinSetVisible (ghButtonAll,flag);
    gwinSetVisible (ghButtonVoice,flag);
    gwinSetVisible (ghButtonTree,!flag);
    screenSaver = !flag;
}

// Control realys part
// *******************

// Toggle state of relay
void controlRelay (int relay, String state = "toggle") {
    if (relay >= 0 && relay < numRelays) {
        if (state == "on")
            relayStates[relay] = 1;
        else if (state == "off")
            relayStates[relay] = 0;
        else if (state == "toggle")
            relayStates[relay] = !relayStates[relay];
        else
            return;
        digitalWrite (relayGpios[relay],relayStates[relay]);
        Serial.printf ("Changing relay state %d => %d\n",relay,relayStates[relay]);
    }
}
// Toggle all relay's state
void controlAllRelay (String state = "toggle") {
    if (state == "toggle" ) {
        state = !relayStates[0]?"on":"off";
    }
    for (int relay = 0; relay < numRelays; ++relay){
        controlRelay (relay,state);
    }
}

// Audio mp3 player part
// *********************
void startPlay ()
{
    char name[256];
    auto dir = SD.open("/");
    if (!dir)
        return;

    String names[128];
    int count=0;

    for (;;) {
        auto entry =  dir.openNextFile();
        if (! entry)
            break;
        if (!entry.isDirectory() && entry.getName(name,sizeof (name)) && name[0] != '.')
        {
            names[count++] = name;
            entry.close();
        }
        Serial.println (name);
    }
    dir.close();

    switchScreen (false);
    musicStatus = true;

    WiievaPlayer player (0x2000);
    for (int i = rand()%count; musicStatus; i = (i + 1) % count) {
        File f = SD.open(names[i]);

        if (!f) {
            Serial.printf ("Can't open file %s\n",names[i].c_str());
            continue;
        }

        Serial.printf ("Start playing\n");

        player.start (AIO_AUDIO_OUT_MP3);

        while (f.available() && musicStatus) {
            player.run(f);
            server.handleClient();
            if (geventEventWait(&glistener, 2)) {
                musicStatus = false;
                switchScreen (true);
                break;
            }
            delay (10);
        }

        Serial.printf ("Stop playing\n");
        player.stop ();
        f.close ();
    }

}

// Audio recorder and recognizer part
// **********************************

// Start audio recorder
void startRecognize () {
    recorder.start (AIO_AUDIO_IN_SPEEX);
    Serial.printf ("Start recording\n");

    timeRecorderStart = millis();
    timeRecorderEnd=0;
    wasVAD = false;
}

// Recording audio process:
// - fill buffer
// - check VAD
// - send to google speech recognition, when VAD ready
// - parse recognized answer and do work
void processRecognize () {
    if (!timeRecorderStart) {
        return;
    }

    bool res = recorder.run ();
    bool vad = recorder.checkVad();

    if (vad && !wasVAD) {
        Serial.printf("VAD: speech started\n");
    }

    wasVAD = wasVAD || vad;

    if (millis () - timeRecorderStart < 3000 || vad)
        timeRecorderEnd = millis ();

    if (res && (!timeRecorderEnd || millis () - timeRecorderEnd < 500))
        return;

    recorder.stop();
    timeRecorderStart = 0;
    if (!wasVAD) {
        return;
    }

    HTTPClient http;

    http.begin(url);
    http.addHeader ("Content-Type","audio/x-speex-with-header-byte; rate=8000");
    int httpCode = http.sendRequest ("POST",&recorder,recorder.recordedSize());

    if(httpCode > 0) {
        Serial.printf("[HTTP] POST... code: %d\n", httpCode);
        String payload = http.getString();
        Serial.println(payload);

        String cmd = "toggle";
        if (payload.indexOf ("выклю")>=0 || payload.indexOf ("погас")>=0)
            cmd = "off";
        else if (payload.indexOf ("вклю")>=0 || payload.indexOf ("зажг")>=0)
            cmd = "on";

        if (payload.indexOf ("музык")>=0) startPlay();
        else if (payload.indexOf ("все")>=0) controlAllRelay (cmd); else {
            if (payload.indexOf ("шарики")>=0) controlRelay (0,cmd);
            if (payload.indexOf ("свечки")>=0) controlRelay (1,cmd);
            if (payload.indexOf ("мишки")>=0|| payload.indexOf ("виски")>=0) controlRelay (2,cmd);
            if (payload.indexOf ("огоньки")>=0) controlRelay (3,cmd);
        }
    }
    http.end();
}

// Cancel recognize:
// - stop recorder
// - reset state variables
void cancelRecognize () {
    timeRecorderEnd = 0;
    timeRecorderStart = 0;
    recorder.stop ();
}

// HTTP routes
// ***********

// Control relay
// http://server:ip/control?relay=<0-3>&state=<on|off|toggle>
void handleControl() {
    String relay = server.arg ("relay");
    String state = server.arg ("state");

    if (relay == "all") {
        controlAllRelay (state);
    } else {
        controlRelay (atoi(relay.c_str()),state);
    }

    server.send(200, "text/plain", "ok");
}

// Get relay state
// http://server:ip/state?relay=<0-3>
void handleState() {

    int relay = atoi (server.arg ("relay").c_str());
    const char *state = "unknown";

    if (relay >=0 && relay < numRelays) {
        state = relayStates[relay]?"on":"off";
    }

    server.send(200,"text/plain", state);

}

// Toggle music on/off
// http://server:ip/music
void handleMusic () {
    musicStatus = !musicStatus;
    server.send(200, "text/plain", "ok");
}

void setup() {

    Serial.begin (115200);
    Serial.setDebugOutput(true);

    // Init relay's GPIO
    for (int relay = 0; relay < numRelays; relay++)
        pinMode (relayGpios[relay],OUTPUT);

    // Init GUI
    guiCreate ();

    // Start WIFI connection
    WiFi.begin(ssid, password);
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Add rooutes handlers to HTTP server
    server.on("/state", handleState);
    server.on("/control", handleControl);
    server.on("/music", handleMusic);
    server.onNotFound([](){
        server.send(404, "text/plain", "Not found");
    });
    server.begin();

    // Init MictoSD
    if (!SD.begin(WIIEVA_SD_CS))
       Serial.println("Error init microsd card!");
}

void loop() {
    unsigned long now = millis();

    // Check and handle events
    GEvent* pe = geventEventWait(&glistener, 2);
    if (pe && pe->type == GEVENT_GWIN_BUTTON) {
        cancelRecognize ();
        GEventGWinButton *we = (GEventGWinButton *)pe;
        if (we->gwin == ghButton1) controlRelay (0);
        if (we->gwin == ghButton2) controlRelay (1);
        if (we->gwin == ghButton3) controlRelay (2);
        if (we->gwin == ghButton4) controlRelay (3);
        if (we->gwin == ghButtonAll) controlAllRelay ();
        if (we->gwin == ghButtonVoice) startPlay ();
        if (we->gwin == ghButtonTree) {switchScreen (true); startRecognize();}
        timeLastActivity = now;
    }

    // Check idle time out, and activate screen saver
    if (!screenSaver && now - timeLastActivity > 10000) {
        switchScreen (false);
    }

    // Tick to voice recognizer
    processRecognize ();

    // Check if music was enabled, than start play
    if (musicStatus) {
        startPlay ();
        timeLastActivity = now;
    }

    delay (10);
    // Tick to http server
    server.handleClient();
}

extern "C" void gwinButtonDraw_ImageText(GWidgetObject *gw, void *param) {
    coord_t sy=0;

    const GColorSet* colors = &gw->pstyle->enabled;

    if (gw->g.flags & GBUTTON_FLG_PRESSED) {
        return;
    }

    gdispImage *img = (gdispImage*)param;

    int x = gdispGImageDraw(gw->g.display, img, gw->g.x + (gw->g.width-img->width)/2, gw->g.y+1, gw->g.width, gw->g.height, 0, sy);
    if (strlen (gw->text)) {
        gdispGDrawStringBox(gw->g.display, gw->g.x+1, gw->g.y+gw->g.height-25 , gw->g.width-2, 25, gw->text, gw->g.font, colors->text, justifyCenter);
        gdispGDrawLine(gw->g.display, gw->g.x+gw->g.width-1, gw->g.y, gw->g.x+gw->g.width-1, gw->g.y+gw->g.height-1, HTML2COLOR(0xD0D0D0));
        gdispGDrawLine(gw->g.display, gw->g.x, gw->g.y+gw->g.height-1, gw->g.x+gw->g.width-2, gw->g.y+gw->g.height-1, HTML2COLOR(0xD0D0D0));
    }
}
