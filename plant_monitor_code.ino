#include "arduino_secrets.h"
/*  

  Arduino IoT Cloud Variables description

  The following variables are automatically generated and updated when changes are made to the Thing

  float current_total_light;
  float internal_hum;
  float internal_temp;
  float light_value;
  float minimum_light_threshold;
  float minimum_water_threshold;
  float outdoor_hum;
  float outdoor_temp;
  float predicted_light;
  float soil_moisture;
  float total_light;
  bool led_on;
  bool need_water;

  Variables which are marked as READ/WRITE in the Cloud Thing will also have functions
  which are called when their values are changed from the Dashboard.
  These functions are generated with the Thing and added at the end of this sketch.
*/

// Include necessary libraries and headers
#include "arduino_secrets.h"
#include "thingProperties.h"
#include <DHT.h>
#include <DHT_U.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <Time.h>
#include <vector>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Constants and settings for sensors and devices
#define DHTPIN 3          // Pin for DHT sensor
#define DHTTYPE DHT11     // DHT sensor type
DHT dht(DHTPIN, DHTTYPE);

// WiFi and NTP settings
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", 0, 3600000);  // Synchronize with NTP server, GMT timezone

// Pin definitions
const int light_pin = 5;  // Pin for light sensor
const int led_pin = 7;    // Pin for LED
const int soil_pin = 2;   // Pin for soil moisture sensor

// Variables for handling sensor data
const int NO_SAMPLES = 250; // Number of samples for soil moisture averaging
float soil_value = 0;       // Current soil moisture value
long readings[NO_SAMPLES];  // Array to store soil moisture readings
int current_index = 0;      // Index for the circular buffer
long total_value = 0;       // Sum of soil moisture readings
bool last_need_water = false; // State for water requirement

// API settings
String weather_api_url = "https://api.openweathermap.org/data/2.5/weather";
String forecast_api_url = "https://api.openweathermap.org/data/2.5/forecast";
String API_KEY = "<YOUR_API_KEY>"; // Replace with your OpenWeatherMap API key
const float longitude = <YOUR_LONG_COORDINATE>; // Replace with your longitude
const float latitude = <YOUR_LAT_COORDINATE>;   // Replace with your latitude

// Variables for tracking time and weather data
long sunrise_time_local;
long sunset_time_local;
unsigned long last_weather_fetch = 0;
unsigned long last_w_check = 0;
unsigned long last_light_time_check = 0;
unsigned long led_on_time = 0;

unsigned long led_remaining_light_time = 0;
bool led_should_on = false;
bool manual_overide = false;
unsigned long start_time = 0;
time_t next_fetch_time;

// Structure to store weather forecast data
struct Forecast {
  String time_str;      // Time of forecast
  float temperature;    // Forecasted temperature
  String description;   // Weather condition description
  float humidity;       // Forecasted humidity
  float wind_speed;     // Forecasted wind speed
};

std::vector<Forecast> forecast_list;  // Vector to hold forecast data

// Setup function: runs once when the board powers up or resets
void setup() {
  Serial.begin(9600);  // Initialize serial communication
  delay(1500);

  WiFi.begin(SECRET_SSID, SECRET_OPTIONAL_PASS);  // Connect to WiFi

  // Wait until WiFi is connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  initProperties();  // Initialize IoT Cloud properties
  pinMode(led_pin, OUTPUT); // Set LED pin as output

  ArduinoCloud.begin(ArduinoIoTPreferredConnection);  // Start Arduino IoT Cloud
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  timeClient.begin(); // Start NTP client
  timeClient.update(); // Synchronize time
}


