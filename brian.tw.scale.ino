// Code written in December 2015, long before it was easy to find documentation on adding custom code to ESP8266.
// In any case not enough GPIOs for this purpose - every segment on the scale LCD has to be monitored, plus the Phase lines

// I still haven't figured out extremely low-power sleep modes. This thing chews through batteries :/
#include <avr/sleep.h>

// LCD has a 4-phase clock - see data log image
unsigned int phase0, phase1, phase2, phase3;

const byte Pin01 = A0;

const unsigned int
  // Pin01 to Pin04 is not used, as these are for the 4 clock phases in the matrix
  Pin05 = 0x0002, Pin06 = 0x0004, Pin07 = 0x0008, Pin08 = 0x0010, Pin09 = 0x0020,                 // PORTC / A1-A5
  Pin10 = 0x0400, Pin11 = 0x0800, Pin12 = 0x1000, Pin13 = 0x2000, Pin14 = 0x4000, Pin15 = 0x8000; // PORTD / 3-7

// the fixed connections to the RGB LED, and the power for the WiFi [so it's not powered all the time]
const byte      
      REDLED = 9, GREENLED = 10, BLUELED =11, WIFIPOWER = 8;
      
const byte BLACK = 0, RED = 1, ORANGE = 2, YELLOW = 3,
           GREEN = 4, CYAN = 5, BLUE = 6, MAGENTA = 7, WHITE = 8;      
           
// 44 display segments - see graphics
boolean fat, water, muscle, kcal, male, female, digit1;  
boolean digit2a, digit2b, digit2c, digit2d, digit2e, digit2f, digit2g;
boolean ftin, colon1;
boolean digit3a, digit3b, digit3c, digit3d, digit3e, digit3f, digit3g;
boolean colon2, dot1;
boolean digit4a, digit4b, digit4c, digit4d, digit4e, digit4f, digit4g;
boolean dot2;
boolean digit5a, digit5b, digit5c, digit5d, digit5e, digit5f, digit5g;
boolean st, lb, kg, percent;

// composites derived from the 4 7-segment digits
char digit2;
char digit3;
char digit4;
char digit5;

byte mode;
unsigned long time;

//----------------------------------------------------------------------
ISR (PCINT1_vect) {}
//----------------------------------------------------------------------
void setup() {
  // wiFiPower, RED, GREEN and BLUE need to be outputs
  // B.6 and B.7 unchanged, as these are being used for OSC1 and OSC2
  DDRB |= 0x0f; PINB &= 0xf0;
  
  // bit B.4 and B.5 need to be inputs (switches) - no change as this is default
  DDRB &= 0xCF; // PORTB |= 0x30;  
  
  // all inputs, with no pull-ups except two MSbs
  DDRC &= 0xC0; PINC &= 0xc0; // |= 0x3f;  
  
  // all inputs, no pull-ups, except serial port
  DDRD &= 0x03; PIND &= 0x03; // |= 0xfC;

  // pin change interrupt
  PCMSK1 |= bit (PCINT8);  // want pin A0
  PCIFR  |= bit (PCIF1);   // clear any outstanding interrupts
  PCICR  |= bit (PCIE1);   // enable pin change interrupts for A0 to A5
}
void(* resetFunc) (void) = 0; //declare reset function at address 0

