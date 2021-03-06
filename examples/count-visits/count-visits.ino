/*    _   _ _ _____ _    _              _____     _ _     ___ ___  _  __
 *   /_\ | | |_   _| |_ (_)_ _  __ _ __|_   _|_ _| | |__ / __|   \| |/ /
 *  / _ \| | | | | | ' \| | ' \/ _` (_-< | |/ _` | | / / \__ \ |) | ' <
 * /_/ \_\_|_| |_| |_||_|_|_||_\__, /__/ |_|\__,_|_|_\_\ |___/___/|_|\_\
 *                             |___/
 *
 * Copyright 2018 AllThingsTalk
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Improve facility management by taking into account actual usage based
 * on real time measurements. This experiment shows how a LoRa sensor can
 * count visits to optimise cleaning based on usage rather than rigid fixed
 * schedules.
 */
 
// Select your preferred method of sending data
//#define CONTAINERS
#define CBOR
//#define BINARY

/***************************************************************************/



#include <Wire.h>
#include <ATT_LoRaWAN.h>
#include "keys.h"
#include <MicrochipLoRaModem.h>

#define SERIAL_BAUD 57600

#define debugSerial Serial
#define loraSerial Serial1

int pushButton = 20;          
int doorSensor = 4;

MicrochipLoRaModem modem(&loraSerial, &debugSerial);
ATTDevice device(&modem, &debugSerial, false, 7000);  // minimum time between 2 consecutive messages set to 7000 milliseconds

#ifdef CONTAINERS
  #include <Container.h>
  Container container(device);
#endif

#ifdef CBOR
  #include <CborBuilder.h>
  CborBuilder payload(device);
#endif

#ifdef BINARY
  #include <PayloadBuilder.h>
  PayloadBuilder payload(device);
#endif

#define SEND_MAX_EVERY 30000 // the (mimimum) time between 2 consecutive updates of visit counts

bool prevButtonState;
bool prevDoorSensor;

short visitCount = 0;  // keep track of the nr of visitors
short prevVisitCountSent = 0;
unsigned long lastSentAt = 0;  // the time when the last visitcount was sent to the cloud

void setup() 
{
  pinMode(pushButton, INPUT);  // initialize the digital pin as an input
  pinMode(doorSensor, INPUT);

  debugSerial.begin(SERIAL_BAUD);
  while((!debugSerial) && (millis()) < 10000){}  // wait until the serial bus is available
  
  loraSerial.begin(modem.getDefaultBaudRate());  // set baud rate of the serial connection to match the modem
  while((!loraSerial) && (millis()) < 10000){}   // wait until the serial bus is available

  while(!device.initABP(DEV_ADDR, APPSKEY, NWKSKEY))
  debugSerial.println("Ready to send data");

  debugSerial.println();
  debugSerial.println("-- Count visits LoRa experiment --");
  debugSerial.print("Sending data every ");
  debugSerial.print(SEND_MAX_EVERY);
  debugSerial.println(" milliseconds, if changed");
  debugSerial.println();
  
  prevButtonState = digitalRead(pushButton);  // read the initial state
  prevDoorSensor = digitalRead(doorSensor);   // read the initial state
}

void process()
{
  while(device.processQueue() > 0)
  {
    debugSerial.print("QueueCount: ");
    debugSerial.println(device.queueCount());
    delay(10000);
  }
}

void sendVisitCount(int16_t val)
{
  #ifdef CONTAINERS
  container.addToQueue(val, INTEGER_SENSOR, false);
  process();
  #endif
  
  #ifdef CBOR
  payload.reset();
  payload.map(1);
  payload.addInteger(val, "4");
  payload.addToQueue(false);
  process();
  #endif

  #ifdef BINARY  
  payload.reset();
  payload.addInteger(val);
  payload.addToQueue(false);
  process();
  #endif

  lastSentAt = millis();
  prevVisitCountSent = val;
}

void loop() 
{
  processButton();
  processDoorSensor();
  delay(100);
  
  // only send a message when something has changed and SEND_MAX_EVERY has been exceeded
  if(prevVisitCountSent != visitCount && lastSentAt + SEND_MAX_EVERY <= millis())
    sendVisitCount(visitCount);
}

// check the state of the door sensor
void processDoorSensor()
{
  bool sensorRead = digitalRead(doorSensor);                         
  if(prevDoorSensor != sensorRead)
  {
    prevDoorSensor = sensorRead;
    if(sensorRead == true)
    {
      debugSerial.println("Door closed");
      visitCount++;  // the door was opened and closed again, so increment the counter
		  debugSerial.print("VisitCount: ");
		  debugSerial.println(visitCount);
    }
    else
      debugSerial.println("Door open");
  }
}

// check the state of the button
void processButton()
{
  bool sensorRead = digitalRead(pushButton);  // check the state of the button
  if (prevButtonState != sensorRead)          // verify if value has changed
  {
    prevButtonState = sensorRead;
    if(sensorRead == true)                                         
    {
      debugSerial.println("Button pressed, counter reset");
      visitCount = 0;
    }
    else
     debugSerial.println("Button released");
  }
}