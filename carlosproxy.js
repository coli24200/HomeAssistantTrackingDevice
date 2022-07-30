var express = require('express')
var bodyParser = require('body-parser');
var rateLimit = require('express-rate-limit');
require('dotenv').config()

var PORT = process.env.PORT || 5000
var app= express()



// Rate Limits on requests (every 10 minutes can only make 3 requests)
var limiter = rateLimit({
    windowMs: 10*60*1000,  //10 minutes
    max: 3
})
app.use(limiter)
app.set('trust proxy',1)



// ROUTES
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));
app.use('/api',require('./routes/homeassist'))


app.listen(PORT, () => console.log('Server running on port '+PORT))
module.exports = app;

