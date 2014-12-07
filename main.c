/* Copyright (C) Jonas Schwartze, Inc - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jonas Schwartze <jschwartze@me.com>, December 2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <signal.h>
#include <curl.h>
#include <uuid/uuid.h>

#ifndef __SEND_BUFFER__
  #define __SEND_BUFFER__ 16
#endif
#ifndef __VERSION__
  #define __VERSION__ "v1.0.0"
#endif

FILE *logfile;
char ip[16];
char port[8];
char uid[8];

struct input_device {
  int handle;
  char name[256];
  char node[128];
  int filled;
  struct input_event queue[8];
  int buffer[__SEND_BUFFER__];
  uuid_t uid;
} device;

char charOf(int key) {
  switch (key) {
    case 2:   return '1';
    case 79:  return '1'; // NumLock
    case 3:   return '2';
    case 80:  return '2'; // NumLock
    case 4:   return '3';
    case 81:  return '3'; // NumLock
    case 5:   return '4';
    case 75:  return '4'; // NumLock
    case 6:   return '5';
    case 76:  return '5'; // NumLock
    case 7:   return '6';
    case 77:  return '6'; // NumLock
    case 8:   return '7';
    case 71:  return '7'; // NumLock
    case 9:   return '8';
    case 72:  return '8'; // NumLock
    case 10:  return '9';
    case 73:  return '9'; // NumLock
    case 11:  return '0';
    case 82:  return '0'; // NumLock
    case 51:  return '.'; // ','
    case 52:  return '.'; // '.'
    case 83:  return '.'; // NumLock ','
    case 30:  return 'a';
    case 98:  return 'a'; // NumLock '/'
    case 48:  return 'b'; // 'b' key for German ausfürhrung
    case 18:  return 'b'; // 'e' key for execution
    case 55:  return 'b'; // NumLock '*'
    case 31:  return 'd'; // 's' key for schwierigkeit
    case 32:  return 'd'; // 'd' key for difficulty
    case 74:  return 'd'; // NumLock '-'
    case 24:  return 'o';
    case 78:  return 'o'; // NumLock '+'
  }
}

int main_loop()
 {
   CURL *curl;
   int j, rd, value, res;
   int rvalue = 0;
   int size = sizeof(struct input_event);
   char out[12];
   char post[256];
   char post_address[256];
   memset(out, '\0', sizeof(out));
   memset(post, '\0', sizeof(post));
   
   curl = curl_easy_init();
   
   fprintf (logfile,"DEBUG: starting event loop for device (%s)\n", device.node);
   fflush(logfile);

   while (1)
   {
     if ((rd = read(device.handle, device.queue, size * 64)) < size) {  
       printf ("ERROR: device (%s) disconnected \n", device.node);
       fprintf (logfile,"ERROR: device (%s) disconnected \n", device.node);
	   fflush(logfile);
       rvalue = -1;
       break;
     }

     value = device.queue[0].value;
      /*printf("DEBUG: 0: C[%d] T[%d] V[%d]; 1: Code[%d] Type[%d] V[%d]; 2: Code[%d] Type[%d] V[%d]; 3: Code[%d] Type[%d] V[%d]; 4: Code[%d] Type[%d] V[%d];\n",
      device.queue[0].code, device.queue[0].type, device.queue[0].value,
      device.queue[1].code, device.queue[1].type, device.queue[1].value,
      device.queue[2].code, device.queue[2].type, device.queue[2].value,
      device.queue[3].code, device.queue[3].type, device.queue[3].value,
      device.queue[4].code, device.queue[4].type, device.queue[4].value);*/
     int idxOfInterest = 1;
     if (device.queue[1].code == 69 && device.queue[3].code != 69 && 
         device.queue[1].type == 1 && device.queue[1].value == 1 && 
         device.queue[3].type == 1 && device.queue[3].value == 1) idxOfInterest = 3;

     // Check for a valid input
     if (value != ' ' && device.queue[idxOfInterest].value == 1 && device.queue[idxOfInterest].type == 1) {
         fprintf (logfile,"DEBUG: Code[%d] read from %s\n", device.queue[idxOfInterest].code, device.node);
         printf ("DEBUG: Code[%d] read from %s\n", device.queue[idxOfInterest].code, device.node);
         // ESC
         if (device.queue[idxOfInterest].code == 1) {
             rvalue = 0;
             break;
         }
         // ENTER
         if (device.queue[idxOfInterest].code == 28 ||
             device.queue[idxOfInterest].code == 96) {

           for (j = 0; j < device.filled; j++) {
             out[j] = charOf(device.buffer[j]);
           }
           fprintf(logfile, "INFO: device %s has emitted %i characters \"%s\" \n",device.node, device.filled,out);
		   fflush(logfile);

           
           //is it a setup?
           if (out[0] == '.' && out[1] == '.' && out[2] == '.') {
             sprintf(post_address,"http://%s:%s/setDevice/%c",ip,port,out[3]);
             sprintf(out,"%c%c", out[4], out[5]);
             
           } else {
             sprintf(post_address,"http://%s:%s/scoreInput",ip,port);
           }
           //send it 
           curl_easy_setopt(curl, CURLOPT_URL, post_address);
           sprintf(post,"device=%s%s&score=%s",uid, device.uid,out);
           curl_easy_setopt(curl,  CURLOPT_COPYPOSTFIELDS, post);
           res = curl_easy_perform(curl);
           /* Check for errors */ 
           if(res != CURLE_OK) {
             fprintf(stderr, "curl_easy_perform() failed: %s\n",
                     curl_easy_strerror(res));	
           }
           //cleanup
           device.filled = 0;
           memset(out, '\0', sizeof(out));
           memset(post, '\0', sizeof(post));
         } //endif ENTER

         // Write valid characters to buffer
         //   - numbers from 0-9
         //   - respective numpad codes ()
         //   - seperators , .
         //   - score letters a, b, e, s, d, o (k is ignored)
         //   - first line of numblock assigned to scores ()
         //   - '+' for score setup
         if ((device.queue[idxOfInterest].code >= 2 && device.queue[idxOfInterest].code <=11)  ||
           
             (device.queue[idxOfInterest].code >= 71 && device.queue[idxOfInterest].code <=73) ||
             (device.queue[idxOfInterest].code >= 75 && device.queue[idxOfInterest].code <=77) ||
             (device.queue[idxOfInterest].code >= 79 && device.queue[idxOfInterest].code <=82) ||
               
             device.queue[idxOfInterest].code == 51 || device.queue[idxOfInterest].code == 52  || 
             device.queue[idxOfInterest].code == 83 ||
               
             device.queue[idxOfInterest].code == 24 || device.queue[idxOfInterest].code == 30  ||
             device.queue[idxOfInterest].code == 31 || device.queue[idxOfInterest].code == 32  ||
             device.queue[idxOfInterest].code == 48 || device.queue[idxOfInterest].code == 18  ||
             
             device.queue[idxOfInterest].code == 98 || device.queue[idxOfInterest].code == 55  ||
             device.queue[idxOfInterest].code == 74 || device.queue[idxOfInterest].code == 78 ) {
           device.buffer[device.filled] = device.queue[idxOfInterest].code;
           if (device.filled < 16 ) {
             device.filled++;
           } else {
             int k;
             for (k = 0; k < (__SEND_BUFFER__ - 1); k++) {
               device.buffer[k] = device.buffer[k+1];
             }
             device.buffer[__SEND_BUFFER__-1] = 0;
           }
         }
      }
   }
   fprintf (logfile,"DEBUG: ended event loop for device (%s)\n", device.node);
   fflush(logfile);

   return(rvalue);
 }

