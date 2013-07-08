!/bin/sh
rm ./a.out
clear
g++ -lbluetooth -lsqlite3 L1.cc L2.cc ProtocolManager.cc sma_sqlite.cc -o sma_sqlite
./sma_sqlite --MAC 00:00:00:00:00:00 --password 0000 --5minute --daily --sqlite /var/share/sqlite/data.sql

