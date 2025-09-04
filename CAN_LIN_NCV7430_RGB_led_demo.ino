/*
For use with :
https://www.skpang.co.uk/collections/esp32-boards/products/esp32s3-can-and-lin-bus-board

skpang.co.uk 09/2025

Ensure these libraries are installed:
https://github.com/gicking/LIN_master_portable_Arduino
https://github.com/handmade0octopus/ESP32-TWAI-CAN

*/

#include "LIN_master_HardwareSerial_ESP32.h"
#include <ESP32-TWAI-CAN.hpp>

#define CAN_TX   11  // Connects to CTX
#define CAN_RX   12  // Connects to CRX
CanFrame rxFrame; // Create frame to read 

#define LIN_CS        46
#define LIN_FAULT     9
// LIN transmit pin
#define PIN_LIN_TX    10

// LIN receive pin
#define PIN_LIN_RX    3

// pause [ms] between LIN frames
#define LIN_PAUSE     400

// serial I/F for console output (comment for no output)
#define SERIAL_CONSOLE  Serial

// SERIAL_CONSOLE.begin() timeout [ms] (<=0 -> no timeout). Is relevant for native USB ports, if USB is not connected 
#define SERIAL_CONSOLE_BEGIN_TIMEOUT  1000

#define ON  LOW
#define OFF HIGH
int LED_R = 39;
int LED_B = 40;
int LED_G = 38;

#define SET_LED_CONTROL 0x23
#define SET_LED_COLOUR  0x24

// setup LIN node. Parameters: interface, Rx, Tx, name, TxEN
LIN_Master_HardwareSerial_ESP32   LIN(Serial2, PIN_LIN_RX, PIN_LIN_TX, "Master");

//                              Grp   Grp   Fade  Intense  G     R     B
uint8_t buffer_red[]   = {0xc0, 0x00, 0x00, 0x00, 0x31, 0x00, 0xff, 0x00};
uint8_t buffer_green[] = {0xc0, 0x00, 0x00, 0x00, 0x31, 0xff, 0x00, 0x00};
uint8_t buffer_blue[]  = {0xc0, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0xff};

void set_nvc7430_color(byte* message) 
{
     LIN.sendMasterRequest(LIN_Master_Base::LIN_V1, SET_LED_COLOUR, 8, message);
}

void init_ncv7430(void) {

  uint8_t control_buffer[] = {0xc0, 0x00, 0x00, 0x7f};
  SERIAL_CONSOLE.println("init_ncv7430");
  LIN.sendMasterRequestBlocking(LIN_Master_Base::LIN_V1, SET_LED_CONTROL, 4, control_buffer);

}

// call when byte was received via Serial2. This routine is run between each time loop() runs, 
// so using delay inside loop delays response. Multiple bytes of data may be available.
void serialEvent2()
{
  // call LIN background handler
  LIN.handler();

} // serialEvent2()

void canReceiver() {
  // try to parse packet
  if(ESP32Can.readFrame(rxFrame, 0)) { // 0 is the timeout value
    // Communicate that a packet was recieved
    Serial.printf("Received frame ID: %03X Len: %X  Data:", rxFrame.identifier,rxFrame.data_length_code);

    // Communicate packet information
    for(int i = 0; i <= rxFrame.data_length_code - 1; i ++) {
      Serial.printf(" %02X", rxFrame.data[i]); // 
    }
    Serial.println(" ");
  }
}
void canSender() {

  static uint64_t payload = 0; 

  CanFrame testFrame = { 0 };
  testFrame.identifier = 0x12;  // Sets the ID
  testFrame.extd = 0; // Set extended frame to false
  testFrame.data_length_code = 8; // Set length of data - change depending on data sent
  testFrame.data[0] = 0xff & payload;
  testFrame.data[1] = (0xff00 & payload) >> 8; 
  testFrame.data[2] = (0xff0000 & payload) >> 16;  
  testFrame.data[3] = (0xff000000 & payload) >> 24; 
  testFrame.data[4] = (0xff00000000 & payload) >> 32; 
  testFrame.data[5] = (0xff0000000000 & payload) >> 40;  
  testFrame.data[6] = (0xff000000000000 & payload) >> 48; 
  testFrame.data[7] = (0xff00000000000000 & payload) >> 56; 

  payload++;
  ESP32Can.writeFrame(testFrame); // transmit frame
}

void setup()
{
    pinMode(LED_R,OUTPUT);
    pinMode(LED_G,OUTPUT);
    pinMode(LED_B,OUTPUT);
    digitalWrite(LED_B, OFF);
    digitalWrite(LED_G, OFF);
 
    digitalWrite(LED_R, ON);
    delay(200); 
    digitalWrite(LED_R, OFF);
    
    digitalWrite(LED_G, ON);
    delay(200); 
    digitalWrite(LED_G, OFF);
    
    digitalWrite(LED_B, ON);
    delay(200); 
    digitalWrite(LED_B, OFF);

  // open console with timeout
  #if defined(SERIAL_CONSOLE)
    SERIAL_CONSOLE.begin(115200);
    #if defined(SERIAL_CONSOLE_BEGIN_TIMEOUT) && (SERIAL_CONSOLE_BEGIN_TIMEOUT > 0)
      for (uint32_t startMillis = millis(); (!SERIAL_CONSOLE) && (millis() - startMillis < SERIAL_CONSOLE_BEGIN_TIMEOUT); );
    #else
      while (!SERIAL_CONSOLE);
    #endif
  #endif // SERIAL_CONSOLE
  SERIAL_CONSOLE.println("ESP32S3 LIN-bus NCV7430 RGB LED demo. skpang.co.uk 08/25");

  pinMode(LIN_FAULT,INPUT);
  pinMode(LIN_CS, OUTPUT);
  digitalWrite(LIN_CS, HIGH);

  LIN.begin(19200);

  init_ncv7430();

      // Set the pins
    ESP32Can.setPins(CAN_TX, CAN_RX);
 
    // Start the CAN bus at 500 kbps
    if(ESP32Can.begin(ESP32Can.convertSpeed(500))) {
        Serial.println("CAN bus started!");
    } else {
        Serial.println("CAN bus failed!");
    }


}

void loop()
{
  static uint32_t           lastLINFrame = 0;
  static uint8_t            count = 0;
  uint8_t                   Tx[4] = {0x01, 0x02, 0x03, 0x04};
  LIN_Master_Base::frame_t  Type;
  LIN_Master_Base::error_t  error;
  uint8_t                   Id;
  uint8_t                   NumData;
  uint8_t                   Data[8];

  // call LIN background handler
  LIN.handler();
  
  canReceiver();

  if (LIN.getState() == LIN_Master_Base::STATE_DONE)
  {
    LIN.resetStateMachine();
    LIN.resetError();
  }

  if (millis() - lastLINFrame > LIN_PAUSE)
  {
    lastLINFrame = millis();
    canSender();  // call function to send data through CAN
    
    switch(count)
    {
      case 0:
        set_nvc7430_color(buffer_red);
        digitalWrite(LED_R, ON);
        digitalWrite(LED_B, OFF);
        digitalWrite(LED_G, OFF);
        count++;
        break;

      case 1:
        set_nvc7430_color(buffer_green);
        digitalWrite(LED_G, ON);
        digitalWrite(LED_B, OFF);
        digitalWrite(LED_R, OFF);
        count++;
        break;

      case 2:
        set_nvc7430_color(buffer_blue);
        digitalWrite(LED_B, ON);
        digitalWrite(LED_R, OFF);
        digitalWrite(LED_G, OFF);
        count = 0;
        break;

      default:
        count = 0;
        break;
    }
  }

} // loop()
