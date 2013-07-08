!/bin/sh
rm ./sma_pvoutput
clear
g++ -lbluetooth -lcurl L1.cc L2.cc ProtocolManager.cc sma_pvoutput.cc -o sma_pvoutput
./sma_pvoutput --MAC 00:00:00:00:00:00 --password 0000 --api_key 12345567aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa --sid 12345 

