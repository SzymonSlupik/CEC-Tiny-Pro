/* Operate a legacy device based on HDMI CEC volume commands.
 * Also prints the CEC messages to the UART port
 * A fork of the https://github.com/tsowell/avr-hdmi-cec-volume/blob/master/main.c
 * Original code Copyright by Thomas Sowell 
 * Arduino IDE adaptation by Szymon Słupik 
 */

/* The HDMI address for this device.  Respond to CEC sent to this address. */
#define ADDRESS 0x05      // Pretend to be a HDMI  Audio Subsystem
#define CECPIN 11         // Input pin for the CEC signal
#define OUTPUTTICKS 20000 // This defines the duration of an "output" signal, sich as a LED blink or a motor turn
#define LEDPIN 13         // Output pin to drive a status LED
#define ACTIVE 0          // Set to 1 to active;y reply to CEC messages
                          // Set to 0 for a passive mode (when another active CEC sink is present)

char serialline[50];
bool outputactive;

void send_ack(void) {
  /* Send ACK.  Called immediately after a falling edge has occured. */
  unsigned long ticks_start;
  unsigned long ticks;

  ticks_start = micros();

  if (ACTIVE) pinMode(CECPIN, OUTPUT); // Pull the CEC line low.

  for (;;) {
    ticks = micros();

    /* optimal 1.5 ms */
    if ((ticks - ticks_start) >= 1500) {
      pinMode(CECPIN, INPUT); // Set the CEC line back to high-Z.
      break;
    }
  }
}


unsigned long wait_edge(bool e) {
  unsigned long ticks;
  unsigned long last, cec;

  last = cec = digitalRead(CECPIN);

  for (;;) {
    ticks = micros();

    last = cec;
    cec = digitalRead(CECPIN);

    if (e) { //rising edge
      if ((last == 0) && (cec == 1)) {
        return ticks;
      }
    }
    else {   //falling edge
      if ((last == 1) && (cec == 0)) {
        return ticks;
      }
    }
  }
}

unsigned long wait_falling_edge(void) {
  return wait_edge(0);
}

unsigned long wait_rising_edge(void) {
  return wait_edge(1);
}

byte recv_data_bit(void) {
  /* Sample a bit, called immediately after a falling edge occurs. */
  unsigned long ticks_start;
  unsigned long ticks;

  ticks_start = micros();

  for (;;) {
    ticks = micros();

    /* optimal 1.05 ms */
    if ((ticks - ticks_start) >= 1050) {
      return digitalRead(CECPIN);
    }
  }
}

byte wait_start_bit(void) {
  unsigned long ticks_start;
  unsigned long ticks;

  /* A start bit consists of a falling edge followed by a rising edge
   * between 3.5 ms and 3.9 ms after the falling edge, and a second
   * falling edge between 4.3 and 4.7 ms after the first falling edge.
   * Wait until those conditions are met, and start over at the next
   * falling edge if any threshold is exceeded. */
  for (;;) {
    ticks_start = wait_falling_edge();
    ticks = wait_rising_edge();

    if ((ticks - ticks_start) >= 3900) {
      continue; // Rising edge took longer than 3.9 ms
    }
    /* Rising edge occured between 3.5 ms and 3.9 ms */
    else if ((ticks - ticks_start) >= 3500) {
      ticks = wait_falling_edge();
      if ((ticks - ticks_start) >= 4700) {
        continue; // Falling edge took longer than 4.7 ms
      }
      /* Falling edge between 4.3 ms and 4.7 ms means that
       * this has been a start bit! */
      else if ((ticks - ticks_start) >= 4300) {
        return 0;
      }
      else {
        continue; // The falling edge came too early
      }
    }
    else {
      continue; // The rising edge came sooner than 3.5 ms
    }
  }
}

