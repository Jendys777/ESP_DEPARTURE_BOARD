#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <time.h>
#include <Preferences.h>
#include <WiFiManager.h> 

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// --- HARDWARE & PINS ---
const int LED_PIN = 12;
const int BUTTON_PIN = 4; // Pin pro prepinani zastavek (zapojit proti GND)

Preferences preferences;

// --- STOPS & FILTERS DATA ---
struct StopConfig {
  String name;
  String place;
  String stop;
  String filter;
};
StopConfig stops[5];
int activeStop = 0;
String apiURL = ""; 

// --- BUTTON DEBOUNCE ---
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 300; // 300ms ochrana proti zakmitum

// --- DISPLAY SCHEDULE DATA ---
struct DaySchedule {
  String off1_start;
  String off1_end;
  String off2_start;
  String off2_end;
};
DaySchedule schedules[7]; 
bool masterDisplayOn = true;
bool isDisplayOn = true; 

const char* dayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// --- STREET LAMP MODES ---
enum LampMode { LAMP_OFF, LAMP_ON, LAMP_SIM, LAMP_SIM_NIGHT, LAMP_SIM_DAY };
LampMode currentLampMode = LAMP_SIM;
unsigned long lastFlickerTime = 0;
unsigned long nextFlickerInterval = 50;

WebServer server(80);

struct Departure {
  String line;
  String destination;
  String time;
  int scrollOffset;
  unsigned long pauseStart;
  bool pausing;
};

Departure departures[4] = {
  {"", "", "", 0, 0, true}, {"", "", "", 0, 0, true},
  {"", "", "", 0, 0, true}, {"", "", "", 0, 0, true}
};

String alertText = "";
String tempStr = "-- C";
unsigned long lastFetch = 0;
unsigned long lastWeatherFetch = 0;
const unsigned long fetchInterval = 30000;        
const unsigned long weatherFetchInterval = 600000; 

enum BottomState { STATE_TIME, STATE_DATE, STATE_TEMP, STATE_ALERT_PRE_PAUSE, STATE_ALERT_SCROLL, STATE_ALERT_POST_PAUSE };
BottomState bottomState = STATE_TIME;
int tickerX = 128;
int alertTextWidth = 0;
unsigned long lastBottomStateChange = 0;

unsigned long lastAlertScrollMove = 0;
unsigned long lastDestScrollMove = 0;

const unsigned long displayModeDurationMs = 5000; 
const unsigned long preScrollPauseMs = 1000;
const unsigned long postScrollPauseMs = 1000;
const unsigned long alertScrollStepMs = 30;
const int displayWidth = 128;
const int scrollStepPixels = 2;

// --- TIME PERIOD CHECK ---
bool isTimeInPeriod(String startStr, String endStr, int currentMins) {
  if (startStr == "" || endStr == "") return false;
  
  int sH = startStr.substring(0, 2).toInt();
  int sM = startStr.substring(3, 5).toInt();
  int eH = endStr.substring(0, 2).toInt();
  int eM = endStr.substring(3, 5).toInt();

  int startMins = sH * 60 + sM;
  int endMins = eH * 60 + eM;

  if (startMins <= endMins) {
    return (currentMins >= startMins && currentMins < endMins);
  } else {
    return (currentMins >= startMins || currentMins < endMins);
  }
}

// --- HELPER FUNCTIONS ---
bool matchesFilter(String line, String filterStr) {
  if (filterStr.length() == 0) return false;
  String f = filterStr + ",";
  int start = 0;
  int end = f.indexOf(',');
  while (end != -1) {
    String token = f.substring(start, end);
    token.trim();
    if (token.length() > 0 && token.equalsIgnoreCase(line)) return true;
    start = end + 1;
    end = f.indexOf(',', start);
  }
  return false;
}

void updateApiUrl() {
  apiURL = "http://85.93.119.139/ElpDepartures/Home/UpdateDepartures?place=" + stops[activeStop].place + "&stop=" + stops[activeStop].stop;
}

