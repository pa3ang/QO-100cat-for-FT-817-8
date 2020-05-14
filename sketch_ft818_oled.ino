/*
    FT-818 / QO-100 controller PA3ANG - May 2020
    Using Arduino Nano Every with Adafruit SSD1306 128x32 OLED display. The sketch uses knobs from the FT-818 to control the 
    functions, so no external knobs are needed. The knobs used are the CLAR, LOCK and SPLIT. 

    My QO-100 RF hardware consist of SG-Lab componets for uplink (transvertor and amplifier). The FT-818 and SG-Lab transverter are 
    both TXCO stabile. The outdoor LNB has an TCXO build inbut does drift a bit based on the environmental temperature. 
    The LNB converts down from 10489 to 432 MHz. Calibration is only needed to accomodate for the outside temperature deviations 
    which are about +- 1kHz on 70cm. 
    
    The FT-818 can be wired directly from the ACC Jack to the Ardunio digital pin 3 (RX D) and 2 (TX D) and GND. No level schifter is 
    needed. 

    The sketch reads VFO A frequency and calculates the associated QO-100 transponder downlink frequency based on
    the LNB_offset (IF) frequency and the calibartion offset (+- 1KHz). 
    Based on the calculated QO_frequency the sketch calculates the uplink TX_frequency for the FT-818 to be programmed in VFO B.
    The TX_frequency is calculated including the TX_LO_frequency (Transvertor IF frequency) and the QO-100 transponder 
    offset 10489.500 => 2400.000 (808950000). 

    The mode from the VFO A (RX) is always copied to the VFO B (TX). Note: VFO A and VFO B can be reversed if so.

    Four different FT-818 statusses (or knobs) are interrogated by the sketch:

    1. PTT.  If the FT-818 is transmitting nothing happens and no display updates are made.
    2. CLAR. If the Clar is switched on, the calibartion procedure will start and the FT-818 VFO A RX frequency is set to the 
             Middle Beacon frequency. Pressing again the Clar knob will stop the calibration process. The new calibration offset 
             will be calculated and the RX_frequency will be set back to the frequency before starting the calibration. 
    3. LOCK  If de calculated uplink TX_frequency deviates from the current TX_frequency in VFO B, the display will show the 
             an up and down arrow symbol. When pressing the LOCK knob on the FT-818 the VFO B mode and frequency will be updated.
    4. SPLIT If the Split mode is switched off on the FT-818, the sketch will automatically switch it on again and program in both VFO A
             and B the Home frequency with Mode USB. 

    The calibration process works as follows:
    RX is set to LSB and the Middle Beacon frequency. Adjust to zero beat (you will hear a low tone +- 200Hz) and when switching
    to USB the tone should be the same +- 200Hz. You need to experiment a bit but when know know how it works this calibration can
    be done very fast. The program will switch 
    back to USB after pressing Clar again.
*/

#include <SoftwareSerial.h>       // load the Serial libray, configure the pins and create the object CAT
const int RX  = 3;                // Connection to FT-818
const int TX  = 2;                // Connection to FT-818
long CATspeed = 38400;            // CAT Speed FT-818
SoftwareSerial CAT(RX, TX);       // Create CAT connection and object

#include <Wire.h>                 // load display library
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RESET 4              // if display has reset connection and create object
Adafruit_SSD1306 display(OLED_RESET);

// Transceiver commands used in this program
byte READ_LOCK[]  = {0x00, 0x00, 0x00, 0x00, 0x80};
byte READ_SPLIT[] = {0x00, 0x00, 0x00, 0x00, 0x02};
byte READ_CLAR[]  = {0x00, 0x00, 0x00, 0x00, 0x85};
byte READ_FREQ[]  = {0x00, 0x00, 0x00, 0x00, 0x03};
byte READ_PTT[]   = {0x00, 0x00, 0x00, 0x00, 0xF7};
byte TOGGLE_VFO[] = {0x00, 0x00, 0x00, 0x00, 0x81};
byte SET_PSK[]    = {0x0C, 0x00, 0x00, 0x00, 0x07};  // mode = 12
byte SET_LSB[]    = {0x00, 0x00, 0x00, 0x00, 0x07};  // mode = 0
byte SET_USB[]    = {0x01, 0x00, 0x00, 0x00, 0x07};  // mode = 1
byte SET_CW[]     = {0x02, 0x00, 0x00, 0x00, 0x07};  // mode = 2

