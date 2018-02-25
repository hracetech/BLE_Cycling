/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

#include <bluefruit.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

/* HRM Service Definitions
 * Heart Rate Monitor Service:  0x180D
 * Heart Rate Measurement Char: 0x2A37 (Mandatory)
 * Body Sensor Location Char:   0x2A38 (Optional)
 */

BLEClientService        hrms(UUID16_SVC_HEART_RATE);
BLEClientCharacteristic hrmc(UUID16_CHR_HEART_RATE_MEASUREMENT);
BLEClientCharacteristic bslc(UUID16_CHR_BODY_SENSOR_LOCATION);


// CYCLING POWER 
#define UUID16_SVC_CYCLING_POWER                                 0x1818
#define UUID16_CHR_CYCLING_POWER_MEASUREMENT                     0x2A63
#define UUID16_CHR_CYCLING_POWER_FEATURE                         0x2A65
// #define UUID16_CHR_SENSOR_LOCATION                            0x2A5D // already defined in BLEUuid.h

BLEClientService        cpms(UUID16_SVC_CYCLING_POWER);
BLEClientCharacteristic cpmc(UUID16_CHR_CYCLING_POWER_MEASUREMENT);
BLEClientCharacteristic cpfc(UUID16_CHR_CYCLING_POWER_FEATURE);
BLEClientCharacteristic cslc(UUID16_CHR_SENSOR_LOCATION);


Adafruit_SSD1306 display = Adafruit_SSD1306();


float instpwr = 0;
float insthr  = 0;

uint8_t connection_num = 0; // for blink pattern

int pwr_conn_handle = 9;
int hr_conn_handle = 9;


void setup() 
{
  Serial.begin(115200);

  Serial.println("Cycling Display");
  Serial.println("---------------------------------\n");

  Serial.println("");
  Serial.println("OLED FeatherWing test");
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  Serial.println("OLED begun");
  display.display();
  delay(1000);
  Serial.println("");


  // Initialize Bluefruit with maximum connections as Peripheral = 0, Central = 1
  // SRAM usage required by SoftDevice will increase dramatically with number of connections
  Bluefruit.begin(0, 2);
  
  // Set max power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(4);

  /* Set the device name */
  Bluefruit.setName("Blue52 CycleDisp");

   // Initialize CPM client
  cpms.begin();
  cpfc.begin();
  cslc.begin();
  cpmc.setNotifyCallback(cpm_notify_callback);
  cpmc.begin();

    // Initialize HRM client
  hrms.begin();
  bslc.begin();
  hrmc.setNotifyCallback(hrm_notify_callback);
  hrmc.begin();


  /* Set the LED interval for blinky pattern on BLUE LED */
  Bluefruit.setConnLedInterval(250);

  // Callbacks for Central
  Bluefruit.Central.setDisconnectCallback(disconnect_callback);
  Bluefruit.Central.setConnectCallback(connect_callback);


  /* Start Central Scanning
   * - Enable auto scan if disconnected
   * - Filter out packet with a min rssi
   * - Interval = 100 ms, window = 50 ms
   * - Use active scan (used to retrieve the optional scan response adv packet)
   * - Start(0) = will scan forever since no timeout is given
   */
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.filterRssi(-80);
//  Bluefruit.Scanner.filterUuid(UUID16_SVC_CYCLING_POWER, UUID16_SVC_HEART_RATE); // Filter for CP/HR-Service
  Bluefruit.Scanner.setInterval(160, 80);       // in units of 0.625 ms
  Bluefruit.Scanner.useActiveScan(true);        // Request scan response data
  Bluefruit.Scanner.start(0);                   // 0 = Don't stop scanning after n seconds
  Serial.println("Scanning ...");

}

// --------------------------------------------------------------------------