//-----------------------------------------------------------------------
void loop() {
  byte user = 0; float kilograms = 0, fatPercent = 0, waterPercent = 0;
  
  if (!isPowerOn()) {
    goSleep(); resetFunc();
  }
  
  // fill up all those global variables representing the 44 segments
  getData();
  
  // Mode 1 - 8 for user in memory, Mode 9 if just stepping on scale
  // keep looping if unknown mode
  if (mode) {
    if (9 == mode) { // Anonymous user
      fadeIn(GREEN);
      
      time = millis();
      while (!kg) {
        getData();
        led(0, 16, 0);
        if (elapsed() > 30)  err(2, 1); // timeout awaiting kilograms
      }
      
      led(GREEN);
      
      kilograms = readDisplayValue();
      
      if (-1 == kilograms) {
        err (2, 2);                     // invalid kilogram reading
      }
      
      led(GREEN);
      
      int sendResult = sendData(kilograms);
      
      if (200 == sendResult) {
        flash(CYAN, 6, 10, 240);
        while(isPowerOn()); // wait for scale to turn off
        goSleep(); resetFunc();
      }
      else {
        err(3, sendResult);
      }
    }
    else if ((0 < mode) && (9 > mode)) {
      fadeIn(YELLOW);
      
      // wait for "0.0"
      time = millis();
      while (9 != mode) {
        getData();
        led(32, 16, 0);
        if (elapsed() > 30) err(4, 1); // timeout waiting for "0.0"
      }
      
      led(GREEN);
      
      //------------------------------------------
      time = millis();
      while (!kg) {
        getData();
        led(0, 32, 0);
        if (elapsed() > 30)  err(4, 2); // timeout awaiting kilograms
      }
      
      led(GREEN);
      
      kilograms = readDisplayValue();
      
      if (-1 == kilograms) {
        err (4, 3);                     // invalid kilogram reading
      }
      //--------------------------------------
      time = millis();
      while (!fat) {
        getData();
        if (elapsed() > 30) err(4, 4); // timeout awaiting fatPercent
      }

      led(YELLOW);

      fatPercent = readDisplayValue();

      if (-1 == fatPercent) {
        err(4, 5); // invalid fatPercent reading
      }      
      //------------------------------------------
      time = millis();
      while (!water) {
        getData();
        if (elapsed() > 30) err(4, 6); // timeout awaiting waterPercent
      }

      led(BLUE);

      waterPercent = readDisplayValue();

      if (-1 == waterPercent) {
        err(4, 7); // invalid waterPercent reading
      }      
      //------------------------------------------
      time = millis();
      while (!fat) {
        getData();
        if (elapsed() > 30) err(4, 8); // timeout awaiting SECOND fatPercent
      }

      led(YELLOW);

      fatPercent = readDisplayValue();

      if (-1 == fatPercent) {
        err(4, 9); // invalid SECOND fatPercent reading
      }      
      //------------------------------------------
      time = millis();
      while (!water) {
        getData();
        if (elapsed() > 30) err(4, 10); // timeout awaiting SECOND waterPercent
      }

      led(BLUE);

      waterPercent = readDisplayValue();

      if (-1 == waterPercent) {
        err(4, 11); // invalid SECOND waterPercent reading
      }      
      //------------------------------------------
      time = millis();
      while ('P' != digit3) {
        getData();
        if (elapsed() > 30) err(4, 11); //timeout waiting for user display
      }
      
      switch (digit4) {
        case '1': user = 1;
                  break;
        case '2': user = 2; 
                  break;
        case '3': user = 3;
                  break;
        case '4': user = 4;
                  break;
        case '5': user = 5;
                  break;
        case '6': user = 6;
                  break;
        case '7': user = 7;
                  break;
        case '8': user = 8;
                  break;
        default: err(4, 12);  // invalid user number;
      }

      led(MAGENTA);
      
      int sendResult = sendData(kilograms, fatPercent, waterPercent, user);
      
      if (200 == sendResult) {
        flash(CYAN, 6, 10, 240);
        while(isPowerOn()); // make sure scale's off or the code will recycle
        goSleep(); resetFunc();
      }
      else {
        err(5, sendResult);
      }
    }
  }
}
//-----------------------------------------------------------------------


// WAIT FOR POWER ON BY MONITORING PHASE 0
void waitForOn() {
  while (!digitalRead(A0));
  
  // arbitrary 4 cycle delay for things to settle
  for (byte i = 0; i < 4; i++) {
    while (digitalRead(A0));
    while (!digitalRead(A0));
  }
}

