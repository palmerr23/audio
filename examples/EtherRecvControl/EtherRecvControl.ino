/*
 * UDP receive datagrams - audio library
 * Example:  Receive a single stream (2 x sine waves) and output to DACs or PWM
 *           Also return regular control packets to modulate one sine wave at the sender
 * using Teensy Audio library and Ethernet Library
 * WIZ 5500 preferred
 * The Ethernet code DOES NOT have an ISR() so something that does is required in the sketch for packets to flow (PWM or DAC used here). 
 */

#define DEBUG 1

//#define T4 // DACs not implemented on T4, so use PWM output instead.
//#define T35_6 // Two DACs
#define T32     // only one DAC

// Assumes WIZNET 5500 shield attached to pins CS/SS=10, MOSI=11, MISO=12, SCK=13, RST=9
//#define ResetWIZ_PIN 9
#define WIZ_CS_PIN 10 

#define SINCONTROL 129 // pktType and payload for a user defined control message
typedef struct sineMessage 
{
  byte channel;
  short freq;
  float ampl;
} sinMsg;

sinMsg mySin;

// Enter a unique HostID (1-254) 
// Assumes a Class C network and IP = (192, 168, 1, MYID).
#define MYID 5    // range: 1 - 254, must be different for each Teensy connected.

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioControlEtherNet     EtherNet1;      
AudioInputNet            net_in1;        
#ifdef T4     // T4 use PWM for output and to provide interrupts
AudioOutputPWM           pwm1;          
AudioConnection          patchCord1(net_in1, 1, pwm1, 0);
#endif
#ifdef T35_6         // other Teensys have DACS for better output quality
AudioOutputAnalogStereo  dacs1;          
AudioConnection          patchCord1(net_in1, 0, dacs1, 0);
AudioConnection          patchCord2(net_in1, 1, dacs1, 1);
#endif
#ifdef T32         // other Teensys have DACS for better output quality
AudioOutputAnalog        dacs1;          
AudioConnection          patchCord1(net_in1, 0, dacs1, 0);
#endif
// GUItool: end automatically generated code

void setup() 
{ 
  AudioMemory(40);
#if DEBUG >0
    // Open serial communications and wait for port to open:
  Serial.begin(38400);
  while (!Serial) 
    ; // wait for serial port to connect. Needed for native USB port only
  Serial.println("Ethernet Audio - Receive a single stream - 2 sine waves");
#endif   

    // start the Ethernet
    EtherNet1.begin(WIZ_CS_PIN); // order is important - start the control first
    net_in1.begin();
    EtherNet1.setMyID(MYID);
    net_in1.setControl(&EtherNet1);
 
#if DEBUG > 0
    printDiagnostics();
#endif 
}

unsigned long  lastpacket, pktnum;

//#define IPKTS 2000  // pkts

#define TIME_PRINT_EVERY 5 // secs

// Sine wave control - scans both frequency and amplitude
#define TESTINCR_F 1.0
#define LOWF 100
#define HIF 500
#define TESTINCR_F_A (0.00125)
#define HIA  0.95
#define LOWA 0.25
#define TESTMSG_EVERY 10 // mS
float freqVal = LOWF;
float ampVal = LOWA;
long controlSentMs;
short adir = 1, fdir = 1;

bool cbq, subscribed = false;
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
     
  // periodcally send control messages to to change the frequency and amplitude of the sender's sine wave
  if((micros() - controlSentMs) > (TESTMSG_EVERY * 1000))
  {
      controlSentMs = micros() ;     
    
      mySin.channel = 0;
      freqVal += (fdir * TESTINCR_F); // up-down ramping of frequency
      if(freqVal > HIF) 
         fdir = -1;
      if(freqVal < LOWF) 
         fdir = 1;         
      mySin.freq = (short)freqVal;
         
      ampVal += (adir * TESTINCR_F_A); // same for amplitude
      if(ampVal > HIA) 
         adir = -1;
      if(ampVal <LOWA)
          adir = 1;
      mySin.ampl = ampVal;      

      // broadcast the control block, could also send it to a specific Host
      cbq = EtherNet1.queueControlBlk(TARGET_BCAST, SINCONTROL, (uint8_t *) &mySin, sizeof(mySin)); 
#if DEBUG > 0      
      if(!cbq)
        Serial.println("Control block not sent");
#endif
   }
     
   // process the incoming control packet queue
   while (EtherNet1.getQueuedUserControlMsg(&cQbuf)) // need to regularly read the control queue for incoming messages  
       ; // processing here of any messages we care about

#if DEBUG > 0
    // general staistics report
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
    Serial.println("Exiting setup() - ethernet link may take some time to connect. \n______________________");
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
