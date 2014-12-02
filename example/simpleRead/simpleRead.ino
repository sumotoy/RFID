/*
 You may copy, alter and reuse this code in any way you like, but please leave
 reference to HobbyComponents.com in your comments if you redistribute this code.
 This software may not be used directly for the purpose of selling products that
 directly compete with Hobby Components Ltd's own range of products.
 
 THIS SOFTWARE IS PROVIDED "AS IS". HOBBY COMPONENTS MAKES NO WARRANTIES, WHETHER
 EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ACCURACY OR LACK OF NEGLIGENCE.
 HOBBY COMPONENTS SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR ANY DAMAGES,
 INCLUDING, BUT NOT LIMITED TO, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY
 REASON WHATSOEVER.
 */


/* Include the RFID library */
#include "RFID.h"
#include <SPI.h>
/* Define the pins used for the SS (SDA) and RST (reset) pins for BOTH hardware and software SPI */
/* Change as required */
#define SS_PIN      10      // Same pin used as hardware SPI (SS)
#define RST_PIN     9

/* Create an instance of the RFID library */

RFID RC522(SS_PIN, RST_PIN);



void setup()
{ 
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo and Teensy3.x only
  }
  /* Initialise the RFID reader */
  RC522.init();
}

void loop()
{
  /* Temporary loop counter */
  uint8_t i;
  /* Has a card been detected? */
  if (RC522.isCard())
  {
    /* If so then get its serial number */
    RC522.readCardSerial();
    Serial.println("Card or Tag detected:");
    /* Output the serial number to the UART */
    for(i = 0; i <= 4; i++)
    {
      Serial.print(RC522.serNum[i],HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
  else
    Serial.println("...wait for card or tag");
  delay(1000);
}