// GET 4 WORDS - 1 FOR EACH PHASE
void getAllPhaseData() {
  int reading;
  unsigned long phaseTime = millis();

  // if Phase 0 is not in the HIGHest state, wait
  while (analogRead(Pin01) < 950) {
    if ((millis() - phaseTime) > 100) err (1, 1); // timeout awaiting start of cycle
  }
  
  // now wait till the HIGHest state is over
  while (analogRead(Pin01) >= 950) {
    if ((millis() - phaseTime) > 100) err (1, 2); // timeout awaiting Phase 0 data
  }
  // and grab Phase 0 data
  phase0 = ((PIND & 0xFC) << 8) | (PINC & 0x3E);
  
  
  // wait until 2nd-highest voltage
  while (analogRead(Pin01) < 640) {
    if ((millis() - phaseTime) > 100) err (1, 3); // timeout awaiting Phase 1 data
  }
  phase1 = ((PIND & 0xFC) << 8) | (PINC & 0x3E);
  // wait until 3rd-hightest voltage
  while (analogRead(Pin01) >= 640) {
    if ((millis() - phaseTime) > 100) err (1, 4); // timeout awaiting Phase 1 / Phase 2 transition
  }

  // wait until 2nd-highest voltage
  while (analogRead(Pin01) < 640) {
    if ((millis() - phaseTime) > 100) err (1, 5); // timeout awaiting Phase 2 data
  }
  phase2 = ((PIND & 0xFC) << 8) | (PINC & 0x3E);
  // wait until 3rd-hightest voltage
  while (analogRead(Pin01) >= 640) {
    if ((millis() - phaseTime) > 100) err (1, 6); // timeout awaiting Phase 2 / Phase 3 transition
  }
  
  // wait until 2nd-highest voltage
  while (analogRead(Pin01) < 640) {
    if ((millis() - phaseTime) > 100) err (1, 7); // timeout awaiting Phase 3 data
  }
  
  phase3 = ((PIND & 0xFC) << 8) | (PINC & 0x3E);
}

void parseData() {
  fat     = (phase0 & Pin15) > 0; // i.e. fat indicator is when Pin 16 is active during Phase 0
  water   = (phase1 & Pin15) > 0; // water = Pin 16 during Phase 1
  muscle  = (phase3 & Pin07) > 0; // muscle = Phase 0, Pin 2
  kcal    = (phase3 & Pin06) > 0;
  male    = (phase2 & Pin15) > 0; // Phase 2, Pin 16
  female  = (phase3 & Pin15) > 0; // 3, 16
  
  digit1  = (phase2 & Pin06) > 0;  // Phase 2, Pin 3 (Digit 1 has only a single segment)
  
  // get all 7 segments of Digit 2, decode them
  digit2a = (phase0 & Pin13) > 0; digit2b = (phase1 & Pin13) > 0; digit2c = (phase2 & Pin13) > 0;
  digit2d = (phase3 & Pin14) > 0; digit2e = (phase2 & Pin14) > 0; digit2f = (phase0 & Pin14) > 0; digit2g = (phase1 & Pin14) > 0;
  digit2 = decodeDigit(digit2a, digit2b, digit2c, digit2d, digit2e, digit2f, digit2g);

  // between digit 2 and 3 there is  the first :  
  colon1  = (phase3 & Pin13) > 0;
  
  // get all 7 segments of Digit 3 and decode them
  digit3a = (phase0 & Pin11) > 0; digit3b = (phase1 & Pin11) > 0; digit3c = (phase2 & Pin11) > 0;
  digit3d = (phase3 & Pin12) > 0; digit3e = (phase2 & Pin12) > 0; digit3f = (phase0 & Pin12) > 0; digit3g = (phase1 & Pin12) > 0;
  digit3 = decodeDigit(digit3a, digit3b, digit3c, digit3d, digit3e, digit3f, digit3g);
  
  // between digit 3 and 4 there is the ', the second : and the first .
  // note that ' is tied to ", so latter is not read independently
  ftin    = (phase0 & Pin06) > 0;
  colon2  = (phase1 & Pin06) > 0;          // don't think I've ever seen the 2nd colon in action
  dot1    = (phase3 & Pin11) > 0;          // ditto
  
  // get and decode Digit 4
  digit4a = (phase0 & Pin09) > 0; digit4b = (phase1 & Pin09) > 0; digit4c = (phase2 & Pin09) > 0;
  digit4d = (phase3 & Pin10) > 0; digit4e = (phase2 & Pin10) > 0; digit4f = (phase0 & Pin10) > 0; digit4g = (phase1 & Pin10) > 0;
  digit4  = decodeDigit(digit4a, digit4b, digit4c, digit4d, digit4e, digit4f, digit4g);
  
  // second . between Digit 4 and Digit 5
  dot2 = (phase3 & Pin09) > 0;
  
  // get and decode Digit 5
  digit5a = (phase0 & Pin07) > 0; digit5b = (phase1 & Pin07) > 0; digit5c = (phase2 & Pin07) > 0;
  digit5d = (phase3 & Pin08) > 0; digit5e = (phase2 & Pin08) > 0; digit5f = (phase0 & Pin08) > 0; digit5g = (phase1 & Pin08) > 0;
  digit5  = decodeDigit(digit5a, digit5b, digit5c, digit5d, digit5e, digit5f, digit5g);
  
  // final symbols, excluding " as it's tied to earlier '
  st      = (phase3 & Pin05) > 0;
  lb      = (phase2 & Pin05) > 0;
  kg      = (phase1 & Pin05) > 0;
  percent = (phase0 & Pin05) > 0;

  if ('P' == digit2) {
    switch (digit5) {
      case '1': mode = 1;
              break;
      case '2': mode = 2; 
              break;
      case '3': mode = 3;
              break;
      case '4': mode = 4;
              break;
      case '5': mode = 5;
              break;
      case '6': mode = 6;
              break;
      case '7': mode = 7;
              break;
      case '8': mode = 8;
              break;
    }
  }
  else if (
             (!digit1)
          && ( (' ' == digit2) || (('0' <= digit2) && ('9' >= digit2))) // hundreds blank or digit
          && ( (' ' == digit3) || (('0' <= digit4) && ('9' >= digit4))) // tens blank or digit
          && ('0' <= digit4) && ('9' >= digit4) // units must be a digit
          && (dot2) // dot must be active
          && ('0' <= digit5) && ('9' >= digit5) // decimal must be a digit
          
          && !kg 
          && !percent) {
    mode = 9;
  }
  else {
    mode = 0;
  }
}