void scan_callback(ble_gap_evt_adv_report_t* report)
{

  
  uint8_t len = 0;
  uint8_t buffer[32];
  memset(buffer, 0, sizeof(buffer));
  
  /* Display the timestamp and device address */
  if (report->scan_rsp)
  {
    Serial.printf("[SR%10d] Packet received from ", millis());
  }
  else
  {
    Serial.printf("[ADV%9d] Packet received from ", millis());
  }
  // MAC is in little endian --> print reverse
  Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
  Serial.print("\n");

  /* Raw buffer contents */
  Serial.printf("%14s %d bytes\n", "PAYLOAD", report->dlen);
  if (report->dlen)
  {
    Serial.printf("%15s", " ");
    Serial.printBuffer(report->data, report->dlen, '-');
    Serial.println();
  }

  /* RSSI value */
  Serial.printf("%14s %d dBm\n", "RSSI", report->rssi);

  /* Adv Type */
  Serial.printf("%14s ", "ADV TYPE");
  switch (report->type)
  {
    case BLE_GAP_ADV_TYPE_ADV_IND:
      Serial.printf("Connectable undirected\n");
      break;
    case BLE_GAP_ADV_TYPE_ADV_DIRECT_IND:
      Serial.printf("Connectable directed\n");
      break;
    case BLE_GAP_ADV_TYPE_ADV_SCAN_IND:
      Serial.printf("Scannable undirected\n");
      break;
    case BLE_GAP_ADV_TYPE_ADV_NONCONN_IND:
      Serial.printf("Non-connectable undirected\n");
      break;
  }

  /* Shortened Local Name */
  if(Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, buffer, sizeof(buffer)))
  {
    Serial.printf("%14s %s\n", "SHORT NAME", buffer);
    memset(buffer, 0, sizeof(buffer));
  }

  /* Complete Local Name */
  if(Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, buffer, sizeof(buffer)))
  {
    Serial.printf("%14s %s\n", "COMPLETE NAME", buffer);
    memset(buffer, 0, sizeof(buffer));
  }

  /* TX Power Level */
  if (Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_TX_POWER_LEVEL, buffer, sizeof(buffer)))
  {
    Serial.printf("%14s %i\n", "TX PWR LEVEL", buffer[0]);
    memset(buffer, 0, sizeof(buffer));
  }

  /* Check for UUID16 Complete List */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE, buffer, sizeof(buffer));
  if ( len )
  {
    printUuid16List(buffer, len);
  }

  /* Check for UUID16 More Available List */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_MORE_AVAILABLE, buffer, sizeof(buffer));
  if ( len )
  {
    printUuid16List(buffer, len);
  }

  /* Check for UUID128 Complete List */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE, buffer, sizeof(buffer));
  if ( len )
  {
    printUuid128List(buffer, len);
  }

  /* Check for UUID128 More Available List */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE, buffer, sizeof(buffer));
  if ( len )
  {
    printUuid128List(buffer, len);
  }  

  /* Check for DIS UUID */
  if ( Bluefruit.Scanner.checkReportForUuid(report, UUID16_SVC_DEVICE_INFORMATION) )
  {
    Serial.printf("%14s %s\n", "DIS", "UUID Found!");
  }

  /* Check for Manufacturer Specific Data */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA, buffer, sizeof(buffer));
  if (len)
  {
    Serial.printf("%14s ", "MAN SPEC DATA");
    Serial.printBuffer(buffer, len, '-');
    Serial.println();
    memset(buffer, 0, sizeof(buffer));
  }  

  Serial.println();

  /* Check for CPS UUID */
  if ( Bluefruit.Scanner.checkReportForUuid(report, UUID16_SVC_CYCLING_POWER) )
  {
    Serial.println("Cycling Power Service found!");
    Serial.println();
    Serial.printf("connecting...");
    Bluefruit.Central.connect(report);
  }

  /* Check for HRM UUID */
  if ( Bluefruit.Scanner.checkReportForUuid(report, UUID16_SVC_HEART_RATE) )
  {
    Serial.println("HEART RATE Service found!");
    Serial.println();
    Serial.printf("connecting...");
    Bluefruit.Central.connect(report);
  }

}

// --------------------------------------------------------------------------