void printHelp() {
  printf("AeroChimp Score Input Server %s\n\n", __VERSION__);
  printf("Usage: inputserver IP PORT UID\n\n");
  printf("  IP    server to report score inputs to\n");
  printf("  PORT  port of server to report score inputs to\n");
  printf("  UID   unique ID (max. 8 chars) of the system to identify devices of multiple parallel systems\n");
  printf("\n");
  printf("Example:\n");
  printf("          inputserver 10.0.1.14 5000 PI1\n");
}

int main(int argc, char* argv[])
{
    pthread_t threads[64];
    int i, nodeCount, argi;
    int *devNum;
    int result = 0;
    int main_return = 0;

    logfile = fopen("logfile.txt","w");
    if (logfile < 0) {
      printf("Could not initialize logfile");
      exit(-2);
    }
    fprintf(logfile, "DEBUG: logfile opened\n");
    fflush(logfile);
    if (argc < 5 ) {
      printf("To few arguments");
      printHelp();
      exit(-1);
    }
    if (strlen(argv[1]) > strlen(argv[2])) {
      strcpy(ip, argv[1]);
      strcpy(port, argv[2]);
    } else {
      strcpy(port, argv[1]);
      strcpy(ip, argv[2]);
    }
    strcpy(uid, argv[3]);
    fprintf(logfile,"DEBUG: server ip is %s with port %s on system %s\n",ip,port, uid);
    fflush(logfile);
    
    //load device
    strncpy(device.node, argv[4], strlen(argv[4]));
    fprintf (logfile,"DEBUG: read address \"%s\" for device\n", device.node);
    fflush(logfile);
    device.handle = -1;
    fprintf(logfile,"DEBUG: opening device %s\n",device.node);
    device.handle = open(device.node, O_RDONLY);
    if (device.handle == -1) {
        fprintf(logfile,"ERROR: Failed to open event device %s.\n", device.node);
        printf("ERROR: Failed to open event device %s.\n", device.node);
        fflush(logfile);
        perror("Failed to open event device");
        printf("errno = %d.\n", errno);
        exit(1);
    }
    uuid_generate(device.uid);
    result = ioctl(device.handle, EVIOCGNAME(sizeof(device.name)), device.handle);
    fprintf(logfile,"DEBUG: reading from %s (%s)\n", device.node, device.name);
    fprintf(logfile,"DEBUG: getting exclusive access: ");
    result = ioctl(device.handle, EVIOCGRAB, 1);
    if (result == 0) {
        device.filled = 0;
        fprintf(logfile,"SUCCESS\n");
        fflush(logfile);
    } else {
        fprintf(logfile,"ERROR: Failed to get exclusive access of device %s.\n", device.node);
        printf("ERROR: Failed to get exclusive access of device %s.\n", device.node);
        fflush(logfile);
        perror("Failed to get exclusive access on device");
        printf("errno = %d.\n", errno);
        exit(errno);
    }

    curl_global_init(CURL_GLOBAL_ALL);
    
    //starting main loop
    
    main_return = main_loop();
    
    curl_global_cleanup();
    
    result = ioctl(device.handle, EVIOCGRAB, 1);
    close(device.handle);
    if (main_return == 0) {
      fprintf(logfile, "INFO: application terminated normally.\n");
      printf("INFO: application terminated normally.\n");
    } else {
      fprintf(logfile, "INFO: application terminated with errors.\n");
      printf("INFO: application terminated with errors.\n");
    }
    fclose(logfile);
    return main_return;
}
