/*
 *  MIPS WiFi network server using a ESP8266
 *
 *  MIPS WiFi interface. This application uses the ESP8266 to communicate with MIPS through its serial port. The serial port
 *  baud rate is assumed to be set to 115200.
 *  On port up this module will not do anything until it is configurated. This is done through the serial port. After the system
 *  is confured and connected to the network it will relay communications through the WiFi to the serial port. The host application
 *  is expected to communicate using scokets. This can be done using telnet to connect to this host.
 *
 *  As currently desiged this system uses DHCP is acquire an IP address. Multicast DNS is supported allow local network IP address
 *  lookup based on name. Note, this lookup is alwas lower case, the defualt host name is MPISnet, the local DNS lookup is mipsnet.local.
 *  
 *  This interface will also support operating as an access point. To use in this way do not connect, issue the AP command then the MDNS con=mmand
 *  and enter repeat mode. Then a remote client can connect
 *
 *  Gordon Anderson
 *
 */
#include <arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <stdio.h>
#include <string.h>

extern "C" {
#include "user_interface.h"
}

MDNSResponder mdns;

const char host[20] = "MIPSnet";
const char *Version = "MIPSnet Version 1.0, Novebmer 27, 2015";
char ssid[20]       = "Linksys20476";
char password[20]   = "";

ESP8266WiFiClass wifi;

WiFiServer server(2015);
WiFiClient client;
bool RepeatEnable = false;
bool APmode = false;

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete

void listNetworks()
{
  // scan for nearby networks:
  Serial.println("");
  Serial.println("** Scan Networks **");
  int numSsid = wifi.scanNetworks();
  if (numSsid == -1) {
    Serial.println("Couldn't get a wifi connection");
    while (true);
  }
  // print the list of networks seen:
  Serial.print("number of available networks:");
  Serial.println(numSsid);

  // print the network number and name for each network found:
  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    Serial.print(thisNet);
    Serial.print(") ");
    Serial.print(wifi.SSID(thisNet));
    Serial.print("\tSignal: ");
    Serial.print(wifi.RSSI(thisNet));
    Serial.print(" dBm");
    Serial.print("\tEncryption: ");
    printEncryptionType(wifi.encryptionType(thisNet));
  }
}

void printEncryptionType(int thisType) {
  // read the encryption type and print out the name:
  switch (thisType) {
    case ENC_TYPE_WEP:
      Serial.println("WEP");
      break;
    case ENC_TYPE_TKIP:
      Serial.println("WPA");
      break;
    case ENC_TYPE_CCMP:
      Serial.println("WPA2");
      break;
    case ENC_TYPE_NONE:
      Serial.println("None");
      break;
    case ENC_TYPE_AUTO:
      Serial.println("Auto");
      break;
  }
}

/*
  SerialEvent occurs whenever a new data comes in the
 hardware serial RX.  This routine is run between each
 time loop() runs, so using delay inside loop can delay
 response.  Multiple bytes of data may be available.
 */
void serialEvent() {
  while (Serial.available())
  {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') stringComplete = true;
  }
}

// Get token from string object, delimited with comma
// Token are numbered 1 through n, returns empty string
// at end of tokens
String GetToken(String cmd, int TokenNum)
{
  int i, j, k;
  String Token;

  // If TokenNum is 0 or 1 then return the first token.
  // Return up to delimiter of the whole string.
  cmd.trim();
  if (TokenNum <= 1)
  {
    if ((i = cmd.indexOf(',')) == -1) return cmd;
    Token = cmd.substring(0, i);
    return Token;
  }
  // Find the requested token
  k = 0;
  for (i = 2; i <= TokenNum; i++)
  {
    if ((j = cmd.indexOf(',', k)) == -1) return "";
    k = j + 1;
  }
  Token = cmd.substring(k);
  Token.trim();
  if ((j = Token.indexOf(',')) == -1) return Token;
  Token = Token.substring(0, j);
  Token.trim();
  return Token;
}

/*
 *  Serial command processor.
 *
 *  Serial commands:
 *     GVER                   Returns current version
 *     RESET                  Perform software reset of system
 *     LIST                   List all detected networks
 *     HOST                   Define host name
 *     SSID,ssidstring        Sets the SSID of the network we are going to use
 *     PSWD,password string   Defines the network password
 *     CONNECT                Connects to the network
 *     AP                     Sets up an access point
 *     MDNS                   Starts mDNS service
 *     DISCONNECT             Disconnect from the network
 *     IP                     Returns the current IP address
 *     STATUS                 Returns the connection status
 *     REPEAT                 Puts the system into repeat mode were traffic is repeated on serial port
 *     STOP                   The stop  command is case sensative and only works when the system is in repeat
 *                            mode. This command will exit repeat mode.
 */