// Function to update soil moisture readings
void update_soil_moisture() {
  soil_value = analogRead(soil_pin);  // Read soil moisture sensor

  // Update circular buffer and calculate average soil moisture
  total_value -= readings[current_index];
  readings[current_index] = soil_value;
  total_value += soil_value;
  current_index = (current_index + 1) % NO_SAMPLES;

  soil_moisture = 100 - (100 * (total_value / NO_SAMPLES - 1600) / (4095 - 1600));
  soil_moisture = constrain(soil_moisture, 0, 100); // Ensure value is within 0-100

  // Hysteresis logic for water requirement
  if (soil_moisture < minimum_water_threshold - 2) {
    need_water = true;
  } else if (soil_moisture > minimum_water_threshold + 2) {
    need_water = false;
  } else {
    need_water = last_need_water;
  }

  last_need_water = need_water; // Update last state
}
// Function to fetch current weather data from OpenWeatherMap
void fetch_weather_data() {
  HTTPClient http;
  String full_url = weather_api_url + "?lat=" + String(latitude, 4) +
                    "&lon=" + String(longitude, 4) +
                    "&appid=" + API_KEY + "&units=metric";

  http.begin(full_url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<1024> jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, payload);

    if (error) {
      Serial.println("Failed to parse JSON!");
    } else {
      outdoor_temp = jsonDoc["main"]["temp"];
      outdoor_hum = jsonDoc["main"]["humidity"];
      sunrise_time_local = jsonDoc["sys"]["sunrise"];
      sunset_time_local = jsonDoc["sys"]["sunset"];
    }
  } else {
    Serial.print("Error fetching weather data. HTTP code: ");
    Serial.println(httpCode);
  }

  http.end();
}

// Include necessary libraries and headers
#include "arduino_secrets.h"
#include "thingProperties.h"
#include <DHT.h>
#include <DHT_U.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <Time.h>
#include <vector>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Constants and settings for sensors and devices
#define DHTPIN 3          // Pin for DHT sensor
#define DHTTYPE DHT11     // DHT sensor type
DHT dht(DHTPIN, DHTTYPE);

// WiFi and NTP settings
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", 0, 3600000);  // Synchronize with NTP server, GMT timezone

// Pin definitions
const int light_pin = 5;  // Pin for light sensor
const int led_pin = 7;    // Pin for LED
const int soil_pin = 2;   // Pin for soil moisture sensor

// Variables for handling sensor data
const int NO_SAMPLES = 250; // Number of samples for soil moisture averaging
float soil_value = 0;       // Current soil moisture value
long readings[NO_SAMPLES];  // Array to store soil moisture readings
int current_index = 0;      // Index for the circular buffer
long total_value = 0;       // Sum of soil moisture readings
bool last_need_water = false; // State for water requirement

// API settings
String weather_api_url = "https://api.openweathermap.org/data/2.5/weather";
String forecast_api_url = "https://api.openweathermap.org/data/2.5/forecast";
String API_KEY = "<YOUR_API_KEY>"; // Replace with your OpenWeatherMap API key
const float longitude = <YOUR_LONG_COORDINATE>; // Replace with your longitude
const float latitude = <YOUR_LAT_COORDINATE>;   // Replace with your latitude

// Variables for tracking time and weather data
long sunrise_time_local;
long sunset_time_local;
unsigned long last_weather_fetch = 0;
unsigned long last_w_check = 0;
unsigned long last_light_time_check = 0;
unsigned long led_on_time = 0;

unsigned long led_remaining_light_time = 0;
bool led_should_on = false;
bool manual_overide = false;
unsigned long start_time = 0;
time_t next_fetch_time;

// Structure to store weather forecast data
struct Forecast {
  String time_str;      // Time of forecast
  float temperature;    // Forecasted temperature
  String description;   // Weather condition description
  float humidity;       // Forecasted humidity
  float wind_speed;     // Forecasted wind speed
};

std::vector<Forecast> forecast_list;  // Vector to hold forecast data

// Setup function: runs once when the board powers up or resets
void setup() {
  Serial.begin(9600);  // Initialize serial communication
  delay(1500);

  WiFi.begin(SECRET_SSID, SECRET_OPTIONAL_PASS);  // Connect to WiFi

  // Wait until WiFi is connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  // Initialize sensors and devices
  dht.begin();
  pinMode(light_pin, INPUT);
  pinMode(soil_pin, INPUT);
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, LOW);

  // Start time synchronization with NTP
  timeClient.begin();

  // Initialize IoT Cloud properties
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  // Populate initial readings array with zeros for soil moisture
  for (int i = 0; i < NO_SAMPLES; i++) {
    readings[i] = 0;
  }
}

