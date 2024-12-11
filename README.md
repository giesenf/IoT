# IoT
IoT Plant Monitor

This project consisted of using a Heltec ESP32 Wi-Fi Lora v(3) as well as:
- Resistive Soil Moisture Sensor (https://www.switchelectronics.co.uk/products/soil-hygrometer-moisture-detection-sensor-module?currency=GBP&variant=45334952837429&utm_source=google&utm_medium=cpc&utm_campaign=Google%20Shopping&stkn=bbf1d20e1ed7&gad_source=1&gclid=Cj0KCQiA88a5BhDPARIsAFj595jywyoueE252E_yTLz8wi3zJb_16HsJSHaLLaZgPXmJOCQOGZ7_ehIaAvbmEALw_wcB)
- Light Dependent Resistor Module
- DHT11 Temperature & Humidity Sensor
- Seven-Color Flash LED Module
- Breadboard + Sufficient Male-2-Male, Female-2-Male, Female-2-Female Cables

The code is run on Arduino Cloud which also hosts the dashboard & mobile application.

Its purpose is to monitor an indoor plant and give the user live insights through the dashboard. Furthermore, using the OpenWeatherMap API, the product makes predictions regarding light exposure. The product turns on an LED if insufficient light reaches the plant and notifies the user if the plant needs to be watered.

