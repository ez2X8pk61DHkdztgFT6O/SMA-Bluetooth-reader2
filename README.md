Code to extract daily and 5 minute yield from an SMA inverter (Sunny Boy) using 
Bluetooth. I use it on my Synology DSM412+ NAS to store production data in a 
local SQLite database and to upload it to ...

Based on:

- Protocol info described by James Ball (http://blog.jamesball.co.uk/). Some
small fixes (e.g. 'Telegram number' is two bytes and includes 'Mystery 3').

- nanodesmapvmonitor-master by Stuart Pittway (https://github.com/stuartpittaway/nanodesmapvmonitor/blob/master/README).
Used to complement the protocol described by James Ball.

Main files:

sma_sqlite    
Read daily and/or 5 minute data and store it incrementally in an SQLite database. Usage:
./sma_sqlite --MAC 01:02:03:04:05:06 --password 0000 --5minute --daily --sqlite /var/share/pv/data.sql

sma_pvoutput:
Upload latest 5 minute value to pvoutput. Usage:
 ./sma_pvoutput --MAC 01:02:03:04:05:06 --password 0000 --api_key fad4faa1eeafde17d4446b739e813121ff80b928d --sid 12345

sma_txt:       
to do: export as text


Files can be compiled separately. For sma_txt, the only requirement is the
presence of libbluetooth on the system. Compilation is currently done using a
script for each program (e.g. sma_sqlite.sh) and is only tested on the DSM412+.              

The SMA protocol consists of 2 parts:

- Level 2 (L2): the communication protocol that seems to have been developed
for modems (PPP encoding, checksums).
- Level 1 (L1): a wrapper around L2; sends L2 information in packets over
Bluetooth (which is a reliable, error-checked protocl).

The source consists of the following parts:

L1.cc / L1.h    
The L1Packet class handles sending and receiving of L1 packets
 
L2.cc / L2.h    
The L2Packet class handles checksumming and (un)escaping of an L2 packet and its data.         

ProtocolManager.cc / ProtocolManager.h
ProtocolManager class handles:
  - Sending/receiving of L2 packets over L1 packets
  - Top-level functionality: connect, login, get data, ...
                
Using the ProtocolManager, interacting with the SMA inverter looks like:

// Start protocol manager    
ProtocolManager *pm = new ProtocolManager();<BR>
// Connect<BR>
if (pm->Connect((char *) "AB:CD:EF:12:34:56"))<BR>
{<BR>
  EXIT_ERR("Error connection to SMA inverter\n");      
}     
// Login<BR> 
if (!pm->Logon((char *) "0000")<BR>
{<BR>
  EXIT_ERR("Error logging in to SMA inverter\n");<BR>
}<BR>
// Get current totals AND SMA time<BR>
YieldInfo yi;   // yi.TimeStamp, yi.DailyYield, yi.TotalYield, ...<BR>
if (pm->GetYieldInfo(yi))<BR>
{<BR>
  EXIT_ERR("Error getting current totals\n");<BR>
}                    
// Get historic daily yield<BR>
HistoricInfo hi; // hi.NoRecords, hi.Records[0...NoRecords].TimeStap, hi.Records[0...NoRecords].Value    
if (pm->GetHistoricYield(0, yi.TimeStamp, hi, true) != 0)<BR>
{<BR>
  EXIT_ERR("Error reading daily yield data.\n");<BR>
}<BR>
// Close session
pm->Close();   