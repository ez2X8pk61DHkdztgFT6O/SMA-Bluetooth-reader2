#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <sqlite3.h>
#include "ProtocolManager.h"
#include "sma_sqlite.h"

#define EXIT_ERR(a)  { printf(a); if (db != NULL) { sqlite3_close(db); }; if (pm != NULL) { pm->Close(); }; return -1; }

// Query for maximum value of timestamp in given table
int MaxTimeStamp(sqlite3 *db, const char *table)
{
  sqlite3_stmt *compiled;
  char query[256];
  int result = 0;
  // Create query
  sprintf(query, "SELECT MAX(timestamp) FROM %s", table);
  // Compile query
  sqlite3_prepare_v2(db, query, -1, &compiled, NULL);
  // Execute query
  if (sqlite3_step(compiled) == SQLITE_ROW)
  {
    result = sqlite3_column_int(compiled, 0);
  }
  // Free query 
  sqlite3_finalize(compiled);
  // Return result
  return result;
}

// Store historic data in indicated table
int StoreHistoricData(sqlite3 *db, const char *table, HistoricInfo *hi)
{
  // Append the data to the yield_5m table            
  sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
  char command[256];
  for (int i = 0; i < hi->NoRecords; i++)
  {
    sprintf(command, "INSERT INTO %s (timestamp, energy) VALUES (%d, %d)", table, hi->Records[i].TimeStamp, hi->Records[i].Value);
    if (sqlite3_exec(db, command, NULL, NULL, NULL) != SQLITE_OK)
    {      
      return -1;
    }
  }
  sqlite3_exec(db, "END", NULL, NULL, NULL);
  return 0;
}


// Main function
int main(int argc, char **argv)
{
    
    sqlite3 *db = NULL;
    // Read options
    Options options;
    if (options.Initialize(argc, argv) < 0)
    {
      return -1;
    }    
    // Start protocol manager    
    ProtocolManager *pm = new ProtocolManager();
    // Connect
    if (pm->Connect(options.MAC))
    {
      EXIT_ERR("Error connecting to SMA inverter\n");      
    }     
    // Login 
    if (!pm->Logon(options.Password))
    {
      EXIT_ERR("Error logging in to SMA inverter\n");
    }
    // Get current totals AND SMA time
    YieldInfo yi;
    if (pm->GetYieldInfo(yi))
    {
      EXIT_ERR("Error getting current totals\n");
    }    
    // Connect to database    
    if (sqlite3_open(options.Database, &db))
    {
      EXIT_ERR("Error opening/creating database\n");
    }
    // Create required tables
    if (
      sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS yield_5m (timestamp INTEGER PRIMARY KEY, energy INTEGER)", NULL, NULL, NULL) != SQLITE_OK ||
      sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS yield_daily (timestamp INTEGER PRIMARY KEY, energy INTEGER)", NULL, NULL, NULL) != SQLITE_OK)
    {
      EXIT_ERR("Error creating tables in SQLite database\n");
    } 
    // Get 5 minute yield values
    if (options.Minute5Yield)
    {
      // Find timestamp from latest insertion
      int from_timestamp =  MaxTimeStamp(db, (char *) "yield_5m");
      // Check whether we have something to do: is it more than 5 minutes later?
      if ((yi.TimeStamp - from_timestamp) > 500)
      { // Yes, more than 5 minutes between now and latest stored point
        // Get historic 5 minute data, starting 1 second after the latest timestamp 
        HistoricInfo hi;    
        if (pm->GetHistoricYield(from_timestamp + 1, time(NULL), hi, false) != 0)
        {
          EXIT_ERR("Error reading 5 minute yield data.\n");
        }
        // Append the data to the yield_5m table
        if (StoreHistoricData(db, (char *) "yield_5m", &hi))
        {            
          EXIT_ERR("Error storing historic data in SQLite database.\n");
        }
        free(hi.Records);
      }      
    }
    // Get daily yield values
    if (options.DailyYield)
    {
      // Find timestamp from latest insertion
      int from_timestamp =  MaxTimeStamp(db, (char *) "yield_daily");
      // Check whether we have something to do
      if ((yi.TimeStamp - from_timestamp) > (24*3600))
      { // Yes, more than 24 hour between now and latest stored point
        // Get historic dayly data, starting 1 second after the latest timestamp 
        HistoricInfo hi;    
        if (pm->GetHistoricYield(from_timestamp + 1, time(NULL), hi, true) != 0)
        {
          EXIT_ERR("Error reading daily yield data.\n");
        }
        // Append the data to the yield_5m table
        if (StoreHistoricData(db, (char *) "yield_daily", &hi))
        {            
          EXIT_ERR("Error storing historic data in SQLite database.\n");
        }
        free(hi.Records);
      }      
    }
    // Close database 
    sqlite3_close(db);
    // Close bluetooth connection (@@@ not closed wen exiting in error)
    pm->Close();
    // Success!
    return 0;
}
