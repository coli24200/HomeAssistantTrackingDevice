// Configure TinyGSM library 
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

#include <TinyGPSPlus.h>
#include <TinyGsmClient.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include "IP5306.h"
//watch dog 
#include <esp_task_wdt.h>
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


// LED
#define LED_GPIO             13
#define LED_ON               HIGH
#define LED_OFF              LOW

// GPS
#define GPS_GPIO             18
#define GPS_ON               HIGH
#define GPS_OFF              LOW


//Car battery level
#define BATT_CAR_LEVEL       14


//Car external power for wake up pin
#define BATT_CAR             12

//Module battery level
#define BATT_LEVEL           35



// battery controller address
//#define IP5306_ADDR          0x75
//#define IP5306_REG_SYS_CTL0  0x00

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to SIM800 module)
#define SerialAT Serial1

/*
   TinyGPSPlus (TinyGPSPlus) object.
   It requires the use of SoftwareSerial, and assumes that you have a
   9600-baud serial GPS device hooked up on pins 32(rx) and 33(tx).
*/
static const int RXPin = 32, TXPin = 33;
static const uint32_t GPSBaud = 9600;


// I2C for SIM800 (to keep it running when powered from battery)
//TwoWire I2CPower = TwoWire(0);



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
*         internal USER SETTINGS                           *
***********************************************************/
// TIME TO SLEEP
#define TIME_TO_SLEEP_BAT  3600    /* Time ESP32 will go to sleep (in seconds) 3600 seconds = 1 hour    , 300 = 5min, 600=10min */
// GPS  LOOP to try to find Sats before go to sleep again, avoid battery drain on spots without Sat view
#define GPS_TIME_REQUEST_ON_POWER 120 // time in seconds to wait until next request on Car power (Beware on WDT to be more than this timer)
//4 minutes Watch Dog Timer to avoid SW hangs
#define WDT_TIMEOUT 240
// GPS loop attemps to avoid battery drain 
#define GPS_LOOP  200
// GPS attemps after car power shut down 
#define GPS_ON_BAT  2
/***********************************************************
*                                             *
***********************************************************/


// State Machine
/*
0-search for GPS
1-send grps data
2-send wifi data
3-sleep 
*/
//init state
volatile int estado=0;
//init gps loop 
int gpsLoop = GPS_LOOP;
// battery level (Lithium)
float voltBat=0;
// battery level car 
float voltBatCar=0;
// flag to detect car power if needed
bool batcarflag=false;
// attempts after car shutdown
int gpsonbat = GPS_ON_BAT;

/***********************************************************
*                        Vars                              *
***********************************************************/

//Init vars
String latitude="";
String longitude="";
String speed="";
String altitude="";
String timedate="";



/***********************************************************
*                  SETUP                                   *
***********************************************************/
void setup()
{
  pinMode( BATT_LEVEL, INPUT) ;
  pinMode( BATT_CAR_LEVEL, INPUT) ;
  pinMode( BATT_CAR, INPUT) ;
  //watch dog
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
  
  //wake on voltage from car (Car start , connected to ACC)
  //Only RTC IO can be used as a source for external wake
  //pins: 0,2,4,12-15,25-27,32-39.
  // wake up everytime this pin is with 1 (3v3), does not have RISING function
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_12,1); //1 = High, 0 = Low
  
  
  // Set serial monitor debugging window baud rate to 115200
  SerialMon.begin(115200);
  ss.begin(GPSBaud);
  SerialMon.println(F("########################## init ##########################")); 


  Wire.begin(); //NEED to IP5306 lib
  // Keep power when running from battery
  IP5306_SetBoostOutputEnabled(1);
  
  printIP5306Stats();
  SerialMon.println(String("IP5306 KeepOn :") + (IP5306_GetBoostOutputEnabled()?"Enabled":"Disabled"));
  SerialMon.println("Vin Current: "+ String((IP5306_GetVinCurrent() * 100) + 50)+" mA");
  getBatteryVoltage();
  getCarBatteryVoltage();

  // mostrar led ok para saber que está Vivo
  pinMode(LED_GPIO, OUTPUT);
  digitalWrite(LED_GPIO, LED_ON);
  
  //GPS
   pinMode(GPS_GPIO, OUTPUT);
   digitalWrite(GPS_GPIO, GPS_ON);
   
  //modem	
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
  SerialMon.print("Modem: ");
  SerialMon.println(modemInfo);
  
  
  // SETUP MODEM //
  
  //Modem STATUS LIGHT 
  String res2="";
  modem.sendAT(GF ("+CNETLIGHT=1"));
  if (modem.waitResponse (1000L, res2) !=1){
	  if (DEBUG){SerialMon.println(GF("ERROR +CNETLIGHT=1"));}
  }else{
	  if (DEBUG){SerialMon.println("TURN GSM MODULE LED ON-> "+res2);}   	  
  }  
  res2="";	    
  delay(1000);
  SerialMon.println(F("Wait for network..."));
  if (!modem.waitForNetwork(240000L)) {
    SerialMon.println(F(" fail wait for network"));
  }
   
  delay(1000);
  
  if (modem.isNetworkConnected()) {
    SerialMon.println(F("Network is connected"));
  }else{
    SerialMon.println(F("Failed network connect"));
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
  printIP5306Stats();
  getBatteryVoltage();
  getCarBatteryVoltage();  
  
  
  SerialMon.println(F("GPS ON"));
  digitalWrite(GPS_GPIO, GPS_ON);
  // Configure the wake up source as timer wake up  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_BAT * uS_TO_S_FACTOR);
	
}




