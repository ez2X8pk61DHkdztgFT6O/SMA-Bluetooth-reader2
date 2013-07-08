#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include "L1.h"


  // Read L1 packet from stream  
  bool L1Packet::Read(int s)
  {
    // Read (blocking) L1_BodyLength bytes. Either gets the data, or time out
    int bytes_read = read(s, &packet, L1_BodyLength);
    if (bytes_read != L1_BodyLength || !CheckCheckSum())
    { // Time-out, connection broken, packet invalid: return false
      return false;
    }
           
    // Read data, when present
    if (packet.length > L1_BodyLength)
    {
      uint8_t data_length = packet.length - L1_BodyLength; 
      bytes_read = read(s, (((char *) &packet) + L1_BodyLength), data_length);
      if (bytes_read != data_length)
      { // Failure reading data
        return false;
      }
    }
    
    // Success
    return true;    
  }
  
  // Set packet header info
  void L1Packet::SetHeader(bdaddr_t *source, bdaddr_t *destination, uint16_t command)
  {
    packet.identity = L1_Identity;
    packet.command = htobs(command); 
    packet.unknown = 0;
    memcpy(&packet.source, source, sizeof(bdaddr_t));
    memcpy(&packet.destination, destination, sizeof(bdaddr_t));
  }
  
  // Send packet including data. Returns number of bytes sent.
  int L1Packet::Send(int s, uint8_t *data, int length)  
  {
    SetData(data, length);    
    SetCheckSum();            
    return write(s, &packet, packet.length) == packet.length;              
  }
