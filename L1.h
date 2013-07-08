#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

// Command values
#define L1_Command_L2_Packet          0x0001
#define L1_Command_LoginPing          0x0002
#define L1_Command_RequestForInfo     0x0003
#define L1_Command_ResponseToRequest  0x0004
#define L1_Command_Login_3            0x0005                                            
#define L1_Command_Error              0x0007
#define L1_Command_L2_PacketPart      0x0008
#define L1_Command_Login_1            0x000a            
#define L1_Command_Login_2            0x000c

// Request values (data part for command L1_Command_RequestForInfo)
#define L1_Request_BluetoothStrength 0x0005

// Body length
#define L1_BodyLength                 18
#define L1_MaxDataLength              73  // from James Ball, http://blog.jamesball.co.uk/search/label/SMA

// Identity byte at start of packet
#define L1_Identity                   0x7e                                            

// L1 packet structure
typedef struct __attribute__ ((__packed__)) 
{
    uint8_t identity;
    uint8_t length;
    uint8_t unknown;
    uint8_t check;
    bdaddr_t source;
    bdaddr_t destination;        
    uint16_t command;
    uint8_t data[256];      // too much, but never overflow in reading...        
} L1Packet_t;


// Data in L1_Command_Login_3 packet
typedef struct __attribute__ ((__packed__)) 
{
    bdaddr_t sma;
    uint8_t fill1;
    uint8_t fill2;
    bdaddr_t us; 
    uint8_t fill3;
    uint8_t fill4;    
} L1Login3Data_t;

// Data in L1_Command_ResponseToRequest for request L1_Request_BluetoothStrength
typedef struct __attribute__ ((__packed__)) 
{
    uint16_t request_no;
    uint16_t fill;
    uint16_t strength;      
} L1BluetoothStrengthData_t;


// Class around L1Packet_t
class L1Packet
{
  L1Packet_t packet;
  
  public:
  
  // Default constructor
  L1Packet()
  {
  }
  
  // Constructor: Create packet header
  L1Packet(bdaddr_t *source, bdaddr_t *destination, uint16_t command)
  {
    SetHeader(source, destination, command);    
  }

  // Set packet header info
  void SetHeader(bdaddr_t *source, bdaddr_t *destination, uint16_t command);
  
  // Send packet including data
  int Send(int s, uint8_t *data, int length);
  
  // Read L1 packet from stream  
  bool Read(int s);
             
  // Return data (and length of data in len)
  uint8_t *Data(int* len)
  {
    *len = packet.length - L1_BodyLength;
    return packet.data;
  }
  
  // Return data 
  uint8_t *Data()
  {
    return packet.data;
  }
  
  // Return length of data
  int DataLength()
  {
    return packet.length - L1_BodyLength;
  }
  
  // Return source address
  bdaddr_t* Source()
  {
    return &packet.source;
  }
  
  // Returns true when source address == mac
  bool CheckSource(bdaddr_t* mac)
  {
    return !memcmp(mac, &packet.source, sizeof(bdaddr_t));
  }
  
  // Return destination address
  bdaddr_t* Destination()
  {
    return &packet.destination;
  }
  
  // Returns true when destination address == mac
  bool CheckDestination(bdaddr_t* mac)
  {
    return !memcmp(mac, &packet.destination, sizeof(bdaddr_t));
  }
  
  uint16_t Command()
  {
    return btohs(packet.command);
  }
    
  
  uint8_t CheckSum()
  {
    return packet.identity ^ packet.length ^ packet.unknown;
  }
  
  // Set checksum in our packet header
  void SetCheckSum()
  {
    packet.check = CheckSum();
  }
  
  // Verify checksum
  bool CheckCheckSum()
  {
    return CheckSum() == packet.check;
  }
  
  // Set data. No length checking!
  void SetData(uint8_t *data, int length)
  {
    memcpy(packet.data, data, length);
    packet.length = L1_BodyLength + length;
  } 
};

