#define BUTTON_1  2
#define BUTTON_2  3
#define GREEN_LED 4
#define RED_LED   5
#define BUZZER    6

#define DATA      9
#define LATCH     8
#define CLOCK     7

#define DIGIT_1   13
#define DIGIT_2   12
#define DIGIT_3   11
#define DIGIT_4   10

#define TIMER_1_SPEED    62500   // Timer will count one second
#define TIMER_2_SPEED    10      // Controls the rate of the display
#define TIMER_3_SPEED    100     // Controls rate of Serial reads

#define BUFFER_SIZE 15

const byte table[] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};    // 7 segment encoding of numbers 0 to 9

volatile unsigned short gCount;  // Keeps track of seconds
volatile bool gTimeEnd_flag;     // Flag for when the timer ends
volatile bool gReadSerial_flag;
volatile byte gCurrentDigit;     // Controls which digit is being displayed

char gMessageBuffer[BUFFER_SIZE];
short gMessageBufferIndex;
bool gMessageStarted_flag;

void setup() {
  // LEDs Pins
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // Button Pins
  pinMode(BUTTON_1, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_1), Button_1_ISR, RISING);
  pinMode(BUTTON_2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_2), Button_2_ISR, RISING);

  // Buzzer Pins
  pinMode(BUZZER, OUTPUT);

  // 7-Seg Display
  pinMode(DIGIT_1, OUTPUT);
  pinMode(DIGIT_2, OUTPUT);
  pinMode(DIGIT_3, OUTPUT);
  pinMode(DIGIT_4, OUTPUT);

  // Shift Register Pins
  pinMode(LATCH, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(DATA, OUTPUT); 

  noInterrupts();

  // Initialize timer 1
  TCCR1A = 0;
  TCCR1B = 0;
  OCR1A = TIMER_1_SPEED;    // compare match register 16MHz/256
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS12);    // 256 prescaler

  // Initialize timer 2
  TCCR2A = 0;
  TCCR2B = 0;
  OCR2A = TIMER_2_SPEED;                      // max value 2^8 - 1 = 255
  TCCR2A |= (1<<WGM21);
  TCCR2B = (1<<CS22) | (1<<CS21) | (1<<CS20); // 1204 prescaler
  TIMSK2 |= (1<<OCIE2A);

  // I'm following the naming convention in the Arduino library for the ATmega328p, but 
  // a new header file with definitions for the ATmega32U4 will be needed
  TCCR3A = 0;
  TCCR3B = 0;
  OCR3A = TIMER_3_SPEED;            // max value 2^16 - 1 = 65535
  TCCR3A |= (1<<WGM11);
  TCCR3B = (1<<CS11) | (1<<CS10);   // 64 prescaler
  TIMSK3 |= (1<<OCIE1A);

  // Set initial state of global variables and leds
  initialize_state();

  // Turn off diplay
  toggleDisplay(HIGH);
  
  interrupts();

}

/********** Functions **********/

void initialize_state()
{
  TCNT1  = 0;               // Clear count register
  TIMSK1 &= ~(1<<OCIE1A);   // Disable timer interrupts
  
  gTimeEnd_flag = 0;
  gCurrentDigit = 0;
  gCount = 0;
  gMessageBufferIndex = 0;
  gMessageStarted_flag = 0;

  clearMessageBuffer();
  
  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, HIGH);
}

void displayNumber(unsigned char num, bool dp)
{
  digitalWrite(LATCH, LOW);
  shiftOut(DATA, CLOCK, MSBFIRST, table[num] | (dp << 7));
  digitalWrite(LATCH, HIGH);
}

void toggleDisplay(bool isOn)
{
   digitalWrite(DIGIT_1, isOn);
   digitalWrite(DIGIT_2, isOn);
   digitalWrite(DIGIT_3, isOn);
   digitalWrite(DIGIT_4, isOn);
}

void activateBuzzer()
{
    unsigned char i;
  
    for(i=0;i<100;i++) {
      digitalWrite(BUZZER,HIGH);
      delay(1);//wait for 1ms
      digitalWrite(BUZZER,LOW);
      delay(1);//wait for 1ms
    }
}

void clearMessageBuffer() 
{
    for(short i = 0; i < BUFFER_SIZE; ++i) gMessageBuffer[i] = 0;
}

int compareCharArray(char gBuff[], char cBuff[], int len) {
  for (int i = 0; i < len; ++i) {
    if (gBuff[i] != cBuff[i]) return 0;
  }
  return 1;
}

