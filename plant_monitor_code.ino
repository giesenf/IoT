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

#define DHTPIN 3
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", 0, 3600000);  // GMT offset, update interval (1 hour)

const int light_pin = 5;
const int led_pin = 7;
const int soil_pin = 2;

const int NO_SAMPLES = 250;
float soil_value = 0;
long readings[NO_SAMPLES];   
int current_index = 0;
long total_value = 0;
bool last_need_water = false;

String weather_api_url = "https://api.openweathermap.org/data/2.5/weather";
String forecast_api_url = "https://api.openweathermap.org/data/2.5/forecast";
String API_KEY = "<YOUR_API_KEY>";
const float longitude = <YOUR_LONG_COORDINATE>;
const float latitude = <YOUR_LAT_COORDINATE>;
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

struct Forecast {
  String time_str;
  float temperature;
  String description;
  float humidity;
  float wind_speed;
};

std::vector<Forecast> forecast_list;

void setup() {
  Serial.begin(9600);
  delay(1500); 

  WiFi.begin(SECRET_SSID, SECRET_OPTIONAL_PASS);  // Start the WiFi connection

  // Wait for the WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  initProperties();
  pinMode(led_pin, OUTPUT);
  
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  // Start NTP client
  timeClient.begin();
  timeClient.update();
}


void update_soil_moisture() {
  soil_value = analogRead(soil_pin);
  Serial.println(soil_value);
  total_value -= readings[current_index];
  readings[current_index] = soil_value;
  total_value += soil_value;
  current_index = (current_index + 1) % NO_SAMPLES;

  soil_moisture = 100 - (100 * (total_value / NO_SAMPLES - 1600) / (4095 - 1600));
  soil_moisture = constrain(soil_moisture, 0, 100);

  // Hysteresis logic
  if (soil_moisture < minimum_water_threshold - 2) {
    need_water = true;
  } else if (soil_moisture > minimum_water_threshold + 2) {
    need_water = false;
  } else {
    need_water = last_need_water;  // Maintain the previous state
  }

  last_need_water = need_water;
}


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

float estimate_light_from_forecast(String weather_condition) {
  // Mapping the weather descriptions to light intensity values in a more compressed range (0 = no light, 4095 = full light)
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
    return 4095.0 - 3500.0;   // Thunderstorm, low visibility, very dim light
  } else if (weather_condition == "drizzle") {
    return 4095.0 - 2200.0;   // Light drizzle, dim light
  } else {
    return 4095.0 - 3000.0;   // Default value for unknown or unexpected weather conditions (dark room)
  }
}

float time_multiplier(int hour_now) {
  time_t sunrise = sunrise_time_local;
  time_t sunset = sunset_time_local;
  
  // Get the hour of sunrise and sunset using the hour() function
  int sunrise_hour = hour(sunrise);
  int sunset_hour = hour(sunset);
  
  
  if (hour_now < sunrise_hour) {
    return 0.0;  // No light before sunrise
  } else if (hour_now < sunrise_hour + 2) {  // Sharply increasing light after sunrise
    return (float)(hour_now - sunrise_hour) / 2.0;  // Divide by 2 to create sharp increase
  } else if (hour_now < sunset_hour - 1.5) {  // Constant light in the middle of the day
    return 1.0;
  } else if (hour_now < sunset_hour) {  // Sharply decreasing light before sunset
    return 1.0 - (float)(hour_now - (sunset_hour - 1.5)) / 1.5;  // Divide by 1.5 to create sharp decrease
  } else {
    return 0.0;  // No light after sunset
  }
}

void onLedOnChange() {
  digitalWrite(led_pin, led_on);
  if (led_on){
    manual_overide = true;
    led_on_time = millis();
  }
  else{
    manual_overide = false;
  }
}

void get_forecast_by_coords() {
  Serial.println("Fetching forecast");
  HTTPClient http;
  String full_url = forecast_api_url + "?lat=" + String(latitude, 4) +
                    "&lon=" + String(longitude, 4) +
                    "&exclude=current,minutely,alerts" +
                    "&appid=" + API_KEY + "&units=metric";
  http.begin(full_url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<1024> jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, payload);
    Serial.println("Fetched forecast");
    if (error) {
      return;  // Just return if there's an error; no need to process further
    }
    Serial.println("Fetched forecast all");
    // Clear the forecast list before adding new data
    forecast_list.clear();

    timeClient.update();
  
    // Get the current time
    time_t current_time = timeClient.getEpochTime();
    tmElements_t tm;
    breakTime(current_time, tm);
    
    // Extract today's date (YYYY-MM-DD)
    String today_date = String(year(current_time)) + "-" + 
                    (month(current_time) < 10 ? "0" : "") + String(month(current_time)) + "-" + 
                    (day(current_time) < 10 ? "0" : "") + String(day(current_time));
    
    // Loop through the forecast data
    for (JsonObject elem : jsonDoc["list"].as<JsonArray>()) {
      String forecast_time_str = elem["dt_txt"].as<String>();
      
      // Extract the date portion (YYYY-MM-DD) from forecast_time_str
      String forecast_date = forecast_time_str.substring(0, 10);
      // Only include forecast for today (matching date)
      if (forecast_date == today_date) {
        Forecast forecast_data;

        // Extract forecast data from JSON
        forecast_data.time_str = forecast_time_str;
        forecast_data.temperature = elem["main"]["temp"];
        forecast_data.description = elem["weather"][0]["description"].as<String>();
        forecast_data.humidity = elem["main"]["humidity"];
        forecast_data.wind_speed = elem["wind"]["speed"];
        Serial.println(forecast_data.description);
        
        // Store the extracted data in the vector
        forecast_list.push_back(forecast_data);
      }
      Serial.print("The size of forecast_list is: ");
      Serial.println(forecast_list.size());
    }
  }

  http.end();
}

