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
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

// BLE Service
BLEDfu  bledfu;  // OTA DFU service
BLEDis  bledis;  // device information
BLEUart bleuart; // uart over ble
BLEBas  blebas;  // battery

String papp, index_;
bool gotpapp, gotindex = false;
int indexes[150];
unsigned long times[150];
int iter = 0;
int indexInt;
const unsigned long millisinhour = 3600000;

void setup()
{
  Serial1.begin(1200);
  Serial.begin(115200);

#if CFG_DEBUG
  // Blocking wait for connection when debug mode is enabled via IDE
  while ( !Serial ) yield();
#endif
  
  Serial.println("Bluefruit52 BLEUART Example");
  Serial.println("---------------------------\n");

  // Setup the BLE LED to be enabled on CONNECT
  // Note: This is actually the default behavior, but provided
  // here in case you want to control this LED manually via PIN 19
  Bluefruit.autoConnLed(true);

  // Config the peripheral connection with maximum bandwidth 
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  Bluefruit.setTxPower(8);    // Check bluefruit.h for supported values
  //Bluefruit.setName(getMcuUniqueID()); // useful testing with multiple central connections
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin();

  // Configure and Start Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();

  // Configure and Start BLE Uart Service
  bleuart.begin();

  // Start BLE Battery Service
  blebas.begin();
  blebas.write(100);

  // Set up and start advertising
  startAdv();

  Serial.println("Please use Adafruit's Bluefruit LE app to connect in UART mode");
  Serial.println("Once connected, enter character(s) that you wish to send");
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void loop()
{

  // Forward data from HW Serial to BLEUART
  while (Serial1.available())
  {
    uint8_t buf[64];
    int count = Serial1.readBytesUntil('\n', buf, sizeof(buf));
    for (int i = 0; i < count; i++ ) {
      buf[i] = buf[i] & 127;
    }
    String line = String((char *)buf);
    //Serial.print(line);
    if (line.startsWith("PAPP")) {
      papp = line.substring(5, 10);
      gotpapp = true;
    }
    if (line.startsWith("BASE")) {
      index_ = line.substring(5, 14);
      indexInt = index_.toInt();
      indexes[iter] = indexInt;
      times[iter] = millis();
      gotindex = true; 
    }
    if (gotpapp && gotindex) {
      gotpapp = false;
      gotindex = false;
      if (iter % 5 == 0) {
        //Serial.println(iter);
        //Serial.print("Puissance: " + papp + " W\nIndex: " + index_ + " kWh");
        int previousIndexInt = iter+1;
        if (previousIndexInt == 150) {
          previousIndexInt = 0;
        }
        int delta = indexes[iter] - indexes[previousIndexInt];
        unsigned long delta_millis = times[iter] - times[previousIndexInt];
        // it takes roughly 240 secs to get 150 values, so we extrapolate the delta
        int estimatedPower = delta * 15;
        //Serial.println(indexes[iter]);
        //Serial.println(indexes[previousIndexInt]);
        bleuart.print("Puissance: " + papp + " VA\nIndex: " + index_ + " Wh\nDelta: " + delta + "Wh\nPuiss. Estim. (4 min): " + estimatedPower + "W\n");
        bleuart.print("Delta Millis: " + String(delta_millis) + " ms\n");
        bleuart.print("Puiss. Estim.: " + String(delta * (millisinhour / delta_millis)) + " W");
        //Serial.print("Puissance: " + papp + " VA\nIndex: " + index_ + " Wh\nDelta: " + delta + "Wh\n");
      }
      iter++;
      if (iter == 150) {
        iter = 0;
      }
    }
  }

}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
}