// variables used in the program
int Word;                         // word received or send from and to CAT
int Hex[5];                       // placeholder for capturing 5 words from CAT in readFrequency

bool calibrate = false;           // flag indicating calibration routine active
bool startup   = true;            // set FT-818 at home frequency at program start
int settle_time = 150;            // time delay (ms) for FT-818 to process commands

long Frequency;                   // placeholder to update RX and TX frequency
long QO_frequency;                // Calculated QO Frequency Transponder Downlink
long TX_frequency;                // Calculated Uplink frequency
long RX_frequency;                // Current RX frequency
long RX_return_frequency;         // Frequency before starting calibrate routine 
long TX_current_frequency;        // Current uplink TX frequency
int Mode;                         // Current RX mode
int TX_current_mode;              // Current uplink TX Mode

// Up and Down link offsets (Frequency *10Hz)
long LNB_offset       = 1005697900;
long LNB_calibrate    = -200;

// SG Labs transverter has 230 Hz minus misalignment of TCXO
long TX_LO_frequency  = 196800000 - 23;

// Beacon & Home Frequency *10Hz
long Beacon_frequency = 1048975000;
long Home_frequency   = 1048968000;

void readFrequency() {  
  CAT.begin(CATspeed);
  for (byte i = 0; i < sizeof(READ_FREQ); i++)(CAT.write(READ_FREQ[i]));
  delay(settle_time);
  for (int i = 0; i < 5; i++) { // expect 5 bytes
    Hex[i] = CAT.read();        // use Hex[1] buffer to capture Frequency digits and Modulation
  }
  CAT.end();
  // format and caculate the 3 frequencies
  RX_frequency = (String(Hex[0], HEX).toInt() * 1000000) + String(Hex[1], HEX).toInt() * 10000 + String(Hex[2], HEX).toInt() * 100 + String(Hex[3], HEX).toInt();
  QO_frequency = RX_frequency + LNB_offset + LNB_calibrate;
  TX_frequency = QO_frequency - 808950000 - TX_LO_frequency;  
  // and capture the mode
  Mode = Hex[4];
}

void readPTT() {                  
  CAT.begin(CATspeed);
  for (byte i = 0; i < sizeof(READ_PTT); i++)(CAT.write(READ_PTT[i]));
  delay(settle_time);
  Word = CAT.read();
  CAT.end();
}

void readLOCK() {                  
  CAT.begin(CATspeed);
  for (byte i = 0; i < sizeof(READ_LOCK); i++)(CAT.write(READ_LOCK[i]));
  delay(settle_time);
  Word = CAT.read();
  CAT.end();
}

void readSPLIT() {                  
  CAT.begin(CATspeed);
  for (byte i = 0; i < sizeof(READ_SPLIT); i++)(CAT.write(READ_SPLIT[i]));
  delay(settle_time);
  Word = CAT.read();
  CAT.end();
}

void readCLAR() {                  
  CAT.begin(CATspeed);
  for (byte i = 0; i < sizeof(READ_CLAR); i++)(CAT.write(READ_CLAR[i]));
  delay(settle_time);
  Word = CAT.read();
  CAT.end();
}

void startCalibrate() {
  calibrate = true;
  RX_return_frequency = RX_frequency;
  Frequency = Beacon_frequency;
  setRXFrequency();
}

void stopCalibrate() {
  calibrate = false; 
  LNB_calibrate = LNB_calibrate - (QO_frequency - Beacon_frequency);
  Frequency = RX_return_frequency + LNB_offset + LNB_calibrate;
  setRXFrequency();
}
  
void setRXFrequency() {                   
  CAT.begin(CATspeed);
  Frequency = Frequency - LNB_offset - LNB_calibrate;
  if (calibrate)  for (byte i = 0; i < sizeof(SET_LSB); i++)(CAT.write(SET_LSB[i]));
  if (!calibrate) for (byte i = 0; i < sizeof(SET_USB); i++)(CAT.write(SET_USB[i]));
  delay(settle_time);
  setFrequency();
  delay(settle_time);
  CAT.end();
}

