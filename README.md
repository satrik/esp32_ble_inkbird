# esp32_ble_inkbird

I've searched a way to increase the range of my inkbird bbq thermometer.
All I've found on github were either with raspberry pi's, with esp32 over MQTT or old stuff.... but I wanted just a simple solution to get the temperature values from the inkbird via wifi on my smartphone. 

So I've used a lot of code from other projects and mixed it with my own stuff^^
Outcome is a Webserver on the esp32 which auto refresh's the temperature values each 3 seconds and changes the color according to the current temperature of the probe. Temperature < 80 = blue, < 100 = orange, > 135 = red, and the range between 100-135 is default gray.
I've also included the ESPmDNS library to reach the esp via "inkbird.local" inside my network.

# credits
thanks to the following projects, I'm sure you will find your code inside mine :D 
https://github.com/Nigho/ibbq-gateway
https://github.com/dereulenspiegel/ibbq-gateway
