// Pin Definitions (Matches Excel: Button 1 to 11 -> Pins 26 to 46)
const int buttonPins[11] = {26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46};
// LED Definitions (Matches Excel: LED 1 to 5 -> Pins 2 to 6)
const int ledPins[5] = {2, 3, 4, 5, 6};

int lastButtonState[11];
unsigned long lastDebounceTime[11] = {0};
const unsigned long debounceDelay = 50;

// System Flags
bool isRunning = false;
bool notifiedFull = false;
bool beltReportedRunning = false;

// Counters
int counter1 = 0;         // Plastic Magazine Counter (Max 5)
int assemblyCount = 0;    // Counter 2: Full Assemblies
int rejectCount = 0;      // Counter 3: Plastic/Metal Rejects

// Multi-Part Conveyor Queue (Max 20)
const int MAX_QUEUE = 20;
int partQueue[MAX_QUEUE]; // 1 = FULL ASSEMBLY, 2 = REJECT (Plastic/Metal)
int queueHead = 0;
int queueTail = 0;
int queueCount = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("=========================================");
  Serial.println("  BYTRONICS ICT3 SYSTEM INITIALIZED      ");
  Serial.println("  Status: LOCKED (Press Button 10/IP6)   ");
  Serial.println("=========================================");

  for (int i = 0; i < 11; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    lastButtonState[i] = HIGH;
  }

  for (int i = 0; i < 5; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
}

void loop() {
  startFlag();
  stopFlag();

  if (isRunning) {
    beltMovement();
  }
}

// QUEUE & HELPER FUNCTIONS

void enqueuePart(int partType) {
  if (queueCount < MAX_QUEUE) {
    partQueue[queueTail] = partType;
    queueTail = (queueTail + 1) % MAX_QUEUE;
    queueCount++;

    Serial.print("[QUEUE] Added ");
    Serial.print(partType == 1 ? "FULL ASSEMBLY" : "REJECT (Metal/Plastic)");
    Serial.print(" | Parts currently on belt: ");
    Serial.println(queueCount);
  } else {
    Serial.println("[QUEUE ERROR] Belt queue full! Cannot track more parts.");
  }
}

int dequeuePart() {
  if (queueCount > 0) {
    int partType = partQueue[queueHead];
    queueHead = (queueHead + 1) % MAX_QUEUE;
    queueCount--;
    return partType;
  }
  return 0; // Empty
}

bool isButtonPressed(int btnNum) {
  return digitalRead(buttonPins[btnNum - 1]) == LOW;
}

void pulseLED(int ledNum, const char* opName, int durationMs) {
  Serial.print("[OUTPUT] LED ");
  Serial.print(ledNum);
  Serial.print(" (");
  Serial.print(opName);
  Serial.println(") -> ON");

  digitalWrite(ledPins[ledNum - 1], HIGH);
  delay(durationMs);
  digitalWrite(ledPins[ledNum - 1], LOW);

  Serial.print("[OUTPUT] LED ");
  Serial.print(ledNum);
  Serial.print(" (");
  Serial.print(opName);
  Serial.println(") -> OFF");
}

void printProductionStats() {
  Serial.println("--- PRODUCTION SUMMARY ---");
  Serial.print("   Full Assemblies (Good): ");
  Serial.println(assemblyCount);
  Serial.print("   Rejected Parts (Plast/Met): ");
  Serial.println(rejectCount);
  Serial.print("   Plastic Magazine Stack: ");
  Serial.print(counter1);
  Serial.println("/5");
  Serial.print("   Parts Remaining on Belt: ");
  Serial.println(queueCount);
  Serial.println("--------------------------\n");
}

//Void Functions From Excel

void startFlag() {
  int reading = digitalRead(buttonPins[9]); // Button 10 (IP6)

  if (reading != lastButtonState[9]) {
    if ((millis() - lastDebounceTime[9]) > debounceDelay) {
      lastDebounceTime[9] = millis();
      lastButtonState[9] = reading;

      if (reading == LOW) {
        if (!isRunning) {
          isRunning = true;
          beltReportedRunning = false;
          digitalWrite(ledPins[0], HIGH); // LED 1 ON (OP0)
          digitalWrite(ledPins[4], HIGH); // LED 5 ON (OP4)

          Serial.println("\n-----------------------------------------");
          Serial.println("[startFlag] Button 10 (IP6: Start) PRESSED");
          Serial.println("[SYSTEM] Buttons 1-9 UNLOCKED");
          Serial.println("[OUTPUT] LED 1 (OP0: Main Conveyor) -> ON");
          Serial.println("[OUTPUT] LED 5 (OP4: Assembly Conveyor) -> ON");
          Serial.println("-----------------------------------------");
        } else {
          Serial.println("[startFlag] Button 10 pressed (System already running)");
        }
      }
    }
  }
}