byte recv_frame(byte *pld, byte address) {
  unsigned long ticks_start;
  unsigned long ticks;
  byte bit_count;
  byte pldcnt;
  byte eom;

  wait_start_bit();

  bit_count = 9;
  pldcnt = 0;
  pld[pldcnt] = 0;

  /* Read blocks into pld until the EOM bit signals that the message is
   * complete.  Each block is 10 bits consisting of information bits 7-0,
   * an EOM bit, and an ACK bit.  The initiator sends the information
   * bits and the EOM bit and expects the follower to send a '0' during
   * the ACK bit to acknowledge receipt of the block. */
  for (;;) {
    /* At this point in the loop, a falling edge has just occured,
     * either in wait_start_bit() above or wait_falling_edge() at
     * the end of the loop, so it is time to sample a bit. */
    ticks_start = micros();

    /* Only store and return the information bits. */
    if (bit_count > 1) {
      pld[pldcnt] <<= 1;
      pld[pldcnt] |= recv_data_bit();
    }
    else {
      eom = recv_data_bit();
    }
    bit_count--;

    /* Wait for the starting falling edge of the next bit. */
    ticks = wait_falling_edge();
    if ((ticks - ticks_start) < 2050) { //2.05 ms
      sprintf(serialline, "# frame aborted - too short");
      return -1;
    }
    ticks_start = ticks;
    /* If that was the EOM bit, it's time to send an ACK and either
     * return the data (if EOM was indicated by the initiator) or
     * prepare to read another block. */
    if (bit_count == 0) {
      /* Only ACK messages addressed to us  */
      if (((pld[0] & 0x0f) == address) || !(pld[0] & 0x0f)) {
        send_ack();
      }
      if (eom) {
        /* Don't consume the falling edge in this case
         * because it could be the start of the next
         * start bit! */
        return pldcnt + 1;
      }
      else {
        /* Wait for the starting falling edge of the next bit. */
        ticks = wait_falling_edge();
        if ((ticks - ticks_start) >= 2750) { //2.75 ms
          sprintf(serialline, "# frame error - too long");
          return -1;
        }
      }
      bit_count = 9;
      pldcnt++;
      pld[pldcnt] = 0;
    }
  }
}

void send_start_bit(void) {
  /* Pull the line low for 3.7 ms and then high again until the 4.5 ms
   * mark.  This function doesn't produce the final falling edge of the
   * start bit - that is left to send_data_bit(). */
  unsigned long ticks;
  unsigned long ticks_start;

  ticks_start = micros();

  pinMode(CECPIN, OUTPUT);  // Pull the CEC line low.

  for (;;) {
    ticks = micros();
    if ((ticks - ticks_start) >= 3700) { //3.7 ms
      break;
    }
  }

  pinMode(CECPIN, INPUT);  // Set the CEC line back to high-Z.

  for (;;) {
    ticks = micros();
    if ((ticks - ticks_start) >= 4500) { //4.5 ms
      break;
    }
  }
}

void send_data_bit(int8_t bit) {
  /* A data bit consists of a falling edge at T=0ms, a rising edge, and
   * another falling edge at T=2.4ms.  The timing of the rising edge
   * determines the bit value.  The rising edge for an optimal logical 1
   * occurs at T=0.6ms.  The rising edge for an optimal logical 0 occurs
   * at T=1.5ms. */
  unsigned long ticks;
  unsigned long ticks_start;

  ticks_start = micros();

  pinMode(CECPIN, OUTPUT); // Pull the CEC line low.

  for (;;) {
    ticks = micros();
    if (bit) {
      /* 0.6 ms */
      if ((ticks - ticks_start) >= 600) {
        break;
      }
    }
    else {
      /* 1.5 ms */
      if ((ticks - ticks_start) >= 1500) {
        break;
      }
    }
  }

  pinMode(CECPIN, INPUT); // Set the CEC line back to high-Z.

  for (;;) {
    ticks = micros();
    if ((ticks - ticks_start) >= 2400) { //2.4 ms
      break;
    }
  }
}

void send_frame(byte pldcnt, byte *pld) {
  byte bit_count;
  byte i;

  delay(13);
  send_start_bit();

  for (i = 0; i < pldcnt; i++) {
    bit_count = 7;
    /* Information bits. */
    do {
      send_data_bit((pld[i] >> bit_count) & 0x01);
    } while (bit_count--);
    /* EOM bit. */
    send_data_bit(i == (pldcnt - 1));
    /* ACK bit (we will assume the block was received). */
    send_data_bit(1);
  }
}

void device_vendor_id(byte initiator, byte destination, uint32_t vendor_id) {
  byte pld[5];

  pld[0] = (initiator << 4) | destination;
  pld[1] = 0x87;
  pld[2] = (vendor_id >> 16) & 0x0ff;
  pld[3] = (vendor_id >> 8) & 0x0ff;
  pld[4] = (vendor_id >> 0) & 0x0ff;

  send_frame(5, pld);
  sprintf(serialline, "\n<-- %02x:87 [Device Vendor ID]", pld[0]);
  Serial.print(serialline);
}

