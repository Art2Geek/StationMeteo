// Code only for ESP32
#if !defined(ESP32)
  #error This code is intended to run on the ESP32 platform! Please check your Tools->Board setting.
#endif

// Arduino librairy
#include <Arduino.h>

// Web server libraries
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

// HTTP Client
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Preferences
#include <Preferences.h>
Preferences preferences;

// 7 segments
#include "SevSeg.h"
SevSeg sevseg; //Instantiate a seven segment controller object
char temperatureString[5] = {' ', ' ', ' ', ' '};
char displayDay1String[5] = {' ', 'J', '1', ' '};
char displayDay2String[5] = {' ', 'J', '2', ' '};

// NeoPixel
#include <Adafruit_NeoPixel.h>
#define WEATHER_PIN 22     // input pin Neopixel is attached to
#define TEMPERATURE_PIN 23   // input pin Neopixel is attached to
#define WEATHER_NUMPIXELS  6 // number of neopixels in strip
#define TEMPERATURE_NUMPIXELS  6 // number of neopixels in strip

Adafruit_NeoPixel weatherPixels = Adafruit_NeoPixel(WEATHER_NUMPIXELS, WEATHER_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel temperaturePixels = Adafruit_NeoPixel(TEMPERATURE_NUMPIXELS, TEMPERATURE_PIN, NEO_GRB + NEO_KHZ800);

int brightness = 100;

// Wifi
const char *ssid = "Art2Geek";
const char *password = "********";

// Server
AsyncWebServer server(80);

// Weather
String city;
const String apiUrl = "https://www.prevision-meteo.ch/services/json/";
unsigned long previousMillis = 0;

int currentTemperature;
String currentCondition;

int day1TemperatureMin;
int day1TemperatureMax;
String day1Condition;

int day2TemperatureMin;
int day2TemperatureMax;
String day2Condition;

// Button
#include <JC_TouchButton.h>
TouchButton button(2);

// List of possible states for the state machine. This state machine has a fixed
enum states_t {
  IS_SHOWING_CURRENT_WEATHER,
  IS_SHOWING_DAY1_WEATHER,
  IS_SHOWING_DAY2_WEATHER,
  IS_TOUCHING_BUTTON,
  IS_LAMP_MODE
};

static states_t STATE = IS_SHOWING_CURRENT_WEATHER; // current state machine state
static states_t LAST_STATE; // last state machine state

void _configWifi()
{
  WiFi.begin(ssid, password);
	Serial.print("Tentative de connexion...");

	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print(".");
		delay(100);
	}

	Serial.println("\n");
	Serial.println("Connexion etablie!");
	Serial.print("Adresse IP: ");
	Serial.println(WiFi.localIP());
}

void _configPreferences()
{
  preferences.begin("weather", false);

  city = preferences.getString("city");

  if (city) {
    Serial.print("Ville : ");
    Serial.println(city ? city : "-");
  } else {
    Serial.println("Aucune ville n'est définie.");
  }
}

String getWeatherPicto(String condition)
{
  String picto;

  // SUN
  if (
    condition == "Ensoleillé" ||
    condition == "Nuit claire" ||
    condition == "Nuit bien dégagée"
  ) {
    picto = "sun";
  }
  // SUN AND CLOUD
  else if (
    condition == "Ciel voilé" ||
    condition == "Nuit légèrement voilée" ||
    condition == "Faibles passages nuageux" ||
    condition == "Nuit claire et stratus" ||
    condition == "Eclaircies"
  ) {
    picto = "sun_cloud";
  }
  // CLOUD
  else if (
    condition == "Stratus" ||
    condition == "Stratus se dissipant" ||
    condition == "Nuit nuageuse" ||
    condition == "Faiblement nuageux" ||
    condition == "Fortement nuageux" ||
    condition == "Développement nuageux" ||
    condition == "Nuit avec développement nuageux" ||
    condition == "Brouillard"
  ) {
    picto = "cloud";
  }
  // RAIN
  else if (
    condition == "Averses de pluie faible" ||
    condition == "Nuit avec averses" ||
    condition == "Averses de pluie modérée" ||
    condition == "Averses de pluie forte" ||
    condition == "Couvert avec averses" ||
    condition == "Pluie faible" ||
    condition == "Pluie forte" ||
    condition == "Pluie modérée"
  ) {
    picto = "rain";
  }
  // THUNDER
  else if (
    condition == "Faiblement orageux" ||
    condition == "Nuit faiblement orageuse" ||
    condition == "Orage modéré" ||
    condition == "Fortement orageux"
  ) {
    picto = "thunder";
  }
  // SNOW
  else if (
    condition == "Averses de neige faible" ||
    condition == "Nuit avec averses de neige faible" ||
    condition == "Neige faible" ||
    condition == "Neige modérée" ||
    condition == "Neige forte" ||
    condition == "Pluie et neige mêlée faible" ||
    condition == "Pluie et neige mêlée modérée" ||
    condition == "Pluie et neige mêlée forte"
  ) {
    picto = "snow";
  }
  // OTHER
  else {
    picto = "sun_cloud"; // Other case
  }

  return picto;
}