void printUuid16List(uint8_t* buffer, uint8_t len)
{
  Serial.printf("%14s %s", "16-Bit UUID");
  for(int i=0; i<len; i+=2)
  {
    uint16_t uuid16;
    memcpy(&uuid16, buffer+i, 2);
    Serial.printf("%04X ", uuid16);
  }
  Serial.println();
}

// --------------------------------------------------------------------------


void printUuid128List(uint8_t* buffer, uint8_t len)
{
  (void) len;
  Serial.printf("%14s %s", "128-Bit UUID");

  // Print reversed order
  for(int i=0; i<16; i++)
  {
    const char* fm = (i==4 || i==6 || i==8 || i==10) ? "-%02X" : "%02X";
    Serial.printf(fm, buffer[15-i]);
  }

  Serial.println();  
}


// --------------------------------------------------------------------------

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{

  if ( conn_handle == pwr_conn_handle) {
    pwr_conn_handle = 9;
  }
  else if (conn_handle == hr_conn_handle) {
    hr_conn_handle = 9;
  }

//  (void) conn_handle;
  (void) reason;

  connection_num--;

}

// --------------------------------------------------------------------------

void connect_callback(uint16_t conn_handle)
{
  Serial.println("Connected!");
  Serial.println();  
  Serial.print("Discovering Services ... ");


  
  if ( cpms.discover(conn_handle) )   {
    Serial.println("Found CP Service!");
    Serial.println();  

    Serial.print("Discovering Measurement characteristic ... ");
    if ( cpmc.discover() )
    {
      pwr_conn_handle = conn_handle;
      Serial.println("Found it!");
    }
    else {
      // Measurement chr is mandatory, if it is not found (valid), then disconnect
      Serial.println("not found !!!");  
      Serial.println("Measurement characteristic is mandatory but not found");
      Bluefruit.Central.disconnect(conn_handle);
      return;
    }
    Serial.println();  

    Serial.print("Discovering Feature characteristic ... ");
    if ( cpfc.discover() )
    {
      Serial.println("Found it!");
    }
    else {
      // Feature chr is mandatory, if it is not found (valid), then disconnect
      Serial.println("not found !!!");  
      Serial.println("Feature characteristic is mandatory but not found");
      Bluefruit.Central.disconnect(conn_handle);
      return;
    }
  
    Serial.println();  
    // Reaching here means we are ready to go, let's enable notification on measurement chr
    if ( cpmc.enableNotify() )
    {
      Serial.println("proceed with measurement...");
      Serial.println("");
      connection_num++;
    }else
    {
      Serial.println("Couldn't enable notify for CP Measurement. Increase DEBUG LEVEL for troubleshooting");
    }

  }
    
  else if ( hrms.discover(conn_handle) )   {
    Serial.println("Found HR Service!");
    Serial.println();  
    Serial.print("Discovering Measurement characteristic ... ");
    if ( hrmc.discover() )     {
      hr_conn_handle = conn_handle;
      Serial.println("Found it");

    }
    else {
      Serial.println("not found !!!");  
      Serial.println("Measurement characteristic is mandatory but not found");
      Bluefruit.Central.disconnect(conn_handle);
      return;
    }
  
    Serial.println();  
    Serial.print("Discovering Body Sensor Location characteristic ... ");
    if ( bslc.discover() )
    {
      Serial.println("Found it");
      const char* body_str[] = { "Other", "Chest", "Wrist", "Finger", "Hand", "Ear Lobe", "Foot" };
      uint8_t loc_value = bslc.read8();
      Serial.print("Body Location Sensor: ");
      Serial.println(body_str[loc_value]);
    }else
    {
      Serial.println("Found NONE");
    }
  
    if ( hrmc.enableNotify() )
    {
      Serial.println("Ready to receive HRM Measurement value");
      connection_num++;

    }else
    {
      Serial.println("Couldn't enable notify for HRM Measurement. Increase DEBUG LEVEL for troubleshooting");
    }
    

  }

    
  
  else {
    Serial.println("no Service found. Disconnecting...");
    Bluefruit.Central.disconnect(conn_handle);
    return;
  }
  
  
}


