#include <stdio.h>
#include <unistd.h>

// List with long options that we accept
static struct option long_options[] =
     {
       /* These options set a flag. */
       {"help",     no_argument,       0, '?'},
       {"daily",    no_argument,       0, 'd'},
       {"5minute",  no_argument,       0, '5'},
       {"MAC",      required_argument, 0, 'M'},
       {"password", required_argument, 0, 'p'},
       {"sqlite",   required_argument, 0, 's'},
       {0, 0, 0, 0}
     };

// Class to process and store options
class Options
{
  public:  
  bool DailyYield;
  bool Minute5Yield;
  char MAC[18];
  uint8_t Password[13]; 
  char Database[1024];
  
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
        case 'd': DailyYield = true; break;
        case '5': Minute5Yield = true; break;                    
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
        case 's':
            if (strlen(optarg) > sizeof(Database)-1)
            {
              printf("Path to database file is more than 1 kB.\n");
              return -1;
            }
            strcpy(Database, optarg);
        break;      
        case '?':
            printf("Usage:\n--MAC MAC address of SMA inverter\n--password Password\n--sqlite Filename in which the sqlite database will be residing\n--daily Get daily yields\n--5minute Get 5 minute yields\n");
            return -1;
        break;
      }
    }
    
    // Check for required arguments
    if (MAC[0] == 0 || Password[0] == 0 || Database[0] == 0)
    {
      printf("Password (--password), MAC address (--MAC), and/or SQLite database (--sqlite) missing!\n");
      return -1;
    }
    // Success
    return 0;
  }  
};
