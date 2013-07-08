#include <stdio.h>
#include <unistd.h>

// List with long options that we accept
static struct option long_options[] =
     {
       /* These options set a flag. */
       {"help",     no_argument,       0, '?'},
       {"MAC",      required_argument, 0, 'M'},
       {"password", required_argument, 0, 'p'},
       {"api_key",  required_argument, 0, 'a'},
       {"sid",      required_argument, 0, 's'},
       {0, 0, 0, 0}
     };

// Class to process and store options
class Options
{
  public:  
  char MAC[18];
  uint8_t Password[13]; 
  char APIKey[1024];
  char SystemID[1024];
  
  int Initialize(int argc, char **argv)
  {
    // Clear values
    memset(this, 0, sizeof(Options));
    // Process arguments
    while (true)
    {
      int option_index = 0;
      int c = getopt_long (argc, argv, "d5M:p:", long_options, &option_index);
      // Last option?    
      if (c == -1) break;
     
      switch (c)
      {
        case 'M':
          if (strlen(optarg) != 17)
          {
            printf("MAC address is invalid, 01:23:45:67:89:ab format expected.\n");
            return -1;
          }
          strcpy(MAC, optarg);
          break;
        case 'p':
            if (strlen(optarg) > 12)
            {
              printf("Password is more than 12 characters.\n");
              return -1;
            }
            strcpy((char *) Password, optarg);
        break;
        case 'a':
            if (strlen(optarg) > sizeof(APIKey)-1)
            {
              printf("API key is more than 1 kB.\n");
              return -1;
            }
            strcpy(APIKey, optarg);
        break;
        case 's':
            if (strlen(optarg) > sizeof(SystemID)-1)
            {
              printf("System ID is more than 1 kB.\n");
              return -1;
            }
            strcpy(SystemID, optarg);
        break;           
        case '?':
            printf("Usage:\n--MAC MAC address of SMA inverter\n--password Password\n--api_key API key set in pvoutput settings\n--sid System ID as known by pvoutput\n");
            return -1;
        break;
      }
    }
    
    // Check for required arguments
    if (MAC[0] == 0 || Password[0] == 0 || SystemID[0] == 0 || APIKey[0] == 0)
    {
      printf("Password (--password), MAC address (--MAC), API key (--api_key), and/or system id (--id) missing!\n");
      return -1;
    }
    // Success
    return 0;
  }  
};