String convertDataToString() {
  String stringData;
  
  stringData = fat?"f":" ";
  stringData += water?"w":" ";
  stringData += muscle?"m":" ";
  stringData += kcal?"k":" ";
  stringData += male?"M":" ";
  stringData += female?"F":" ";
  stringData += " ";
  stringData += digit1?"1 ":"  ";
  stringData += digit2;
  stringData += colon1?":":" ";
  stringData += digit3;
  stringData += colon2?":":" ";
  stringData += ftin?"'":" ";
  stringData += dot1?".":" ";
  stringData += digit4;
  stringData += dot2?".":" ";
  stringData += digit5;
  stringData += " ";
  stringData += st?"st ":"   ";
  stringData += lb?"lb ":"   ";
  stringData += kg?"kg ":"   ";
  stringData += percent?"%":" ";
  
  return stringData;
}

// TURN 7 SEGMENTS INTO A VALUE
char decodeDigit(boolean a, boolean b, boolean c, boolean d, boolean e, boolean f, boolean g) {
  byte binaryValue = (a?0x40:0) + (b?0x20:0) + (c?0x10:0) + (d?0x08:0) + (e?0x04:0) + (f?0x02:0) + (g?0x01:0);
  
  switch (binaryValue) {
    case 0x00: return ' '; break;
    case 0x01: return '-'; break;
    case 0x02: return '\''; break;
    case 0x06: return '|'; break;
    case 0x08: return '_'; break;
    case 0x09: return '='; break;
    case 0x20: return '\''; break;
    case 0x22: return '"'; break;
    case 0x40: return '`'; break;
    case 0x62: return '~'; break;
    case 0x63: return 'º'; break;
    case 0x65: return '?'; break;
    case 0x77: return 'A'; break;
    case 0x1f: return 'b'; break;
    case 0x4e: return 'C'; break;
    case 0x0d: return 'c'; break;
    case 0x43: return 'c'; break;
    case 0x3d: return 'd'; break;
    case 0x4f: return 'E'; break;
    case 0x47: return 'F'; break;
    case 0x5e: return 'G'; break;
    case 0x37: return 'H'; break;
    case 0x17: return 'h'; break;
    case 0x3c: return 'J'; break;
    case 0x0e: return 'L'; break;
    case 0x55: return 'm'; break;
    case 0x15: return 'n'; break;
    case 0x1d: return 'o'; break;
    case 0x67: return 'P'; break;
    case 0x73: return 'q'; break;
    case 0x05: return 'r'; break;
    case 0x3e: return 'U'; break;
    case 0x1c: return 'u'; break;
    case 0x2b: return 'w'; break;
    case 0x3b: return 'y'; break;

    case 0x7e: return '0'; break;
    case 0x30: return '1'; break;
    case 0x6d: return '2'; break;
    case 0x79: return '3'; break;
    case 0x33: return '4'; break;
    case 0x5b: return '5'; break;
    case 0x5f: return '6'; break;
    case 0x70: return '7'; break;
    case 0x7f: return '8'; break;
    case 0x7b: return '9'; break;
  }   
}

