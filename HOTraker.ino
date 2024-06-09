#include <WiFi.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <WebServer.h>
#include <SPI.h>
#include <HTTPClient.h>
#include <MFRC522.h>
#include "base64.h"

//wifi infos
const char* ssid = "";
const char* password = "";

//gps info
static const int RXPin = 16, TXPin = 17;
static const uint32_t GPSBaud = 9600;

//twilio infos 
const char* account_sid = ""; // Your Twilio account SID
const char* auth_token = ""; // Your Twilio auth token
const char* twilio_number = ""; // Your Twilio phone number
const char* recipient_number = ""; // Recipient phone number

//gps inisialisation
TinyGPSPlus gps;
SoftwareSerial mygps(RXPin, TXPin);
WebServer server(80);

double latitude, longitude;

// Pins for RFID module
#define SS_PIN 5
#define RST_PIN 4
#define MISO_PIN 19
#define MOSI_PIN 23
#define SCK_PIN 18

MFRC522 rfid(SS_PIN, RST_PIN);

// Variable to hold the RFID message
String rfidMessage = "";

// Function declarations
void handleRoot();
void handleGPS();
void handleRFID();
void displayRFIDMessage();

void setup() {
  Serial.begin(115200);
  mygps.begin(GPSBaud);

  // Setup RFID
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN); // Initialize SPI bus with custom pins
  rfid.PCD_Init(); // Initialize MFRC522

  // Connexion au réseau WiFi existant
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connexion au réseau Wi-Fi...");
  }
  Serial.println("Connecté au réseau Wi-Fi !");
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/gps", handleGPS);
  server.on("/rfid", handleRFID);
  server.begin();
}

void loop() {
//twilio message condition
  if (mygps.available() > 0) {
    gps.encode(mygps.read());
    if (gps.location.isUpdated()) {
      double currentLatitude = gps.location.lat();
      double currentLongitude = gps.location.lng();

      // Calculate distance from initial coordinates
      double distance = TinyGPSPlus::distanceBetween(initialLatitude, initialLongitude, currentLatitude, currentLongitude);

      // If the distance is greater than 1 meter, send SMS
      if (distance > 1) {
        sendTwilioMessage(currentLatitude, currentLongitude);
        // Update initial coordinates
        initialLatitude = currentLatitude;
        initialLongitude = currentLongitude;
      }
    }
  }


//gps detection
  if (mygps.available() > 0) {
    gps.encode(mygps.read());
    if (gps.location.isUpdated()) {
      latitude = gps.location.lat();
      longitude = gps.location.lng();
    }
  }

  // Check for RFID card presence
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    // RFID tag detected
    displayRFIDMessage();
    rfid.PICC_HaltA();
  }

  server.handleClient();
}

void handleRoot() {
  String html = "<!DOCTYPE html>\
<html>\
<head>\
  <title>ESP32 GPS Tracker</title>\
  <link rel=\"stylesheet\" href=\"https://unpkg.com/leaflet/dist/leaflet.css\" />\
  <style>\
    #map { height: 100vh; }\
    .marker-popup {\
      background-color: #fff;\
      border-radius: 5px;\
      padding: 10px;\
      box-shadow: 0 2px 5px rgba(0,0,0,0.1);\
      animation: marker-pop 0.5s ease-out;\
    }\
    @keyframes marker-pop {\
      0% { transform: scale(0); }\
      100% { transform: scale(1); }\
    }\
  </style>\
</head>\
<body>\
  <div id=\"map\"></div>\
  <script src=\"https://unpkg.com/leaflet/dist/leaflet.js\"></script>\
  <script>\
    var map = L.map('map').setView([0, 0], 13);\
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {\
      attribution: '&copy; <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap</a> contributors'\
    }).addTo(map);\
    var marker = L.marker([0, 0]).addTo(map);\
    function updateLocation() {\
      fetch('/gps')\
      .then(response => response.json())\
      .then(data => {\
        var latLng = [data.lat, data.lng];\
        marker.setLatLng(latLng);\
        marker.bindPopup(\"<div class='marker-popup'>Latitude: \" + data.lat + \"<br>Longitude: \" + data.lng + \"</div>\").openPopup();\
        map.setView(latLng, 13);\
      });\
      setTimeout(updateLocation, 5000);\
    }\
    updateLocation();\
  </script>\
</body>\
</html>";
  server.send(200, "text/html", html);
}