// Function to update soil moisture reading
void update_soil_moisture() {
  total_value -= readings[current_index];  // Subtract the oldest reading
  readings[current_index] = analogRead(soil_pin); // Get new reading from sensor
  total_value += readings[current_index];  // Add the new reading to total

  current_index = (current_index + 1) % NO_SAMPLES; // Circular buffer index update
  soil_value = total_value / NO_SAMPLES;  // Calculate the average value
}

// Function to estimate light intensity from weather description
float estimate_light_from_forecast(String weather_condition) {
  // Map weather descriptions to light intensity values (0 = no light, 4095 = full light)
  if (weather_condition == "clear sky") {
    return 4095.0 - 50.0;     // Full sunlight (clear sky), very high light intensity
  } else if (weather_condition == "few clouds") {
    return 4095.0 - 200.0;    // Bright sunlight with a few clouds
  } else if (weather_condition == "scattered clouds") {
    return 4095.0 - 500.0;    // Slightly overcast, still bright
  } else if (weather_condition == "broken clouds") {
    return 4095.0 - 800.0;    // Partly cloudy, moderate sunlight
  } else if (weather_condition == "overcast clouds") {
    return 4095.0 - 1200.0;   // Overcast, dim light
  } else if (weather_condition == "fog") {
    return 4095.0 - 1800.0;   // Very foggy, very dim light
  } else if (weather_condition == "rain") {
    return 4095.0 - 2500.0;   // Light rain, very low light
  } else if (weather_condition == "snow") {
    return 4095.0 - 2500.0;   // Snow, low light (similar to rain)
  } else if (weather_condition == "thunderstorm") {
    return 4095.0 - 3500.0;   // Thunderstorm, very low light
  } else if (weather_condition == "drizzle") {
    return 4095.0 - 2200.0;   // Light drizzle, dim light
  } else {
    return 4095.0 - 3000.0;   // Default for unknown weather conditions (dark room)
  }
}

// Function to calculate the time multiplier based on the current hour
float time_multiplier(int hour_now) {
  time_t sunrise = sunrise_time_local;
  time_t sunset = sunset_time_local;

  // Get the hour of sunrise and sunset
  int sunrise_hour = hour(sunrise);
  int sunset_hour = hour(sunset);

  if (hour_now < sunrise_hour) {
    return 0.0;  // No light before sunrise
  } else if (hour_now < sunrise_hour + 2) {  // Sharply increasing light after sunrise
    return (float)(hour_now - sunrise_hour) / 2.0;  // Linear increase
  } else if (hour_now < sunset_hour - 1.5) {  // Constant light during midday
    return 1.0;
  } else if (hour_now < sunset_hour) {  // Sharply decreasing light before sunset
    return 1.0 - (float)(hour_now - (sunset_hour - 1.5)) / 1.5;  // Linear decrease
  } else {
    return 0.0;  // No light after sunset
  }
}

// Function to handle LED state change
void onLedOnChange() {
  digitalWrite(led_pin, led_on);  // Update LED state
  if (led_on) {
    manual_overide = true;
    led_on_time = millis();
  } else {
    manual_overide = false;
  }
}

