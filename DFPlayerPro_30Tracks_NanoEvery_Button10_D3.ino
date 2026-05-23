/*
  Arduino Nano Every + Fermion DFPlayer Pro (DFR0768 / DF1201S)
  + 10 Gravity Digital Push Buttons
  + B10K potentiometer with integrated ON/OFF switch

  VERSION: 30 AUDIO TRACKS / 3 BANKS / BUTTON 10 ON D3 INSTEAD OF D13

  ------------------------------------------------------------
  AUDIO BANKS
  ------------------------------------------------------------
  SWITCH OFF:
    Button 1..10 -> /1.mp3 .. /10.mp3
    1-5  = English
    6-10 = Lithuanian

  SWITCH ON:
    Potentiometer reading 0..511:
      Button 1..10 -> /11.mp3 .. /20.mp3
      11-15 = Italian
      16-20 = Spanish

    Potentiometer reading 512..1023:
      Button 1..10 -> /21.mp3 .. /30.mp3
      21-25 = French
      26-30 = German

  ------------------------------------------------------------
  IMPORTANT
  ------------------------------------------------------------
  This sketch DOES NOT use the DFRobot_DF1201S library.
  It sends DFPlayer Pro AT commands directly through Serial1.


  NOTE ABOUT BUTTON 10:
    Button 10 is now on D3 instead of D13.
    D13 on Nano Every is also SPI SCK/GPIO and in your diagnostic test it did not respond through the shield.
    D3 is free in this project, so it is the cleanest replacement.

  ------------------------------------------------------------
  DFPLAYER PRO <-> NANO EVERY WIRING
  ------------------------------------------------------------
    DFPlayer TX  -> Nano Every D0 / RX1
    DFPlayer RX  -> Nano Every D1 / TX1
    DFPlayer VIN -> 5V
    DFPlayer GND -> GND

  ------------------------------------------------------------
  10 GRAVITY DIGITAL PUSH BUTTONS
  ------------------------------------------------------------
    Button 1  Signal -> D4
    Button 2  Signal -> D5
    Button 3  Signal -> D6
    Button 4  Signal -> D7
    Button 5  Signal -> D8
    Button 6  Signal -> D9
    Button 7  Signal -> D10
    Button 8  Signal -> D11
    Button 9  Signal -> D12
    Button 10 Signal -> D3   // moved from D13 because D13 failed the button diagnostic

    Each Gravity button module:
      VCC -> 5V
      GND -> GND
      SIG -> corresponding digital pin

  ------------------------------------------------------------
  B10K POTENTIOMETER CONNECTION
  ------------------------------------------------------------
    One outer potentiometer pin  -> 5V
    Middle potentiometer pin     -> A0
    Other outer potentiometer pin-> GND

    If turning direction feels reversed, swap the two outer pins.

  ------------------------------------------------------------
  INTEGRATED ON/OFF SWITCH CONNECTION
  ------------------------------------------------------------
    One switch pin -> D2
    Other switch pin -> GND

    D2 uses INPUT_PULLUP.
    In the normal wiring used here:
      switch ON  -> D2 reads LOW
      switch OFF -> D2 reads HIGH

    If your particular switch behaves opposite, change:
      const bool SWITCH_ON_IS_LOW = true;
    to:
      const bool SWITCH_ON_IS_LOW = false;
*/

// ------------------------------------------------------------
// Pins
// ------------------------------------------------------------
const uint8_t BUTTON_COUNT = 10;

const uint8_t buttonPins[BUTTON_COUNT] = {
  4, 5, 6, 7, 8, 9, 10, 11, 12, 3
};

const uint8_t MODE_SWITCH_PIN = 2;  // Integrated ON/OFF switch
const uint8_t POT_PIN = A0;         // Potentiometer wiper

// true:  switch ON = LOW, because we use INPUT_PULLUP and switch goes to GND
// false: switch ON = HIGH, only if your switch behaves the opposite way
const bool SWITCH_ON_IS_LOW = true;

// ------------------------------------------------------------
// Behaviour settings
// ------------------------------------------------------------
const unsigned long DEBOUNCE_MS = 45;
const unsigned long MIN_RETRIGGER_MS = 250;

// ------------------------------------------------------------
// Button state memory
// ------------------------------------------------------------
uint8_t restState[BUTTON_COUNT];
uint8_t lastRawState[BUTTON_COUNT];
uint8_t stableState[BUTTON_COUNT];
unsigned long lastChangeTime[BUTTON_COUNT];
unsigned long lastPlayTime[BUTTON_COUNT];

// ------------------------------------------------------------
// Send a simple AT command to DFPlayer Pro
// ------------------------------------------------------------
void sendAT(const char* command) {
  Serial1.print(command);
  Serial1.print("\r\n");

  Serial.print(F("Sent: "));
  Serial.println(command);
}

// ------------------------------------------------------------
// Play an exact root-file path: /1.mp3 ... /30.mp3
// ------------------------------------------------------------
void playTrackNumber(uint8_t trackNumber) {
  Serial1.print(F("AT+PLAYFILE=/"));
  Serial1.print(trackNumber);
  Serial1.print(F(".mp3\r\n"));

  Serial.print(F("Playing track: /"));
  Serial.print(trackNumber);
  Serial.println(F(".mp3"));
}

