#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include "ProtocolManager.h"
  
  ProtocolManager::ProtocolManager()
  {
    packet_index = 0;
    // Initialize empty_mac to zero (needed, not zero by default?)
    memset(&empty_mac, 0, sizeof(bdaddr_t));
  }
  
  ProtocolManager::~ProtocolManager()
  {
    Close();
  }
  
  // Connect to inverter. Returns 0 on failure, negative value on connect error, positive value on protocol error
  int ProtocolManager::Connect(char* mac_address)
  {
      // Convert string to mac address
      str2ba(mac_address, &sma_mac);  
      // Make bluetooth RFCOMM connection, return on error
      int status;
      if ((status = BTConnect(&sma_mac)) < 0)
      {
        return status;
      }
      // Read login ping packet, try twice. Check for read failure, correct command, and correct source address.      
      L1Packet packet;
      for (int attempt = 0; !packet.Read(s) || packet.Command() != L1_Command_LoginPing || !packet.CheckSource(&sma_mac); attempt++)
      {
        if (attempt == 2)
        { // Could not understand login packet twice                  
          return 1;
        }
      }
      // Respond. Note that I use the same packet, and set the data to its own data (which is the way to connect)
      packet.SetHeader(&empty_mac, &sma_mac, L1_Command_LoginPing);
      if (!packet.Send(s, packet.Data(), packet.DataLength()))
      { // Error sending data
        return 2;
      }
      // Wait for reply L1_Command_Login_3 (last in the series of 3)  
      if (!WaitForPacket(L1_Command_Login_3, &sma_mac, &packet) || packet.DataLength()!= sizeof(L1Login3Data_t))
      { // Did not receive Login_3 packet
        return 3;
      }
      // Copy our MAC address from this reply
      // @@@ LAME but I don't know how to get it from bluez...
      memcpy(&our_mac, &((L1Login3Data_t *) packet.Data())->us, sizeof(bdaddr_t));
      // Done, success
      return 0;      
  
  }
  
  // Close connection
  void ProtocolManager::Close()
  {
    if (s != 0)
    {
      close(s);
      s = 0;
    }
  }
  
  // Return bluetooth signal strength (@ inverter)
  double ProtocolManager::BluetoothStrength()
  {
    uint16_t request = htobs(L1_Request_BluetoothStrength);
    // For some odd reason only seems to work this way...
    L1Packet packet;
    packet.SetHeader(&our_mac, &sma_mac, L1_Command_RequestForInfo);
    if (!packet.Send(s, (uint8_t *) &request, sizeof(request)))
    { // error sending
      return -1;
    }
    if (!WaitForPacket(L1_Command_ResponseToRequest, &sma_mac, &packet) || packet.DataLength() != sizeof(L1BluetoothStrengthData_t))
    { // received nothing or something wrong
      return -2;
    }
    double strength = (((L1BluetoothStrengthData_t *) packet.Data())->strength)*100.0/256.0; 
    return strength;
  }
  
// Read one or more L1 packets that transmit an L2 packet. Return the concatenated data.
// Waits (indefinitely) for L1_Command_L2_Packet or L1_Command_L2_PacketPart. Checks source address of the packets. Concatenates data. 
// Returns when the final L1_Comamand_L2_Packet is received   
uint8_t *ProtocolManager::ReadL2Packet(int* length)
{
  L1Packet packet;
  uint8_t *data = NULL;
  int data_length = 0;
  
  do
  {
    bool status;
    // Wait for a L2 packet (part). Wait indefinitely, not good/nice...
    while ( (status = packet.Read(s)) &&          // status has to be ok
            (
              !packet.CheckSource(&sma_mac) ||     // ignore packet not for us  
              ((packet.Command() != L1_Command_L2_Packet && packet.Command() != L1_Command_L2_PacketPart))  // should be L2 packet (part)
            )
          );
    // Something bad happened; return null
    if (!status)
    {
      if (data != NULL)
      {
        free(data);
      }
      *length = 0;
      return NULL;
    }
    // Enlarge our data buffer
    data = (uint8_t *) realloc(data, data_length + packet.DataLength());
    // Copy new packet data to buffer
    memcpy(data + data_length, packet.Data(), packet.DataLength());
    // Update buffer size
    data_length += packet.DataLength();
  } while (packet.Command() != L1_Command_L2_Packet); 
  // Done, return result
  *length = data_length;
  return data;  
} 
  