void updateStreetlamp() {
  struct tm timeinfo;
  bool timeValid = getLocalTime(&timeinfo, 10);
  int currentHour = timeValid ? timeinfo.tm_hour : 12;

  bool isNight = (currentHour >= 20 || currentHour < 6);
  bool active = false;
  bool simulate = false;

  switch (currentLampMode) {
    case LAMP_OFF: active = false; break;
    case LAMP_ON: active = true; simulate = false; break;
    case LAMP_SIM: active = true; simulate = true; break;
    case LAMP_SIM_NIGHT: active = isNight; simulate = isNight; break;
    case LAMP_SIM_DAY: active = !isNight; simulate = !isNight; break;
  }

  if (!active) {
    digitalWrite(LED_PIN, LOW);
    return;
  }
  if (!simulate) {
    digitalWrite(LED_PIN, HIGH);
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastFlickerTime >= nextFlickerInterval) {
    lastFlickerTime = currentMillis;
    int r = random(0, 100);
    if (r < 65) {
      digitalWrite(LED_PIN, HIGH);
      nextFlickerInterval = random(200, 1200);
    } else if (r < 85) {
      digitalWrite(LED_PIN, random(0, 2) ? HIGH : LOW);
      nextFlickerInterval = random(15, 60);
    } else {
      digitalWrite(LED_PIN, LOW);
      nextFlickerInterval = random(80, 350);
    }
  }
}

String decodeHtmlEntities(String str) {
  str.replace("\r", " "); str.replace("\n", " ");
  str.replace("<br>", " | "); str.replace("<br/>", " | "); str.replace("<br />", " | ");
  str.replace("&#225;", "a"); str.replace("&#193;", "A"); str.replace("&#233;", "e"); str.replace("&#201;", "E"); str.replace("&#283;", "e"); str.replace("&#282;", "E");
  str.replace("&#237;", "i"); str.replace("&#205;", "I"); str.replace("&#243;", "o"); str.replace("&#211;", "O");
  str.replace("&#250;", "u"); str.replace("&#218;", "U"); str.replace("&#367;", "u"); str.replace("&#366;", "U");
  str.replace("&#253;", "y"); str.replace("&#221;", "Y"); str.replace("&#269;", "c"); str.replace("&#268;", "C");
  str.replace("&#271;", "d"); str.replace("&#270;", "D"); str.replace("&#328;", "n"); str.replace("&#327;", "N");
  str.replace("&#345;", "r"); str.replace("&#344;", "R"); str.replace("&#353;", "s"); str.replace("&#352;", "S");
  str.replace("&#357;", "t"); str.replace("&#356;", "T"); str.replace("&#382;", "z"); str.replace("&#381;", "Z");
  str.replace("á", "a"); str.replace("Á", "A"); str.replace("é", "e"); str.replace("É", "E"); str.replace("ě", "e"); str.replace("Ě", "E");
  str.replace("í", "i"); str.replace("Í", "I"); str.replace("ó", "o"); str.replace("Ó", "O");
  str.replace("ú", "u"); str.replace("Ú", "U"); str.replace("ů", "u"); str.replace("Ů", "U");
  str.replace("ý", "y"); str.replace("Ý", "Y"); str.replace("č", "c"); str.replace("Č", "C");
  str.replace("ď", "d"); str.replace("Ď", "D"); str.replace("ň", "n"); str.replace("Ň", "N");
  str.replace("ř", "r"); str.replace("Ř", "R"); str.replace("š", "s"); str.replace("Š", "S");
  str.replace("ť", "t"); str.replace("Ť", "T"); str.replace("ž", "z"); str.replace("Ž", "Z");
  str.replace("&nbsp;", " "); str.replace("&gt;", ">"); str.replace("&lt;", "<"); str.replace("&amp;", "&"); str.replace("&quot;", "\"");
  while (str.indexOf("  ") != -1) str.replace("  ", " ");
  return str;
}

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.setTimeout(1500); 
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin("http://api.open-meteo.com/v1/forecast?latitude=49.1951&longitude=16.6078&current=temperature_2m");
  
  if (http.GET() == 200) {
    String payload = http.getString();
    int currentSection = payload.indexOf("\"current\":");
    if (currentSection != -1) {
      int idx = payload.indexOf("\"temperature_2m\":", currentSection);
      if (idx != -1) {
        idx += 17;
        int endIdx = payload.indexOf(",", idx);
        if (endIdx == -1) endIdx = payload.indexOf("}", idx);
        if (endIdx != -1) {
          String t = payload.substring(idx, endIdx);
          t.trim();
          tempStr = String(t.toFloat(), 1) + " C";
        }
      }
    }
  }
  http.end();
}

