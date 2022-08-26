# Homeassistant
Install GPSLogger Webhook on integration tab.

Create a Long-Lived Access Token:

User/Long-Lived Access Tokens/create token

Use it in webhook:

< HA_URL >/api/webhook/< TOKEN >


Example of curl:

```ruby
curl -d "latitude=40.825359&longitude=-9.363611&device=DEVICE&accuracy=10&battery=20&speed=10&altitude=100" -H "Content-Type: application/x-www-form-urlencoded" -X POST https://<HA_URL>/api/webhook/<token>
```


After the entity DEVICE will be created in HA automaticaly and can be used in tracking on HA map


# PROXY Server
Proxy http to HA running in https 

When Home Assistant is set with https and CA , some (or most) of the GSM modules do not support https, mainly because they do not support TLS v1.2.
This proxy server runs in Node.JS and redirect a request on a specific port (e.g 5000) received some parameters and resend it to the HA webhook.


Server protected with NPM Helmet, Supervisor and Express-rate-limit 

deploy all these files in your server with Node.js and run;
> npm install
> npm run prod


# HARDWARE:
Instead of using Android device we will use an Arduino/ESP hardware to do the logging via HTTP POST.
board used: https://github.com/Xinyuan-LilyGO/LilyGo-T-Call-SIM800

ESP32 model used: SIM800L IP5306 20190610

![ESP+GSM module](/HARDWARE/images/TTGO_T-Call.png)


GPS module used:

![GPS module](/HARDWARE/images/gps.png)



# Pin connection


+ [GPS Module] pin VCC ---> 3v3 pin [ESP+GSM Module]

+ [GPS Module] pin GND ---> GND pin [ESP+GSM Module]

+ [GPS Module] pin TX  ---> 33 pin [ESP+GSM Module]

+ [GPS Module] pin RX  ---> 32 pin [ESP+GSM Module] 

+ [Car Battery] (voltage devider to half of 3v3)----> 14 pin [ESP+GSM Module]

+ [Car Battery] (voltage detector on USB 5v to 3v3 voltage devider)----> 12 pin [ESP+GSM Module]

REMARK: boths voltage sensors are still to be designed (TODO)


# DEV PROTOTYPE
![Breadboard](/HARDWARE/images/board.png)