// ------------------------------------------------------------
// Print possible replies from DFPlayer Pro in Serial Monitor
// ------------------------------------------------------------
void printDFPlayerReplies() {
  while (Serial1.available()) {
    char c = Serial1.read();
    Serial.write(c);
  }
}

// ------------------------------------------------------------
// Read integrated switch state
// ------------------------------------------------------------
bool isModeSwitchOn() {
  uint8_t state = digitalRead(MODE_SWITCH_PIN);

  if (SWITCH_ON_IS_LOW) {
    return (state == LOW);
  } else {
    return (state == HIGH);
  }
}

// ------------------------------------------------------------
// Average a few A0 readings for a steadier choice near the middle
// ------------------------------------------------------------
uint16_t readPotAverage() {
  const uint8_t samples = 8;
  uint32_t sum = 0;

  for (uint8_t i = 0; i < samples; i++) {
    sum += analogRead(POT_PIN);
  }

  return (uint16_t)(sum / samples);
}

// ------------------------------------------------------------
// Decide audio bank:
//   0 -> tracks 1..10
//   1 -> tracks 11..20
//   2 -> tracks 21..30
// ------------------------------------------------------------
uint8_t getAudioBank(uint16_t &potValueUsed) {
  if (!isModeSwitchOn()) {
    potValueUsed = 0;
    return 0;
  }

  potValueUsed = readPotAverage();

  if (potValueUsed <= 511) {
    return 1;
  } else {
    return 2;
  }
}

// ------------------------------------------------------------
// Convert button index 0..9 to actual track number 1..30
// ------------------------------------------------------------
uint8_t getTrackForButton(uint8_t buttonIndex, uint8_t bank) {
  return (buttonIndex + 1) + (bank * 10);
}

// ------------------------------------------------------------
// Buttons setup
// ------------------------------------------------------------
void setupButtons() {
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    pinMode(buttonPins[i], INPUT);

    // Start with all buttons released while powering up.
    uint8_t state = digitalRead(buttonPins[i]);

    restState[i] = state;
    lastRawState[i] = state;
    stableState[i] = state;
    lastChangeTime[i] = millis();
    lastPlayTime[i] = 0;
  }
}

// ------------------------------------------------------------
// Switch + potentiometer setup
// ------------------------------------------------------------
void setupModeSelector() {
  pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);
  pinMode(POT_PIN, INPUT);
}

// ------------------------------------------------------------
// DFPlayer Pro setup with official AT commands
// ------------------------------------------------------------
void setupDFPlayerPro() {
  Serial1.begin(115200);
  delay(800);

  sendAT("AT");
  delay(150);

  // Music mode
  sendAT("AT+FUNCTION=1");
  delay(1600);

  // Volume 0..30
  sendAT("AT+VOL=30");
  delay(150);

  // Mode 3: play one file, then pause/stop
  sendAT("AT+PLAYMODE=3");
  delay(150);

  Serial.println(F("DFPlayer Pro setup completed."));
}

// ------------------------------------------------------------
// Arduino setup
// ------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println(F("Nano Every + DFPlayer Pro + 10 Buttons + Switch/Pot Bank Selector"));
  Serial.println(F("Direct AT-command version, no DFRobot library."));

  setupModeSelector();
  setupButtons();
  setupDFPlayerPro();

  Serial.println(F("Ready."));
  Serial.println(F("Switch OFF -> tracks 1..10"));
  Serial.println(F("Switch ON + pot <= 511 -> tracks 11..20"));
  Serial.println(F("Switch ON + pot >= 512 -> tracks 21..30"));
}

// ------------------------------------------------------------
// Main loop
// ------------------------------------------------------------
void loop() {
  const unsigned long now = millis();

  // Show any DFPlayer Pro serial response in Serial Monitor
  printDFPlayerReplies();

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    uint8_t raw = digitalRead(buttonPins[i]);

    // Restart debounce timer when raw signal changes
    if (raw != lastRawState[i]) {
      lastRawState[i] = raw;
      lastChangeTime[i] = now;
    }

    // Accept a new stable button state after debounce time
    if ((now - lastChangeTime[i]) >= DEBOUNCE_MS && raw != stableState[i]) {
      stableState[i] = raw;

      // A press is detected when the button state differs from its resting startup state.
      // This keeps compatibility with Gravity buttons that may be active-HIGH or active-LOW.
      if (stableState[i] != restState[i]) {
        if ((now - lastPlayTime[i]) >= MIN_RETRIGGER_MS) {
          uint16_t potValue = 0;
          uint8_t bank = getAudioBank(potValue);
          uint8_t trackNumber = getTrackForButton(i, bank);

          Serial.print(F("Button "));
          Serial.print(i + 1);
          Serial.print(F(" pressed | Bank "));
          Serial.print(bank);

          if (bank == 0) {
            Serial.print(F(" | switch OFF"));
          } else {
            Serial.print(F(" | switch ON | pot="));
            Serial.print(potValue);
          }

          Serial.print(F(" | track="));
          Serial.println(trackNumber);

          playTrackNumber(trackNumber);
          lastPlayTime[i] = now;
        }
      }
    }
  }
}
