// DATA FOR GSM CON 
extern const char apn[]      = "internet"; // APN (depends on the GSM provider, google it) this example is for meo/uzo Portugal
extern const char gprsUser[] = ""; // GPRS User
extern const char gprsPass[] = ""; // GPRS Password
// SIM card PIN (leave empty, if not defined or disabled in SIM card)
extern const char simPIN[]   = ""; 


//SERVER
extern const char server[] = "<your url server>";
extern const char api[] = "<your api url>";
extern const int  port = 80;


//Device vars: name and passkey, if do not match with the server it will be rejected 
extern const String device="<your device name>";
extern const String key="<password>";

// DEBUG enhanced output
extern const bool DEBUG =false;
