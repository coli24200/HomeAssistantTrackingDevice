const express = require('express')
const bodyParser = require('body-parser');
const rateLimit = require('express-rate-limit');
require('dotenv').config()
const helmet = require("helmet");

var PORT = process.env.PORT || 5000
var app= express()

// Rate Limits on requests (every 10 minutes can only make 3 requests)
var limiter = rateLimit({
    windowMs: 10*60*1000,  //10 minutes
    max: 3
})
app.use(limiter)
app.set('trust proxy',1)

//Vulnerabilities protection , setting various HTTP headers , better than nothing...
app.use(helmet());


// ROUTES redirect
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));
app.use('/',require('./routes/homeassist'))


// APP START
app.listen(PORT, () => console.log('Server running on port '+PORT))
module.exports = app;