void ProcessSerialCommand(String cmd)
{
  String Token;
  int i;

  if ((Token = GetToken(cmd, 1)) == "") return;
  Token.toUpperCase();
  if (Token == String("GVER")) Serial.println(Version);
  else if (Token == String("RESET")) 
  {
    system_restart();
  }
  else if (Token == String("LIST")) listNetworks();
  else if (Token == String("HOST"))
  {
    Token = GetToken(cmd, 2);
    strcpy((char *)host, (const char*)(Token.c_str()));
  }
  else if (Token == String("SSID"))
  {
    Token = GetToken(cmd, 2);
    strcpy((char *)ssid, (const char*)(Token.c_str()));
  }
  else if (Token == String("PSWD"))
  {
    Token = GetToken(cmd, 2);
    strcpy((char *)password, (const char*)(Token.c_str()));
  }
  else if (Token == String("CONNECT"))
  {
    wifi.begin(ssid, password);
    wifi.hostname(host);
  }
  else if (Token == String("AP"))
  {
    if(strlen(password) == 0)
    {
      APmode = true;
      wifi.hostname(host);
      wifi.softAP((const char*)ssid);
    }
    else
    {
      APmode = true;
      wifi.hostname(host);
      wifi.softAP((const char*)ssid, (const char*)password);
    }
  }
  else if (Token == String("MDNS"))
  {
    if (!mdns.begin(host, wifi.localIP(), 1))
    {
      Serial.println("Error setting up MDNS responder!");
    }
    else Serial.println("mDNS responder started");
    mdns.addService("mips", "tcp", 2015);
  }
  else if (Token == String("DISCONNECT"))
  {
    if (wifi.status() != WL_CONNECTED)
    {
      Serial.println("Not connected to a network!");
      return;
    }
    wifi.disconnect();
  }
  else if (Token == String("STATUS"))
  {
    Serial.println(wifi.status());
  }
  else if (Token == String("IP")) 
  {
    if(APmode) Serial.println(wifi.softAPIP());
    else Serial.println(wifi.localIP());
  }
  else if (Token == String("REPEAT"))
  {
    // Make sure we are connected then enter the traffic repeat mode
    if(!APmode) if (wifi.status() != WL_CONNECTED)
    {
      Serial.println("Can not enter repeater mode, not connected to a network!");
      return;
    }
    // Start the server and set the repeat enable flag
    server.begin();
    server.setNoDelay(true);
    RepeatEnable = true;
  }
  else if (Token == String("STOP")) Serial.println("Not in repeat mode!");
  else Serial.println("?");
}

void setup()
{
  inputString.reserve(200);
  Serial.begin(115200);
  delay(100);
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH);
  Serial.println("");
  Serial.println(Version);
  APmode=false;
  wifi.disconnect();
}

#define  SAVESIZE   5
char LastString[SAVESIZE];

// This function saves the last 5 received characters in a string. This is done so we can search for
// the STOP command when the system is in the repeat mode.
void PushChar(char c)
{
  int  i;

  // shift all characters to the left one position
  for (i = 0; i < SAVESIZE - 1; i++) LastString[i] = LastString[i + 1];
  // insert new char
  LastString[SAVESIZE - 1] = c;
}

// Main processing loop.
void loop(void)
{
  int     i,j;
  uint8_t buf[32];

  if (RepeatEnable)
  {
    //if (!client) client = server.available();
    client = server.available();
    while (client)
    {
      digitalWrite(0, LOW);
      if (!client.connected()) break;
      while (client.available() > 0)
      {
//        digitalWrite(0, LOW);
        // read the bytes incoming from the client:
        char thisChar = client.read();
        // echo the bytes to the serial port:
        Serial.write(thisChar);
      }
//      digitalWrite(0, HIGH);
      while (Serial.available() > 0)
      {
        for (i = 0; i < 32; i++)
        {
          buf[i] = Serial.read();
          if (Serial.available() == 0) {i++; break;}
        }
        client.write((const uint8_t *)buf, (size_t)i);
        // Look for stop command and exit if found
        for(j=0;j<i;j++)
        {
          PushChar(buf[j]);
          if (strncmp("StoP", LastString, 4) == 0)
          {
            RepeatEnable = false;
            client.stop();
            digitalWrite(0, HIGH);
            return;
          }
        }
      }
    }
    digitalWrite(0, HIGH);
    // Here with no client and in repeat mode, look for stop command and exit repeat mode if found.
    while (Serial.available() > 0)
    {
      // read bytes from the serial port
      char thisChar = Serial.read();
      PushChar(thisChar);
      // Look for stop command and exit if found
      if (strncmp("StoP", LastString, 4) == 0)
      {
        RepeatEnable = false;
        return;
      }
    }
  }
  else  // Here is not in repeat mode
  {
    // process any serial commands
    serialEvent();
    // Process the string when a newline arrives:
    if (stringComplete)
    {
      //      Serial.println(inputString);
      ProcessSerialCommand(inputString);
      // clear the string:
      inputString = "";
      stringComplete = false;
    }
  }
}

