#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_ILI9341esp.h>
#include <Adafruit_GFX.h>
#include <XPT2046.h>

/*
// ARRIANA NWODU, INC.
// LATEST DATA CODE
// CREATED ON: 9.26.16
// CODE CREATED BY: ON BEHALF OF ARRIANA NWODU
// LAST UPDATED ON: 12.11.2016
// TODO LIST:
//  1) "Use Case" Optimize DNS SERVER / CaptivePortalAdvanced Library Example (https://github.com/esp8266/Arduino/blob/master/libraries/DNSServer/examples/CaptivePortalAdvanced/CaptivePortalAdvanced.ino)
//  2) Make WiFi Portal more "friendly" for naive users
//  3) Make error message occur if the user inputs a bad SSID / PASS
//  4) TERRIBLE, TERRIBLE THINGS HAPPEN IF MORE THAN ONE PERSON TRIES TO CONNECT TO THE SAME ACCESS POINT (AP)
*/


/* * * * * * * * * * * * * * * * Adafruit_GFX Vars * * * * * * * * * * * * * * * */
// Define DC & Chipselect (CS) Pins
#define TFT_DC 2
#define TFT_CS 15

// Define for XPT2046 Driver the ILI9341's Chipselect and DC Pins
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
XPT2046 touch(/*cs=*/ 4, /*irq=*/ 5);

// Define On-Screen Buttons
Adafruit_GFX_Button btn1;
Adafruit_GFX_Button btn2;

Adafruit_GFX_Button backbtn;
boolean back = false;

/* * * * * * * * * * * * * * * * BEGIN WIFI / AP CODE * * * * * * * * * * * * * * * * */
/* Set these to your desired softAP credentials. They are not configurable at runtime */
const char *softAP_ssid = "Hello, alphaData";
const char *softAP_password = "alphadata";

/* hostname for mDNS. Should work at least on windows. Try http://alphadata.local */
const char *myHostname = "alphadata";

/* Don't set this wifi credentials. They are configurated at runtime and stored on EEPROM */
char ssid[32] = "";
char password[32] = "";

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Web server
ESP8266WebServer server(80);

/* Soft AP network parameters */
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);


/** Should I connect to WLAN asap? */
boolean connect;

/** Last time I tried to connect to WLAN */
long lastConnectTry = 0;

/** Current WLAN status */
int status = WL_IDLE_STATUS;