// Logon to inverter  
bool ProtocolManager::Logon(uint8_t* password)
{
    L2Packet l2;
    uint8_t data_logon[sizeof(L2_data_logon)];
       
    // Send login_1 command
    usleep(3000);
    l2.SetFields(0xA0, 0, 0, ++packet_index, L2_command_login_1); 
    if (!SendL2(&l2, L2_data_login_1, sizeof(L2_data_login_1))) 
    { // Error sending L2 packet
      return false; 
    }
    // Ignore response
    if (!DummyL2Read())
    {
#ifdef __DEBUG    
      printf("login 1 failed\n");
#endif
// We sometimes seem to recover. Mostly this fails when we left the SMA inverter
// in a confused state after a protocol error (by us...).     
//      return false;
    }
    
    // Send login_2 command (no reply)
    usleep(3000);
    l2.SetFields(0xA0, 0x03, 0x03, ++packet_index, L2_command_login_2);
    if (!SendL2(&l2, L2_data_login_2, sizeof(L2_data_login_2)))
    { // Error sending L2 packet
      return false;
    }                       
    // No response   
         
    // Create login data including the password        
    memcpy(data_logon, L2_data_logon, sizeof(L2_data_logon)); 
    for (int i = 0; i < 12 && password[i] != 0; i++)
    {
      data_logon[i+16] = (uint8_t) (password[i] + 0x88);
    }
    // Send logon command
    usleep(3000);
    l2.SetFields(0xA0, 0x01, 0x01, ++packet_index, L2_command_logon);
    if (!SendL2(&l2, data_logon, sizeof(data_logon)))
    { // Error sending L2 packet
      return false;
    }
    /// Ignore response
    if (!DummyL2Read())
    {
      return false;
    }
    return true;
}
  
  
  
  
  
  // Get yield info
  int ProtocolManager::GetYieldInfo(YieldInfo& yi)
  {
    L2Packet l2;
    uint8_t *data;
    int data_length;
    // Set structure to zer
    memset(&yi, 0, sizeof(YieldInfo));
    // Send command
    l2.SetFields(0xA0, 0x00, 0x00, ++packet_index, L2_command_daily_yield);    
    if (!SendL2(&l2, L2_data_daily_yield, sizeof(L2_data_daily_yield)))
    {
      return PM_ERROR_SENDING_COMMAND;
    }
    
    // Get reply. Checks size of the returned data.
    data = GetFramedReply(&l2, &data_length, sizeof(_ValueInfo));
    if (data == NULL)
    {
      return PM_ERROR_INTERPRETING_REPLY;
    }        
    // Interpret data. We assume the fields will be present, otherwise zero's are returned!                                
    _ValueInfo *vi =  (_ValueInfo *) (data + sizeof(_FrameInfo));
    int no_frames = (((_FrameInfo *)data)->end_frame - ((_FrameInfo *)data)->start_frame) + 1;    
    for (int i = 0; i < no_frames; i++)
    {
      switch(btohs(vi[i].code))
      { 
        case 0x2601: yi.Total = btohl(vi[i].value); break;
        case 0x2622:
          yi.TimeStamp = btohl(vi[i].timestamp); 
          yi.Today = btohl(vi[i].value); 
        break;
        case 0x462E: yi.OperatingTime = btohl(vi[i].value); break;
        case 0x462F: yi.FeedInTime = btohl(vi[i].value); break;            
      }
    }
    // Done: free data and return result
    free(data);
    return 0;
  }
  
  
  
  // Get historic yield. daily = true: daily values, daily = false: 5 minute updates
  int ProtocolManager::GetHistoricYield(uint32_t from, uint32_t to, HistoricInfo& hi, bool daily)
  {
    L2Packet l2;
    uint8_t *data;
    int data_length;
    
    // Clear structure
    memset(&hi, 0, sizeof(HistoricInfo));
    // Set request data: start and end of enquiry interval
    L2_data_historic_yield hyd;
    hyd.timestamp_from = from; 
    hyd.timestamp_to = to;     
    // Send command    
    l2.SetFields(0xA0, 0x00, 0x00, ++packet_index, (daily) ? L2_command_historic_yield_daily : L2_command_historic_yield_5);    
    if (!SendL2(&l2, (uint8_t*) &hyd, sizeof(L2_data_historic_yield)))
    {
      return PM_ERROR_SENDING_COMMAND;
    }
            
    // Read answer to our request and store data in structure
    do
    {  
      // Get reply. Checks size of the returned data.
      data = GetFramedReply(&l2, &data_length, sizeof(_HistoricYieldInfo));
      if (data == NULL)
      {
        return PM_ERROR_INTERPRETING_REPLY;
      }
      int no_frames = (((_FrameInfo *)data)->end_frame - ((_FrameInfo *)data)->start_frame) + 1;
      // Reserve memory for the new frames
      hi.Records = (HistoricInfoItem *) realloc(hi.Records, (hi.NoRecords + no_frames) * sizeof(HistoricInfoItem));    
      // Copy date to our storage                        
      _HistoricYieldInfo *vi =  (_HistoricYieldInfo *) (data + sizeof(_FrameInfo));    
      for (int i = 0; i < no_frames; i++)
      {                                   
        hi.Records[hi.NoRecords + i].TimeStamp = btohl(vi[i].timestamp);
        hi.Records[hi.NoRecords + i].Value = btohl(vi[i].value); 
      }
      // Update record counter
      hi.NoRecords += no_frames;
      // Continue until we have read all records (or reach a limit)
    } while (l2.TelegramNumber() != 0 && hi.NoRecords < PM_MAX_RECORDS);
    // Success
    return 0;
  }
  
  // Private part
  
  
  
  // Bluetooth connect; returns < 0 on error.  
  int ProtocolManager::BTConnect(bdaddr_t* mac_address)
  {
    struct sockaddr_rc addr = { 0 };
    // Close (when needed)
    Close();
    // allocate a socket (I do not specify non-blocking, so it should be blocking)
    s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    // Set time-out to 4s
    struct timeval timeout;      
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt (s, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));    
    // set the connection parameters (who to connect to)
    addr.rc_family = AF_BLUETOOTH;                                                                  
    addr.rc_channel = (uint8_t) 1;
    memcpy(&addr.rc_bdaddr, &sma_mac, sizeof(bdaddr_t));    
    // connect to server, return status
    return connect(s, (struct sockaddr *)&addr, sizeof(addr));
  }
  
  // Wait (indefinitely...) for packet with given command from given sender. Returns false on connection failure.
  bool ProtocolManager::WaitForPacket(uint16_t command, bdaddr_t *sender, L1Packet *p)
  {
    bool status;
    while ((status = p->Read(s)) && (p->Command() != command || !p->CheckSource(sender)));
    return status;
  }

  // Print MAC address
  void ProtocolManager::PrintMac(bdaddr_t *m)
  {
    printf("[%02X:%02X:%02X:%02X:%02X:%02X]", m->b[0],m->b[1],m->b[2],m->b[3],m->b[4],m->b[5]);
  }        
  
  // Send L2 packet as L1 data (over on or more L1 packets)
  bool ProtocolManager::SendL2(L2Packet *l2, const uint8_t *packet_data, int data_length)
  {
    L1Packet l1;
    int bytes_left;
    uint8_t *data = l2->PreparePacket(packet_data, data_length, &bytes_left);
    // Send    
    while (bytes_left > 0)
    {     
      int bytes_to_sent = (bytes_left > L1_MaxDataLength) ? L1_MaxDataLength : bytes_left;
      bytes_left -= bytes_to_sent;
      l1.SetHeader(&our_mac, &sma_mac, (bytes_left == 0) ? L1_Command_L2_Packet : L1_Command_L2_PacketPart);
      if (!l1.Send(s, data, bytes_to_sent)) 
      { // error sending
        return false;
      }
      // Move data pointer
      data += bytes_to_sent;
    }    
    return true;
  }

