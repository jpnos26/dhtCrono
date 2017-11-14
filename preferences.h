#include <Arduino.h>

//LOCAL CRONO FUNCTION
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define CRONO 1

//CLOCK
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define CLOCK 1
#define TIME_ZONE 1  // Central European Time
#define DAYLIGHTSAVINGTIME 0 // Ora legale




//OFFSET DHT
#define OFFSET_DHT -0.3

//WIFI CONNECTION
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//DHCP
//1 Use DHCP IP
//0 Use Static IP Address

#define dhcp 0

//STATIC IP

// use commas between number
#define STATIC_IP 192,168,1,130
#define STATIC_SUBNET 255,255,255,0
#define STATIC_IP_GW 192,168,1,1

// **** Define the WiFi name and password ****

#define Wifi_ssid           "wifi sid"
#define Wifi_password       "pass wifi"  
#define HostName            "dht-thermo"
#define Http_username       "admin"
#define Http_password       "admin"
#define myrealm             "test"


////// ***** Define AP *****

#define ap_ssid             "thermo_dht"
#define ap_password         "password ap"