void fetchLiveDepartures() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.setTimeout(2000); 
  http.begin(apiURL);
  
  if (http.GET() == 200) {
    String payload = http.getString();

    struct TempDep { String line; String dest; String time; int origIndex; };
    TempDep parsed[12];
    int parsedCount = 0;

    int index = 0;
    while (parsedCount < 12) {
      String lineTag = "width:40px; vertical-align:top\">";
      index = payload.indexOf(lineTag, index);
      if (index == -1) break;
      index += lineTag.length();
      int endIndex = payload.indexOf("</div>", index);
      String lineStr = payload.substring(index, endIndex);
      lineStr.trim();

      String destTag = "class=\"stationNameBox\"";
      index = payload.indexOf(destTag, endIndex);
      if (index == -1) break;
      index = payload.indexOf(">", index) + 1;
      endIndex = payload.indexOf("</div>", index);
      String destStr = decodeHtmlEntities(payload.substring(index, endIndex));
      destStr.trim();

      String timeTag = "right:25px; display:inline-block\">";
      index = payload.indexOf(timeTag, endIndex);
      if (index == -1) break;
      index += timeTag.length();
      endIndex = payload.indexOf("</div>", index);
      String timeStr = payload.substring(index, endIndex);
      timeStr.trim();
      timeStr.replace("min", "m");
      if (timeStr.indexOf(":") == -1 && !timeStr.endsWith("m")) timeStr += "m";

      parsed[parsedCount] = {lineStr, destStr, timeStr, parsedCount};
      parsedCount++;
    }

    TempDep selected[4];
    int selCount = 0;
    String currentFilter = stops[activeStop].filter;

    if (currentFilter.length() > 0) {
      for (int i = 0; i < parsedCount && selCount < 4; i++) {
        if (matchesFilter(parsed[i].line, currentFilter)) {
          selected[selCount] = parsed[i];
          selected[selCount].origIndex = i; 
          parsed[i].origIndex = -1; 
          selCount++;
        }
      }
    }

    for (int i = 0; i < parsedCount && selCount < 4; i++) {
      if (parsed[i].origIndex != -1) {
        selected[selCount] = parsed[i];
        selected[selCount].origIndex = i;
        parsed[i].origIndex = -1;
        selCount++;
      }
    }

    for (int i = 0; i < selCount - 1; i++) {
      for (int j = 0; j < selCount - i - 1; j++) {
        if (selected[j].origIndex > selected[j + 1].origIndex) {
          TempDep temp = selected[j];
          selected[j] = selected[j + 1];
          selected[j + 1] = temp;
        }
      }
    }

    for (int i = 0; i < 4; i++) departures[i] = {"", "", "", 0, millis(), true};
    for (int i = 0; i < selCount; i++) {
      departures[i] = {selected[i].line, selected[i].dest, selected[i].time, 0, millis(), true};
    }

    int infoIndex = payload.indexOf("<div id=\"infoText\">");
    if (infoIndex != -1) {
      infoIndex += 19;
      String rawInfo = payload.substring(infoIndex, payload.indexOf("</div>", infoIndex));
      rawInfo.replace("<b>Dodatkové informace: </b>", "");
      rawInfo = decodeHtmlEntities(rawInfo);
      rawInfo.trim();
      alertText = (rawInfo.length() > 3) ? rawInfo : "";
    } else {
      alertText = "";
    }
  }
  http.end();
}