void stopFlag() {
  int reading = digitalRead(buttonPins[10]); // Button 11 (IP7)

  if (reading != lastButtonState[10]) {
    if ((millis() - lastDebounceTime[10]) > debounceDelay) {
      lastDebounceTime[10] = millis();
      lastButtonState[10] = reading;

      if (reading == LOW) {
        isRunning = false;
        beltReportedRunning = false;
        
        // Clear belt queue on emergency stop
        queueHead = 0;
        queueTail = 0;
        queueCount = 0;

        for (int i = 0; i < 5; i++) {
          digitalWrite(ledPins[i], LOW);
        }

        Serial.println("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        Serial.println("[stopFlag] Button 11 (IP7: Stop) PRESSED");
        Serial.println("[SYSTEM] EMERGENCY STOP -> Buttons 1-9 LOCKED");
        Serial.println("[OUTPUT] ALL LEDs (1-5) -> OFF");
        Serial.println("[QUEUE] Belt tracking queue cleared");
        printProductionStats();
        Serial.println("[SYSTEM] Waiting until Button 10 is pressed...");
        Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
      }
    }
  }
}

void beltMovement() {
  if (isRunning) {
    if (!beltReportedRunning) {
      Serial.println("[beltMovement] Conveyors active. Scanning all stations...");
      beltReportedRunning = true;
    }

    digitalWrite(ledPins[0], HIGH); // LED 1 ON (OP0)
    digitalWrite(ledPins[4], HIGH); // LED 5 ON (OP4)

    sortSolenoid();
    reloadPlastic();
    notify5();
    detectionPoint(); // Scans incoming parts & adds to queue
    assemblyPoint();  // Independently checks end-of-belt sensor (Button 6)
  }
}

void sortSolenoid() {
  if (counter1 < 5) {
    if (isButtonPressed(2)) {
      delay(debounceDelay);

      if (isButtonPressed(1)) {
        Serial.println("\n[sortSolenoid] Button 1 (IP0) + Button 2 (IP1) PRESSED");
        Serial.println("[sortSolenoid] Object = METAL -> NFA (Letting pass)");
        Serial.println("[sortSolenoid] Waiting for Button 2 (IP1) to clear...");

        while (isButtonPressed(2) && isRunning) {
          stopFlag();
        }
        if (isRunning) {
          Serial.println("[sortSolenoid] Object cleared IP1.\n");
        }
        return;
      } else {
        Serial.println("\n[sortSolenoid] Button 2 (IP1) PRESSED (Button 1 OFF)");
        Serial.println("[sortSolenoid] Object = PLASTIC -> Kicking into magazine");

        pulseLED(2, "OP1: Solenoid Plastic Release", 500);

        counter1++;
        notifiedFull = false;
        Serial.print("[COUNTER 1] Magazine Stack: ");
        Serial.print(counter1);
        Serial.println("/5");

        Serial.println("[sortSolenoid] WAITING: Press Button 8 (IP9) to confirm OP1 Retracted...");
        while (!isButtonPressed(8) && isRunning) {
          stopFlag();
        }

        if (isRunning) {
          Serial.println("[sortSolenoid] Button 8 (IP9) PRESSED -> OP1 Retract Confirmed!\n");
          delay(200);
        }
      }
    }
  }
}

void reloadPlastic() {
  if (counter1 == 0 && !isButtonPressed(3)) {
    return;
  }

  if (counter1 > 0 && counter1 <= 5 && !isButtonPressed(3)) {
    Serial.println("\n[reloadPlastic] Button 3 (IP2) NOT pressed & Counter 1 > 0");
    Serial.println("[reloadPlastic] Dispensing plastic ring from magazine...");

    pulseLED(3, "OP2: Rotatory Solenoid Release", 500);

    counter1--;
    notifiedFull = false;
    Serial.print("[COUNTER 1] Magazine Stack: ");
    Serial.print(counter1);
    Serial.println("/5");

    Serial.println("[reloadPlastic] WAITING: Press and HOLD Button 3 (IP2) to confirm plastic ring present...");
    while (!isButtonPressed(3) && isRunning) {
      stopFlag();
    }

    if (isRunning) {
      Serial.println("[reloadPlastic] Button 3 (IP2) HELD -> Plastic ring confirmed!\n");
    }
  }
}

void notify5() {
  if (counter1 == 5 && !notifiedFull) {
    Serial.println("\n*****************************************");
    Serial.println("[notify5] COUNTER 1 = 5/5 (MAGAZINE FULL)");
    Serial.println("[notify5] Further plastic parts enter OVERFLOW state (NFA)");
    Serial.println("*****************************************\n");
    notifiedFull = true;
  }
}

void detectionPoint() {
  // Trigger scan if Button 4 (IP3), Button 7 (IP8), OR Button 5 (IP4) detects a part
  if (isButtonPressed(4) || isButtonPressed(7) || isButtonPressed(5)) {
    delay(debounceDelay); // Settle simultaneous presses

    bool capSensor = isButtonPressed(4);   // IP3: Ring Assembly
    bool metalSensor = isButtonPressed(7); // IP8: Metal Peg Detect
    int detectedCategory = 2;              // Default to 2 (REJECT_PART)

    Serial.println("\n[detectionPoint] Parts Checking Area Triggered:");
    Serial.print("   -> Button 4 (IP3 Ring Sensor): ");
    Serial.println(capSensor ? "PRESSED (TRUE)" : "NOT PRESSED (FALSE)");
    Serial.print("   -> Button 7 (IP8 Metal Sensor): ");
    Serial.println(metalSensor ? "PRESSED (TRUE)" : "NOT PRESSED (FALSE)");

    if (capSensor && metalSensor) {
      Serial.println("[detectionPoint] Identity = FULL ASSEMBLY (True & True)");
      detectedCategory = 1; // FULL ASSEMBLY
    } else if (!capSensor && metalSensor) {
      Serial.println("[detectionPoint] Identity = METAL PEG ONLY (False & True) -> Reject Category");
      detectedCategory = 2; // REJECT
    } else if (!capSensor && !metalSensor) {
      Serial.println("[detectionPoint] Identity = PLASTIC ONLY (False & False at IP4) -> Reject Category");
      detectedCategory = 2; // REJECT
    } else {
      Serial.println("[detectionPoint] Identity = PARTIAL / MISALIGNED RING (True & False) -> Reject Category");
      detectedCategory = 2; // REJECT
    }

    // Only wait for Button 5 if Button 4 or Button 7 detected the part first
    if ((capSensor || metalSensor) && !isButtonPressed(5)) {
      Serial.println("[detectionPoint] WAITING: Press Button 5 (IP4: Object Passed assembly detector)...");

      while (!isButtonPressed(5) && isRunning) {
        stopFlag();
    }
  }

    if (isRunning) {
      Serial.println("[detectionPoint] Button 5 (IP4) PRESSED -> Part logged to belt queue!");
      enqueuePart(detectedCategory);

      // Wait for Button 5 release so one press doesn't double-log
      while (isButtonPressed(5) && isRunning) {
        stopFlag();
      }
      Serial.println("[detectionPoint] Ready for next scan!\n");
    }
  }
}

void assemblyPoint() {
// Independently checks if an object arrived at Button 6 (IP5: Photoswitch Reject Solenoid)
  if (isButtonPressed(6)) {
    delay(debounceDelay);
    Serial.println("\n[assemblyPoint] Button 6 (IP5) PRESSED -> Object arrived at end of belt");
    if (queueCount == 0) {
      Serial.println("[assemblyPoint] WARNING: Queue empty (Unscanned part at IP5) -> Defaulting to REJECT");
      rejectCount++;
      pulseLED(4, "OP3: Rejection Solenoid", 500);

      Serial.println("[assemblyPoint] WAITING: Press Button 9 (IP10) to confirm OP3 Retracted...");
      while (!isButtonPressed(9) && isRunning) {
        stopFlag();
      }

      if (isRunning) {
        Serial.println("[assemblyPoint] Button 9 (IP10) PRESSED -> OP3 Retract Confirmed!");
        printProductionStats();
      }
    } else {
    int currentPart = dequeuePart();
  if (currentPart == 1) {
// FULL ASSEMBLY
    assemblyCount++;
    Serial.println("[assemblyPoint] Queue Identity = FULL ASSEMBLY -> Letting pass (NFA)");
    printProductionStats();
  } else {
// REJECT PART (Plastic or Metal)
    rejectCount++;
    Serial.println("[assemblyPoint] Queue Identity = REJECT (Plastic/Metal) -> KICKING PART!");
    pulseLED(4, "OP3: Rejection Solenoid", 500);
    Serial.println("[assemblyPoint] WAITING: Press Button 9 (IP10) to confirm OP3 Retracted...");
    while (!isButtonPressed(9) && isRunning) {
      stopFlag();
    }
  if (isRunning) {
    Serial.println("[assemblyPoint] Button 9 (IP10) PRESSED -> OP3 Retract Confirmed!");
    printProductionStats();
  }
  }
  }
// Wait for Button 6 to clear before checking for the next arriving part
    while (isButtonPressed(6) && isRunning) {
      stopFlag();
    }
  }
}