/***********************************************************
*                  LOOP                                    *
***********************************************************/
void loop() // run over and over
{ 
  automatoMain();
  esp_task_wdt_reset();
  
}



/***********************************************************
*                  Automato                                *
***********************************************************/

void automatoMain() {
  switch(estado) {
	  case 0:// detect GPS
		digitalWrite(LED_GPIO, LED_OFF);
		delay(100);
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
				  --gpsLoop;	
				  SerialMon.println("-GPS NOK-  try# "+String(gpsLoop));
				  if (gpsLoop==0) 
					  {
						  SerialMon.println(F("End of tries...go to sleep")); 
						  estado=3;
						  break;
					  }
				  else
				      {
						  estado=0;
					  }
				} 
							
			}
		}	
	  		
		break;

	  case 1:// send GSM (GPRS)	  

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
                
				if (false){  //ESPECIAL DEBUG , set it to true if needed
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
					if (DEBUG){  SerialMon.println("  VinCurrent: "+ String((IP5306_GetVinCurrent() * 100) + 50)); }  
					if (DEBUG){SerialMon.println(server);}
					if (DEBUG){SerialMon.println(port);}
					SerialMon.println(F("server NOK"));

				}else{
				 	SerialMon.println("server OK : "+String(server));
					if(upload_data_buffer()){
					   SerialMon.println(F("--> UPLOAD OK <--"));
                    }else{
					   SerialMon.println(F("UPLOAD NOK"));
					}						
				}			
				
		}
		delay(1000);  
		estado=3; 
		break;

	  case 2:// STATE TODO SMS WARNING CAR BATT
	    if (DEBUG){ 				
				SerialMon.println("  VinCurrent: "+ String((IP5306_GetVinCurrent() * 100) + 50)+" mA");
		 }
	    estado=3;

		break;

	  case 3:// DEEP SLEEP or Wait			
		 
		getCarBatteryVoltage();
		
		
        if(!getPowerSourceCar()){
		 SerialMon.println("# Powered by module battery, last tries :"+String(gpsonbat)); 
         --gpsonbat;				 
		}
			
		if(gpsonbat==0){
		 printIP5306Stats();	
		 getBatteryVoltage();		
		 //LED OFF
		 digitalWrite(LED_GPIO, LED_OFF);
		 //shutdown GPS
		 digitalWrite(GPS_GPIO, GPS_OFF);
		 //shutdown MODEM
		 digitalWrite(MODEM_POWER_ON, LOW);
		 //ESP DEEP SLEEP
		 // retrive POWER source Car or Bat, pin 12 has it 	
		 SerialMon.println("#### ESP+GSM to DEEP SLEEP: "+String(TIME_TO_SLEEP_BAT)+" Seconds");
		 esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_BAT * uS_TO_S_FACTOR);
		 esp_deep_sleep_start();
		}	
		
		SerialMon.println("#### ESP+GSM to wait: "+String(GPS_TIME_REQUEST_ON_POWER)+" Seconds");
		delay(GPS_TIME_REQUEST_ON_POWER*1000);
		gpsLoop=0;
		estado=0;  
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

			  SerialMon.print(F("  Date: "));
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
				SerialMon.print(F("INVALID"));
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
				SerialMon.print(F("INVALID"));
			  }

			  SerialMon.print(F(" Sats: "));
			  SerialMon.print(gps.satellites.value());

			  SerialMon.println();
}