void handleGPS() {
  String gpsData = "{\"lat\": " + String(latitude, 6) + ", \"lng\": " + String(longitude, 6) + "}";
  server.send(200, "application/json", gpsData);
}

void handleRFID() {
  String html = "<!DOCTYPE html>\
<html>\
<head>\
  <title>RFID Message</title>\
  <style>\
    body { \
      font-family: Arial, sans-serif; \
      display: flex; \
      justify-content: center; \
      align-items: center; \
      height: 100vh; \
      margin: 0; \
      background-image: url('https://st2.depositphotos.com/1186248/7904/i/950/depositphotos_79044690-stock-photo-help-me.jpg'); /* Background image URL */ \
      background-size: cover; \
      background-position: center; \
    } \
    #message-container { \
      background-color: rgba(255, 255, 255, 0.8); /* Semi-transparent white background */ \
      padding: 20px; \
      border-radius: 10px; \
      box-shadow: 0 2px 5px rgba(0,0,0,0.1); \
      max-width: 500px; \
      text-align: center; \
      animation: fadeIn 1s ease, bounce 2s infinite; /* Apply fade-in and bounce animation */ \
    } \
    h1 { \
      color: #333; /* Dark gray text color */ \
    } \
    p { \
      color: #000000; /* Medium gray text color */ \
    } \
    @keyframes fadeIn { /* Define the fade-in animation */ \
      from { opacity: 0; } /* Start from fully transparent */ \
      to { opacity: 1; } /* End at fully opaque */ \
    } \
    @keyframes bounce { /* Define the bounce animation */ \
      0%, 20%, 50%, 80%, 100% { transform: translateY(0); } \
      40% { transform: translateY(-20px); } \
      60% { transform: translateY(-10px); } \
    } \
  </style>\
</head>\
<body>\
  <div id=\"message-container\">\
    <h1>Message RFID</h1>\
    <p>" + rfidMessage + "</p>\
  </div>\
</body>\
</html>";
  server.send(200, "text/html", html);
}


void displayRFIDMessage() {
  rfidMessage = "Bonjour, je m'appelle Halima Elhagouchi. S'il vous plaît, si vous trouvez cet objet, merci de m'appeler au numéro suivant: 0666650938.";
  server.send(200, "text/plain", "RFID Message Set");  // This line is for debugging purposes, can be removed
}

void sendTwilioMessage(double latitude, double longitude) {
  // Construire le message
  String message = "Attention, la localisation de l'objet a changé ! Nouvelles coordonnées : Latitude ";
  message += String(latitude, 6);
  message += ", Longitude ";
  message += String(longitude, 6);

  // Construire l'URL de l'API Twilio
  String url = "https://api.twilio.com/2010-04-01/Accounts/";
  url += account_sid;
  url += "/Messages.json";

  // Construire l'en-tête HTTP avec l'authentification basique
  String auth = String(account_sid) + ":" + String(auth_token);
  String auth_base64 = base64::encode(auth); // Utilisation de la fonction encode() de la bibliothèque Base64

  // Initialiser la requête HTTP
  HTTPClient http;
  http.begin(url);
  http.addHeader("Authorization", "Basic " + auth_base64);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Construire les données à envoyer
  String postData = "From=" + String(twilio_number) + "&To=" + String(recipient_number) + "&Body=" + message;

  // Envoyer la requête POST
  int httpResponseCode = http.POST(postData); // Utilisation de la fonction POST avec les données à envoyer
  http.end(); // Libérer les ressources HTTP
}