// Function to fetch weather forecast data using coordinates
void get_forecast_by_coords() {
  Serial.println("Fetching forecast");
  HTTPClient http;

  // Construct the API request URL using coordinates, API key, and metric units
  String full_url = forecast_api_url + "?lat=" + String(latitude, 4) +
                    "&lon=" + String(longitude, 4) +
                    "&exclude=current,minutely,alerts" +
                    "&appid=" + API_KEY + "&units=metric";
  http.begin(full_url);

  // Send GET request to the API and check the response code
  int httpCode = http.GET();

  if (httpCode == 200) { // Successful response
    String payload = http.getString();
    StaticJsonDocument<1024> jsonDoc;

    // Parse the JSON response
    DeserializationError error = deserializeJson(jsonDoc, payload);
    Serial.println("Fetched forecast");

    if (error) {
      return;  // If JSON parsing fails, exit the function
    }
    Serial.println("Fetched forecast all");

    // Clear the existing forecast list to avoid duplicates
    forecast_list.clear();

    // Update the current time from the NTP client
    timeClient.update();
    time_t current_time = timeClient.getEpochTime();
    tmElements_t tm;
    breakTime(current_time, tm);

    // Extract today's date in YYYY-MM-DD format
    String today_date = String(year(current_time)) + "-" + 
                    (month(current_time) < 10 ? "0" : "") + String(month(current_time)) + "-" + 
                    (day(current_time) < 10 ? "0" : "") + String(day(current_time));

    // Loop through each forecast item in the JSON response
    for (JsonObject elem : jsonDoc["list"].as<JsonArray>()) {
      String forecast_time_str = elem["dt_txt"].as<String>();

      // Extract the date from the forecast timestamp
      String forecast_date = forecast_time_str.substring(0, 10);

      // Only process forecasts for today's date
      if (forecast_date == today_date) {
        Forecast forecast_data;

        // Extract relevant weather data from JSON
        forecast_data.time_str = forecast_time_str;
        forecast_data.temperature = elem["main"]["temp"];
        forecast_data.description = elem["weather"][0]["description"].as<String>();
        forecast_data.humidity = elem["main"]["humidity"];
        forecast_data.wind_speed = elem["wind"]["speed"];
        Serial.println(forecast_data.description);

        // Add the forecast data to the list
        forecast_list.push_back(forecast_data);
      }
      Serial.print("The size of forecast_list is: ");
      Serial.println(forecast_list.size());
    }
  }

  http.end();  // End the HTTP connection
}

// Function to predict the total light for the day based on weather forecasts
void predict_total_light_for_day() {
  predicted_light = 0.0;

  // Check if there are any forecasts available
  if (forecast_list.size() == 0) {
      Serial.println("No forecasts available.");
      predicted_light = 0.0;
  } else {
      // Loop through each forecast and estimate the light contribution
      for (const Forecast& forecast : forecast_list) {
        String hour_str = forecast.time_str.substring(11, 13); // Extract hour from timestamp
        int hour = hour_str.toInt();

        // Accumulate predicted light for 3-hour blocks based on forecasted conditions
        for (int i = 0; i < 3; i++) {
          predicted_light += time_multiplier(hour + i) * estimate_light_from_forecast(forecast.description) * 60 * 2;
        }
      }
  }

  // Add the current observed light to the prediction
  current_total_light = total_light + predicted_light;

  // Check if additional light is needed to meet the minimum threshold
  if (current_total_light < minimum_light_threshold) {
    float remaining_light = minimum_light_threshold - current_total_light;

    // Calculate the required LED on time based on the remaining light needed
    led_should_on = true;
    led_remaining_light_time = (remaining_light / 2000.0) * 2 * 60 * 1000; // Convert to milliseconds
  }
}


// Function to fetch the forecast periodically based on the current time
void fetch_forecast() {
  timeClient.update(); // Synchronize time with the NTP client
  time_t current_time = timeClient.getEpochTime();

  tmElements_t tm;
  breakTime(current_time, tm);  // Break the current time into components

  int current_hour = tm.Hour;
  int current_minute = tm.Minute;
  int current_second = tm.Second;

  // Fetch forecast at the end of each 3-hour block
  if (current_hour % 3 == 2) {
    if (current_hour == 2) { // Reset light values at 2 AM
      total_light = 0.0;
      predicted_light = 0.0;
      current_total_light = 0.0;
    }

    int next_fetch_hour = (current_hour) % 24;
    int next_fetch_minute = 55;
    int next_fetch_second = 0;

    time_t next_fetch_time = (next_fetch_hour * 3600) + (next_fetch_minute * 60) + next_fetch_second;
    time_t current_system_time = (current_hour * 3600) + (current_minute * 60) + current_second;

    // Calculate the time difference between the current and next fetch times
    long time_difference = next_fetch_time - current_system_time;

    // Fetch the forecast if the time difference is within a 30-second window
    if (time_difference <= 30 && time_difference > -30) {
      Serial.println("Fetching forecast for the day");

      // Fetch and process the forecast data
      get_forecast_by_coords();
      predict_total_light_for_day();
    }
  }
}