int getWeatherPictoIndex(String picto)
{
  int index = -1;

  if (picto == "sun") {
    index = 5;
  } else if (picto == "sun_cloud") {
    index = 4;
  } else if (picto == "cloud") {
    index = 3;
  } else if (picto == "rain") {
    index = 2;
  } else if (picto == "thunder") {
    index = 1;
  } else if (picto == "snow") {
    index = 0;
  }

  return index;
}

String getWeatherPictoFromCondition(String condition)
{
  String picto = getWeatherPicto(condition);

  return getWeatherPicto(picto);
}

int getWeatherPictoIndexFromCondition(String condition)
{
  String picto = getWeatherPictoFromCondition(condition);

  return getWeatherPictoIndex(picto);
}

void activateWeatherPicto(String picto) {
  uint16_t index = getWeatherPictoIndex(picto);
  uint32_t color = -1;

  if (brightness == 0) {
    weatherPixels.clear();
    weatherPixels.show();
  }

  Serial.print("Picto : ");
  Serial.println(picto);

  Serial.print("Index : ");
  Serial.println(index);

  switch (index) {
    // Snow
    case 0:
        color = weatherPixels.Color(255, 255, 255);
      break;

    // Thunder
    case 1:
        color = weatherPixels.Color(22, 42, 255);
      break;

    // Rain
    case 2:
        color = weatherPixels.Color(0, 187, 255);
      break;

    // Cloud
    case 3:
        color = weatherPixels.Color(100, 100, 100);
      break;

    // Sund & Cloud
    case 4:
        color = weatherPixels.Color(255, 255, 73);
      break;

    // Sun
    case 5:
        color = weatherPixels.Color(255, 255, 0);
      break;
  }

  weatherPixels.clear();
  weatherPixels.show();

  if (color > 0) {
    weatherPixels.setPixelColor(index, color);
    weatherPixels.show();

    Serial.print("Couleur : ");
    Serial.println(color);
  }
}

void showTemperatureGradient(int temperature)
{
  // Generated with https://cssgradient.io

  if (brightness == 0) {
    temperaturePixels.clear();
    temperaturePixels.show();

    return;
  }

  if (temperature < 5) {
    temperaturePixels.setPixelColor(5, temperaturePixels.Color(0, 255, 255));
    temperaturePixels.setPixelColor(4, temperaturePixels.Color(0, 255, 255));
    temperaturePixels.setPixelColor(3, temperaturePixels.Color(0, 255, 255));
    temperaturePixels.setPixelColor(2, temperaturePixels.Color(0, 255, 255));
    temperaturePixels.setPixelColor(1, temperaturePixels.Color(0, 255, 255));
    temperaturePixels.setPixelColor(0, temperaturePixels.Color(0, 255, 255));
  } else if (temperature < 10) {
    temperaturePixels.setPixelColor(5, temperaturePixels.Color(0, 255, 255));
    temperaturePixels.setPixelColor(4, temperaturePixels.Color(0, 222, 255));
    temperaturePixels.setPixelColor(3, temperaturePixels.Color(0, 189, 255));
    temperaturePixels.setPixelColor(2, temperaturePixels.Color(0, 156, 255));
    temperaturePixels.setPixelColor(1, temperaturePixels.Color(0, 123, 255));
    temperaturePixels.setPixelColor(0, temperaturePixels.Color(0, 89, 255));
  } else if (temperature < 15) {
    temperaturePixels.setPixelColor(5, temperaturePixels.Color(0, 89, 255));
    temperaturePixels.setPixelColor(4, temperaturePixels.Color(51, 122, 204));
    temperaturePixels.setPixelColor(3, temperaturePixels.Color(102, 155, 153));
    temperaturePixels.setPixelColor(2, temperaturePixels.Color(153, 188, 102));
    temperaturePixels.setPixelColor(1, temperaturePixels.Color(204, 222, 51));
    temperaturePixels.setPixelColor(0, temperaturePixels.Color(255, 255, 0));
  } else if (temperature < 20) {
    temperaturePixels.setPixelColor(5, temperaturePixels.Color(255, 255, 0));
    temperaturePixels.setPixelColor(4, temperaturePixels.Color(255, 239, 0));
    temperaturePixels.setPixelColor(3, temperaturePixels.Color(255, 223, 0));
    temperaturePixels.setPixelColor(2, temperaturePixels.Color(255, 206, 0));
    temperaturePixels.setPixelColor(1, temperaturePixels.Color(255, 190, 0));
    temperaturePixels.setPixelColor(0, temperaturePixels.Color(255, 173, 0));
  } else if (temperature < 30) {
    temperaturePixels.setPixelColor(5, temperaturePixels.Color(255, 173, 0));
    temperaturePixels.setPixelColor(4, temperaturePixels.Color(255, 163, 0));
    temperaturePixels.setPixelColor(3, temperaturePixels.Color(255, 103, 0));
    temperaturePixels.setPixelColor(2, temperaturePixels.Color(255, 142, 0));
    temperaturePixels.setPixelColor(1, temperaturePixels.Color(255, 132, 0));
    temperaturePixels.setPixelColor(0, temperaturePixels.Color(255, 121, 0));
  } else {
    temperaturePixels.setPixelColor(5, temperaturePixels.Color(255, 121, 0));
    temperaturePixels.setPixelColor(4, temperaturePixels.Color(255, 97, 0));
    temperaturePixels.setPixelColor(3, temperaturePixels.Color(255, 73, 0));
    temperaturePixels.setPixelColor(2, temperaturePixels.Color(255, 49, 0));
    temperaturePixels.setPixelColor(1, temperaturePixels.Color(255, 25, 0));
    temperaturePixels.setPixelColor(0, temperaturePixels.Color(255, 0, 0));
  }

  temperaturePixels.show();
}