//-----------------------------------------------------------------------------------------
// getData()
//
// reads all segments in all phases, TWICE, until both reads are consistent (error-correction)
// then converts the data to more useful variables
//
void getData() {
  boolean dataValid = false;
  unsigned int oldPhase0, oldPhase1, oldPhase2, oldPhase3;
  
  while (false == dataValid) {
    // take at 2 samples of a reading, as a transition might yield garbage data
    
    // get the data
    getAllPhaseData();
    // store it
    oldPhase0 = phase0; oldPhase1 = phase1; oldPhase2 = phase2; oldPhase3 = phase3;
    // get it again
    getAllPhaseData();
    // compare: if the two samples aren't identical, we don't consider it valid
    dataValid = ((oldPhase0 == phase0) && (oldPhase1 == phase1) && (oldPhase2 == phase2) && (oldPhase3 == phase3));
  }
  // make something usable out of all that binary data (put it in individual variables)

  parseData();
}

//--------------------------------------------------------------------------------------------
// readDisplayValue()
//
//
float readDisplayValue() {
  float hundreds = 0, tens = 0, units = 0, tenths = 0;

  if ((0x2f < digit2) && (0x3a > digit2)) {
    hundreds = (digit2 - 0x30);
  }
  else if (0x20 != digit2) return -1;
  
  if ((0x2f < digit3) && (0x3a > digit3)) {
    tens = (digit3 - 0x30);
  }
  else if (0x20 != digit3) return -1;
  
  if ((0x2f < digit4) && (0x3a > digit4)) {
    units = (digit4 - 0x30);
  }
  else if (0x20 != digit4) return -1;
  
  if ((0x2f < digit5) && (0x3a > digit5)) {
    tenths = (digit5 - 0x30);
  }
  else if (0x20 != digit5) return -1;
  
  return hundreds * 100 + tens * 10 + units + tenths / 10;
}
//--------------------------------------------------------------------------------------------

