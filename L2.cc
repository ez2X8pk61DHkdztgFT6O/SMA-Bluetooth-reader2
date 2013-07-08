#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <vector>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include "L2.h"



// Reconstruct packet from data (alters content of data!)
// Returns length of data, data in input array *data. len is the original length of the input.
int L2Packet::ReadPacket(uint8_t *data, int len)
{
    // Check whether any data is present
    if (data == NULL || len < sizeof(L2PacketHeader))
    {
      return ERR_SMA_INVALID_PACKET;
    }
    // Check first and last byte
    if (data[0] != L2_head || data[len-1] != L2_tail)
    { // First or last byte of L2 packet invalid
      return ERR_SMA_INVALID_PACKET;
    }            
    // Unescape, leave out the first byte. Note: last 0x7E byte will be copied since it is the last byte...
    int new_len = UnescapeData(data + 1, len - 1) + 1;
    // Get checksum
    uint16_t fcs = (uint16_t) data[new_len-3] + (((uint16_t)data[new_len-2])<<8);    
    // Copy header part to header
    memcpy((uint8_t *) &header, data, sizeof(L2PacketHeader));
    // Check header
    if (memcmp(&header.header, &L2_default_header, sizeof(L2_default_header)))
    { // No match...
      return ERR_SMA_INVALID_PACKET;
    }
    // Calculate length of data portion    
    int data_length = new_len - sizeof(L2PacketHeader) - 3;   // data length: total length minus header minus checksum minus end byte    
    // Copy data to data
    memmove(data, data + sizeof(L2PacketHeader), data_length);    
    // Check checksum (checksum is calculate over header, copied in code above, and the data)
    int actual_fcs = CheckSum(data, data_length);    
    if (actual_fcs != fcs)
    {
      return ERR_SMA_L2_CHECKSUM;
    }
    // Check length
    // Do this?
    // Sucessfully read packet
    return data_length;    
}
  

// Escape L2 packet (make it ready for sending as payload of L1 packets) and add checksum and footer. User should free returned array.
uint8_t* L2Packet::PreparePacket(const uint8_t *data, int data_length, int *len)
{
  std::vector<uint8_t> result;
  uint8_t *p = (uint8_t*) &header;
  // Set packet length
  header.length = (sizeof(L2PacketHeader) - 5 + data_length) / 4;
  // Add first byte of header without escaping (0x7E)
  result.push_back(*p++);
  // Add escaped header  
  EscapeData(result, p, sizeof(L2PacketHeader)-1);
  // Add escaped data
  EscapeData(result, data, data_length);
  // Calculate and add checksum
  uint16_t fcs = CheckSum(data, data_length);
  result.push_back((uint8_t) (fcs&0xFF));
  result.push_back((uint8_t) ((fcs>>8)&0xFF));
  result.push_back(L2_tail);
  // Return result. Make a copy, something nasty with local variables and references. Don't know how to do it differently (get internal buffer, ...)
  p = (uint8_t *) malloc(result.size());
  memcpy(p, (uint8_t *) result.data(), result.size()); // &result[0] works
  *len = (int) result.size();
#ifdef __DEBUG  
printf("packet index: %d\n", PacketIndex());  
#endif        
  return p;  
}

// Calculate PPP checksum, http://tools.ietf.org/html/rfc1662#page-19 and https://github.com/stuartpittaway/nanodesmapvmonitor  
uint16_t L2Packet::CheckSum(const uint8_t *data, int data_length)
{
  uint16_t fcs = 0xFFFF;
  const uint8_t *p;
  
  // Checksum over header (excluding first byte) [could be optimized using real pointer addressing]
  p = (uint8_t *) &header;
  for(int i = 1; i < sizeof(L2PacketHeader); i++) 
  {        
    fcs = (fcs >> 8) ^ fcstab[(fcs ^ p[i]) & 0xff];
  }
  // Checksum over data [dito, (*p++) style]
  p = data;
  for (int i = 0; i < data_length; i++)
  {
    fcs = (fcs >> 8) ^ fcstab[(fcs ^ p[i]) & 0xff];
  }
  // Final xor
  fcs =fcs ^ 0xffff;
  return fcs;
} 


// Unescape data in-place, return length of data.
int L2Packet::UnescapeData(uint8_t *src, int length)
{
  uint8_t *dest = src;
  int d_len = 0;
  for (int i = 0; i < length; i++)
  {
    int c = src[i]; 
    if (c == 0x7D && i < (length-1))  // can be made faster
    {
      i++;
      *dest++ = src[i]^0x20;            
    }
    else    
    {
      *dest++ = c;
    }
    d_len++;
  }
  return d_len;
}

// Escape data, append to destination vector
void L2Packet::EscapeData(std::vector<uint8_t>& destination, const uint8_t *src, unsigned int length)
{
  for (int i = 0; i < length; i++)
  { 
    uint8_t c = *src;
    if (c == 0x7D || c == 0x7E || c == 0x11 || c == 0x12 || c == 0x13) 
    {
      destination.push_back(0x7D);
      destination.push_back(c ^0x20);
    }
    else
    { 
      destination.push_back(c);
    }
    src++;
  }
}
