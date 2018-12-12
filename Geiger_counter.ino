// Geiger counter firmware for dual-input ATtiny841 @ 11.0592MHz board
//
// Thomas Kircher <tkircher@gnu.org>, 2018

// Note to self:
// Fuses must be set for external high-speed crystal oscillator:
//    avrdude ... -U lfuse:w:0xFE:m
//
// 1 - unprogrammed, 0 - programmed
//
// CKDIV8 - 1 - Don't divide clock by 8
// CKOUT  - 1 - Don't output system clock on a port pin
// -        1 - (reserved)
// SUT    - 1 - System start-up time set to 1k clocks
//
// CKSEL3 - 1 |
// CKSEL2 - 1 | Crystal oscillator (>8MHz)
// CKSEL1 - 1 | CKSEL0 actually used for SUT setting (p.30)
// CKSEL0 - 0 |

// Global variables
unsigned char i;
unsigned char seconds = 0; // Number of seconds
unsigned short cpm_ch1 = 0; // Counts per minute from channel 1
unsigned short cpm_ch2 = 0; // Counts per minute from channel 2
volatile byte state = LOW; // Click state

// This is an implementation of AN2447 for ATtiny841
//
// We let the ADC input be the internal 1.1v bandgap reference (Vbg), and
// set the ADC reference to Vcc. Then we perform an ADC conversion (10-bit),
// which gives us:
//    ADC = Vin x 2^10 / Vref, or Vcc = 2^10 x Vbg / ADC
//
// (12/11/2018 - Using a 3.3v regulator, so this is redundant)
/*
float battery_level() {
  float Vcc = 0.0;

  ADMUXA = 0b00001101; // Internal 1.1v BG reference - MUX[5:0] = 00 1101
  ADMUXB = 0b00000000; // ADC reference is Vcc: REFS[2:0] = 000, gain set to 1
  ADCSRA = 0b10000101; // Enable ADC, no interrupts, clock prescaler = 32
  ADCSRB = 0b00001000; // Left-adjust result, no auto trigger

  ADCSRA |= (1 << ADSC); // Start ADC conversion
  while(ADCSRA & (1 << ADSC)); // Wait for conversion to complete
  Vcc = (0x400 * 1.1) / ADCH; // Throw away the LSB, only use 8 bits.

  return Vcc;
}
*/

void setup() {
  // Initialize I/O pins
  pinMode(2, OUTPUT); // LED
  pinMode(3, OUTPUT); // Piezo

  pinMode(10, INPUT); // Input channel 1
  pinMode(7, INPUT);  // Input channel 2

  // Initialize port states
  digitalWrite(2, LOW);
  digitalWrite(3, LOW);

  // Enable interrupts for PCINT[7:0]
  GIMSK |= (1 << PCIE0);

  // Enable pin change interrupts for PC0, PC3
  PCMSK0 |= (1 << PCINT0) | (1 << PCINT3);

  // Initialize timer
  // On the ATtiny841, CTC (Clear Timer on Compare) is what the
  // normal timer mode is called.

  // No compare outputs, CTC mode (OCR1A), timer stopped for config
  // WGM[3:0] = 0100 - see datasheet, p.113
  TCCR1A = 0x00;
  TCCR1B = 0x08;

  // Zero the timer
  TCNT1H = 0x00;
  TCNT1L = 0x00;

  // Reset the prescaler
  GTCCR = 0x01;

  // Set the timer interval to one second:
  // 11.0592MHz with clk/256 prescale = 43200 = 0xA8C0
  OCR1AH = 0xA8;
  OCR1AL = 0xC0;

  // Enable timer interrupt (CTC)
  TIMSK1 = 0x02;
  
  // Initialize serial comms
  Serial.begin(57600);

  // Say hello
  Serial.println("Geiger counter initialized");

  // Start timer
  TCCR1B |= 0x04;
}

// the loop function runs over and over again forever
void loop() {
  if(state == HIGH) {
     // Blink
     digitalWrite(2, HIGH);
     delayMicroseconds(200);
     digitalWrite(2, LOW);

     // Also chirp
     for(i = 0; i < 200; i++) {
        digitalWrite(3, HIGH);
        delayMicroseconds(47);
        digitalWrite(3, LOW);
        delayMicroseconds(47);
    }

    // Set state back to low
    state = LOW;
  }
}

// Pin state change interrupt
ISR(PCINT0_vect) {
  // PA0 is channel 1, PA3 is channel 2
  // Keep track of the number of counts on each channel, and
  // set the click state high for either.
  if(PINA & (1 << PINA0)) {
      cpm_ch1++;
      state = HIGH;
  }
  if(PINA & (1 << PINA3)) {
      cpm_ch2++;
      state = HIGH;
  }
}

// Timer interrupt
//
// For now, we just output total counts every second
ISR(TIMER1_COMPA_vect) {
   if(seconds < 60)
      seconds++;
   else {
      seconds = 0;
   }
   Serial.print("counts: ");
   Serial.print(cpm_ch1);
   Serial.print(" ");
   Serial.println(cpm_ch2);
}

// EOF