void report_power_status(byte initiator, byte destination, byte power_status) {
  byte pld[3];

  pld[0] = (initiator << 4) | destination;
  pld[1] = 0x90;
  pld[2] = power_status;

  send_frame(3, pld);
  sprintf(serialline, "\n<-- %02x:90 [Report Power Status]", pld[0]);
  Serial.print(serialline);
}

void set_system_audio_mode(byte initiator, byte destination, byte system_audio_mode) {
  byte pld[3];

  pld[0] = (initiator << 4) | destination;
  pld[1] = 0x72;
  pld[2] = system_audio_mode;

  send_frame(3, pld);
  sprintf(serialline, "\n<-- %02x:72 [Set System Audio Mode]", pld[0]);
  Serial.print(serialline);
}

void report_audio_status(byte initiator, byte destination, byte audio_status) {
  byte pld[3];

  pld[0] = (initiator << 4) | destination;
  pld[1] = 0x7a;
  pld[2] = audio_status;

  send_frame(3, pld);
  sprintf(serialline, "\n<-- %02x:7a [Report Audio Status]", pld[0]);
  Serial.print(serialline);
}

void system_audio_mode_status(byte initiator, byte destination, byte system_audio_mode_status) {
  byte pld[3];

  pld[0] = (initiator << 4) | destination;
  pld[1] = 0x7e;
  pld[2] = system_audio_mode_status;

  send_frame(3, pld);
  sprintf(serialline, "\n<-- %02x:7e [System Audio Mode Status]", pld[0]);
  Serial.print(serialline);
}

void set_osd_name(byte initiator, byte destination) {
  byte pld[6] = {
      0, 0x47,
      'B', 'o', 's', 'e' };

  pld[0] = (initiator << 4) | destination;

  send_frame(6, pld);
  sprintf(serialline, "\n<-- %02x:47 [Set OSD Name]", pld[0]);
  Serial.print(serialline);
}

void report_physical_address(byte initiator, byte destination, unsigned int physical_address, byte device_type) {
  byte pld[5];

  pld[0] = (initiator << 4) | destination;
  pld[1] = 0x84;
  pld[2] = (physical_address >> 8) & 0x0ff;
  pld[3] = (physical_address >> 0) & 0x0ff;
  pld[4] = device_type;

  send_frame(5, pld);
  sprintf(serialline, "\n<-- %02x:84 [Report Physical Address]", pld[0]);
  Serial.print(serialline);
}

void setup() {
  pinMode(CECPIN, INPUT);

  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1B |= (1 << CS12);    // 256 prescaler 
  TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt
  
  Serial.begin(500000);
  while (!Serial);
  Serial.println("HDMI CEC Decoder and Audio System Emulator");
  Serial.println("Based on the original work by Thomas Sowell");
  Serial.println("Arduino adaoptation (C) 2021 Szymon Słupik");
              Serial.println("------------------------------------------");
  if (ACTIVE) {
    Serial.println("************** ACTIVE MODE ***************");
  }
  else {
    Serial.println("************** PASSIVE MODE **************");
  }
}

/* When we turn on an output port, we schedule this overflow
 * interrupt to turn off the port off after OUTPUTTICKS */
ISR(TIMER1_OVF_vect)
{
//  noInterrupts();
  /* code for turning off outputs after a period of time */
  digitalWrite(LEDPIN, digitalRead(LEDPIN) ^ 1);
  if (outputactive) {
    /* plug in your output handling code here, such as stopping a motor */
    outputactive = false;
    Serial.println("<*> [Interrupt: GPIO turned off]");
  }  
}

