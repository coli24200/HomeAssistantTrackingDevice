// Configure TinyGSM library 
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

#include <TinyGPSPlus.h>
#include <TinyGsmClient.h>
#include <SoftwareSerial.h>
#include <Wire.h>
// all user vars in this file
#include "vars.h"

// Define the serial console for debug prints, if needed
//#define DUMP_AT_COMMANDS

// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22

// LED
#define LED_GPIO             13
#define LED_ON               HIGH
#define LED_OFF              LOW


// ligar outros sensores
#define I2C_SDA_2            18
#define I2C_SCL_2            19

// battery controller address
#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00



// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to SIM800 module)
#define SerialAT Serial1

/*
   TinyGPSPlus (TinyGPSPlus) object.
   It requires the use of SoftwareSerial, and assumes that you have a
   9600-baud serial GPS device hooked up on pins 35(rx) and 34(tx).
*/
static const int RXPin = 35, TXPin = 34;
static const uint32_t GPSBaud = 9600;

// I2C for SIM800 (to keep it running when powered from battery)
TwoWire I2CPower = TwoWire(0);

// The TinyGPSPlus object
TinyGPSPlus gps;
// The serial connection to the GPS module
SoftwareSerial ss(RXPin, TXPin);


#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif


// GSM client
TinyGsmClient gsmClient(modem,1);


#define uS_TO_S_FACTOR 1000000UL   /* Conversion factor for micro seconds to seconds */

/***********************************************************
*                  USER SETTINGS                           *
***********************************************************/
// TIME TO SLEEP
#define TIME_TO_SLEEP  600       /* Time ESP32 will go to sleep (in seconds) 3600 seconds = 1 hour    , 300 = 5min, 600=10min */


// State Machine
/*
0-search for GPS
1-send grps data
2-send wifi data
3-sleep 
*/
//init state
int estado=0; 


/***********************************************************
*                                                          *
***********************************************************/

//Init vars
String latitude="";
String longitude="";
String speed="";
String altitude="";
String timedate="";
String batteryLevel="";



// Function needed to allow the Module to work under 4.0Volts of battery, 
// if not set the battery does not feed the module when below 4.0Volts

bool setPowerBoostKeepOn(int en){
  I2CPower.beginTransmission(IP5306_ADDR);
  I2CPower.write(IP5306_REG_SYS_CTL0);
  if (en) {
    I2CPower.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    I2CPower.write(0x35); // 0x37 is default reg value
  }
  return I2CPower.endTransmission() == 0;
}





/***********************************************************
*                  SETUP                                   *
***********************************************************/
void setup()
{
  // Set serial monitor debugging window baud rate to 115200
  SerialMon.begin(115200);
  ss.begin(GPSBaud);
  if (DEBUG){
   SerialMon.print(F("TinyGPSPlus library v. ")); SerialMon.println(TinyGPSPlus::libraryVersion());
   SerialMon.println();
  }
  // Start I2C communication
  I2CPower.begin(I2C_SDA, I2C_SCL, 400000);

  delay(2000);

  // Keep power when running from battery
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

  // mostrar led ok para saber que está Vivo
  pinMode(LED_GPIO, OUTPUT);
  digitalWrite(LED_GPIO, LED_ON);
	
  pinMode(MODEM_RST, OUTPUT);
  digitalWrite(MODEM_RST, HIGH);

  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);

  // Turn on the Modem power first
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Pull down PWRKEY for more than 1 second according to manual requirements
  digitalWrite(MODEM_PWKEY, HIGH);
  delay(100);
  digitalWrite(MODEM_PWKEY, LOW);
  delay(1000);
  digitalWrite(MODEM_PWKEY, HIGH);


  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(2000);

  // Restart SIM800 module, it takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  //  modem.restart(); 
  
  delay(1000);
  modem.init();
  
  
  // Unlock your SIM card with a PIN if needed (NOT USED  SIM used is unlocked)
  if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
    modem.simUnlock(simPIN);
  }
  
  
  String modemInfo = modem.getModemInfo();
  Serial.print("Modem: ");
  Serial.println(modemInfo);
  
  
  // SETUP MODEM //
  
  //Modem STATUS LIGHT 
  String res2="";
  modem.sendAT(GF ("+CNETLIGHT=1"));
  if (modem.waitResponse (1000L, res2) !=1){
	  if (DEBUG){SerialMon.println(GF("#CO#  ERROR"));}
  }else{
	  if (DEBUG){SerialMon.println("#CO# +CNETLIGHT=1 - TURN GSM MODULE LED -> "+res2);   }   	  
  }  
  res2="";	    
  delay(1000);

  if (!modem.waitForNetwork(240000L)) {
    SerialMon.println(" fail wait for network");
  }
   
  delay(1000);
  
  if (modem.isNetworkConnected()) {
    SerialMon.println("Network is connected");
  }else{
    SerialMon.println("Failed network connect");
  }
  
  // SIM INFO
 if (DEBUG){ 
  String ccid = modem.getSimCCID();
  SerialMon.println("CCID:"+ ccid);

  String imei = modem.getIMEI();
  SerialMon.println("IMEI:"+ imei);

  String imsi = modem.getIMSI();
  SerialMon.println("IMSI:"+ imsi);

  String cop = modem.getOperator();
  SerialMon.println("Operator:"+ cop);
 }


  // Configure the wake up source as timer wake up  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
		
}




