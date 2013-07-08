#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h> 
#include <fcntl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <curl/curl.h>
#include "ProtocolManager.h"
#include "sma_pvoutput.h"

#define EXIT_ERR(a)  { printf(a); if (pm != NULL) { pm->Close(); }; return -1; }

// Function that ignores any curl data
size_t ignore_curl_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
}

// Main function
int main(int argc, char **argv)
{
    
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
      EXIT_ERR("Error connection to SMA inverter\n");      
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
    // Get timestamp round down to 5 minutes
    uint32_t from_timestamp = ((uint32_t)(yi.TimeStamp / 300)) * 300;
    // Get the two lastest 5 minute historic data entries 
    HistoricInfo hi;    
    if (pm->GetHistoricYield(from_timestamp - 501, from_timestamp + 1, hi, false) != 0)
    {
      EXIT_ERR("Error reading 5 minute yield data.\n");
    }
    // We selected the latest value 2 values, so we should have 1 record
    if (hi.NoRecords > 1)
    {
      // Convert timestamp to local time
      struct tm* ti = localtime((time_t *) &hi.Records[0].TimeStamp);
      // Generate curl query
      char curl_query[1024];
      sprintf(curl_query,
              "http://pvoutput.org/service/r2/addstatus.jsp?d=%04d%02d%02d&t=%02d:%02d&v1=%d&c1=0&sid=%s&key=%s", 
              1900 + ti->tm_year, 
              ti->tm_mon+1, 
              ti->tm_mday, 
              ti->tm_hour, 
              ti->tm_min, 
              hi.Records[1].Value - hi.Records[0].Value,
              options.SystemID,
              options.APIKey);
#ifdef __DEBUG__              
printf(curl_query);
printf("\n");
#endif
      // Make curl request
      curl_global_init(CURL_GLOBAL_DEFAULT);
      CURL *curl = curl_easy_init();
      if (curl == NULL)
      {
        curl_global_cleanup();
        EXIT_ERR("Error opening CURL session.\n");
      }
      curl_easy_setopt(curl, CURLOPT_URL, curl_query);
      curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ignore_curl_data);
      int result = curl_easy_perform(curl);
	    curl_easy_cleanup(curl);
      curl_global_cleanup();
      if (result != 0)
      {
        printf("curl result: %d\n", result);
        EXIT_ERR("Error writing data to pvoutput.org. Wrong sid, api-key?\n");
      }              
    }    
    free(hi.Records);          
    // Close bluetooth connection (@@@ not closed wen exiting in error)
    pm->Close();
    // Success!
    return 0;
}