void configModeCallback(WiFiManager *myWiFiManager) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_profont10_tr);
  u8g2.drawStr(5, 15, "Please proceed to");
  u8g2.drawStr(5, 30, "setup on WiFi AP:");
  u8g2.drawStr(5, 50, myWiFiManager->getConfigPortalSSID().c_str());
  u8g2.sendBuffer();
}

// --- WEB INTERFACE ---
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: monospace; background: #111; color: #eee; margin: 20px auto; max-width: 650px; }";
  html += "fieldset { border: 1px solid #444; margin-bottom: 20px; padding: 15px; }";
  html += "legend { font-weight: bold; padding: 0 5px; color: #fff; }";
  html += "input, select, button { width: 100%; padding: 8px; margin: 4px 0 12px 0; box-sizing: border-box; background: #222; color: #eee; border: 1px solid #555; font-family: monospace; }";
  html += "button { background: #333; cursor: pointer; font-weight: bold; }";
  html += "button:hover { background: #444; }";
  html += ".btn-group { display: flex; gap: 5px; }";
  html += ".btn-group a { flex: 1; text-align: center; text-decoration: none; background: #222; color: #eee; border: 1px solid #555; padding: 8px; }";
  html += ".btn-group a.active { background: #eee; color: #111; font-weight: bold; }";
  html += ".slot-row { display: flex; gap: 8px; flex-wrap: wrap; margin-bottom: 10px; border-bottom: 1px dashed #333; padding-bottom: 5px; }";
  html += ".slot-row > div { flex: 1; min-width: 60px; }";
  html += "label { font-size: 0.9em; color: #aaa; }";
  html += "table { width: 100%; border-collapse: collapse; margin-top: 10px; }";
  html += "th, td { padding: 4px; text-align: left; font-size: 0.9em; border-bottom: 1px solid #333;}";
  html += "a { color: #88ccff; text-decoration: none; } a:hover { text-decoration: underline; }";
  html += "</style></head><body>";
  
  html += "<h2>ESP32 Departure Board IDSJMK edition</h2>";
  
  html += "<fieldset><legend>Displayed Stop</legend>";
  html += "<form action='/setactive' method='POST'>";
  html += "<select name='active'>";
  for(int i = 0; i < 5; i++) {
    html += "<option value='" + String(i) + "'" + (activeStop == i ? " selected" : "") + ">";
    html += stops[i].name + " (Place: " + stops[i].place + " | Stop: " + stops[i].stop + ")</option>";
  }
  html += "</select>";
  html += "<button type='submit'>Switch Stop</button>";
  html += "</form></fieldset>";

  html += "<fieldset><legend>Saved Stops (Slots 1-5)</legend>";
  html += "<form action='/savestops' method='POST'>";
  for(int i = 0; i < 5; i++) {
    html += "<label>Slot " + String(i + 1) + "</label>";
    html += "<div class='slot-row'>";
    html += "<div style='flex: 1.5;'><input type='text' name='n" + String(i) + "' value='" + stops[i].name + "' placeholder='Name'></div>";
    html += "<div><input type='text' name='p" + String(i) + "' value='" + stops[i].place + "' placeholder='Place'></div>";
    html += "<div><input type='text' name='s" + String(i) + "' value='" + stops[i].stop + "' placeholder='Stop'></div>";
    html += "<div style='flex: 1.5;'><input type='text' name='f" + String(i) + "' value='" + stops[i].filter + "' placeholder='Lines (e.g. 6, P6)'></div>";
    html += "</div>";
  }
  html += "<button type='submit'>Save Stops</button>";
  html += "</form></fieldset>";

  html += "<fieldset><legend>Display Sleep Schedule</legend>";
  html += "<form action='/saveschedule' method='POST'>";
  html += "<label><input type='checkbox' name='master' value='1' " + String(masterDisplayOn ? "checked" : "") + "> Master Mode (Ignore schedule, always ON)</label>";
  html += "<table><tr><th>Day</th><th>Night FROM</th><th>Night TO</th><th>Day FROM</th><th>Day TO</th></tr>";
  for(int i = 0; i < 7; i++) {
    html += "<tr><td>" + String(dayNames[i]) + "</td>";
    html += "<td><input type='time' name='d" + String(i) + "_s1' value='" + schedules[i].off1_start + "'></td>";
    html += "<td><input type='time' name='d" + String(i) + "_e1' value='" + schedules[i].off1_end + "'></td>";
    html += "<td><input type='time' name='d" + String(i) + "_s2' value='" + schedules[i].off2_start + "'></td>";
    html += "<td><input type='time' name='d" + String(i) + "_e2' value='" + schedules[i].off2_end + "'></td></tr>";
  }
  html += "</table>";
  html += "<button type='submit' style='margin-top: 10px;'>Save Schedule</button>";
  html += "</form></fieldset>";

  html += "<fieldset><legend>Street Lamp (GPIO 12)</legend>";
  html += "<p>Current mode: <b>";
  switch (currentLampMode) {
    case LAMP_OFF: html += "OFF"; break;
    case LAMP_ON: html += "ON (Always On)"; break;
    case LAMP_SIM: html += "SIM (Constant Simulation)"; break;
    case LAMP_SIM_NIGHT: html += "SIM + TIME (Night only)"; break;
    case LAMP_SIM_DAY: html += "SIM - TIME (Day only)"; break;
  }
  html += "</b></p><div class='btn-group'>";
  html += "<a href='/led?mode=on' class='" + String(currentLampMode == LAMP_ON ? "active" : "") + "'>ON</a>";
  html += "<a href='/led?mode=sim' class='" + String(currentLampMode == LAMP_SIM ? "active" : "") + "'>SIM</a>";
  html += "<a href='/led?mode=sim_night' class='" + String(currentLampMode == LAMP_SIM_NIGHT ? "active" : "") + "'>SIM+TIME</a>";
  html += "<a href='/led?mode=sim_day' class='" + String(currentLampMode == LAMP_SIM_DAY ? "active" : "") + "'>SIM-TIME</a>";
  html += "<a href='/led?mode=off' class='" + String(currentLampMode == LAMP_OFF ? "active" : "") + "'>OFF</a>";
  html += "</div></fieldset>";

  html += "<fieldset><legend>System</legend>";
  html += "<form action='/resetwifi' method='POST' onsubmit='return confirm(\"Are you sure you want to erase Wi-Fi settings and restart the device?\");'>";
  html += "<button type='submit' style='background: #802020; color: #fff; border-color: #aa3030;'>Erase Wi-Fi & Restart</button>";
  html += "</form></fieldset>";

  html += "<div style='margin-top: 30px; border-top: 1px solid #333; padding-top: 15px; text-align: center; font-size: 0.85em; color: #888;'>";
  html += "ESP32 Departure Board by Bojler.<br><br>";
  html += "Be free to use and modify. If you modify it to your public transport system, please consider sharing your improvements, with me and i will add it to the repository.<br><br>";
  html += "Contact: <a href='mailto:info@bojler.org'>info@bojler.org</a> | Web: <a href='https://bojler.org' target='_blank'>bojler.org</a>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleSetActive() {
  if (server.hasArg("active")) {
    int requestedStop = server.arg("active").toInt();
    if (requestedStop >= 0 && requestedStop < 5) {
      activeStop = requestedStop;
      preferences.putInt("actStop", activeStop);
      updateApiUrl();
      fetchLiveDepartures(); 
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSaveStops() {
  for (int i = 0; i < 5; i++) {
    String argN = "n" + String(i); String argP = "p" + String(i);
    String argS = "s" + String(i); String argF = "f" + String(i);
    
    if (server.hasArg(argN)) stops[i].name = server.arg(argN);
    if (server.hasArg(argP)) stops[i].place = server.arg(argP);
    if (server.hasArg(argS)) stops[i].stop = server.arg(argS);
    if (server.hasArg(argF)) stops[i].filter = server.arg(argF);

    preferences.putString(argN.c_str(), stops[i].name);
    preferences.putString(argP.c_str(), stops[i].place);
    preferences.putString(argS.c_str(), stops[i].stop);
    preferences.putString(argF.c_str(), stops[i].filter);
  }
  updateApiUrl();
  fetchLiveDepartures(); 
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSaveSchedule() {
  masterDisplayOn = server.hasArg("master");
  preferences.putBool("masterDisp", masterDisplayOn);

  for (int i = 0; i < 7; i++) {
    String p1s = "d" + String(i) + "_s1"; String p1e = "d" + String(i) + "_e1";
    String p2s = "d" + String(i) + "_s2"; String p2e = "d" + String(i) + "_e2";

    if (server.hasArg(p1s)) schedules[i].off1_start = server.arg(p1s);
    if (server.hasArg(p1e)) schedules[i].off1_end = server.arg(p1e);
    if (server.hasArg(p2s)) schedules[i].off2_start = server.arg(p2s);
    if (server.hasArg(p2e)) schedules[i].off2_end = server.arg(p2e);

    preferences.putString(p1s.c_str(), schedules[i].off1_start);
    preferences.putString(p1e.c_str(), schedules[i].off1_end);
    preferences.putString(p2s.c_str(), schedules[i].off2_start);
    preferences.putString(p2e.c_str(), schedules[i].off2_end);
  }
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLed() {
  if (server.hasArg("mode")) {
    String mode = server.arg("mode");
    if (mode == "off") currentLampMode = LAMP_OFF;
    else if (mode == "on") currentLampMode = LAMP_ON;
    else if (mode == "sim") currentLampMode = LAMP_SIM;
    else if (mode == "sim_night") currentLampMode = LAMP_SIM_NIGHT;
    else if (mode == "sim_day") currentLampMode = LAMP_SIM_DAY;
    
    preferences.putInt("savedLamp", currentLampMode); 
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleResetWifi() {
  server.send(200, "text/plain", "Wi-Fi settings erased. Restarting...");
  delay(1000);
  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
}

void setup() {
  Serial.begin(115200);

  // Inicializace hardware tlacitka (INPUT_PULLUP = logicka 1, po stisku GND logicka 0)
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  preferences.begin("nastaveni", false);
  
  activeStop = preferences.getInt("actStop", 0);
  if (activeStop < 0 || activeStop > 4) activeStop = 0;
  
  for (int i = 0; i < 5; i++) {
    stops[i].name = preferences.getString(("n" + String(i)).c_str(), "Stop " + String(i + 1));
    stops[i].place = preferences.getString(("p" + String(i)).c_str(), "4");
    stops[i].stop = preferences.getString(("s" + String(i)).c_str(), "1286");
    stops[i].filter = preferences.getString(("f" + String(i)).c_str(), "");
  }

  masterDisplayOn = preferences.getBool("masterDisp", true);
  for (int i = 0; i < 7; i++) {
    schedules[i].off1_start = preferences.getString(("d" + String(i) + "_s1").c_str(), "");
    schedules[i].off1_end = preferences.getString(("d" + String(i) + "_e1").c_str(), "");
    schedules[i].off2_start = preferences.getString(("d" + String(i) + "_s2").c_str(), "");
    schedules[i].off2_end = preferences.getString(("d" + String(i) + "_e2").c_str(), "");
  }
  
  updateApiUrl();
  currentLampMode = (LampMode)preferences.getInt("savedLamp", LAMP_SIM);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_profont10_tr);
  u8g2.drawStr(5, 25, "Starting...");
  u8g2.drawStr(5, 45, "Connecting Wi-Fi...");
  u8g2.sendBuffer();

  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm); 

  WiFiManager wm;
  wm.setAPCallback(configModeCallback);
  
  if (!wm.autoConnect("ESP32-Departure-Board")) {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
  }

  u8g2.clearBuffer();
  u8g2.drawStr(5, 25, "WiFi Connected!");
  u8g2.setCursor(5, 45);
  u8g2.print(WiFi.localIP().toString());
  u8g2.sendBuffer();
  delay(1500);

  server.on("/", handleRoot);
  server.on("/setactive", HTTP_POST, handleSetActive);
  server.on("/savestops", HTTP_POST, handleSaveStops);
  server.on("/saveschedule", HTTP_POST, handleSaveSchedule);
  server.on("/led", handleLed);
  server.on("/resetwifi", HTTP_POST, handleResetWifi);
  server.begin();

  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");

  fetchLiveDepartures(); 
  fetchWeather();
}

void loop() {
  unsigned long currentMillis = millis();

  // --- HARDWARE BUTTON LOGIC ---
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (currentMillis - lastButtonPress > debounceDelay) {
      lastButtonPress = currentMillis;
      
      // Cykluj zastavky
      activeStop = (activeStop + 1) % 5;
      preferences.putInt("actStop", activeStop);
      updateApiUrl();

      // Zobraz feedback na displeji pred stažením dat
      if (isDisplayOn) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_profont10_tr);
        u8g2.drawStr(5, 30, "Switching to:");
        u8g2.setCursor(5, 45);
        u8g2.print(stops[activeStop].name);
        u8g2.sendBuffer();
      }

      fetchLiveDepartures();
    }
  }

  server.handleClient();
  updateStreetlamp();

  struct tm timeinfo;
  bool timeValid = getLocalTime(&timeinfo, 0);

  // --- DISPLAY SCHEDULE LOGIC ---
  bool shouldBeOn = true;

  if (!masterDisplayOn && timeValid) {
    int wday = timeinfo.tm_wday; 
    int currentMins = timeinfo.tm_hour * 60 + timeinfo.tm_min;

    bool inPeriod1 = isTimeInPeriod(schedules[wday].off1_start, schedules[wday].off1_end, currentMins);
    bool inPeriod2 = isTimeInPeriod(schedules[wday].off2_start, schedules[wday].off2_end, currentMins);

    if (inPeriod1 || inPeriod2) {
      shouldBeOn = false;
    }
  }

  if (shouldBeOn != isDisplayOn) {
    isDisplayOn = shouldBeOn;
    u8g2.setPowerSave(isDisplayOn ? 0 : 1); 
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (currentMillis - lastFetch > fetchInterval) {
      lastFetch = currentMillis;
      fetchLiveDepartures();
    }
    if (currentMillis - lastWeatherFetch > weatherFetchInterval) {
      lastWeatherFetch = currentMillis;
      fetchWeather();
    }
  }

  // --- RENDERING (ONLY IF DISPLAY IS ON) ---
  if (isDisplayOn) {
    u8g2.setFont(u8g2_font_profont10_tr);
    
    bool advanceDestScroll = false;
    if (currentMillis - lastDestScrollMove >= 45) {
      lastDestScrollMove = currentMillis;
      advanceDestScroll = true;
    }

    for (int i = 0; i < 4; i++) {
      if (departures[i].line == "") continue;

      String tStr = departures[i].time;
      int tWidth = u8g2.getStrWidth(tStr.c_str());
      int timeX = 128 - tWidth;
      int maxDestWidth = timeX - 24;

      int textWidth = u8g2.getStrWidth(departures[i].destination.c_str());

      if (textWidth > maxDestWidth) {
        if (departures[i].pausing) {
          if (currentMillis - departures[i].pauseStart > 2000) {
            departures[i].pausing = false;
          }
        } else {
          if (advanceDestScroll) {
            departures[i].scrollOffset += 1;
            if (departures[i].scrollOffset > textWidth + 12) {
              departures[i].scrollOffset = 0;
              departures[i].pausing = true;
              departures[i].pauseStart = currentMillis;
            }
          }
        }
      } else {
        departures[i].scrollOffset = 0;
      }
    }

    switch (bottomState) {
      case STATE_TIME:
        if (currentMillis - lastBottomStateChange >= displayModeDurationMs) {
          bottomState = STATE_DATE; lastBottomStateChange = currentMillis;
        }
        break;
      case STATE_DATE:
        if (currentMillis - lastBottomStateChange >= displayModeDurationMs) {
          bottomState = STATE_TEMP; lastBottomStateChange = currentMillis;
        }
        break;
      case STATE_TEMP:
        if (currentMillis - lastBottomStateChange >= displayModeDurationMs) {
          bottomState = (alertText.length() > 0) ? STATE_ALERT_PRE_PAUSE : STATE_TIME;
          lastBottomStateChange = currentMillis;
        }
        break;
      case STATE_ALERT_PRE_PAUSE:
        if (currentMillis - lastBottomStateChange >= preScrollPauseMs) {
          bottomState = STATE_ALERT_SCROLL; lastBottomStateChange = currentMillis;
          lastAlertScrollMove = currentMillis; tickerX = displayWidth;
          u8g2.setFont(u8g2_font_profont10_tr); alertTextWidth = u8g2.getStrWidth(alertText.c_str());
        }
        break;
      case STATE_ALERT_SCROLL:
        if (currentMillis - lastAlertScrollMove >= alertScrollStepMs) {
          lastAlertScrollMove = currentMillis; tickerX -= scrollStepPixels;
          if (tickerX < -alertTextWidth - 15) {
            bottomState = STATE_ALERT_POST_PAUSE; lastBottomStateChange = currentMillis;
          }
        }
        break;
      case STATE_ALERT_POST_PAUSE:
        if (currentMillis - lastBottomStateChange >= postScrollPauseMs) {
          bottomState = STATE_TIME; lastBottomStateChange = currentMillis;
        }
        break;
    }

    u8g2.clearBuffer();

    int y = 10;
    for (int i = 0; i < 4; i++) {
      if (departures[i].line == "") continue;

      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.setCursor(0, y);
      u8g2.print(departures[i].line);

      String tStr = departures[i].time;
      u8g2.setFont(u8g2_font_profont10_tr);
      int tWidth = u8g2.getStrWidth(tStr.c_str());
      int timeX = 128 - tWidth;

      u8g2.setClipWindow(22, y - 9, timeX - 4, y + 3);
      u8g2.setCursor(22 - departures[i].scrollOffset, y);
      u8g2.print(departures[i].destination);

      int textWidth = u8g2.getStrWidth(departures[i].destination.c_str());
      if (textWidth > (timeX - 24) && departures[i].scrollOffset > 0) {
        u8g2.setCursor(22 - departures[i].scrollOffset + textWidth + 15, y);
        u8g2.print(departures[i].destination);
      }
      u8g2.setClipWindow(0, 0, 128, 64);

      u8g2.setCursor(timeX, y);
      u8g2.print(tStr);
      y += 14; 
    }

    u8g2.drawHLine(0, 54, 128); 

    struct tm timeinfo;
    bool timeValid = getLocalTime(&timeinfo, 10);
    u8g2.setFont(u8g2_font_profont10_tr);

    if (bottomState == STATE_ALERT_SCROLL || bottomState == STATE_ALERT_PRE_PAUSE || bottomState == STATE_ALERT_POST_PAUSE) {
      u8g2.setCursor(tickerX, 63);
      u8g2.print(alertText);
    } else if (bottomState == STATE_TIME) {
      if (timeValid) {
        char timeBuf[12]; strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &timeinfo);
        u8g2.setCursor(128 - u8g2.getStrWidth(timeBuf), 63); u8g2.print(timeBuf);
      } else {
        u8g2.setCursor(80, 63); u8g2.print("--:--:--");
      }
    } else if (bottomState == STATE_DATE) {
      if (timeValid) {
        char dateBuf[12]; strftime(dateBuf, sizeof(dateBuf), "%d.%m.%Y", &timeinfo);
        u8g2.setCursor(128 - u8g2.getStrWidth(dateBuf), 63); u8g2.print(dateBuf);
      } else {
        u8g2.setCursor(70, 63); u8g2.print("--.--.----");
      }
    } else if (bottomState == STATE_TEMP) {
      u8g2.setCursor(128 - u8g2.getStrWidth(tempStr.c_str()), 63);
      u8g2.print(tempStr);
    }

    u8g2.sendBuffer();
  }
}