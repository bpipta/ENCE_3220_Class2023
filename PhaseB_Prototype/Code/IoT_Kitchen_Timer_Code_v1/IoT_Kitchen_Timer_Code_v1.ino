#include <string.h>

#define BUFFER_SIZE 15

char messageStart;
char messageBuffer[BUFFER_SIZE];

void setup() 
{
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
}

void clearBuffer() 
{
    int i;
    for(i = 0; i < BUFFER_SIZE; ++i) messageBuffer[i] = 0;
}

void loop() 
{
  if(Serial.available()) {
    messageStart = Serial.read();
    Serial.println(messageStart);
    if (messageStart == "$") {
  
      clearBuffer();
    
      char message = Serial.read();
      char index = 0;
      messageBuffer[index] = message;
      while(message != "\n" && index < BUFFER_SIZE - 1) {
        message = Serial.read();
        index++;
        messageBuffer[index] = message;
      }

      messageBuffer[index++] = "\0";  // Null byte is necessary for strcmp

      Serial.println(messageBuffer[0]);
      Serial.println(messageBuffer[1]);
      Serial.println(messageBuffer[2]);
      Serial.println(messageBuffer[3]);

      // Bad comparison without using strcmp
      if (messageBuffer[0] == "S" && messageBuffer[1] == "T" && messageBuffer[2] == "P") digitalWrite(LED_BUILTIN, LOW);
      else if (messageBuffer[0] == "S" && messageBuffer[1] == "T" && messageBuffer[2] == "R") digitalWrite(LED_BUILTIN, LOW);
      
    }
  }
}