/***********************************************************
*                  LOOP                                    *
***********************************************************/
void loop() // run over and over
{ 
  automatoMain();
}



/***********************************************************
*                  Automato                                *
***********************************************************/

void automatoMain() {
  switch(estado) {
	  case 0:// detect GPS
		digitalWrite(LED_GPIO, LED_ON);
	   // displays information every time a new sentence is correctly encoded.
		while (ss.available() > 0){
			if (gps.encode(ss.read())){
			    displayInfo();			
				delay(1000);    
				if(gps.location.isValid()){
					SerialMon.println(F("#### GPS OK collect vars ####"));		 
					latitude=String(gps.location.lat(),6);
					longitude=String(gps.location.lng(),6);
					speed=String(gps.speed.kmph());
					 
					if (gps.altitude.isUpdated()){
					   altitude=String(gps.altitude.meters());
					}else {
					   altitude="0"; 	
					}	
					
					 if (gps.date.isValid() && gps.time.isValid()){
						 timedate=String(gps.date.day())+"/"+String(gps.date.month())+"/"+String(gps.date.year());
						 timedate=timedate+" "+String(gps.time.hour())+":"+String(gps.time.minute())+":"+String(gps.time.second())+" UTC";
						 }else{
							timedate="invalid"; 
						 }		 
							 

					 SerialMon.println("OUT:"+latitude+" "+longitude+" "+" "+altitude+" "+speed+" "+timedate);
					
					estado=1;  // to change new state
				}else{
				  //displayInfo();	
				  SerialMon.println(F("--------GPS NOK repeat request --------"));	
				  estado=0;
				} 
							
			}
		}	
	  		
		break;

	  case 1:// GSM (GPRS)	  
		delay(1000);  
		SerialMon.print(F("Connecting to APN: "));
		SerialMon.println(apn);

		SerialMon.println(F(" ## Waiting for network... ## "));
		if (!modem.waitForNetwork()) {
			SerialMon.println(F(" ## Failed wait for network ##"));
		}
		
		if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
				SerialMon.println(F(" ## Failed gprs connect ##"));
		}
		else {
				if (modem.isGprsConnected()) { 
				  SerialMon.println(F(" ## GPRS is connected ##")); 
				}
             
				if (false){  //ESPECIAL DEBUG , set it to true if needed a ping
					String res="";
					modem.sendAT("+CIPPING=\""+String(server)+"\"");
					if (modem.waitResponse(10000L,res) != 1) {
					  SerialMon.println(GF("## ERROR PING"));	
					}else{		
					  SerialMon.println(GF("## +CIPPING: ")+res); 
					  res="";
					}
					res="";				
					delay(2000);
			    }
				
				if(!gsmClient.connect(server, port)){
					SerialMon.println(server);
					SerialMon.println(port);
					SerialMon.println(F("server NOK"));

				}else{
				 	SerialMon.println("server OK : "+String(server));
					if(upload_data_buffer()){
					   SerialMon.println(F("--- %%%% UPLOAD OK %%%% ---"));
                    }else{
					   SerialMon.println(F("UPLOAD NOK"));
					}						
				}			
				
		}
		delay(1000);  
		estado=3; 
		break;

	  case 2:// TODO: SMS WARNING CAR BATT
	  
	    estado=3;

		break;

	  case 3:// DEEP SLEEP
        
		digitalWrite(LED_GPIO, LED_OFF);
		SerialMon.println(F("###########  SLEEP ###########"));
		esp_deep_sleep_start();
		estado=0;  //never read , it will wakeup on estado=0
		break;		

	  default:
		break;
	  }
	}
		
	


