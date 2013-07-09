#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h> 
#include <fcntl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <curl/curl.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <math.h>
#include "ProtocolManager.h"
#include "sma_pvoutput.h"

using namespace std;

#define EXIT_ERR(a)       { printf(a); if (pm != NULL) { pm->Close(); }; return -1; }
#define GETSTATUS         "http://pvoutput.org/service/r2/getstatus.jsp"
#define ADDBATCHSTATUS    "http://pvoutput.org/service/r2/addbatchstatus.jsp"

// Function that accumulates data in given string
size_t store_curl_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
  if (userp != NULL)
  {
    string *str = (string *) userp;
    str->append((char *) buffer, size*nmemb); 
  }
  return size*nmemb;
}

// Get timestamp of latest addition to PVOutput.org
time_t GetTimeStamp(CURL *curl, Options *options)
{
  string output;
  ostringstream query;
  long response_code;
  // Create query for latest addition  
  query << GETSTATUS << "?h=1&limit=1&sid=" << options->SystemID << "&key=" << options->APIKey;
  // Get data
  curl_easy_reset(curl);
  curl_easy_setopt(curl, CURLOPT_URL, query.str().c_str());
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, store_curl_data); 
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
  int result = curl_easy_perform(curl);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
  if (result != 0 || (response_code != 200 && response_code != 400))
  { // Error with query
    printf("%s\n", output.c_str());
    return -1;
  }
  // Check for 'No status found': first upload
  if (output.find("No status found") != string::npos)
  { // First upload, not an error. Return a very historic data.
    return 0;
  }
  // Get time info
  struct tm time;
  if (sscanf(output.c_str(), "%4d%2d%2d,%2d:%2d", &time.tm_year, &time.tm_mon, &time.tm_mday, &time.tm_hour, &time.tm_min) != 5)  
  { // Error interpreting answer
    printf("%s\n", output.c_str());  
    return -2;
  }
  // Adjust and return timestamp
  time.tm_year -= 1900;
  time.tm_mon--;
  time.tm_sec = 0;   
  return mktime(&time);
}


// Main function
int main(int argc, char **argv)
{
    CURL *curl;
    Options options;
    ProtocolManager *pm;    
    // Read options    
    if (options.Initialize(argc, argv) < 0)
    {
      return -1;
    }        
    // Start protocol manager    
    pm = new ProtocolManager();    
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
    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl == NULL)
    {
      curl_global_cleanup();
      EXIT_ERR("Error opening CURL session.\n");
    }         
    // Get current totals AND SMA time
    YieldInfo yi;
    if (pm->GetYieldInfo(yi))
    {
      EXIT_ERR("Error getting current totals\n");
    }
    // Get timestamp round down to 5 minutes -> latest available timestamp
    time_t current_time = ((uint32_t)(yi.TimeStamp / 300)) * 300;    
    // Get timestamp of latest upload to pvoutput
    time_t latest_upload = GetTimeStamp(curl, &options);
    if (latest_upload < 0)
    {
      EXIT_ERR("Error getting timestamp of latest live upload from PVOutput.org.\n");
    }
    // Upload at most from DaysMaximum days in the past
    time_t from_upload = ((current_time - (options.DaysMaximum * 3600 * 24)) > latest_upload) ? (current_time - (options.DaysMaximum * 3600 * 24)) : latest_upload;         
    // Get all historic data from 'from_upload' to 'latest_upload' 
    HistoricInfo hi;    
    if (pm->GetHistoricYield(from_upload - 1, current_time + 1, hi, false) != 0)
    {
      EXIT_ERR("Error reading 5 minute yield data.\n");
    }
#ifdef __DEBUG__    
printf( "%d records retrieved\n", hi.NoRecords);
#endif    
    // At least 2 records needed: we sent kWh produced in a 5 minute interval. Got issues with post (system id errors), using get instead
    if (hi.NoRecords > 1)
    {
      ostringstream get_query;
      // Add URL
      get_query << ADDBATCHSTATUS;
      // Add API key and System ID to get_query
      get_query << "?sid=" << options.SystemID << "&key=" << options.APIKey;
      // Add data part (cumulative energy)
      get_query << "&c1=1&data=" << setfill('0');
      for (int i = 1; i < hi.NoRecords && i <= options.BatchMaximum; i++)
      {
        // Convert timestamp to local time, add to get_query
        struct tm* ti = localtime((time_t *) &hi.Records[i].TimeStamp);
        get_query << setw(4) << (1900+ti->tm_year) << setw(2) << (1+ti->tm_mon) << setw(2) << ti->tm_mday << ',';
        get_query << setw(2) << ti->tm_hour << ':' << setw(2) << ti->tm_min << ',';
        // Add Wh produced to get_query
        time_t dt = hi.Records[i].TimeStamp - hi.Records[i-1].TimeStamp;
        int Wh = hi.Records[i].Value - hi.Records[i-1].Value;
        int power = (int) round((double) Wh * 3600.0/(double)dt);
        get_query << hi.Records[i].Value << ',' << power << ';';
      }       
#ifdef __DEBUG__
printf("%s\n", get_query.str().c_str());
#endif
      // Post query, cleanup, and CLOSE CURL 
      string post_result;
      long response_code;
      curl_easy_reset(curl);
      curl_easy_setopt(curl, CURLOPT_URL, get_query.str().c_str());
      curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, store_curl_data); 
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &post_result);      
      int result = curl_easy_perform(curl);      
	    curl_easy_cleanup(curl);      
      curl_global_cleanup();
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
      if (result != 0 || response_code != 200)
      { // Post went wrong: show message
        printf("%s\n", post_result.c_str());
        EXIT_ERR("Error writing data to pvoutput.org. Wrong sid, api-key?\n");
      }
      if ((hi.NoRecords-1) > options.BatchMaximum)
      {
        printf("Could not upload all data due to batch limits; %d intervals remaining.\n", (hi.NoRecords-1) - options.BatchMaximum);
      }
    }    
    free(hi.Records);          
    // Close bluetooth connection (@@@ not closed wen exiting in error)
    pm->Close();
    // Success!
    return 0;
}