void showTemperature(int temperature) {
  sprintf(temperatureString, "%d", temperature);

  if (temperature > -1) {
    char tempString[5] = {' ', ' ', ' ', ' '};

    // For number lower than 10, put the number at the third place (add two spaces at the beginning)
    if (temperature < 10) {
      tempString[2] = temperatureString[0];
    }
    // else put the number at the second place (adds a space at the beginning)
    else {
      tempString[1] = temperatureString[0];
      tempString[2] = temperatureString[1];
    }

    strcpy(temperatureString, tempString);
  }

}

void weatherLoadingAnim() {
  Serial.println("Animation...");

  if (brightness > 0) {
    // Sun
    activateWeatherPicto("sun");
    delay(1000);

    // Sun & Cloud
    activateWeatherPicto("sun_cloud");
    delay(1000);

    // Cound
    activateWeatherPicto("cloud");
    delay(1000);

    // Rain
    activateWeatherPicto("rain");
    delay(1000);

    // Thunder
    activateWeatherPicto("thunder");
    delay(1000);

    // Snow
    activateWeatherPicto("snow");
    delay(1000);
  }

  weatherPixels.clear();
  weatherPixels.show();
}

void getWeather(bool showAnim = true)
{
  if (showAnim) {
    weatherLoadingAnim();
  }

  Serial.println("");
  Serial.println("Récupération de la météo...");

  HTTPClient http;

  http.useHTTP10(true);
  http.begin(apiUrl + city); //Specify the URL
  http.GET();

  DynamicJsonDocument doc(84334);

  deserializeJson(doc, http.getStream());

  // Current day
  JsonObject currentData = doc["current_condition"];
  currentTemperature = currentData["tmp"];
  String currentConditionTemp = currentData["condition"];
  currentCondition = currentConditionTemp;

  // Day + 1
  JsonObject day1Data = doc["fcst_day_1"];
  day1TemperatureMin = day1Data["tmin"];
  day1TemperatureMax = day1Data["tmax"];
  String day1ConditionTemp = day1Data["condition"];
  day1Condition = day1ConditionTemp;

  // Day + 2
  JsonObject day2Data = doc["fcst_day_2"];
  day2TemperatureMin = day2Data["tmin"];
  day2TemperatureMax = day2Data["tmax"];
  String day2ConditionTemp = day2Data["condition"];
  day2Condition = day2ConditionTemp;

  // Debug
  Serial.print("Temperature : ");
  Serial.println(currentTemperature);

  Serial.print("Temps : ");
  Serial.println(currentCondition);

  // Update current weather
  activateWeatherPicto(getWeatherPictoFromCondition(currentCondition));
  showTemperatureGradient(currentTemperature);
  showTemperature(currentTemperature);

  http.end(); //Free the resources
}