int sendData(float kilograms) {
  // using CRLF in communicating with the ESP8266 caused endless frustration with CIPSEND
  // payload length. It uses just the CR and then counts the LF as part of the payload,
  // resulting in the server returning "500 Internal Server Error"
  // Rather just use CR (\r)
  String atRST      = "AT+RST\r";
  String atCWJAP    = "AT+CWJAP=\"SSID\",\"password\"\r";
  String atCIPMUX   = "AT+CIPMUX=0\r";
  String atCIPSTART = "AT+CIPSTART=\"TCP\",\"example.com\",80\r";
  String atCIPSEND  = "AT+CIPSEND="; // to be completed:...
  
  String postData = "kilograms=" + (String)kilograms;
  
  // in this case, while just \n works, the protocol specification for HTTP
  // defines a line break as \r\n, so that's what I'll use
  String headers  = "POST /example_api.php HTTP/1.1\r\n";
         headers += "Host: example.com\r\n";
         headers += "User-Agent: brian.tw.scale/1.0\r\n";
         headers += "Connection: close\r\n";
         headers += "Content-Type: application/x-www-form-urlencoded\r\n";
         headers += "Content-Length: " + (String)postData.length() + "\r\n";
         headers += "\r\n";
  
  int payLoadLength = headers.length() + postData.length();

  atCIPSEND += (String)payLoadLength;
  atCIPSEND += "\r";               // ...here.
  
  Serial.begin(115200);
  digitalWrite(WIFIPOWER, HIGH);

  time = millis();
  while (!Serial.find("\r\nready\r\n")) {
    if (elapsed() > 3) {
      if (Serial.available()) {
        return 1; // received something other than "\r\nready\r\n" in response to power on
      }
      else {
        return 2; // timeout awaiting response to power on
      }
    }
  }
  
  // we have "ready", move on...
  
  Serial.print(atCIPMUX);
  time = millis();
  while (!Serial.find("\r\nOK\r\n")) {
    if (elapsed() > 1) {
      if (Serial.available()) {
        return 3; // received something other than "\r\nOK\r\n" in response to CIPMUX
      }
      else {
        return 4; // timeout awaiting response to CIPMUX
      }
    }
  }
  
  // we have "OK", move on...
  
  Serial.print(atCIPSTART);
  time = millis();
  while (!Serial.find("\r\nLinked\r\n")) {
    if (elapsed() > 20) {
      return 9; // timeout waiting for "Linked"
    }
    
    // none of this works - I have very little idea why
    // but probably Heisenberg. the previous read changes the data in the buffer
    /*
    else {
      if (Serial.find("\r\nERROR\r\n")) {
        return 5; // general error
      }
      else if (Serial.find("DNS Fail/r/n")) {
        return 6; // DNS lookup fail
      }
      else if (Serial.find("Link typ ERROR\r\n")) {
        return 7; // link type error
      }
      else if (Serial.find("ENTRY ERROR")) {
        return 8; // entry error
      } 
    } */
    
  }
  
  // we have "Linked", move on...
  
  Serial.print(atCIPSEND);
  
  time = millis();
  while (!Serial.find("> ")) {
    if (elapsed() > 1) {
      if (Serial.available()) {
        return 10; // something other than "> " received in response to CIPSEND
      }
      else {
        return 11; // timeout - nothing received in response to CIPSEND
      }
    }
  }
  
  // We have the go-ahead, "> ", send...
  
  Serial.print(headers);
  Serial.print(postData);
  
  // We've sent, wait for chip response...
  
  time = millis();
  while (!Serial.find("SEND OK\r\n")) {
    if (elapsed() > 2) {
      if (Serial.available()) {
        return 12; // something other than "SEND OK\r\n" received
      }
      else {
        return 13; // timeout - nothing received in response to headers & postData;
      }
    }
  }

  // chip reports data sent, await webpage...
  
  while (!Serial.find("+IPD,")) {
    if (elapsed() > 13) {
      if (Serial.available()) {
        return 14; // something other than "+IPD" came back from chip in response to web query
      }
      else {
        return 15; // timeout - nothing come back after "SEND OK\r\n";
      }
    }
  }
  
  // we got data back from the site, see if it's valid...
  
  // Does it appear to have a header code?
  if (Serial.find("HTTP/1.1 ")) {
    
    String responseCode = "";
    
    delay(1); // give enough time for 3 bytes to come in, just in case...
    
    for (int i = 0; i < 3; i++) {
      responseCode += (char)Serial.read();
    }
    
    if (responseCode == "200") {
      return 200; // success!
    }
    else {
      return 17; // got a response code, but it was other than 200;
    }
  }
  else {
    return 16; // got +IPD back, but no HTTP/1.1
  }
}
  
