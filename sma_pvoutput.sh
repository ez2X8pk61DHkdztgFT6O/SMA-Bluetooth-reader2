#!/bin/sh
rm ./sma_pvoutput
clear
g++ $1 -lbluetooth -lcurl L1.cc L2.cc ProtocolManager.cc sma_pvoutput.cc -o sma_pvoutput
./sma_pvoutput --MAC 00:00:00:00:00:00 --password 0000 --api_key fad4f5a10ea9de57d4546b939e813b1ff80b928d --sid 21379