/** Is this an IP? */
boolean isIp(String str) {
  for (int i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

/** IP to String? */
String toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

/** Handle root or redirect to captive portal */
void handleRoot() {
  if (captivePortal()) { // If captive portal redirect instead of displaying the page.
    return;
  }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.sendContent(
    "<html><head></head><body>"
    "<h1>ESP8266 Prototype - alphaData</h1>"
  );
  if (server.client().localIP() == apIP) {
    server.sendContent(String("<p>You are connected directly to the following device: <b>") + softAP_ssid + "</b></p>");
  } else {
    server.sendContent(String("<p>Your device is on a shared network with ") + softAP_ssid + String("through the wifi network: ") + ssid + "</p>");
  }
  server.sendContent(
    "<p>Click here to <a href='/wifi'>configure the wifi connection</a>.</p>"
    "<p><h3>First Time Here?</h3></p>"
    "<p>Click the link above to configure the wifi connection. Input your wireless networks username / password and allow the device time to connect. After a few minutes, your screen should turn on and you can begin collecting data.</p>"
    "</body></html>"
  );
  server.client().stop(); // Stop is needed because we sent no content length
}

/** Redirect to captive portal if we got a request for another domain. Return true in that case so the page handler do not try to handle the request again. */
boolean captivePortal() {
  if (!isIp(server.hostHeader()) && server.hostHeader() != (String(myHostname)+".local")) {
    Serial.print("Request redirected to captive portal");
    server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
    server.send ( 302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

/** Wifi config page handler */
void handleWifi() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.sendContent(
    "<html><head></head><body>"
    "<h1>Wifi config</h1>"
  );
  if (server.client().localIP() == apIP) {
    server.sendContent(String("<p>You are directly connected to this device via this WiFi network: ") + softAP_ssid + "</p>");
  } else {
    server.sendContent(String("<p>You are connected to this device through the wifi network: ") + ssid + "</p>");
  }
  server.sendContent(
    "\r\n<br />"
    "<table><tr><th align='left'>This Device broadcasts:</th></tr>"
  );
  server.sendContent(String() + "<tr><td>Network Name: " + String(softAP_ssid) + "</td></tr>");
  server.sendContent(String() + "<tr><td>IP " + toStringIp(WiFi.softAPIP()) + "</td></tr>");
  server.sendContent(
    "</table>"
    "\r\n<br />"
    "<table><tr><th align='left'>This device is currently connected to:</th></tr>"
  );
  if (toStringIp(WiFi.localIP()) == "0.0.0.0" ) {
    server.sendContent(String() + "<tr><td><font color='red'><b>FAILED TO CONNECT TO NETWORK!</b></font> Please check <u>network name</u> and <u>password</u></td></tr>");
    tft.fillScreen(ILI9341_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(ILI9341_YELLOW);  
    tft.setTextSize(3);
    tft.println("     Error");
    tft.println("");
    tft.println("Couldn't connect the to network");
    tft.println("");
    tft.println("Please try again");
  } else {  
    server.sendContent(String() + "<tr><td>Network Name: " + String(ssid) + "</td></tr>");
    server.sendContent(String() + "<tr><td>IP " + toStringIp(WiFi.localIP()) + "</b></td></tr>");
    tft.fillScreen(ILI9341_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(ILI9341_WHITE);  
    tft.setTextSize(2);
    tft.println("");
    tft.println("");
    tft.println("Successfully connected to: ");
    tft.println("");
    tft.println(toStringIp(WiFi.localIP()));
    btn1.initButton(&tft, 0, 0, 0, 0, ILI9341_WHITE, ILI9341_BLUE, ILI9341_GREENYELLOW, "", 2);
    btn1.drawButton();
    backbtn.initButton(&tft, 40, 150, 70, 40, ILI9341_WHITE, ILI9341_RED, ILI9341_GREENYELLOW, "Back", 2);
    backbtn.drawButton(); // draw normal
    back = true;
  }
  server.sendContent(
    "</table>"
    "\r\n<br />"
    "<table><tr><th align='left'><u>Available Networks (please refresh to update this list)</u></th></tr>"
  );
  Serial.println("scan start");
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      server.sendContent(String() + "\r\n<tr><td>Network Name: <b>" + WiFi.SSID(i) + String((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"</b><i> This network is password protected</i>") + "</b> (WiFi Signal Strength: " + WiFi.RSSI(i) + ")</td></tr>");
    }
  } else {
    server.sendContent(String() + "<tr><td>No WLAN found</td></tr>");
  }
  server.sendContent(
    "</table>"
    "\r\n<br /><form method='POST' action='wifisave'><h4>Connect to network:</h4>"
    "<input type='text' placeholder='network' name='n'/>"
    "<br /><input type='password' placeholder='password' name='p'/>"
    "<br /><input type='submit' value='Connect/Disconnect'/></form>"
    "<p>You may want to <a href='/'>return to the home page</a>.</p>"
    "</body></html>"
  );
  server.client().stop(); // Stop is needed because we sent no content length
}

/** Handle the WLAN save form and redirect to WLAN config page again */
void handleWifiSave() {
  Serial.println("wifi save");
  server.arg("n").toCharArray(ssid, sizeof(ssid) - 1);
  server.arg("p").toCharArray(password, sizeof(password) - 1);
  server.sendHeader("Location", "wifi", true);
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send ( 302, "text/plain", "");  // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.client().stop(); // Stop is needed because we sent no content length
  saveCredentials();
  connect = strlen(ssid) > 0; // Request WLAN connect with new credentials if there is a SSID
}

void handleNotFound() {
  if (captivePortal()) { // If caprive portal redirect instead of displaying the error page.
    return;
  }
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send ( 404, "text/plain", message );
}

/** Load WLAN credentials from EEPROM */
void loadCredentials() {
  EEPROM.begin(512);
  EEPROM.get(0, ssid);
  EEPROM.get(0+sizeof(ssid), password);
  char ok[2+1];
  EEPROM.get(0+sizeof(ssid)+sizeof(password), ok);
  EEPROM.end();
  if (String(ok) != String("OK")) {
    ssid[0] = 0;
    password[0] = 0;
  }
  Serial.println("Recovered credentials:");
  Serial.println(ssid);
  Serial.println(strlen(password)>0?"********":"<no password>");
}

/** Store WLAN credentials to EEPROM */
void saveCredentials() {
  EEPROM.begin(512);
  EEPROM.put(0, ssid);
  EEPROM.put(0+sizeof(ssid), password);
  char ok[2+1] = "OK";
  EEPROM.put(0+sizeof(ssid)+sizeof(password), ok);
  EEPROM.commit();
  EEPROM.end();
}

void setup() {
  delay(1000);
  Serial.begin(74880);
  // Initialize Display
  tft.begin();
  touch.begin(tft.width(), tft.height());  // Must be done before setting rotation
  //touch.setCalibration(209, 1759, 1775, 273);
  touch.setCalibration(1759,1775,209,273);
  tft.setRotation(3);
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println("Connecting...");
  tft.println("");
  tft.setTextSize(2);
  // default screen
  tft.println("Not loading?");
  tft.println("");
  tft.println("To configure this device:");
  tft.println("");
  tft.print("1) Connect WiFi to this device: ");
  //http://alphadata.local/wifi
  tft.setTextColor(ILI9341_PINK);
  tft.println(String(softAP_ssid));
  tft.setTextColor(ILI9341_WHITE);  
  tft.println("");
  tft.println("2) Go to:");
  tft.setTextColor(ILI9341_GREEN);  
  tft.println("http://alphadata.local/wifi");
  tft.println("");
  tft.setTextColor(ILI9341_WHITE);  
  tft.println("3) Input your WiFi login information");
  
  Serial.println();
  Serial.print("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(softAP_ssid, softAP_password);
  delay(500); // Without delay I've seen the IP address blank
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  /* Setup the DNS server redirecting all the domains to the apIP */  
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  /* Setup web pages: root, wifi config pages, SO captive portal detectors and not found. */
  server.on("/", handleRoot);
  server.on("/wifi", handleWifi);
  server.on("/wifisave", handleWifiSave);
  server.on("/generate_204", handleRoot);  //Android captive portal. Maybe not needed. Might be handled by notFound handler.
  server.on("/fwlink", handleRoot);  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
  server.onNotFound ( handleNotFound );
  server.begin(); // Web server start
  Serial.println("HTTP server started");
  loadCredentials(); // Load WLAN credentials from network
  connect = strlen(ssid) > 0; // Request WLAN connect if there is a SSID

  /* Configure On-Screen Buttons */ 
  btn1.initButton(&tft, 40, 50, 70, 40, ILI9341_WHITE, ILI9341_BLUE, ILI9341_GREENYELLOW, "SIB", 2);
  backbtn.initButton(&tft, 40, 50, 70, 40, ILI9341_WHITE, ILI9341_RED, ILI9341_GREENYELLOW, "Back", 2);
}

void connectWifi() {
  Serial.println("Connecting as wifi client...");
  WiFi.disconnect();
  WiFi.begin ( ssid, password );
  int connRes = WiFi.waitForConnectResult();
  Serial.print ( "connRes: " );
  Serial.println ( connRes );
}

static uint16_t prev_x = 0xffff, prev_y = 0xffff;

void loop() {

  uint16_t x, y;
  if (touch.isTouching()) {
    touch.getPosition(x, y);
    //Serial.print("x ="); Serial.print(x); Serial.print(" y ="); Serial.println(y);
    prev_x = x;
    prev_y = y;
  } else {
    prev_x = prev_y = 0xffff;
  }

  // event listeners
  btn1.press(btn1.contains(x, y)); // tell the button it is pressed
  backbtn.press(backbtn.contains(x, y)); // tell the button it is pressed

  // now we can ask the buttons if their state has changed
  if (btn1.justReleased()) {
    btn1.drawButton(); // draw normal
  }

  if (btn1.justPressed()) {
    btn1.drawButton(true); // draw invert!
    delay(20);
    btn1.drawButton(); // in case justReleased doesn't trigger correctly
    //getPage1();
  }

  if (backbtn.justPressed() && back == true) {
    if (WiFi.status() == WL_CONNECTED) {
      tft.fillScreen(ILI9341_BLACK);
      tft.setCursor(0, 0);
      tft.setTextColor(ILI9341_WHITE);  
      tft.setTextSize(2);
      tft.println("Connected to " + toStringIp(WiFi.localIP()));
      // Display Data Collection UI
      delay(250);
      btn1.initButton(&tft, 40, 50, 70, 40, ILI9341_WHITE, ILI9341_BLUE, ILI9341_GREENYELLOW, "SIB", 2);
      btn1.drawButton();
      backbtn.initButton(&tft, 40, 50, 70, 40, ILI9341_WHITE, ILI9341_RED, ILI9341_GREENYELLOW, "Back", 2);
      backbtn.drawButton();
    }
  }
  
  if (connect) {
    Serial.println ( "Connect requested" );
    connect = false;
    connectWifi();
    lastConnectTry = millis();
  }
  {
    int s = WiFi.status();
    if (s == 0 && millis() > (lastConnectTry + 60000) ) {
      /* If WLAN disconnected and idle try to connect */
      /* Don't set retry time too low as retry interfere the softAP operation */
      connect = true;
    }
    if (status != s) { // WLAN status change
      Serial.print ( "Status: " );
      Serial.println ( s );
      status = s;
      if (s == WL_CONNECTED) {
        /* Just connected to WLAN */
        tft.fillScreen(ILI9341_BLACK);
        tft.setCursor(0, 0);
        tft.setTextColor(ILI9341_WHITE);  
        tft.setTextSize(2);
        tft.println("Connected to " + toStringIp(WiFi.localIP()));
        Serial.println ( "" );
        Serial.print ( "Connected to " );
        Serial.println ( ssid );
        Serial.print ( "IP address: " );
        Serial.println ( WiFi.localIP() );

        // Display Data Collection UI
        delay(250);
        btn1.drawButton();

        // Setup MDNS responder
        if (!MDNS.begin(myHostname)) {
          Serial.println("Error setting up MDNS responder!");
        } else {
          Serial.println("mDNS responder started");
          // Add service to MDNS-SD
          MDNS.addService("http", "tcp", 80);
        }
      } else if (s == WL_NO_SSID_AVAIL) {
        WiFi.disconnect();
      } else {
        // no connection screen
        tft.fillScreen(ILI9341_BLACK);
        tft.setCursor(0, 0);
        tft.setTextColor(ILI9341_RED);  
        tft.setTextSize(2);
        tft.println("Couldn't connect to WiFi");
        tft.println("");
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_WHITE); 
        tft.println("Problems connecting?");
        tft.println("");
        tft.println("To configure this device:");
        tft.println("");
        tft.print("1) Connect WiFi to this device: ");
        //http://alphadata.local/wifi
        tft.setTextColor(ILI9341_PINK);
        tft.println(String(softAP_ssid));
        tft.setTextColor(ILI9341_WHITE);  
        tft.println("");
        tft.println("2) Go to:");
        tft.setTextColor(ILI9341_GREEN);   
        tft.println("http://alphadata.local/wifi");
        tft.println("");
        tft.setTextColor(ILI9341_WHITE);  
        tft.println("3) Input your WiFi login information");
      }
    }
  }


  //DNS
  dnsServer.processNextRequest();
  //HTTP
  server.handleClient();
}