//--------------------------------------------------------------------------------------------------------
int sendData(float kilograms, float fatPercent, float waterPercent, int user) {
  // using CRLF in communicating with the ESP8266 caused endless frustration with CIPSEND
  // payload length. It uses just the CR and then counts the LF as part of the payload,
  // resulting in the server returning "500 Internal Server Error"
  // Rather just use CR (\r)
  String atRST      = "AT+RST\r";
  String atCWJAP    = "AT+CWJAP=\"SSID\",\"password\"\r";
  String atCIPMUX   = "AT+CIPMUX=0\r";
  String atCIPSTART = "AT+CIPSTART=\"TCP\",\"example.com\",80\r";
  String atCIPSEND  = "AT+CIPSEND="; // to be completed:...
  
  String postData = "kilograms=" + (String)kilograms + "&fat_percent=" + (String)fatPercent + 
                    "&water_percent=" + (String)waterPercent + "&user=" + (String)user + 
                    "&unique_key=anwldughansidlwyhdnaiwplchaldsah";
  
  // in this case, while just \n works, the protocol specification for HTTP
  // defines a line break as \r\n, so that's what I'll use
  String headers  = "POST /example_api.php HTTP/1.1\r\n";
         headers += "Host: example.com\r\n";
         headers += "User-Agent: brian.tw.scale/1.0\r\n";
         headers += "Connection: close\r\n";
         headers += "Content-Type: application/x-www-form-urlencoded\r\n";
         headers += "Content-Length: " + (String)postData.length() + "\r\n";
         headers += "\r\n";
  
  int payLoadLength = headers.length() + postData.length();

  atCIPSEND += (String)payLoadLength;
  atCIPSEND += "\r";               // ...here.
  
  Serial.begin(115200);
  digitalWrite(WIFIPOWER, HIGH);

  time = millis();
  while (!Serial.find("\r\nready\r\n")) {
    if (elapsed() > 3) {
      if (Serial.available()) {
        return 1; // received something other than "\r\nready\r\n" in response to power on
      }
      else {
        return 2; // timeout awaiting response to power on
      }
    }
  }
  
  // we have "ready", move on...
  
  Serial.print(atCIPMUX);
  time = millis();
  while (!Serial.find("\r\nOK\r\n")) {
    if (elapsed() > 1) {
      if (Serial.available()) {
        return 3; // received something other than "\r\nOK\r\n" in response to CIPMUX
      }
      else {
        return 4; // timeout awaiting response to CIPMUX
      }
    }
  }
  
  // we have "OK", move on...
  
  Serial.print(atCIPSTART);
  time = millis();
  while (!Serial.find("\r\nLinked\r\n")) {
    if (elapsed() > 20) {
      return 9; // timeout waiting for "Linked"
    }
    
    // none of this works - I have very little idea why
    // but probably Heisenberg. the previous read changes the data in the buffer
    /*
    else {
      if (Serial.find("\r\nERROR\r\n")) {
        return 5; // general error
      }
      else if (Serial.find("DNS Fail/r/n")) {
        return 6; // DNS lookup fail
      }
      else if (Serial.find("Link typ ERROR\r\n")) {
        return 7; // link type error
      }
      else if (Serial.find("ENTRY ERROR")) {
        return 8; // entry error
      } 
    } */
    
  }
  
  // we have "Linked", move on...
  
  Serial.print(atCIPSEND);
  
  time = millis();
  while (!Serial.find("> ")) {
    if (elapsed() > 1) {
      if (Serial.available()) {
        return 10; // something other than "> " received in response to CIPSEND
      }
      else {
        return 11; // timeout - nothing received in response to CIPSEND
      }
    }
  }
  
  // We have the go-ahead, "> ", send...
  
  Serial.print(headers);
  Serial.print(postData);
  
  // We've sent, wait for chip response...
  
  time = millis();
  while (!Serial.find("SEND OK\r\n")) {
    if (elapsed() > 2) {
      if (Serial.available()) {
        return 12; // something other than "SEND OK\r\n" received
      }
      else {
        return 13; // timeout - nothing received in response to headers & postData;
      }
    }
  }

  // chip reports data sent, await webpage...
  
  while (!Serial.find("+IPD,")) {
    if (elapsed() > 13) {
      if (Serial.available()) {
        return 14; // something other than "+IPD" came back from chip in response to web query
      }
      else {
        return 15; // timeout - nothing come back after "SEND OK\r\n";
      }
    }
  }
  
  // we got data back from the site, see if it's valid...
  
  // Does it appear to have a header code?
  if (Serial.find("HTTP/1.1 ")) {
    
    String responseCode = "";
    
    delay(1); // give enough time for 3 bytes to come in, just in case...
    
    for (int i = 0; i < 3; i++) {
      responseCode += (char)Serial.read();
    }
    
    if (responseCode == "200") {
      return 200; // success!
    }
    else {
      return 17; // got a response code, but it was other than 200;
    }
  }
  else {
    return 16; // got +IPD back, but no HTTP/1.1
  }
}
//-------------------------------------------------------------------------------------------------------
void fadeIn(byte colour) {
  
  allLedsOff(); delay(250);
  
  switch (colour) {
    case BLACK: led(0, 0, 0);
                delay(256);
                break;
                
    case RED: for (int i = 0; i < 128; i++) {
                analogWrite(REDLED, i);
                delay(2);
              }
              break;
              
    case ORANGE: for (int i = 0; i < 128; i++) {
                   analogWrite(REDLED, i);
                   analogWrite(GREENLED, i >> 1);  
                   delay(2);
                 }
                 break;
                 
    case YELLOW: for (int i = 0; i < 128; i++) {
                   analogWrite(REDLED, i);
                   analogWrite(GREENLED, i);
                   delay(2);
                 }
                 break;
  
                   
     case GREEN: for (int i = 0; i < 128; i++) {
                   analogWrite(REDLED, i >> 6);
                   analogWrite(GREENLED, i);
                   delay(2);
                 }
                 break;
                 
     case CYAN: for (int i = 0; i < 255; i++) {
                  analogWrite(GREENLED, i / 3);
                  analogWrite(BLUELED, i);
                  delay(1);
                }
                break;  
                 
     case BLUE: for (int i = 0; i < 256; i++) {
                  analogWrite(BLUELED, i);
                  delay(1);
                }
                break;
                
     case MAGENTA: for (int i = 0; i < 256; i++) {
                    analogWrite(REDLED, i >> 2);
                    analogWrite(BLUELED, i);
                    delay(1);
                  }
                  break;
                  
     case WHITE: for (int i = 0; i < 256; i++) {
                   analogWrite(REDLED, i / 2.5);
                   analogWrite(GREENLED, i / 2);
                   analogWrite(BLUELED, i);
                   delay(1);
                 }
                 break;
  }
}

