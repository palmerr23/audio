/* Network input for Teensy Audio Library 
 * Richard Palmer (C) 2019
 * Based on Paul Stoffregen's TDM library
 * Copyright (c) 2017, Paul Stoffregen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

 // (RP) ****** This object does not accept update_responsibility - ONE device that does needs to be in your sketch (e.g. a DAC output) ****
 // All IO handling is in the Ethernet, EthernetUDP & SPI Libraries 
 // WIZNET handles registration of hosts, other control (not audio related) messaging through separate UDP port
 // This version handles 2 channels, 128 sample blocks. 



#include <Arduino.h>
#include "input_net.h"

// debugging
#define IT_REPORT_EVERY 2000
#define IN_ETH_SERIAL_DEBUG 1  // set to 0 for production 

extern short bloxx;

void AudioInputNet::begin(void)
{
	currentPacket_I = 0; // initialize packet sequence
	myControl_I = NULL;
	 _myObjectID = -1; 
//	memset(&zeroBlockI, 0, SAMPLES_2); // create an audio block of zeros for when no packets are available.
	_myStream_I = -1; 
	blocksRecd_I = missingBlocks_I = 0;
	inputBegun = true;
#if IN_ETH_SERIAL_DEBUG > 0
  //Serial.print(AudioMemoryUsage());
  Serial.println("IN: inputNet.begin() complete");
#endif
}

void AudioInputNet::update(void)
{
short aIndex;
//	Serial.print("NI ");
#if IN_ETH_SERIAL_DEBUG > 15
	Serial.print("{I ");
	//Serial.print((myControl_I == NULL)? "NC " : "CTL ");
#endif	
	if (!inputBegun) // no processing at all until begin() is complete
		return;
	// transmit silence until the ethernet control is defined and enabled
	// this code may not be strictly necessary, as AudioLib copes with no blocks transmitted???
	if (myControl_I == NULL) 
	{
		//transmit(&zeroBlockI, 0);
		//transmit(&zeroBlockI, 1);	
//Serial.print("1");
		return;
	}
	
	if (!(myControl_I->ethernetEnabled))	// Ethernet not enabled. Need sequential tests to avoid myControl_I NULL pointer issues	with this test
	{
		//transmit(&zeroBlockI, 0);
		//transmit(&zeroBlockI, 1);	
//Serial.print("2");
		return;
	}
//Serial.print("A ");		
	// register for control queue packets if not already done 
	if(_myObjectID < 0)
		_myObjectID = myControl_I->registerObject();
	
	// audio blocks already created in ethernet_control - destroyed there as well
	// check for audio packets from control_ethernet object for this stream
	// get next block and mark as consumed by me (subscribed--)
	// transmit and release blocks
	aIndex = myControl_I->getNextAudioQblk_I( _myStream_I );  // anything in my subscribed stream? If so, block index is returned.

	if (aIndex != -1) { 		
		currentPacket_I = myControl_I->Qblk_A[aIndex].sequence;
		transmit(myControl_I->Qblk_A[aIndex].bufPtr[0], 0);    // cleanup routine will release these blocks. as someone else might be subscribed
		transmit(myControl_I->Qblk_A[aIndex].bufPtr[1], 1);	   
		// myControl_I->Qblk_A[aIndex].subscribed--; // I'm subscribed to this stream, and have consumed this block ||| WHY IS THIS COMMENTED OUT?
		blocksRecd_I++;
//Serial.print("3");
	} 
	else
	{  // no current block from ethernet, so send a block of zeros (may not need to do this??)
		//transmit(&zeroBlockI, 0);
		//transmit(&zeroBlockI, 1);	
		missingBlocks_I++;
//Serial.print("4");
		// reset counters on disconnection
		if(myControl_I->getLinkStatus() && myControl_I->streamsIn[_myStream_I].active)
		{

//Serial.print(" *"); Serial.print(_myObjectID);

#if IN_ETH_SERIAL_DEBUG > 0
			Serial.print("*** IN: Missing block: "); Serial.print(missingBlocks_I);
			Serial.print(" input["); Serial.print(_myObjectID); 
			Serial.print("], total recd = "); Serial.print(blocksRecd_I); 
			if(blocksRecd_I > 0) 
			{
				Serial.print(". Loss ratio = "); Serial.print((float)missingBlocks_I*100/blocksRecd_I,3); Serial.print("%");
			}
			Serial.print(", AQ Free ");
			Serial.println(myControl_I->countQ_A(&(myControl_I->audioQ_Free)));
#endif
		} else
			blocksRecd_I = missingBlocks_I = 0; 
	} // transmit block
	
	// transit control queue looking for things we care about
	// stream messages handled by network layer
#if IN_ETH_SERIAL_DEBUG > 5
			if ((it_report_cntr++ % IT_REPORT_EVERY )== 0)
			{	
			Serial.print("{I "); Serial.print(AudioMemoryUsage()); Serial.print("/"); 
			Serial.print(bloxx); 
			Serial.println("}");
		 } 
		 
	// check and process control blocks
	/*
	 *	NOTHING YET DEFINED
	 */
	 
#endif
#if IN_ETH_SERIAL_DEBUG > 15
	Serial.println("}");
#endif
	//Serial.println(" NX");
}

void AudioInputNet::setControl(AudioControlEtherNet * cont) 
{
	myControl_I = cont; // set the control object associated with this input object
} 

bool AudioInputNet::subscribeStream(short stream) // subscribe to an audio stream	
{ 
	if(stream < 0 || stream >= myControl_I->activeTCPstreams_I) // not a legal streamID
		return false; 
		
	if(_myStream_I >= 0) // already subscribed? release old stream
		releaseStream();
		
	myControl_I->streamsIn[stream].subscribers++;
	_myStream_I = stream; 
	return true; 
} 
void AudioInputNet::releaseStream(void)  // release the subscribed stream. 
{
	if (_myStream_I >= 0)
		myControl_I->streamsIn[_myStream_I].subscribers--;
	_myStream_I = -1; 	
}

short AudioInputNet::getMyStream(void) 
{
	return _myStream_I; 
}
 
short AudioInputNet::getNextStreamIndex(short thisStream)  // find the next active input stream
{
	short i;
	thisStream++; //start at next index (i.e. 0 if thisStream == -1)
	// else start
	for(i = thisStream; i < myControl_I->activeTCPstreams_I; i++) // only active streams
		if (myControl_I->streamsIn[i].active && myControl_I->streamsIn[i].hostStreamID >= 0)
			return i;
	return -1; // no next active stream
}
void AudioInputNet::isr(void)
{
	// do nothing - hardware control is by Ethernet
}
