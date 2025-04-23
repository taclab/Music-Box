
/*!************************************************************************

      Asher Music Box 2
      Wilksy 2020
      1_6 clean up vars
      updated Easy DFplayer to from 1.1.12 to 1.2
      Implemented end of track detection using Busy pin 
      Implemented Musical Chairs mode

**************************************************************************/


/*************NFC Reader****************************************************/
#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>

#include <ESP8266WiFi.h>
PN532_I2C pn532i2c(Wire); // Im using I2c for commuication, so default SDA/SLA pins in play(cant be changed on esp8266 they are BB'd).
PN532 nfc(pn532i2c);
/**************************************************************************/

/*************AudioPlayer****************************************************/
#include "Arduino.h"
#include "SoftwareSerial.h"
#include <DFPlayerMini_Fast.h>
SoftwareSerial mySoftwareSerial(14, 13); // RX, TX Serial Comms using Software Serial RX --> TX, TX --> RX
#define AudioBusyPin 16

#include "DFRobotDFPlayerMini.h"
DFRobotDFPlayerMini myMP3;
/*********************************************************************/

/*************Wifi****************************************************/
#include <ESP8266WiFi.h> // I will expand on network functionaity later, for now i need this to turn off wifi for power savings
/*********************************************************************/

/*************Card Definitions*******************************************/
#include "cards.h" // include the card definitions and functions

// Program Control Vars
struct adminSettings {
  uint8_t VolumeRoundRobin[4] = {10, 15, 20, 25}; //volume is adjusted on a round robin
  uint8_t initVolume = 2; // initial volume level (in array)
  uint8_t DFPlayerProcessDelay = 100; //time for the Audio Player to reset
  bool isCardPresent = false;
  bool isValidCardPresent = false;
  bool isMultipleCardsPresent = false;
  uint8_t PlayingType = 0; //0-nothing,1-music,2-story.
  bool isMusicalChairsMode = false;
  unsigned long NextStatueTime = 0;
  unsigned long MilliStartPlaytime = 0;
  unsigned long ReadStateMillis = 0;
  unsigned long ReadStateDelay = 250;
  unsigned long LastCardMillis = 0;
  unsigned long LastCardThreshold = 100; // Time to wait until we register a card as being removed
  unsigned long LastProcessedEvent = 0;
  String CurrentCardUID = "";
  String PreviousCardUID = "";
};

adminSettings CurrentSettings;


void SetnewStatueTime() {
  //return a random time until next status time!
  //at least 10 seconds away and not more than 30
  int randomNumber = random(10, 30);
  CurrentSettings.NextStatueTime = (millis() + (randomNumber * 1000));
}

void ProcessEvent(String UID, int Action) {
  //Action 1 = card found, 2 = multiple cards found, 3 = valid card

  //block events happening too fast and causing the DFplayer to have a fit and reset

  if ((millis() - CurrentSettings.LastProcessedEvent) < CurrentSettings.DFPlayerProcessDelay) {
    //woah there buddy
    delay(CurrentSettings.DFPlayerProcessDelay);
  }
  CurrentSettings.LastProcessedEvent = millis();
  //myMP3.pause();//seems i can do without this pause here now.
  myMP3.stop();
  delay(20); //pause and audio and wait for DFplayer to be happy


  if (Action == 1) { //a card has been removed, play a nice sound
    CurrentSettings.PlayingType = 0;
    myMP3.playFolder(1, 5);
    Serial.println("Card Removed");
    return;
  }
  if (Action == 2) { //multiple cards detected, play a fun warning sound
    CurrentSettings.PlayingType = 0;
    myMP3.playFolder(1, 7);
    Serial.println("Multiple Cards" + UID);
    return;
  }

  if (Action == 3) { //A valid card presented event, process it.
    int PTResult = -1;
    PTResult = isControlCard(UID); //It's a control card, at the moment thats just a volume control card
    if (PTResult >= 0) { //found a control card

      if (PTResult == 0 || PTResult == 1) { // volume card
        CurrentSettings.initVolume++;
        if (CurrentSettings.initVolume > 3) {
          CurrentSettings.initVolume = 0;
        }
        myMP3.volume(CurrentSettings.VolumeRoundRobin[CurrentSettings.initVolume]);
        delay (CurrentSettings.DFPlayerProcessDelay);
        myMP3.playFolder(1, 6);
        Serial.println("Volume Adjust - ");
        Serial.print(CurrentSettings.initVolume);
        Serial.println("");
        return;
      }
      if (PTResult == 2 ) { // Statue card
        //toggle statue mode
        CurrentSettings.isMusicalChairsMode = !CurrentSettings.isMusicalChairsMode;
        //tell debug
        if (CurrentSettings.isMusicalChairsMode) {
                myMP3.playFolder(1, 1);      
          Serial.println("Statue on");
        }
        else {
          Serial.println("Statue off");
          myMP3.playFolder(1, 5);
        }
        return;
      }

    }

    PTResult = isTrackCard(UID);
    if (PTResult >= 0) { //Found a valid music card / play it!
      myMP3.playFolder(1, 1);
      delay(610); //ugly delaky while card noise playsouts. this was adjusted until it 'felt' right.
      myMP3.stop();
      delay (CurrentSettings.DFPlayerProcessDelay);
      myMP3.playFolder(2, PTResult);
      Serial.println("Play Track - ");
      Serial.print(UID);
      Serial.println("PTResult - ");
      Serial.print(PTResult);
      Serial.println("");
      CurrentSettings.PlayingType = 1;
      CurrentSettings.MilliStartPlaytime = millis();
      if (CurrentSettings.isMusicalChairsMode) {
        SetnewStatueTime();
      }
      return;
    }

    //card not recognised, player fun sound
    Serial.println("Card not found - ");
    Serial.print(UID);
    Serial.println("");
    myMP3.playFolder(1, 4);
    return;
  }

}