void allLedsOff() {
  analogWrite(REDLED, 0);
  analogWrite(GREENLED, 0);
  analogWrite(BLUELED, 0);
}

void led(int r, int g, int b) {
  analogWrite(REDLED, r);
  analogWrite(GREENLED, g);
  analogWrite(BLUELED, b);
}

void led(byte colour) {
  switch (colour) {
    case BLACK:   led(0,     0,   0); break;
    case RED:     led(128,   0,   0); break;
    case ORANGE:  led(128,  64,   0); break;
    case YELLOW:  led(128, 128,   0); break;
    case GREEN:   led(1,   128,   0); break;
    case CYAN:    led(0,    92, 255); break;
    case BLUE:    led(0,     0, 255); break;
    case MAGENTA: led(64,    0, 255); break;
    case WHITE:   led(96,  140, 255); break;
  }
}

void flash(int colour, int times, int onTime, int offTime) {
  for (int i = 0; i < times; i++) {
    led(colour);
    delay(onTime);
    led(BLACK);
    delay(offTime);
  }
}

boolean isPowerOn() {
  boolean powerOn = 0;
  unsigned long end_time = millis() + 1000;
  
  while (millis() < end_time) {
    if (analogRead(Pin01) > 50) {
      powerOn = true;
    }
  }
  
  return powerOn;
}

void err(int type, int subtype) {
  flash(RED, type, 1000, 1000);
  flash(WHITE, subtype, 333, 333);
  while(isPowerOn());
  goSleep(); resetFunc();
}

int elapsed() {
  return (millis() - time) / 1000;
}

void goSleep() {
  digitalWrite(8, LOW);
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);  
  sleep_mode ();
}    