void _configServer()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/bootstrap.min.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/script.js", "text/javascript");
  });

  server.on("/saveConfig", HTTP_POST, [](AsyncWebServerRequest *request)
  {
    String city;
    if (request->hasParam("city"), true) {
      city = request->getParam("city", true)->value();
      preferences.putString("city", city);
    }

    request->send(200, "text/plain", city);

    getWeather(true);
  });

  server.begin();
}

void _config7Segments()
{
  byte numDigits = 4;
  byte digitPins[] = {26, 27, 14, 12};
  byte segmentPins[] = {0, 4, 16, 17, 5, 18, 19, 21};
  bool resistorsOnSegments = false; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_CATHODE; // See README.md for options
  bool updateWithDelays = false; // Default 'false' is Recommended
  bool leadingZeros = false; // Use 'true' if you'd like to keep the leading zeros
  bool disableDecPoint = false; // Use 'true' if your decimal point doesn't exist or isn't connected

  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros, disableDecPoint);
  sevseg.setBrightness(100);

}

////////////////////////////////////////

void setup()
{
  // ---------- SERIAL
  Serial.begin(115200);
  while (!Serial) {} // Décommentez cette ligne pour faciliter le débug. Attend que le port Série soit disponible. Ne pas utiliser pour la production

  // ---------- GPIO

  // ---------- SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("Erreur d\'initialisation du SPIFFS");
    return;
  }

  button.begin();

  // Affiche la liste des fichiers présents dans la mémoire SPIFFS
  // File root = SPIFFS.open("/");
  // File file = root.openNextFile();

  // while (file) {
  //   Serial.print("File: ");
  //   Serial.println(file.name());
  //   file.close();
  //   root.openNextFile();
  // }

  // ---------- WIFI
  _configWifi();

  // ---------- SERVER
  _configServer();

  // ---------- PREFERENCES
  _configPreferences();

  // ---------- 7 segments
  _config7Segments();

  // Initialize the NeoPixel library.
  weatherPixels.begin();
  weatherPixels.setBrightness(brightness);
  weatherPixels.clear();
  weatherPixels.show();

  temperaturePixels.begin();
  temperaturePixels.setBrightness(brightness);
  temperaturePixels.clear();
  temperaturePixels.show();

  getWeather(true);

  //activateWeatherPicto("sun");
}

