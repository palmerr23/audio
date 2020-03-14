/*
 * UDP receive datagrams - audio library
 * Example:  Receive a single stream (2 x sine waves) and output to DACs or PWM
 * using Teensy Audio library and Ethernet Library
 * WIZ 5500 preferred
 * The Ethernet code DOES NOT have an ISR() so something that does is required in the sketch for packets to flow (PWM or DAC used here). 
 */
 // Defaults to WIZNET 5500 shield attached to pins CS/SS=10, MOSI=11, MISO=12, SCK=13, RST=9 (RST optional)
#define CS_PIN_E   (10)   // required
#define SCK_PIN_E  (14)   // optional
#define MISO_PIN_E (12)   // optional
#define MOSI_PIN_E  (7)   // optional
#define DEBUG 1 // Set to 0 for no serial monitor messages

//#define T4    // DACs not implemented on T4, so use PWM output instead.
//#define T35_6 // Two DACs
#define T32     // only one DAC

// Assumes WIZNET 5500 shield attached to pins CS/SS=10, MOSI=11, MISO=12, SCK=13, RST=9 (RST optional)
// If required, change pins in audio_net.h 

// Enter a unique HostID (1-254) 
// Assumes a Class C network and IP = (192, 168, 1, MYID).
#define MYID 25    // range: 1 - 254, must be different for each Teensy connected.

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioInputNet            net_in1;       
AudioControlEtherNet     EtherNet1;    
AudioOutputAnalog        dac1;           
AudioConnection          patchCord1(net_in1, 0, dac1, 0);
// GUItool: end automatically generated code

const int myInput = AUDIO_INPUT_LINEIN;

void setup() 
{
   AudioMemory(10);

#if DEBUG > 0
    // Open serial communications and wait for port to open:
  Serial.begin(38400);
  while (!Serial) 
    ; // wait for serial port to connect. Needed for native USB port only
  Serial.println("Ethernet Audio - Test Alt SPI = RECV");
  //delay(100);
#endif   

Serial.print("SCK ");
//Serial.println(SPI.pinIsSCK(AUDIO_ETHER_PIN_SPI_SCK));

    // start Ethernet and input object
    EtherNet1.begin(CS_PIN_E, SCK_PIN_E, MOSI_PIN_E, MISO_PIN_E); // order is important - start the control first
    net_in1.begin();
    EtherNet1.setMyID(MYID);
    net_in1.setControl(&EtherNet1);
 
#if DEBUG > 0
    printDiagnostics();
    Serial.println("Exiting setup() - ethernet link may take some time to connect. \n______________________");
#endif 
}
#define TIME_PRINT_EVERY 5 // secs

bool subscribed = false;
controlQueue cQbuf;
short streamsAvail;

void loop() {
  // all audio processing happens in background

  // subscribe to the incoming packet stream
  if(!subscribed)
  {
       streamsAvail = EtherNet1.getActiveStreams(); // Ethernet control owns the streams
#if DEBUG > 0
      if (streamsAvail > 0)
        printStreams();
#endif   
      // Subscribe to an incoming stream, via an input control, once they become available 
      if(streamsAvail > 0)
          subscribed = net_in1.subscribeStream( 0 );  // using streamID = 0 as an example
  }    
  
  // process the incoming control packet queue
   while (EtherNet1.getQueuedUserControlMsg(&cQbuf)) // best to regularly read the control queue for incoming messages  
     ; // processing here of any messages we care about


  // end of mandatory processing
#if DEBUG > 0
    if((micros() % (TIME_PRINT_EVERY * 1000000)) == 0) // print every few seconds
    {
         printGeneralStats();     
         printStreams();
    }     
#endif
}

void printStreams()
{
  short i, maxStream;
  netAudioStream stream;
  maxStream = EtherNet1.getActiveStreams();
  Serial.print("\nStreams\n");
  for (i = 0; i < maxStream; i++)
  {
      stream = EtherNet1.getStream(i);
      Serial.print("Name: "); Serial.print(stream.streamName); 
      Serial.print(", Host: "); Serial.print(stream.remoteHostID);
      Serial.print(", Stream: "); Serial.print(stream.hostStreamID);
      Serial.print(", Active: "); Serial.println(stream.active ? " Y": " N");
  }
}
void printDiagnostics()
{
short wiztype;
    // Check for Ethernet hardware present
    // Warning: very few direct calls to the Ethernet library may be executed from a user program, as most directly issue SPI transactions.
    if ((wiztype = Ethernet.hardwareStatus()) == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware.");   
    }
    else {
        Serial.print("Found WIZ type ");
        Serial.println(wiztype);    
    }
    printIP("My IP is ", EtherNet1.getMyIP());
    Serial.print("Ethernet cable connected = ");
    Serial.println(EtherNet1.getLinkStatus() ? "Yes" : "No");
    Serial.print("Audio Memory "); Serial.println(AudioMemoryUsage());

}
void printGeneralStats()
{
        Serial.print("\nEthernet cable connected = ");
        Serial.println(EtherNet1.getLinkStatus() ? "Yes" : "No");
        Serial.print("Processor (Curr/Max) = ");  Serial.print(AudioProcessorUsage());
        Serial.print("/"); Serial.print(AudioProcessorUsageMax());   
        Serial.print(". Audio Memory (Curr/Max) = ");  Serial.print(AudioMemoryUsage());
        Serial.print("/"); Serial.print(AudioMemoryUsageMax());  
}

void printIP(char text[], IPAddress buf){
  short j;
     Serial.print(text);
     for (j=0; j<4; j++){
       Serial.print(buf[j]); Serial.print(" ");
     }
     Serial.println();
}