void loop() {
  // Update IoT Cloud properties
  ArduinoCloud.update();
  
  // Get the current time in seconds since the program started
  unsigned long current_time = millis() / 1000;

  // Check if the LED should be turned on
  if (led_should_on) {
    if (!led_on) { // If the LED is not already on
      digitalWrite(led_pin, HIGH); // Turn on the LED
      led_on_time = millis(); // Record the time the LED was turned on
      led_should_on = false; // Reset the flag to prevent repeated triggering
    }
  }

  // Automatically turn off the LED after the required duration unless manually overridden
  if (!manual_overide && led_on && (millis() - led_on_time >= led_remaining_light_time)) {
    digitalWrite(led_pin, LOW); // Turn off the LED
    led_remaining_light_time = 0; // Reset the remaining light time
    led_should_on = false; // Reset the flag
  }

  // Periodically fetch weather data every 10 seconds
  if (current_time - last_w_check >= 10) {
    fetch_weather_data(); // Fetch the latest weather data
    last_w_check = current_time; // Update the last check timestamp
  }

  // Calculate specific times relative to sunrise
  unsigned long time_62mins_before_sunrise = sunrise_time_local - (62 * 60); // 62 minutes before sunrise
  unsigned long time_60mins_before_sunrise = sunrise_time_local - (60 * 60); // 60 minutes before sunrise

  // Reset light-related values between 62 and 60 minutes before sunrise
  if (current_time >= time_62mins_before_sunrise && current_time <= time_60mins_before_sunrise) {
    total_light = 0; // Reset total accumulated light
    predicted_light = 0; // Reset predicted light
  }

  // Fetch forecast data and predict light levels for the day
  fetch_forecast();

  // Read temperature and humidity from the DHT sensor
  internal_temp = dht.readTemperature(); // Measure the temperature
  internal_hum = dht.readHumidity();     // Measure the humidity

  // Read light sensor value and invert it for easier interpretation
  light_value = 4095 - analogRead(light_pin);

  // Update total light accumulation every 30 seconds
  if (current_time - last_light_time_check >= 30) {
    last_light_time_check = current_time; // Update the last check timestamp
    total_light += light_value; // Add current light value to the total
    current_total_light = total_light + predicted_light; // Calculate combined total light
  }

  // Update the soil moisture value
  update_soil_moisture();
}

/*
  Since MinimumWaterThreshold is READ_WRITE variable, onMinimumWaterThresholdChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onMinimumWaterThresholdChange()  {
  // Add your code here to act upon MinimumWaterThreshold change
}

/*
  Since MinimumLightThreshold is READ_WRITE variable, onMinimumLightThresholdChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onMinimumLightThresholdChange()  {
  // Add your code here to act upon MinimumLightThreshold change
}

/*
  Since TotalLight is READ_WRITE variable, onTotalLightChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onTotalLightChange()  {
  // Add your code here to act upon TotalLight change
}

/*
  Since ForecastDescription is READ_WRITE variable, onForecastDescriptionChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onForecastDescriptionChange()  {
  // Add your code here to act upon ForecastDescription change
}

/*
  Since ForecastData is READ_WRITE variable, onForecastDataChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onForecastDataChange()  {
  // Add your code here to act upon ForecastData change
}

/*
  Since PredictedLight is READ_WRITE variable, onPredictedLightChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onPredictedLightChange()  {
  // Add your code here to act upon PredictedLight change
}

/*
  Since CurrentTotalLight is READ_WRITE variable, onCurrentTotalLightChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onCurrentTotalLightChange()  {
  // Add your code here to act upon CurrentTotalLight change
}




