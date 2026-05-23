/*
  Arduino Nano classic (ATmega328P) + Fermion DFPlayer Pro (DFR0768 / DF1201S)
  + 10 Gravity Digital Push Buttons
  + ON/OFF switch + B10K potentiometer

  VERSION:
    Classic Arduino Nano version corresponding to the Nano Every version
    where Button 10 was moved from D13 to D3.

  DFPlayer Pro wiring:
    DFPlayer TX  -> Arduino Nano D0 / RX
    DFPlayer RX  <- Arduino Nano D1 / TX
    DFPlayer VIN -> 5V
    DFPlayer GND -> GND

  Switch:
    One switch pin -> D2
    Other switch pin -> GND

  Potentiometer B10K:
    One outer pin   -> 5V
    Middle pin      -> A0
    Other outer pin -> GND

  Buttons:
    Button 1  signal -> D4
    Button 2  signal -> D5
    Button 3  signal -> D6
    Button 4  signal -> D7
    Button 5  signal -> D8
    Button 6  signal -> D9
    Button 7  signal -> D10
    Button 8  signal -> D11
    Button 9  signal -> D12
    Button 10 signal -> D3

    Gravity Digital Push Button:
      VCC -> 5V
      GND -> GND
      SIG -> Arduino pin

  IMPORTANT FOR CLASSIC ARDUINO NANO:
    The classic Nano has only one hardware serial port.
    This is Serial, and it is shared by:
      - USB upload
      - Serial Monitor
      - D0/RX and D1/TX
      - DFPlayer Pro

    Therefore, if upload fails:
      1) Disconnect DFPlayer TX/RX from D0/D1.
      2) Upload the sketch.
      3) Reconnect DFPlayer TX/RX.

    In normal operation, keep DFPlayer connected to D0/D1.

  Audio files in the root of DFPlayer Pro storage:
    /1.mp3  ... /30.mp3

  Function:
    Switch OFF:
      Button 1-10 -> /1.mp3 ... /10.mp3

    Switch ON and potentiometer A0 from 0 to 511:
      Button 1-10 -> /11.mp3 ... /20.mp3

    Switch ON and potentiometer A0 from 512 to 1023:
      Button 1-10 -> /21.mp3 ... /30.mp3
*/

// ------------------------- Pin settings -------------------------

const byte BUTTON_COUNT = 10;

// Button 10 is on D3, NOT D13.
const byte buttonPins[BUTTON_COUNT] = {
  4, 5, 6, 7, 8, 9, 10, 11, 12, 3
};

const byte SWITCH_PIN = 2;
const byte POT_PIN = A0;

// With this wiring, switch ON means D2 is connected to GND.
// Because we use INPUT_PULLUP, ON = LOW and OFF = HIGH.
const bool SWITCH_ON_IS_LOW = true;

// ------------------------- Behaviour settings -------------------------

const unsigned long DEBOUNCE_MS = 45;
const unsigned long MIN_RETRIGGER_MS = 300;

// DFPlayer Pro default UART speed.
const unsigned long DFPLAYER_BAUD = 115200;

// Volume range: 0 to 30.
const byte VOLUME = 30;

// ------------------------- Button state variables -------------------------

byte restState[BUTTON_COUNT];
byte lastRawState[BUTTON_COUNT];
byte stableState[BUTTON_COUNT];

unsigned long lastChangeTime[BUTTON_COUNT];
unsigned long lastPlayTime = 0;

// ------------------------- DFPlayer Pro AT commands -------------------------

void sendAT(const char* command) {
  Serial.print(command);
  Serial.print("\r\n");
}

void clearDFPlayerReplies() {
  while (Serial.available()) {
    Serial.read();
  }
}

void setupDFPlayerPro() {
  Serial.begin(DFPLAYER_BAUD);
  delay(1000);

  // Test communication.
  sendAT("AT");
  delay(150);

  // Music mode.
  sendAT("AT+FUNCTION=1");
  delay(1600);

  // Set volume.
  Serial.print(F("AT+VOL="));
  Serial.print(VOLUME);
  Serial.print("\r\n");
  delay(150);

  // Play one file once, then stop.
  sendAT("AT+PLAYMODE=3");
  delay(150);

  clearDFPlayerReplies();
}

void playTrackNumber(byte trackNumber) {
  // Official command format:
  // AT+PLAYFILE=/1.mp3
  Serial.print(F("AT+PLAYFILE=/"));
  Serial.print(trackNumber);
  Serial.print(F(".mp3\r\n"));

  delay(20);
  clearDFPlayerReplies();
}

// ------------------------- Input reading -------------------------

bool isSwitchOn() {
  byte s = digitalRead(SWITCH_PIN);

  if (SWITCH_ON_IS_LOW) {
    return (s == LOW);
  } else {
    return (s == HIGH);
  }
}

byte getTrackOffset() {
  // Switch OFF -> tracks 1 to 10.
  if (!isSwitchOn()) {
    return 0;
  }

  // Switch ON -> read potentiometer.
  int potValue = analogRead(POT_PIN);

  if (potValue <= 511) {
    return 10;  // tracks 11 to 20
  } else {
    return 20;  // tracks 21 to 30
  }
}

void setupButtons() {
  for (byte i = 0; i < BUTTON_COUNT; i++) {
    pinMode(buttonPins[i], INPUT);

    // Important: power on with all buttons released.
    byte state = digitalRead(buttonPins[i]);

    restState[i] = state;
    lastRawState[i] = state;
    stableState[i] = state;
    lastChangeTime[i] = millis();
  }
}

// ------------------------- Arduino setup -------------------------

void setup() {
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(POT_PIN, INPUT);

  setupButtons();
  setupDFPlayerPro();
}

// ------------------------- Arduino loop -------------------------

void loop() {
  unsigned long now = millis();

  clearDFPlayerReplies();

  for (byte i = 0; i < BUTTON_COUNT; i++) {
    byte raw = digitalRead(buttonPins[i]);

    // Raw state changed: restart debounce timer.
    if (raw != lastRawState[i]) {
      lastRawState[i] = raw;
      lastChangeTime[i] = now;
    }

    // Stable state confirmed.
    if ((now - lastChangeTime[i]) >= DEBOUNCE_MS && raw != stableState[i]) {
      stableState[i] = raw;

      /*
        Button press = state different from the startup released state.
        This works for both active-HIGH and active-LOW Gravity button modules.

        Important:
        When powering on the Arduino, all buttons must be released.
      */
      bool pressed = (stableState[i] != restState[i]);

      if (pressed && (now - lastPlayTime >= MIN_RETRIGGER_MS)) {
        byte offset = getTrackOffset();
        byte trackNumber = offset + i + 1;

        playTrackNumber(trackNumber);
        lastPlayTime = now;
      }
    }
  }
}