void loop()
{
  if (!city) {
    return;
  }

  unsigned long currentMillis = millis();

  button.read();
  sevseg.refreshDisplay(); // Must run repeatedly

  switch (STATE)
  {
    // Show weather for current day
    case IS_SHOWING_CURRENT_WEATHER:
      showTemperature(currentTemperature);
      sevseg.setChars(temperatureString);

      if (button.wasPressed()) {
        LAST_STATE = IS_SHOWING_CURRENT_WEATHER;
        STATE = IS_TOUCHING_BUTTON;
      }

      if (currentMillis - previousMillis >= 600000) {
        previousMillis = currentMillis;

        getWeather(true);
      }
      break;

    // Show weather for day + 1
    case IS_SHOWING_DAY1_WEATHER:
      if (button.wasPressed()) {
        LAST_STATE = IS_SHOWING_DAY1_WEATHER;
        STATE = IS_TOUCHING_BUTTON;
      }

      if (currentMillis - previousMillis >= 0 && currentMillis - previousMillis <= 2000) {
        sevseg.setChars(displayDay1String);
      }
      else if (currentMillis - previousMillis > 2000 && currentMillis - previousMillis <= 5000) {
        showTemperature(day1TemperatureMin);
        sevseg.setChars(temperatureString);
      }
      else if (currentMillis - previousMillis > 5000 && currentMillis - previousMillis <=8000) {
        showTemperature(day1TemperatureMax);
        sevseg.setChars(temperatureString);
      }
      else if (currentMillis - previousMillis > 8000) {
        previousMillis = currentMillis;
      }
    break;

    // Show weather for day + 2
    case IS_SHOWING_DAY2_WEATHER:
      if (button.wasPressed()) {
        LAST_STATE = IS_SHOWING_DAY2_WEATHER;
        STATE = IS_TOUCHING_BUTTON;
      }

      if (currentMillis - previousMillis >= 0 && currentMillis - previousMillis <= 2000) {
        sevseg.setChars(displayDay2String);
      }
      else if (currentMillis - previousMillis > 2000 && currentMillis - previousMillis <= 5000) {
        showTemperature(day2TemperatureMin);
        sevseg.setChars(temperatureString);
      }
      else if (currentMillis - previousMillis > 5000 && currentMillis - previousMillis <=8000) {
        showTemperature(day2TemperatureMax);
        sevseg.setChars(temperatureString);
      }
      else if (currentMillis - previousMillis > 8000) {
        previousMillis = currentMillis;
      }

      break;

    // Is touching button
    case IS_TOUCHING_BUTTON:
      if (button.pressedFor(3000)) {
        if (LAST_STATE == IS_LAMP_MODE) {
          Serial.println("Long press : Changing to Weather Mode");
          temperaturePixels.clear();
          temperaturePixels.show();

          weatherPixels.clear();
          weatherPixels.show();

          getWeather(true);
          STATE = IS_SHOWING_CURRENT_WEATHER;
        }
        else {
          Serial.println("Long press : Changing to Lamp Mode");
          char temperatureStringTemp[5] = {' ', ' ', ' ', ' '};
          sevseg.setChars(temperatureStringTemp);

          // Set by default brightness to 66%
          brightness = 66;

          STATE = IS_LAMP_MODE;
        }
      }
      else if (button.wasReleased()) {
        Serial.println("Short press");

        // From Current to Day 1
        if (LAST_STATE == IS_SHOWING_CURRENT_WEATHER) {
          Serial.println("Day +1");

          previousMillis = millis();
          activateWeatherPicto(getWeatherPictoFromCondition(day1Condition));
          showTemperatureGradient(day1TemperatureMax);

          STATE = IS_SHOWING_DAY1_WEATHER;
        }
        // From Day 1 to Day 2
        else if (LAST_STATE == IS_SHOWING_DAY1_WEATHER) {
          Serial.println("Day +2");

          previousMillis = millis();
          activateWeatherPicto(getWeatherPictoFromCondition(day2Condition));
          showTemperatureGradient(day2TemperatureMax);

          STATE = IS_SHOWING_DAY2_WEATHER;
        }
        // From Day 2 to Current
        else if (LAST_STATE == IS_SHOWING_DAY2_WEATHER) {
          Serial.println("Current day");
          STATE = IS_SHOWING_CURRENT_WEATHER;

          activateWeatherPicto(getWeatherPictoFromCondition(currentCondition));
          showTemperatureGradient(currentTemperature);
        }
        // Lamp Mode
        else if (LAST_STATE == IS_LAMP_MODE) {
          if (brightness >= 100) {
            brightness = 0;
          } else if (brightness == 0) {
            brightness = 33;
          } else if (brightness == 33) {
            brightness = 66;
          } else {
            brightness = 100;
          }

          weatherPixels.setBrightness(brightness);
          temperaturePixels.setBrightness(brightness);

          STATE = IS_LAMP_MODE;
        }
      }
      break;

      // Lamp Mode
      case IS_LAMP_MODE:
           if (button.wasPressed()) {
            LAST_STATE = IS_LAMP_MODE;
            STATE = IS_TOUCHING_BUTTON;
          }

          if (brightness > 0) {
            temperaturePixels.setPixelColor(0, temperaturePixels.Color(255, 255, 255));
            temperaturePixels.setPixelColor(1, temperaturePixels.Color(255, 255, 255));
            temperaturePixels.setPixelColor(2, temperaturePixels.Color(255, 255, 255));
            temperaturePixels.setPixelColor(3, temperaturePixels.Color(255, 255, 255));
            temperaturePixels.setPixelColor(4, temperaturePixels.Color(255, 255, 255));
            temperaturePixels.setPixelColor(5, temperaturePixels.Color(255, 255, 255));
            temperaturePixels.show();

            weatherPixels.setPixelColor(0, weatherPixels.Color(255, 255, 255));
            weatherPixels.setPixelColor(1, weatherPixels.Color(255, 255, 255));
            weatherPixels.setPixelColor(2, weatherPixels.Color(255, 255, 255));
            weatherPixels.setPixelColor(3, weatherPixels.Color(255, 255, 255));
            weatherPixels.setPixelColor(4, weatherPixels.Color(255, 255, 255));
            weatherPixels.setPixelColor(5, weatherPixels.Color(255, 255, 255));
            weatherPixels.show();
          }
          else {
            temperaturePixels.clear();
            temperaturePixels.show();

            weatherPixels.clear();
            weatherPixels.show();
          }

          delay(50);

        break;
  }
}

