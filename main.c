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
#include <pthread.h>
#include <curl.h>

FILE *logfile;
char ip[16];
char port[8];
char uid[8];

struct input_device {
  int handle;
  char name[256];
  char node[128];
  int buffer[16];
  int filled;
  struct input_event queue[8];
} devices[64];

int loadInputEventNodes() {
  int result = 0;
  int i = 0, j;
  FILE *eventFile;
  char line[128];
  size_t len = 0;
  ssize_t read;
  
  fprintf(logfile, "DEBUG: opening events source file\n");
  eventFile = fopen("events.txt", "r");
  if (eventFile == NULL) exit(EXIT_FAILURE);
  fprintf(logfile, "DEBUG: events source file opened\n");
  fprintf(logfile, "DEBUG: reading input events nodes from events source file\n");

  while ( fgets ( line, sizeof line, eventFile ) != NULL ) {
     strncpy(devices[i].node, line, strlen(line)-1);
     fprintf (logfile,"DEBUG: read address \"%s\" for device (%i)\n", devices[i].node, i);
     i++;
  }
  
  for (j = 0; j < i; j++) {
    devices[j].handle = -1;
    fprintf(logfile,"DEBUG: opening device %s\n",devices[j].node);
    devices[j].handle = open(devices[j].node, O_RDONLY);
    if (devices[j].handle == -1) {
        fprintf(logfile,"ERROR: Failed to open event device %s.\n", devices[j].node);
        perror("Failed to open event device");
        printf("errno = %d.\n", errno);
        exit(1);
    }
    result = ioctl(devices[j].handle, EVIOCGNAME(sizeof(devices[j].name)), devices[j].handle);
    fprintf(logfile,"DEBUG: reading from %s (%s)\n", devices[j].node, devices[j].name);
    fprintf(logfile,"DEBUG: getting exclusive access: ");
    result = ioctl(devices[j].handle, EVIOCGRAB, 1);
    fprintf(logfile,"%s\n", (result == 0) ? "SUCCESS" : "FAILURE");
    devices[j].filled = 0;
  }
  fprintf(logfile,"DEBUG: opened %i devices\n",i);
  return i;
}

int end() {
  int result = 0;
  printf("Exiting.\n");
  result = ioctl(devices[0].handle, EVIOCGRAB, 1);
  close(devices[0].handle);
}

char toChar(int key) {
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
    case 69:  return 'a'; // NumLock Key
    case 48:  return 'b';
    case 117: return 'b'; // NumLock '='
    case 31:  return 'd'; // 's' key for schwierigkeit
    case 32:  return 'd'; // 'd' key for difficulty
    case 98:  return 'd'; // NumLock '/'
    case 24:  return 'o';
    case 55:  return 'o'; // NumLock '*'
    case 27:  return '+'; // '+'
    case 78:  return '+'; // NumLock '+'
  }
}

void *thread(void *arg)
 {
   CURL *curl;
   int j, rd, value, res;
   int size = sizeof(struct input_event);
   int i = *((int*)arg);
   char out[12];
   char post[256];
   char post_address[256];
   memset(out, '\0', sizeof(out));
   memset(post, '\0', sizeof(post));
   
   curl = curl_easy_init();
   
   fprintf (logfile,"DEBUG: THREAD[%i]: starting event loop for device %i (%s)\n", i, i, devices[i].node);
   while (1)
   {
     if ((rd = read(devices[i].handle, devices[i].queue, size * 64)) < size) {
         break;
     }

     value = devices[i].queue[0].value;

     if (value != ' ' && devices[i].queue[1].value == 1 && devices[i].queue[1].type == 1) {
         fprintf (logfile,"DEBUG: Code[%d] read from %s\n", devices[i].queue[1].code, devices[i].node);
         printf ("DEBUG: Code[%d] read from %s\n", devices[i].queue[1].code, devices[i].node);
         // ESC
         if (devices[i].queue[1].code == 1) {
           end(devices);
           exit(0);
         }
         // ENTER
         if (devices[i].queue[1].code == 28 ||
             devices[i].queue[1].code == 96) {

           for (j = 0; j < devices[i].filled; j++) {
             out[j] = toChar(devices[i].buffer[j]);
           }
           fprintf(logfile, "INFO: device %s has emitted %i characters %s \n",devices[i].node, devices[i].filled,out);
           
           //is it a setup?
           if (out[0] == '+') {
             sprintf(post_address,"http://%s:%s/setDevice/%c",ip,port,out[1]);
           } else {
             sprintf(post_address,"http://%s:%s/scoreInput",ip,port);
           }
           
           curl_easy_setopt(curl, CURLOPT_URL, post_address);
           sprintf(post,"device=%s%i&score=%s",uid, i,out);
           curl_easy_setopt(curl,  CURLOPT_COPYPOSTFIELDS, post);
           res = curl_easy_perform(curl);
           /* Check for errors */ 
           if(res != CURLE_OK) {
             fprintf(stderr, "curl_easy_perform() failed: %s\n",
                     curl_easy_strerror(res));
           }
           //cleanup
           devices[i].filled = 0;
           memset(out, '\0', sizeof(out));
           memset(post, '\0', sizeof(post));
         }
         // Write valid characters to buffer
         //   - numbers from 0-9
         //   - respective numpad codes ()
         //   - seperators , .
         //   - score letters a, b, s, d, o (k is ignored)
         //   - first line of numblock assigned to scores ()
         //   - '+' for score setup
         if ((devices[i].queue[1].code >= 2 && devices[i].queue[1].code <=11)  ||
           
             (devices[i].queue[1].code >= 71 && devices[i].queue[1].code <=73) ||
             (devices[i].queue[1].code >= 75 && devices[i].queue[1].code <=77) ||
             (devices[i].queue[1].code >= 79 && devices[i].queue[1].code <=81) ||
               
             devices[i].queue[1].code == 51 || devices[i].queue[1].code == 52  || 
             devices[i].queue[1].code == 83 ||
               
             devices[i].queue[1].code == 24 || devices[i].queue[1].code == 30  ||
             devices[i].queue[1].code == 31 || devices[i].queue[1].code == 32  ||
             devices[i].queue[1].code == 48  ||
             
             devices[i].queue[1].code == 55 || devices[i].queue[1].code == 69  ||
             devices[i].queue[1].code == 98 || devices[i].queue[1].code == 117 ||
             
             devices[i].queue[1].code == 27 || devices[i].queue[1].code == 78) {
           devices[i].buffer[devices[i].filled] = devices[i].queue[1].code;
           devices[i].filled++;
         }
      }
      
      sleep(0);
   }
   return(0);
 }

void printHelp() {
  printf("AeroChimp Score Input Server\n\n");
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

    logfile = fopen("logfile.txt","w");
    if (logfile < 0) {
      printf("Could not initialize logfile");
      exit(-2);
    }
    fprintf(logfile, "DEBUG: logfile opened\n");
    
    if (argc < 4 ) {
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
    
    nodeCount = loadInputEventNodes(devices);
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    for (i=0; i < nodeCount && i < 64; i++) {
      fprintf(logfile,"DEBUG: starting thread %i of %i listening on %s\n", i+1, nodeCount, devices[i].node);
      devNum = (int *) malloc(sizeof(int));
      *devNum = i;
      pthread_create(&threads[i], NULL, thread, (void*)devNum);
    }
    
    for (i=0; i < nodeCount && i < 64; i++) {
      pthread_join(threads[i], NULL);
    }
    
    curl_global_cleanup();
    end(devices);
    fclose(logfile);
    return 0;
}