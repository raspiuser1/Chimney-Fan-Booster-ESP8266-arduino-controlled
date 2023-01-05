# Chimney-Fan-Booster-ESP8266-arduino-controlled
Do it youself chimney with fan booster, below you will find a youtube video
Youtube link: https://youtu.be/LbCBmjPsJkQ <br />
[![IMAGE VIDEO](https://img.youtube.com/vi/LbCBmjPsJkQ/0.jpg)](https://www.youtube.com/watch?v=LbCBmjPsJkQ)<br />

## Ingredients
- DHT22 temperature sensor 
- IRF50 Fet arduino module for the fans 
- 6pcs 40mm silent fans from amazon, 12 volt 
- 1pcs 200mm computer fan, 12 volt 
- ESP8266 arduino board 
- a power supply, in my case an ATC power supply
When connected to the WiFi you can control the fans with a slider at the ip address of the webserver, standard the fans will turn on when the temperature is over 30 degrees 

## Scematic
All the fans are connected in paralell, you can cut the tach wire 
![Naamloos](https://user-images.githubusercontent.com/13587295/210729237-4440a264-8050-47b8-83fe-4a5cdbbf0c63.png)


## Setup
- edit the code with your wifi ssid and password
- optional you can fill in the emoncms server details if you have one, it will log to your server.