void setTXFrequency() {                   
  CAT.begin(CATspeed);
  for (byte i = 0; i < sizeof(TOGGLE_VFO); i++)(CAT.write(TOGGLE_VFO[i]));
  delay(settle_time);
  TX_current_frequency = TX_frequency;
  TX_current_mode = Mode;
  if (Mode ==  1) for (byte i = 0; i < sizeof(SET_USB); i++)(CAT.write(SET_USB[i]));
  if (Mode ==  2) for (byte i = 0; i < sizeof(SET_CW); i++)(CAT.write(SET_CW[i]));
  if (Mode == 12) for (byte i = 0; i < sizeof(SET_PSK); i++)(CAT.write(SET_PSK[i]));
  delay(settle_time);
  Frequency = TX_frequency;
  setFrequency();
  delay(settle_time);
  for (byte i = 0; i < sizeof(TOGGLE_VFO); i++)(CAT.write(TOGGLE_VFO[i]));
  delay(settle_time);
  CAT.end();
}

void setFrequency() {
  // write 8 formated frequency words to FT-818  /w delimiter \x01 
  for (byte i = 0; i < 7; i = i + 2)(CAT.write(((((String (Frequency).substring(i, i+2)).toInt()) / 10) << 4) + (((String (Frequency).substring(i, i+2)).toInt()) % 10)));
  CAT.write(((1 / 10) << 4) + (1 % 10));
}

void displayIntro() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  // print welcom message
  display.setCursor(4,0);
  display.print("QO-100 cat");
  display.setCursor(16,18);
  display.print("FT-817/8"); 
  // display for 2 seconds
  display.display();
  delay(2000);
}

void displayInfo() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  // create first line
  // QO frequency
  display.setCursor(2,0);
  display.print(String(QO_frequency).substring(0, 5)) ;
  display.print(String(QO_frequency).substring(5, 8)) ;
  // make decimal point
  for (int x = 98; x < 100; x++) { for (int y = 12; y < 14; y++) (display.drawPixel(x, y, WHITE)); }
  display.setCursor(102,0);
  display.print(String(QO_frequency).substring(8, 10)) ;

  // create second line 
  // calibration offset
  display.setCursor(2,18);
  if (calibrate) display.print(" C  ");
  if (!calibrate) display.print(LNB_calibrate);

  // mode
  display.setCursor(62,18);
  if (Mode != 1 and Mode !=2 and Mode != 12) display.print("---");
  if (Mode == 1) display.print("USB");
  if (Mode == 2) display.print(" CW");
  if (Mode == 12) display.print("PSK");

  // sign difference in uplink versus TX frequency
  display.setCursor(114,18);
  if ((TX_current_mode != Mode or TX_current_frequency != TX_frequency )and !calibrate) display.print(char(18));
  if ((TX_current_mode == Mode and TX_current_frequency == TX_frequency )and !calibrate) display.print(" ");

  // and display new content
  display.display();
}

void setup() {                        // Setup ports, lcd 
  pinMode(RX, INPUT);                 // Set the CAT port directions
  pinMode(TX, OUTPUT);
  Serial.begin(9600);                 // Start the serial terminal (if needed)
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  displayIntro();
}

void loop() {
  readPTT();                          // if transmitting do nothing
  if (Word == 255) {
    readFrequency();                  
    readPTT();                        // recheck PTT to avoid wrong frequency display
    if (Word == 255) displayInfo();
    readLOCK();                       // if Lock key was presses do update uplink 
    if (Word == 0 and !calibrate) setTXFrequency();
    readSPLIT();                      // if Split was off then go to home frequency 
    if ((Word == 0 and !calibrate) or startup) {
      startup = false;
      Frequency=Home_frequency;
      setRXFrequency();
      readFrequency();
      setTXFrequency();
    }
    readCLAR();                       // if Clar key pressed do calbrate routine 
    if (Word == 0) {
      if (!calibrate) { startCalibrate(); } else { stopCalibrate(); }
    }
  }
}
