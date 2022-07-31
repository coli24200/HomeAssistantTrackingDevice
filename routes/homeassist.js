var url = require('url')
var express = require ('express')
var needle = require('needle')
var { json } = require('express')
var router = express.Router()



//HOME ASSISTANT URL and Long-Lived Access Tokens to webhook
var API_HA_URL = process.env.API_HA_URL;
var API_HA_KEY = process.env.API_HA_KEY;
//KEYPASS is security passkey (TODO: Base64 encode all data in POST request)
var KEY_PASS = process.env.API_KEYPASS;
// DEVICE name used in remote sender must be the same that the server knowns 
var XDEVICE = process.env.API_DEVICE;

//transfer vars
var xlatitude="";
var xlongitude="";
var xaccuracy="";
var xbattery="";
var xspeed="";
var xaltitude="";


    // Answer to a browser 
    router.get('*', async (req, res) => {
        if(process.env.NODE_ENV != 'production') { 
            console.log("ALL GETS REJECT");  
        }       
        res.status(403).end();
    });//END OF GET



    /// ROUTE only accept a post with some validation, device and key
    router.post('/device', async (req, res) => {
        
        try {
            var keyP=req.body.key;
            var xdev=req.body.device;
            var success=false;

            if ((keyP != KEY_PASS) ){
             console.log("wrong key:",keyP);   
             res.status(401).json("bye bye!")  
            }else{
                if (xdev!=XDEVICE){
                    console.log("wrong device:",xdev); 
                    res.status(401).json("wrong device!") 
                }else{
                    xlatitude=req.body.latitude;
                    xlongitude=req.body.longitude;
                    xaccuracy=req.body.accuracy;
                    xbattery=req.body.battery;
                    xspeed=req.body.speed;
                    xaltitude=req.body.altitude;
                    if(process.env.NODE_ENV != 'production') {
                        console.log("All vars set",xlatitude,xlongitude,xaccuracy,xbattery,xspeed,xaltitude);
                    }

                    success=true;

                    res.status(200).json("Great success !");
                }

            }
         
            if(success){
                console.log("Success"); 


                var url=String(API_HA_URL+API_HA_KEY);
                var data=String('latitude='+xlatitude+'&longitude='+xlongitude+'&device='+XDEVICE
                +'&accuracy='+xaccuracy+'&battery='+xbattery+'&speed='+xspeed+'&altitude='+xaltitude);


                if(process.env.NODE_ENV != 'production') { 
                    console.log("Success ... Sending to HA: ",url,data);
                }

                const apiRes = await needle.post(url,data,
                {
                parse: false, 
                rejectUnauthorized: false
                },
                function(err, response, body) {
                    console.log("HA answer: ",body);
                    if(err)
                        console.log(err); }
                );


            }

        } catch (error) {
            next(error)
            res.status(500).json("ERROR") 
        }



    }); // END OF POST

      // Answer to a browser 
    router.post('*', async (req, res) => {
        if(process.env.NODE_ENV != 'production') { 
            console.log("ALL OTHER POSTS REJECT");  
        }     
        res.status(403).end();
    });//END OF GET
    // END OF ROUTE

module.exports = router ;