/// UPLOAD DATA TO BUFFER BEFORE POST ////
void make_post(){

  String postdata = "key="+key+"&latitude=" + latitude + "&"+"longitude="+longitude+"&device=Berlingo&accuracy=10&battery="+voltBat+"v&speed="+speed+"&altitude=100"+"&carbattery="+voltBatCar+"v\r\n";
  gsmClient.print(String("POST ") + api + " HTTP/1.1\r\n");
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



//IP5306 checks
void printIP5306Stats(){
    bool usb = IP5306_GetPowerSource();
	//SerialMon.println("usb:"+ String(usb));
    bool full = IP5306_GetBatteryFull();
	//SerialMon.println("full:"+ String(full));
    uint8_t leds = IP5306_GetLevelLeds();
    SerialMon.println("IP5306: Power Source: "+String(usb?"USB":"BATTERY")+", Battery State: "+String(full?"CHARGED":(usb?"CHARGING":"DISCHARGING"))+", Battery Available: "+String(IP5306_LEDS2PCT(leds))+"\n");
}

// AVOID USING IT , just for IP5306 debug
void printIP5306Settings(){
    Serial.println("IP5306 Settings:");
    Serial.printf("  KeyOff: %s\n", IP5306_GetKeyOffEnabled()?"Enabled":"Disabled");
    Serial.printf("  BoostOutput: %s\n", IP5306_GetBoostOutputEnabled()?"Enabled":"Disabled");
    Serial.printf("  PowerOnLoad: %s\n", IP5306_GetPowerOnLoadEnabled()?"Enabled":"Disabled");
    Serial.printf("  Charger: %s\n", IP5306_GetChargerEnabled()?"Enabled":"Disabled");
    Serial.printf("  Boost: %s\n", IP5306_GetBoostEnabled()?"Enabled":"Disabled");
    Serial.printf("  LowBatShutdown: %s\n", IP5306_GetLowBatShutdownEnable()?"Enabled":"Disabled");
    Serial.printf("  ShortPressBoostSwitch: %s\n", IP5306_GetShortPressBoostSwitchEnable()?"Enabled":"Disabled");
    Serial.printf("  FlashlightClicks: %s\n", IP5306_GetFlashlightClicks()?"Long Press":"Double Press");
    Serial.printf("  BoostOffClicks: %s\n", IP5306_GetBoostOffClicks()?"Double Press":"Long Press");
    Serial.printf("  BoostAfterVin: %s\n", IP5306_GetBoostAfterVin()?"Open":"Not Open");
    Serial.printf("  LongPressTime: %s\n", IP5306_GetLongPressTime()?"3s":"2s");
    Serial.printf("  ChargeUnderVoltageLoop: %.2fV\n", 4.45 + (IP5306_GetChargeUnderVoltageLoop() * 0.05));
    Serial.printf("  ChargeCCLoop: %s\n", IP5306_GetChargeCCLoop()?"Vin":"Bat");
    Serial.printf("  VinCurrent: %dmA\n", (IP5306_GetVinCurrent() * 100) + 50);
    Serial.printf("  VoltagePressure: %dmV\n", IP5306_GetVoltagePressure()*14);
    Serial.printf("  ChargingFullStopVoltage: %u\n", IP5306_GetChargingFullStopVoltage());
    Serial.printf("  LightLoadShutdownTime: %u\n", IP5306_GetLightLoadShutdownTime());
    Serial.printf("  EndChargeCurrentDetection: %u\n", IP5306_GetEndChargeCurrentDetection());
    Serial.printf("  ChargeCutoffVoltage: %u\n", IP5306_GetChargeCutoffVoltage());
    Serial.println();
}

//MODULE BAT LEVEL
void getBatteryVoltage(){  // need finetune does not collect correct value
	   //float voltBat=analogRead(BATT_LEVEL)*2 / 1135;
	   voltBat=analogRead(BATT_LEVEL);// mV
	   voltBat=(voltBat*2.0)/1.155; //adapt
	   voltBat=voltBat/1000; //mV to V
       SerialMon.println("Battery Voltage= "+String(voltBat)+" Volts");

/*
Voltage devider with 2x 100KOhm from T-Call HW

4.2v
|
# 100k
|
-  2.1v
|
#100k
|
_
-

*/	   
}	

// CAR BAT LEVEL 
float getCarBatteryVoltage(){  // need finetune does not collect correct value
	   voltBatCar=analogRead(BATT_CAR_LEVEL);// mV
	   voltBatCar = (voltBatCar * 3.3 ) / (4095);
	   //voltBat=(voltBat*2.0)/1.155; //adapt
	   //voltBat=voltBat/1000; //mV to V
	   SerialMon.println("Car Battery Voltage= "+String(voltBatCar)+" Volts");	
	   return voltBatCar;           
}   

bool getPowerSourceCar(){
	  if(analogRead(BATT_CAR)>2){
		  batcarflag=true;	
		  SerialMon.println("PowerSource Connected to car battery");
	  }else{ 
	      batcarflag=false;
		  SerialMon.println("PowerSource Connected to module battery ");
	  }
      return batcarflag ;
}	