void predict_total_light_for_day() {
  predicted_light = 0.0;
  
  // Add predicted light from forecasted hours
  if (forecast_list.size() == 0) {
      Serial.println("No forecasts available.");
      predicted_light = 0.0;
  } else {
      for (const Forecast& forecast : forecast_list) {
        String hour_str = forecast.time_str.substring(11, 13);
        int hour = hour_str.toInt();
        for (int i=0; i<3; i++){
          predicted_light += time_multiplier(hour+i)*estimate_light_from_forecast(forecast.description)*60*2;
        }
      }
  }
  // Add the current observed light
  current_total_light = total_light + predicted_light;

  // If the total light is less than the threshold, calculate the remaining light required
  if (current_total_light < minimum_light_threshold) {
    float remaining_light = minimum_light_threshold - current_total_light;
    // Turn on the LED for the remaining amount
    led_should_on = true;
    led_remaining_light_time = (remaining_light/2000.0)*2*60*1000;
  }
}

void fetch_forecast() {
  timeClient.update();
  time_t current_time = timeClient.getEpochTime();

  tmElements_t tm;
  breakTime(current_time, tm);  // Break the time into components
  
  int current_hour = tm.Hour;
  int current_minute = tm.Minute;
  int current_second = tm.Second;
  
  // Get the next 3-hour block time
  if (current_hour % 3 == 2) {
    if (current_hour==2){
      total_light = 0.0;
      predicted_light = 0.0;
      current_total_light= 0.0;
    }
    int next_fetch_hour = (current_hour) % 24; 
    int next_fetch_minute = 55; 
    int next_fetch_second = 0;

    time_t next_fetch_time = (next_fetch_hour * 3600) + (next_fetch_minute * 60) + next_fetch_second;
    time_t current_system_time = (current_hour * 3600) + (current_minute * 60) + current_second;

    // Print the time difference between the two
    long time_difference = next_fetch_time - current_system_time;

    // If the time difference is within 30 seconds before or after the next fetch time, fetch the forecast
    if (time_difference <= 30 && time_difference > -30) {
      Serial.println("Fetching forecast for the day");
  
      // Get forecast data for the day
      get_forecast_by_coords();
      predict_total_light_for_day();
    }
  }
}

void loop() {
  ArduinoCloud.update();

  //get_forecast_by_coords();
  //predict_total_light_for_day();
  //Serial.println("In loop test"); 
  
  unsigned long current_time = millis() / 1000;
  
  if (led_should_on) {
    if (!led_on) {
      digitalWrite(led_pin, HIGH);
      led_on_time = millis();
      led_should_on = false;
    }
  }
  
  if (!manual_overide && led_on && (millis() - led_on_time >= led_remaining_light_time)) {
    digitalWrite(led_pin, LOW);
    led_remaining_light_time = 0;
    led_should_on = false;
  }

  if (current_time - last_w_check >= 10) {
    fetch_weather_data();
    last_w_check = current_time;
  }

  unsigned long time_62mins_before_sunrise = sunrise_time_local - ((62 * 60));  // 62 minutes before sunrise
  unsigned long time_60mins_before_sunrise = sunrise_time_local - (60 * 60);  // 60 minutes before sunrise

  // Check if the current time is between 62 and 60 minutes before sunrise
  if (current_time >= time_62mins_before_sunrise && current_time <= time_60mins_before_sunrise) {
    total_light = 0;
    predicted_light = 0;
  }
  
  fetch_forecast();
  
  internal_temp = dht.readTemperature();
  internal_hum = dht.readHumidity();

  light_value = 4095-analogRead(light_pin);

  if (current_time - last_light_time_check >= 30) {
    last_light_time_check = current_time;
    total_light += light_value;
    current_total_light = total_light + predicted_light;
  }
  
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




