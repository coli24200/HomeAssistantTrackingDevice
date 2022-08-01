# PROXY Server
Proxy http to HA running in https 

When Home Assistant is set with https and CA , some (or most) of the GSM modules do not support https, mainly because they do not support TLS v1.2.
This proxy server runs in Node.JS and redirect a request on a specific port (e.g 5000) received some parameters and resend it to the HA webhook.


Server protected with NPM Helmet, Supervisor and Express-rate-limit 

deploy all these files in your server with Node.js and run;

> npm run prod


# HARDWARE:

