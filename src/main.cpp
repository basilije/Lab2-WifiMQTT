/***********************************************************************************
* Project: Lab2 - Exercise #3 - Wifi and MQTT
* Class: CIT324 - Networking for IoT
* Author: Vasilije Mehandzic
*
* File: main.cpp
* Description: Main file for Exercise #3
* Date: 12/3/2020
**********************************************************************************/

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "serial-utils.h"
#include "wifi-utils.h"
#include "string-utils.h"
#include "whiskey-bug.h"
// next two includes are for disabling brownout detector https://github.com/espressif/arduino-esp32/issues/863
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// operation modes enum
enum operation_type { 
    OPERATION_TYPE_NORMAL,
    OPERATION_TYPE_UDP_BROADCAST,
    OPERATION_TYPE_MQTT_MODE,
};

// the global variable needed
const unsigned int UDP_PORT = 8888;
const unsigned int UDP_PACKET_SIZE = 64;
const char UDP_MESSAGE[] = "^^^^^^<| DoN'T MoVE DoN'T SToP {<waka waka waka waka>} |>^^^^^^";
const char MQTT_BROKER_SERVER[] = "maqiatto.com";
const uint16_t MQTT_BROKER_PORT = 1883;
const char MQTT_BROKER_CLIENT_ID[] = "WhiskeyBug";
const char MQTT_TEMPERATURE_TOPIC[] = "vasske@gmail.com/Temperature";
const char MQTT_PRESSURE_TOPIC[] = "vasske@gmail.com/Pressure";
const char MQTT_ALCOHOL_TOPIC[] = "vasske@gmail.com/AlcoholContent";
const char MQTT_BROKER_USERNAME[] = "vasske@gmail.com";
const char MQTT_BROKER_PASSWORD[] = "emkjutitiPASS";
unsigned int local_port = 8888;
int serial_read, incoming_byte, num_ssid, key_index = 0, current_mode_of_operation = OPERATION_TYPE_NORMAL;
char ch_temp[16], ch_pres[16], ch_alco[16];
WiFiUDP Udp;
IPAddress remote_ip;
byte mac[6];
wl_status_t status = WL_IDLE_STATUS; 
WhiskeyBug whiskey_bug;
WiFiClient espClient;
PubSubClient client(espClient);
time_t seconds = time(NULL);
// make some pointers to fixed const chars
const char* P_UDP_MESSAGE = UDP_MESSAGE;
const char* P_MQTT_BROKER_SERVER = MQTT_BROKER_SERVER;
const char* P_MQTT_BROKER_CLIENT_ID = MQTT_BROKER_CLIENT_ID;
const char* P_MQTT_TEMPERATURE_TOPIC = MQTT_TEMPERATURE_TOPIC;
const char* P_MQTT_PRESSURE_TOPIC = MQTT_PRESSURE_TOPIC;
const char* P_MQTT_ALCOHOL_TOPIC = MQTT_ALCOHOL_TOPIC;

/***********************************************************************************
* Purpose: Print the main menu content.
* No arguments, no returns
**********************************************************************************/
void printMainMenu() {  
  Serial.print("A – Display MAC address\nL - List available wifi networks\nC – Connect to a wifi network\nD – Disconnect from the network\nI – Display connection info\nM – Display the menu options\nV - change the current mode to ");
  if (current_mode_of_operation == OPERATION_TYPE_NORMAL)
    Serial.print("UDP_BROADCAST\n");
  else
    Serial.print("NORMAL\n");
  Serial.print("Q - change the current mode to MQTT mode\n");    
  }

/***********************************************************************************
* Purpose: Print on the serial port the mac address in use.
* No arguments, no returns 
**********************************************************************************/
void printMacAddresses() {  
    WiFi.macAddress(mac);  // get your MAC address
    Serial.println(macAddressToString(mac));  // and print  your MAC address
}

/***********************************************************************************
* Purpose: Scan and detailed serial port print of the Network APs found
* No arguments, no returns
**********************************************************************************/
void networkList() {
  num_ssid = WiFi.scanNetworks(); 
  if (num_ssid > -1) {
    for (int this_net = 0; this_net < num_ssid; this_net++) {     
      Serial.print(this_net + 1);  // print the network number      
      Serial.println(". " + WiFi.SSID(this_net) + " [" + wifiAuthModeToString(WiFi.encryptionType(this_net)).c_str() + "]  (" + WiFi.RSSI(this_net)+" dBm)");  // print the ssid, encryption type and rssi for each network found:
    }
  }
  else
    Serial.println("Couldn't get a wifi connection!");
}