void loop() {
  byte pld[16];
  byte pldcnt, pldcntrcvd;
  byte initiator, destination;
  byte i;

  pldcnt = recv_frame(pld, ADDRESS);
  pldcntrcvd = pldcnt;
  if (pldcnt < 0) {
    sprintf(serialline, "%i\n", pldcnt);
    Serial.print(serialline);
    return;
  }
  initiator = (pld[0] & 0xf0) >> 4;
  destination = pld[0] & 0x0f;

  Serial.print("--> ");
  for (i = 0; i < pldcnt - 1; i++) {
    sprintf(serialline, "%02x:", pld[i]);
    Serial.print(serialline);
}
  sprintf(serialline, "%02x ", pld[i]);
  Serial.print(serialline);

  if ((pldcnt > 1)) {
    switch (pld[1]) {
      case 0x04:
        Serial.print(F("[Image View On]"));
        break;       
      case 0x0d:
        Serial.print(F("[Text View On]"));
        break;       
      case 0x36:
        outputactive = true;
        TCNT1 = 65536 - OUTPUTTICKS; // preload the overflow timer
        Serial.println(F("[Standby]"));
        Serial.print(F("<*> [Turn the display OFF]"));
        break;
      case 0x70:
        Serial.print(F("[System Audio Mode Request]"));
        if (ACTIVE && (destination == ADDRESS)) set_system_audio_mode(ADDRESS, 0x0f, 1);
        break;
      case 0x71:
        Serial.print(F("[Give Audio Status]"));
        if (ACTIVE && (destination == ADDRESS)) report_audio_status(ADDRESS, initiator, 0x32); //volume 50%, mute off
        break;
      case 0x72:
        Serial.print(F("[Set System Audio Mode]"));
        break;
      case 0x7d:
        Serial.print(F("[Give System Audio Mode Status]"));
        if (ACTIVE && (destination == ADDRESS)) system_audio_mode_status(ADDRESS, initiator, 1);
        break;
      case 0x7e:
        Serial.print(F("[System Audio Mode Status]"));
        break;
      case 0x82:
        outputactive = true;
        TCNT1 = 65536 - OUTPUTTICKS; // preload the overflow timer
        Serial.println(F("[Active Source]"));
        Serial.print(F("<*> [Turn the display ON]"));
        break;
      case 0x84:
        Serial.print(F("[Report Physical Address>]"));
        break;
      case 0x85:
        Serial.print(F("[Request Active Source]"));
        break;
      case 0x87:
        Serial.print(F("[Device Vendor ID]"));
        break;
      case 0x8c:
        Serial.print(F("[Give Device Vendor ID]"));
        if (ACTIVE && (destination == ADDRESS)) device_vendor_id(ADDRESS, 0x0f, 0x0010FA);
        break;
      case 0x8e:
        Serial.print(F("[Menu Status]"));
        break;       
      case 0x8f:
        Serial.print(F("[Give Device Power Status]"));
        if (ACTIVE && (destination == ADDRESS)) report_power_status(ADDRESS, initiator, 0x00);
        /* Hack for Google Chromecast to force it sending V+/V- if no CEC TV is present */
        if (ACTIVE && (destination == 0)) report_power_status(0, initiator, 0x00);
        break;
      case 0x90:
        Serial.print(F("[Report Power Status]"));
        break;
      case 0x91:
        Serial.print(F("[Get Menu Language]"));
        break;
      case 0x9d:
        Serial.print(F("[Inactive Source]"));
        break;
      case 0x9e:
        Serial.print(F("[CEC Version]"));
        break;
      case 0x9f:
        Serial.print(F("[Get CEC Version]"));
        break;
      case 0x46:
        Serial.print(F("[Give OSD Name]"));
        if (ACTIVE && (destination == ADDRESS)) set_osd_name(ADDRESS, initiator);
        break;
      case 0x47:
        Serial.print(F("[Set OSD Name]"));
        break;
      case 0x83:
        Serial.print(F("[Give Physical Address]"));
        if (ACTIVE && (destination == ADDRESS)) report_physical_address(ADDRESS, 0x0f, 0x0005, 0x05);
        break;
      case 0x44:
        if (pld[2] == 0x41) { 
          outputactive = true;
          TCNT1 = 65536 - OUTPUTTICKS; // preload the oveflow timer
          Serial.print(F("[User Control Volume Up]"));
        }
        else if (pld[2] == 0x42) {
          outputactive = true;
          TCNT1 = 65536 - OUTPUTTICKS; // preload the overflow timer
          Serial.print(F("[User Control Volume Down]"));
        }
        break;
      case 0x45:
        Serial.print(F("[User Control Released]"));
        break;
      default:
        if (pldcntrcvd > 1) Serial.print(F("???")); //undecoded command
        break;
    }
  }
  else Serial.print(F("[Ping]"));
  Serial.println();
}