void setup(void) {


  randomSeed(analogRead(A0)); // read noise from an Analogue GIO for the random number generator


  // turn off wifi to reduce battery drain until app is ready
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();

  // Serial for debugging // remove when finished
  Serial.begin(115200);

  // Afficher un message de démarrage
  Serial.println(F("=== Music Box Booting ==="));
  Serial.println(F("Initialisation des composants..."));

  /*************Start Audio Player****************************************************/
  mySoftwareSerial.begin(9600);
  myMP3.begin(mySoftwareSerial);
  pinMode(16, INPUT);

  Serial.println(F("Audio online."));
  delay(750); //give time for things to settle and have a cup of tea.
  myMP3.volume(CurrentSettings.VolumeRoundRobin[CurrentSettings.initVolume]);  //Set default volume.
  myMP3.EQ(DFPLAYER_EQ_NORMAL); // Set default EQ
  // EQ_NORMAL       = 0;EQ_POP          = 1;EQ_ROCK         = 2;EQ_JAZZ         = 3;EQ_CLASSIC      = 4; EQ_BASE         = 5;

  /*********************************************************************/

  /*************Start NFC Reader***************************************/
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("PN53x NOT FOUND");
    while (true) {
      delay(0); // Keep ESP8266 watch dog happy.
    }
  }

  // configure read RFID tags
  nfc.SAMConfig();
  nfc.setPassiveActivationRetries(0x1); // set NFC read attempts to 1. So we can read up to 2 cards and not have the nfc block code.
  Serial.println("NFC OK");

  /*********************************************************************/

  // Startup Complete
  Serial.println("All Systems go...");

  CurrentSettings.ReadStateMillis = millis();

  //Start up sound
  // myMP3.playFolder(1, 2);
  myMP3.playMp3Folder(7);
}


void loop(void) {

  String tmptag  = "";
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // UID Buffer
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes)

  // Is an NFC card Present
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);


  //process nfc card touch

  // If anybody is thinking why im doing it this way.
  // 1. This is non-blocking
  // 2. It allows for processing for up to 2 card (not implemented yet)
  // 3. This is the first working way I could process a new/removed card well.
  if (success) {
    //hold a flag if a card is present
    CurrentSettings.isCardPresent = true;

    for (uint8_t i = 0; i < uidLength; i++) {
      tmptag.concat(uid[i]); //convert hex card id to a string (yes i know, strings!!)
    }
    CurrentSettings.CurrentCardUID = tmptag;
    if (CurrentSettings.PreviousCardUID == "") {
      CurrentSettings.PreviousCardUID = tmptag; //first time a card has been presented
    }

    if (CurrentSettings.PreviousCardUID == tmptag) {//assume single card
      if (!CurrentSettings.isValidCardPresent) {
        CurrentSettings.isValidCardPresent = true;
        CurrentSettings.isMultipleCardsPresent = false;
        ProcessEvent(tmptag, 3);
      }
    } else {
      if (!CurrentSettings.isMultipleCardsPresent) { //assume Multiple Cards
        CurrentSettings.isValidCardPresent = false;
        CurrentSettings.isMultipleCardsPresent = true;
        ProcessEvent("", 2);
      }
    }
    CurrentSettings.LastCardMillis = millis();
    CurrentSettings.PreviousCardUID = tmptag;
  }

  //check for a card removed

  if (((millis() - CurrentSettings.LastCardMillis) > CurrentSettings.LastCardThreshold) && CurrentSettings.isCardPresent) {
    CurrentSettings.PreviousCardUID = "";//CurrentCardUID;
    CurrentSettings.CurrentCardUID = "";
    CurrentSettings.isCardPresent = false;
    CurrentSettings.isValidCardPresent = false;
    CurrentSettings.isMultipleCardsPresent = false;
    ProcessEvent("", 1);
  }

  //check for playback stopped
  if ((millis() -  CurrentSettings.ReadStateMillis) >  CurrentSettings.ReadStateDelay) {
    //we also wait two seconds after a track has started until we check to see if it has ended
    if ((millis() - CurrentSettings.MilliStartPlaytime) > 2000 ) {

      CurrentSettings.ReadStateMillis = millis();
      int IsAudioPlaying = digitalRead(AudioBusyPin);
      if (IsAudioPlaying == 1 && CurrentSettings.PlayingType > 0) {
        Serial.println("Play has stopped");
        CurrentSettings.PlayingType = 0;
      }
    }
  }

  //check for Musical Chairs /Statue mode

  //we also wait two seconds after a track has started until we check to see if we need an eent
  if ((millis() - CurrentSettings.MilliStartPlaytime) > 2000 ) {
    if ( CurrentSettings.PlayingType > 0 && CurrentSettings.isMusicalChairsMode) {
      long timetillpause = (CurrentSettings.NextStatueTime - millis());
                            Serial.print("Time till pause  ");
                            Serial.println(timetillpause);
      if  (timetillpause < 0 ) { //time for a pause. min 3 sec max  7 
          int randomNumber = random(3, 6); 
          SetnewStatueTime() ; 
          Serial.println("Pause...");
          myMP3.pause();     
          delay(1000* randomNumber);
          Serial.println("Resume...");
          myMP3.start();
                   
      }


    }
  }






}