/***********************************************************
*                  AUX                                    *
***********************************************************/
//DISPLAY INFO
	
void displayInfo()
			{
			  SerialMon.print(F("Location: ")); 
			  if (gps.location.isValid())
			  {
				SerialMon.print(gps.location.lat(), 6);
				SerialMon.print(F(","));
				SerialMon.print(gps.location.lng(), 6);
			  }
			  else
			  {
				SerialMon.print(F("INVALID FIX"));
			  }

			  SerialMon.print(F("  Date/Time: "));
			  if (gps.date.isValid())
			  {
				 SerialMon.print(gps.date.day());
				SerialMon.print(F("/"));
				SerialMon.print(gps.date.month());
				SerialMon.print(F("/")); 
				SerialMon.print(gps.date.year());
			  }
			  else
			  {
				SerialMon.print(F("INVALID DATE"));
			  }

			  SerialMon.print(F(" "));
			  if (gps.time.isValid())
			  {
				if (gps.time.hour() < 10) SerialMon.print(F("0"));
				SerialMon.print(gps.time.hour());
				SerialMon.print(F(":"));
				if (gps.time.minute() < 10) SerialMon.print(F("0"));
				SerialMon.print(gps.time.minute());
				SerialMon.print(F(":"));
				if (gps.time.second() < 10) SerialMon.print(F("0"));
				SerialMon.print(gps.time.second());
				SerialMon.print(F("."));
				if (gps.time.centisecond() < 10) SerialMon.print(F("0"));
				SerialMon.print(gps.time.centisecond());
			  }
			  else
			  {
				SerialMon.print(F("INVALID TIME"));
			  }

			  SerialMon.print(F(" Sats: "));
			  SerialMon.print(gps.satellites.value());

			  SerialMon.println();
}



/// UPLOAD DATA TO BUFFER BEFORE POST ///
void make_post(){
  String postdata = "key="+key+"&latitude=" + latitude + "&"+"longitude="+longitude+"&device=Berlingo&accuracy=10&battery=20&"+"speed="+speed+"&altitude=100"+"\r\n";gsmClient.print(String("POST ") + api + " HTTP/1.1\r\n");
  if (DEBUG){SerialMon.println(String("POST ") + api + " HTTP/1.1\r\n");}
  gsmClient.print(String("Host: ") + server + "\r\n");
  if (DEBUG){SerialMon.println(String("Host: ") + server + "\r\n");}
  gsmClient.print("Connection: close\r\n");
  gsmClient.print("Content-Type: application/x-www-form-urlencoded\r\n");
  gsmClient.print(String("Content-Length: ") + String(postdata.length()) + String("\r\n\r\n") );
  gsmClient.print(postdata);

}


bool upload_data_buffer(){

  SerialMon.println();
  SerialMon.println("Connecting to " + String(server)+":"+port);
  if (!gsmClient.connect(server, port)) {
    SerialMon.println("... failed");
    return false;
  } else {

    SerialMon.println("...start make post:");
       
    make_post();
    
    // Wait for data to arrive
    uint32_t startS = millis();
    while (gsmClient.connected() && !gsmClient.available() && millis() - startS < 30000L) {
          delay(100);
     };
    
     // Read data
     startS = millis();
     String response = "";
     bool  response_ok = false;
     bool  response_first_line = false;
     while (gsmClient.connected() && millis() - startS < 5000L) {
       while (gsmClient.available()) {
          char c = gsmClient.read();
          if(!response_first_line && !response_ok){
            if(c == '\n'){
                response_first_line = true;
                if(response.indexOf("200 OK") >=0){
				  response_ok=true;	
				  SerialMon.println(" ");
                  SerialMon.println("200 OK <--- CONFIRMATION");
                }
              }
              response += c;
          }
    
        SerialMon.write(c);
    
        startS = millis();
       }
     }

     if(!response_ok){
          gsmClient.stop();
          return false;
     }
        
      SerialMon.println();
    }
  
  gsmClient.stop();
  return true;
  
}