/********** Interrupt Handlers **********/

// Button1 interrupt service routine (ISR)
// Increments gCount
void Button_1_ISR()
{
  if (!gTimeEnd_flag) gCount++;               // Do not increase if timer is going off
  //This works too: gCount+=(!gTimeEnd_flag);
  if (gCount > 3599) gCount = 0;              // Check if display is over limit: 59 min 59 sec
}

// Button2 interrupt service routine (ISR)
// Toggles the Timer2 interrupt on/off and resets when gTimeEnd_flag is set
void Button_2_ISR()
{ 
  // Only toggle clock if count > 0
  if (gCount) {
      TIMSK1 ^= (1<<OCIE1A);                         // toggle timer interrupt (flip the second bit: OCIE1A)
      TCNT1  = 0;                                    // Clear the count register
      digitalWrite(RED_LED, !digitalRead(RED_LED));  // Bitmask second bit for TIMSK1 and right shift 1 instead of read?
  }
  // If time has ended and button 2 is pressed, reset
  else if (gTimeEnd_flag) {
    initialize_state();
  }
}

// Timer1 interrupt service routine (ISR)
// Decreaments gCount
ISR(TIMER1_COMPA_vect)
{
  gCount--; 
  
  // Check when the timer reaches 0
  if (!gCount) {
    TIMSK1 &= ~(1<<OCIE1A);     // Disable timer interrupt
    gTimeEnd_flag = 1;
  }
}

// Timer2 interrupt service routine (ISR)
// Writes the correct digit to the correct 7 seg display
ISR(TIMER2_COMPA_vect)   
{
  toggleDisplay(HIGH);
  switch (gCurrentDigit)
  {
    case 1: //0x:xx
      displayNumber( int(gCount / 600), 0 );   // prepare to display digit 1 (most left)
      digitalWrite(DIGIT_1, LOW);  // turn on digit 1
      break;
 
    case 2: //x0:xx
      displayNumber( int(gCount / 60) % 10, 1 );   // prepare to display digit 2
      digitalWrite(DIGIT_2, LOW);     // turn on digit 2
      break;
 
    case 3: //xx:0x
      displayNumber( (gCount / 10) % 6, 0 );   // prepare to display digit 3
      digitalWrite(DIGIT_3, LOW);    // turn on digit 3
      break;
 
    case 4: //xx:x0
      displayNumber((gCount % 10), 0); // prepare to display digit 4 (most right)
      digitalWrite(DIGIT_4, LOW);  // turn on digit 4
  }
  gCurrentDigit = (gCurrentDigit % 4) + 1;
}

ISR(TIMER3_COMPA_vect)
{
  if(Serial.available()) {
    gReadSerial_flag = 1;
  }
}

/********** Main Loop **********/

void loop() {

  if (gTimeEnd_flag) {

    toggleDisplay(LOW);

    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);

    while(gTimeEnd_flag) {
      activateBuzzer();
    }
  }

  if (gReadSerial_flag) {
     char incomingChar = Serial.read();

     // Start of message
     if (incomingChar == '$') {
      clearMessageBuffer();
      gMessageBufferIndex = 0;
      gMessageStarted_flag = 1;
     }
     // Message has already started
     else if (gMessageStarted_flag) {
        // End of message
        if(incomingChar == '\n' || gMessageBufferIndex >= BUFFER_SIZE - 1) {
          // Start timer
          if (compareCharArray(gMessageBuffer, (char*)"STR", 3)) {
            // Call the same function that is used then the start/stop button is physically pressed
            Button_2_ISR(); // Probably better to set the interrupt flag than call the function directly
          } else if (compareCharArray(gMessageBuffer, (char*)"STP", 3)) {
            // Stop Timer
            Button_2_ISR();
          } else if (compareCharArray(gMessageBuffer, (char*)"GET", 3)) {
            Serial.print("$");
            Serial.print(char(gCount / 600));
            Serial.print(char((gCount / 60) % 10));
            Serial.print(char((gCount / 10) % 6));
            Serial.print(char(gCount % 10));
            Serial.print('\n');
          }
          gMessageStarted_flag = 0;
          
        // Message is still going
        } else {
          gMessageBuffer[gMessageBufferIndex++] = incomingChar;
        }
     }
     gReadSerial_flag = 0;
  }
}