// --------------------------------------------------------------------------

void cpm_notify_callback(BLEClientCharacteristic* chr, uint8_t* data, uint16_t len)
{
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.heart_rate_measurement.xml
  // Measurement contains of control byte0 and measurement (8 or 16 bit) + optional field
  // if byte0's bit0 is 0 --> measurement is 8 bit, otherwise 16 bit.

  Serial.print("Power Measurement: ");


  if ( data[0] & bit(0) )
  {
    uint16_t value;
    memcpy(&value, data+1, 2);

    Serial.print(value);
  }
  else
  {
    Serial.print(data[2]);
  }
  Serial.println("W");

  instpwr = float(data[2]);

}

void hrm_notify_callback(BLEClientCharacteristic* chr, uint8_t* data, uint16_t len)
{

  Serial.print("HRM Measurement: ");

  if ( data[0] & bit(0) )   {
    uint16_t value;
    memcpy(&value, data+1, 2);

    Serial.print(value);
    insthr = float(value);
  }
  else   {
    Serial.print(data[1]);  
    insthr = float(data[1]);  
    }
  
  Serial.println("bpm");

}


// --------------------------------------------------------------------------

void disppwr(){
  display.clearDisplay();
  display.setTextColor(WHITE);

  display.setTextSize(2);
  display.setCursor(0,4);
  display.print("Pwr");

  display.setTextSize(1);
  display.setCursor(0,22);
  display.print("[Watt]");
  
  display.setTextSize(4);
  display.setCursor(60,2);
  display.print(instpwr, 0);
  
  display.display();
}

// --------------------------------------------------------------------------

void disphr(){
  display.clearDisplay();
  display.setTextColor(WHITE);

  display.setTextSize(2);
  display.setCursor(0,4);
  display.print("HR");

  display.setTextSize(1);
  display.setCursor(0,22);
  display.print("[BPM]");
  
  display.setTextSize(4);
  display.setCursor(60,2);
  display.print(insthr, 0);
  
  display.display();
}

// --------------------------------------------------------------------------

void dispdual(){
  display.clearDisplay();
  display.setTextColor(WHITE);

  display.setTextSize(1);
  display.setCursor(0,4);
  display.print("P");

  display.setTextSize(1);
  display.setCursor(0,22);
  display.print("[W]");
  
  display.setTextSize(2);
  display.setCursor(20,2);
  display.print(instpwr, 0);
  
  display.setTextSize(1);
  display.setCursor(70,4);
  display.print("HR");

  display.setTextSize(1);
  display.setCursor(70,22);
  display.print("[BPM]");
  
  display.setTextSize(2);
  display.setCursor(100,2);
  display.print(insthr, 0);

  display.display();
}

// --------------------------------------------------------------------------

void dispnosensor(){
  display.clearDisplay();
  display.setTextColor(WHITE);

  display.setTextSize(1);
  display.setCursor(0,8);
  display.print("no sensors found");
  
  display.display();
}


// --------------------------------------------------------------------------

void loop() 
{

  
  if(Bluefruit.Central.connected(hr_conn_handle) && Bluefruit.Central.connected(pwr_conn_handle)){
    dispdual();
    delay(1000);
  }
  else if(Bluefruit.Central.connected(hr_conn_handle) ){
    disphr();
    Bluefruit.setConnLedInterval(500);
    Bluefruit.Scanner.start(0);
    Serial.println("Scanning ...");
    delay(1000);
      
  }
  else if(Bluefruit.Central.connected(pwr_conn_handle) ){
    disppwr();
    Bluefruit.setConnLedInterval(500);
    Bluefruit.Scanner.start(0);
    Serial.println("Scanning ...");
    delay(1000);
  
  }
  else {
    dispnosensor();
    Bluefruit.setConnLedInterval(250);
    Bluefruit.Scanner.start(0);
    Serial.println("Scanning ...");
    delay(1000);
  } 

}
