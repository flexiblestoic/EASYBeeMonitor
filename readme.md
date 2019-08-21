# EASYBeeMonitor

## Disclaimer of Warranties and Limitation of Liability
Please review the license in the "License" section.


## Mission

Affordable, easy to source, easy to make, long range, autonomous behive monitoring device.

EASYBeeMonitor is designed with common components found in most electronic stores with the benefit of being low cost and well supported by the makers community. The Sigfox platform provides good coverage in [Europe](https://www.sigfox.com/en/coverage) and its low energy signature fits well with the project's requirements.

## Hardware

Note: we are not sponsored by any sellers, the links are provided purely for informational purposes and without any warranty.

- Microcontroller: ESP12E/F (aka ESP8266) along with a breakout board ([eg. seller](https://www.ebay.fr/itm/5119-ESP12-E-esp8266-module-wifi-sans-fils-ARDUINO-ESP8266-/191849920712?var=&hash=item2cab2578c8)) 
- Witty cloud to program the barebone ESP23E/F ([eg. seller](https://www.ebay.fr/itm/Module-Witty-cloud-WIFI-ESP-12F-RGB-nodemcu-ESP8266-CH340-Arduino-proto-/323300210773?hash=item4b4631b855)) 
- Weight sensors: 4 x 50 kg load cells coupled with a HX711 load cell amplifier ([specs](media/SpecWeightSensors.pdf))([eg. seller](https://www.ebay.fr/itm/Capteur-de-pesage-50KG-4PCS-cellule-charge-Strain-Weight-Sensor-HX711-Arduino-/323418502142?hash=item4b4d3eb3fe)) 
- Wisol Sigfox Module BRKWS01 and antenna ([specs](https://yadom.fr/downloadable/download/sample/sample_id/162/)) ([eg. seller](https://yadom.eu/carte-breakout-sfm10r1.html)) 
- HT7333 low consumption LDO ([specs](media/SpecHT73xx.pdf)) ([eg. seller](https://www.ebay.fr/itm/1595-3-3-2-%C3%A0-50pcs-r%C3%A9gulateur-3-3-v-HT7333-Low-Power-Consumption-LDO-TO92/192699773175?ssPageName=STRK%3AMEBIDX%3AIT&var=492841460613&_trksid=p2060353.m2749.l2649)) 
- Two Water proof DS18B20 temperature sensors  ([specs](https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf)) ([eg. seller](https://www.ebay.fr/itm/5146-1-a-10pcs-DS18B20-Dallas-1-Wire-capteur-temperature-Etanche-Waterproof-/142380214782?var=&hash=item0)) 

In order to achieve a low deep sleep current, it is essential to get a bare-bone ESP12E or F chip. The development boards (for eg. Nodemcu or other similar development boards) often embed a linear voltage regulator with significant quiescent current which is not ideal in a low power setting. We recommend a low quiescent LDO such as the HT7333. The barebone solution is best to optimize power consumption. The current is around 0.1mA in deep sleep and 80 mA during transmission (10s every half an hour). In our setup, using 6 AA batteries, we estimate the battery life to be roughly a year.  

You are free to choose the way you flash the ESP12. One way is to buy a Witty Cloud and use the flash shield to program the barebone ESP12 + dev board + LDO. However, please be advised that in any case, the barebone is not practical for prototyping. We do recommend a NodeMCU setup ([eg. seller](https://www.ebay.fr/itm/Module-WeMos-LoLin-NodeMCU-V3-WiFi-ESP8266-ESP12-E-ARDUINO-IoT-Robot-web/323287974108?hash=item4b457700dc:g:GxgAAOSwYl9a9g~5)) which facilitates the prototyping should you wish to temper with the design.

The schematics of the hardware available [here](media/EBM_schematics.pdf).



## Software
The firmware needs to be downloaded onto the micro controller. Please alter the code depending on chosen configuration (1) wifi or (2) Sigfox. 
Please be careful in the versions of the apps/librairies installed as newer versions may break the code.

1. Install [Arduino](arduino.cc)
2. Install ESP8266 control for Arduino [tutorial](https://dzone.com/articles/programming-the-esp8266-with-the-arduino-ide-in-3)
3. Install libraries (in Arduino, Tools>Library Manager): DallasTemperature by Miles Burton, OneWire by Jim Studt
4. We are using the [HX711 libraries](https://github.com/bogde/HX711) by Bogdan Necula


### Wifi
The micro controller will send sensor information to a chosen database provider, which in the code provided, is [thingspeak.com](thingspeak.com)

### Sigfox 
The micro controller will send sensor information to the Sigfox backend, a call back will have to be configured to route the information onto a database provider (in our case [thingspeak.com](thingspeak.com)). Please note that due to limitation of the architecture, the Sigfox network only allows a maximum number of 140 messages per day with each message no longer than 12 bits. 

When configuring the Sigfox back end, the custom payload config should be as follows. Sigfox back end tutorial available [here](https://www.youtube.com/watch?v=v0U5honpVYc).

`weight::uint:16:little-endian tempInt::int:16:little-endian tempExt::int:16:little-endian espVoltage::uint:8`

Given the fact that the data has been compressed. It needs to be decoded on thingspeak before beeing usable. The following code needs to be added under "apps>all apps>new" in order to translate the coded information into original values (please replace readChannelID and writeChannelID IDs and API Keys with your personal IDs and Keys)


```
% TODO - Replace the [] with channel ID to read data from: 
readChannelID = 0000000; 
% TODO - Enter the Read API Key between the '' below: 
readAPIKey = 'XXXXXXXX';  
% TODO - Replace the [] with channel ID to write data to: 
writeChannelID = 0000000; 
% TODO - Enter the Write API Key between the '' below: 
writeAPIKey = 'XXXXXXXX';  
%% Read Data %% 
data = thingSpeakRead(readChannelID, 'ReadKey', readAPIKey,'OutputFormat','table');  
analyzedData = data;  
%% Analyze Data %% 

UINT16_t_MAX = 65536
UINT8_t_MAX = 256

analyzedData.('weight') = round(data.('weight') * 300 / UINT16_t_MAX + 0, 2)
analyzedData.('tempInt') = round(data.('tempInt') * 170 / UINT16_t_MAX - 50, 2)
analyzedData.('tempExt') = round(data.('tempExt') * 170 / UINT16_t_MAX - 50, 2)
analyzedData.('espVoltage') = round(data.('espVoltage') * 7 / UINT8_t_MAX + 1, 2)

%% Write Data %% 
thingSpeakWrite(writeChannelID, analyzedData, 'WriteKey', writeAPIKey); 
%% Schedule action: React -> every 10 minutes 
```

### Number coding
In order to optimize message length, 4 pieces of informations (weight, tempInt, tempExt, ESPVcc) are coded onto 7 bits in the current version of the protocol. Each variable has been scaled depending on its useful range.


## License

[Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)](https://creativecommons.org/licenses/by-nc/4.0/)