/***********************************************************************************
* Purpose: Connect to the chosen network from the list 
* No arguments, no returns
**********************************************************************************/
void connect() {  
  String ssid = WiFi.SSID(std::atoi(serialPrompt("\nChoose Network: ", 3).c_str()) - 1);
  String network_password = serialPrompt("Password: ", 42);  // that's it
  const char* cch_ssid = ssid.c_str();
  const char* cch_net_pss = network_password.c_str();
  Serial.print("Connecting to "); Serial.print(cch_ssid); Serial.print("...\n\n");
  WiFi.begin(cch_ssid, cch_net_pss);
  delay(2000);
  Serial.println(wifiStatusToString(WiFi.status()).c_str()); 
}

/***********************************************************************************
* Purpose: Disconnect WiFi and print the current status
* No arguments, no returns
**********************************************************************************/
void disconnect() {
  Serial.print("Disonnecting... ");
  WiFi.disconnect();
  delay(2000);
  status = WiFi.status();
  Serial.print("Current status: ");
  Serial.println(wifiStatusToString(status).c_str());   
}

/***********************************************************************************
* Purpose: Print the connection info
* No arguments, no returns
**********************************************************************************/
void connectionInfo() {
  Serial.print("Status:\t\t");  Serial.println(wifiStatusToString(WiFi.status()).c_str());
  Serial.print("Network:\t");  Serial.println(WiFi.SSID());
  Serial.print("IP Address:\t");  Serial.println(WiFi.localIP());
  Serial.print("Subnet Mask:\t");  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway:\t");  Serial.println(WiFi.gatewayIP());
} 

/***********************************************************************************
* Purpose: Change the operation mode to normal
* No arguments, no returns 
**********************************************************************************/
void changeModeToNormal() {
  current_mode_of_operation = OPERATION_TYPE_NORMAL;
  Serial.println("NORMAL MODE");
}

/***********************************************************************************
* Purpose: Change the operation mode to udp broadcast
* No arguments, no returns
**********************************************************************************/
void changeModeToUDP() {
  current_mode_of_operation = OPERATION_TYPE_UDP_BROADCAST;
  Serial.println("UDP_BROADCAST MODE\nESC - change the mode to NORMAL");
}

/***********************************************************************************
* Purpose: Switch between two main operation modes
* No arguments, no returns 
**********************************************************************************/
void changeMode() {
  if (current_mode_of_operation == OPERATION_TYPE_NORMAL)
    changeModeToUDP();
  else 
    changeModeToNormal();
}

/***********************************************************************************
* Purpose: Change the operation mode to mqtt
* No arguments, no returns 
**********************************************************************************/
void changeModeToMQTT() {
  current_mode_of_operation = OPERATION_TYPE_MQTT_MODE;
  Serial.print("\nMQTT MODE\nX - change the mode to NORMAL\n");
}

/***********************************************************************************
* Purpose: Check if the key "x" is pressed
* No arguments, no returns, affects some globals
**********************************************************************************/
void checkForXPressed() {
  if (Serial.available() > 0)
    serial_read = Serial.read();
  if ((serial_read == 88)||(serial_read == 120))  //  if X or x is pressed
    changeModeToNormal();
}

/***********************************************************************************
* Purpose: Check if the key "Esc" is pressed
* No arguments, no returns, affects some globals
**********************************************************************************/
void checkForESCPressed() {
  if (Serial.available() > 0)
      serial_read = Serial.read();
  if (serial_read == 27)
    changeModeToNormal();
}

