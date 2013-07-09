#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include "L2.h"


#define PM_ERROR_SENDING_COMMAND      -1
#define PM_ERROR_RECEIVING_REPLY      -2
#define PM_ERROR_INTERPRETING_REPLY   -3

#define PM_MAX_RECORDS                10000     // maximum 10000 historic records retreived in a single read

typedef struct
{
  int32_t TimeStamp;     // s since...
  uint32_t Total;         // [Wh]
  uint32_t Today;         // [Wh]
  uint32_t OperatingTime; // [s]
  uint32_t FeedInTime;    // [s]
} YieldInfo;

typedef struct
{
  int32_t TimeStamp;
  uint32_t Value;
} HistoricInfoItem;

typedef struct
{
  uint32_t NoRecords;
  HistoricInfoItem *Records;
} HistoricInfo;



typedef struct __attribute__ ((__packed__))
{
  uint32_t start_frame;
  uint32_t end_frame;
} _FrameInfo;

typedef struct __attribute__ ((__packed__))
{
  uint8_t one;
  uint16_t code;
  uint8_t zero;  
  int32_t timestamp;
  uint32_t value;
  uint32_t fill;
} _ValueInfo;

typedef struct __attribute__ ((__packed__))
{    
  int32_t timestamp;
  uint32_t value;
  uint32_t fill;
} _HistoricYieldInfo;


class ProtocolManager
{
  private:
  // Our, SMA, and all zero MAC addresses
  bdaddr_t our_mac;
  bdaddr_t sma_mac;
  bdaddr_t empty_mac;
  // Stream for communication
  int s;
  // Packet index
  uint16_t packet_index;
  
  public:  
  ProtocolManager();
  ~ProtocolManager();
  
  // Connect to inverter with given MAC address
  int Connect(char* mac_address);
  
  // Get bluetooth strength (0-99.x%)
  double BluetoothStrength();
  bool Logon(uint8_t* password);
  int GetYieldInfo(YieldInfo& yi);
  
  int GetHistoricYield(int32_t from, int32_t to, HistoricInfo& hi, bool daily);
  
  // Close connection
  void Close();
  
  private:
  // Send L2 packet. Note that the data and length of the data is provided separately
  bool SendL2(L2Packet *l2, const uint8_t *data, int data_length);
  // Read L2 packet by combining the data read from one or more L1 packets. 
  uint8_t *ReadL2Packet(int* length);
  
  int BTConnect(bdaddr_t* mac_address);
  bool WaitForPacket(uint16_t command, bdaddr_t *sender, L1Packet *p);
  void PrintMac(bdaddr_t *m);
  
  // Read L2 packet from inverter. Check validity of the packet (counter, etc) and return contents. Note: when receiving an
  // out-of-order packet (invalid packet number), we ignore it and try again...
  uint8_t *ReadAndCheck(L2Packet *l2, int *data_length)
  {
    uint8_t *data;
    do
    {
      // Read packet as array of bytes (may return null)
      data = ReadL2Packet(data_length);
      // Interpret packet data (handles data = null)    
      *data_length = l2->ReadPacket(data, *data_length);
      if (*data_length < 0)
      { // Error interpreting packet
          free(data);
          return NULL;
      }
#ifdef __DEBUG__      
if (l2->PacketIndex() != packet_index)
{      
printf("\tpacket index mismatch received %d != local %d\n", l2->PacketIndex(), packet_index);
printf("\t\tpacket index mismatch received %d != local %d\n", l2->PacketIndex(), packet_index);
}
for (int i = 0; i < sizeof(L2PacketHeader); i++)
{
  printf("%02X ", ((uint8_t *) &l2->header)[i]);
}
printf("\n");
#endif      
    } while (l2->PacketIndex() != packet_index);
    // Succesfully read and ignored an L2 packet
    return data;
  }
  
  // Read L2 packet from inverter. Only check presence and validity of the packet, ignore contents
  bool DummyL2Read()
  {
    L2Packet l2;
    uint8_t *data;
    int data_length;
    // Read data
    data = ReadAndCheck(&l2, &data_length);
    if (data == NULL)
    { // failure
      return false;
    }
    // Succesfully read and ignored an L2 packet
    free(data);
    return true;
  }
  
  uint8_t *GetFramedReply(L2Packet* l2, int* data_length, int frame_size)
  {
      uint8_t *data;      
      // Read reply
      data = ReadAndCheck(l2, data_length);
      if (data == NULL)
      {
#ifdef __DEBUG__      
printf("GetFramedReply: data = null\n");
#endif      
        return NULL;
      }
      // Check minimum reply length
      if (*data_length < sizeof(_FrameInfo))
      {
#ifdef __DEBUG__      
printf("GetFramedReply: Response too short\n");
#endif      
        free(data);
        return NULL;
      }      
      // Check reply length
      int expected_size = sizeof(_FrameInfo) + (((_FrameInfo *)data)->end_frame - ((_FrameInfo *)data)->start_frame + 1) * frame_size; 
      if (*data_length != expected_size)
      {
#ifdef __DEBUG__      
printf("GetFramedReply: Size mismatch, expected != actual: %d != %d!", expected_size, *data_length);
#endif
        free(data);
        return NULL;        
      }
      // Ok!
      return data;
  }
  
};