/***********************************************************************************
* Purpose: Send earlies specified UDP packet with possible loop break with ESC key
* No arguments, no returns
**********************************************************************************/
void sendUDP() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nYou need to connect first! Switching back to the normal mode.\n");
    changeModeToNormal();
  }
  else {
    remote_ip = WiFi.gatewayIP();
    remote_ip[3] = 255;    
    checkForESCPressed();

    // exit from loop every 10 seconds
    while (time(NULL) - seconds < 10 && current_mode_of_operation == OPERATION_TYPE_UDP_BROADCAST)
      checkForESCPressed();      

    // and finally send it
    Udp.begin(local_port);
    Udp.beginPacket(remote_ip, local_port);

    for (int i = 0; i < UDP_PACKET_SIZE; i++)
      Udp.write(P_UDP_MESSAGE[i]);

    Udp.endPacket();
    Udp.stop(); 
    seconds = time(NULL);   
  }
}

/***********************************************************************************
* Purpose: Send the payload to topic
* Arguments: topic - ; payload
* No returns
**********************************************************************************/
void myMQTT(const char* topic, const char* payload) {
  client.setServer(P_MQTT_BROKER_SERVER, MQTT_BROKER_PORT);

  if (client.connect(P_MQTT_BROKER_CLIENT_ID, MQTT_BROKER_USERNAME, MQTT_BROKER_PASSWORD)) {
      if (client.publish(topic, payload)) {}
      else {
        Serial.print("ERROR: publishing failed with state ");
        Serial.print(client.state());
        delay(2000);
      }
  } else {
    Serial.print("ERROR: connect failed with state ");
    Serial.print(client.state());
    delay(2000);
  }
}


/***********************************************************************************
* Purpose: Setup the port, detector and connect to the WhiskeyBug
* No arguments, no returns
**********************************************************************************/
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin(115200,  SERIAL_8N1); // initialize the serial port
  printMainMenu();
  whiskey_bug = WhiskeyBug();
}

/***********************************************************************************
* Purpose: The main loop
* No arguments, no returns
**********************************************************************************/
void loop() {
  Serial.println("–––––––––––––––––––––––––––––––––––––––––");
  delay(1000); 
  
  switch (current_mode_of_operation)
  {
    case OPERATION_TYPE_MQTT_MODE:
      if (WiFi.status() == WL_CONNECTED) {       
        checkForXPressed();  
        // exit from loopmore than every second after previous
        while (time(NULL) - seconds < 1 && current_mode_of_operation == OPERATION_TYPE_MQTT_MODE) {
          checkForXPressed();
        }
        // Read the sensors from the whiskey bug, convert it to char[] and send it
        sprintf(ch_temp, "%f", whiskey_bug.getTemp());
        sprintf(ch_pres, "%f", whiskey_bug.getPressure());
        sprintf(ch_alco, "%f", whiskey_bug.getAlcoholContent());
        myMQTT(P_MQTT_TEMPERATURE_TOPIC, ch_temp);
        myMQTT(P_MQTT_PRESSURE_TOPIC, ch_pres);
        myMQTT(P_MQTT_ALCOHOL_TOPIC, ch_alco);
        seconds = time(NULL);
      }
      else
      {
        Serial.println("\nYou need to connect first! Will not try mqtt operations. Switching back to normal mode.\n");
        changeModeToNormal();        
      }
      
      break;

    case OPERATION_TYPE_UDP_BROADCAST: 
      sendUDP();
      break;

    case OPERATION_TYPE_NORMAL:
      incoming_byte = int(*serialPrompt("\nChoice: ", 1).c_str()); // read the key, convert to const char* and back to ascii int
      Serial.println("");
      
      if (incoming_byte > 96)  // to_upper string int function, based on ascii table
        incoming_byte -= 32; 

      switch (incoming_byte)  // to_upper calculation and next switch are based on http://www.asciitable.com/
      {        
        case 65:  // "A"=65; "a"=97
          printMacAddresses();
          break;
        
        case 76:  // "L"=76; "l"=108
          networkList();      
          break;
        
        case 67:  // "C"=67; "c"=99
          networkList();
          connect();
          break;
        
        case 68:  // "D"=68; "d"=100
          disconnect();
          break;
        
        case 73:  // "I"=73; "i"=105
          connectionInfo();
          break;
        
        case 77:  // "M"=77; "m"=109
          printMainMenu();
          break;

        case 86:  // "V"=86; "m"=118
          changeMode();
          break;

        case 81:  // "Q"=81; "q"=113
          changeModeToMQTT();
          break;
      }
      
      break; 
  }
}
