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
#include <syslog.h>
#include <termios.h>
#include <signal.h>
#include <curl.h>
#include <uuid/uuid.h>

#ifndef __OUT_BUFFER_SIZE__
	#define __OUT_BUFFER_SIZE__ 32
#endif
#ifndef __VERSION__
	#define __VERSION__ "v1.2.3"
#endif
#ifndef __EVENT_QUEUE_SIZE__
	#define __EVENT_QUEUE_SIZE__ 8
#endif
#ifndef __POST_BUFFER_SIZE__
	#define __POST_BUFFER_SIZE__ 256
#endif
#ifndef __POST_ADDRESS_SIZE__
	#define __POST_ADDRESS_SIZE__ 256
#endif
#ifndef __IP_ADDRESS_LENGTH__
	#define __IP_ADDRESS_LENGTH__ 16
#endif
#ifndef __PORT_LENGTH__
	#define __PORT_LENGTH__ 8
#endif
#ifndef __PI_ID_LENGTH__
	#define __PI_ID_LENGTH__ 8
#endif
#ifndef __DEVICE_NAME_LENGTH__
	#define __DEVICE_NAME_LENGTH__ 1024
#endif
#ifndef __DEVICE_NODE_LENGTH__
	#define __DEVICE_NODE_LENGTH__ 1024
#endif
#ifndef __DEVICE_UID_LENGTH__
#define __DEVICE_UID_LENGTH__ 1024
#endif


FILE *logfile;
char ip[__IP_ADDRESS_LENGTH__];
char port[__PORT_LENGTH__];
char uid[__PI_ID_LENGTH__];

struct input_device {
	int handle;
	char name[__DEVICE_NAME_LENGTH__];
	char node[__DEVICE_NODE_LENGTH__];
	int filled;
	struct input_event queue[__EVENT_QUEUE_SIZE__];
	int buffer[__OUT_BUFFER_SIZE__];
	char uid[__DEVICE_UID_LENGTH__];
} device;

#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t size) {
    char *d = dst;
    const char *s = src;
    size_t n = size;

    /* Copy as many bytes as will fit */
    if (n != 0) {
        while (--n != 0) {
            if ((*d++ = *s++) == '\0') {
                break;
            }
        }
    }

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0) {
        if (size != 0) {
            *d = '\0'; /* NUL-terminate dst */
        }

        while (*s++) {
        }
    }

    return (s - src - 1); /* count does not include NUL */
}
#endif


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
	char out[__OUT_BUFFER_SIZE__];
	char post[__POST_BUFFER_SIZE__];
	char post_address[__POST_ADDRESS_SIZE__];
	//clear out the buffers (char size * buffer size with NULL-Character)
	memset( out,  '\0', sizeof(char)*__OUT_BUFFER_SIZE__ );
	memset( post, '\0', sizeof(char)*__POST_BUFFER_SIZE__ );
	memset( post_address, '\0', sizeof(char)*__POST_ADDRESS_SIZE__ );

	curl = curl_easy_init();

	syslog (LOG_DEBUG,"DEBUG: starting event loop for device (%s)\n", device.node);

	snprintf(post_address, __POST_ADDRESS_SIZE__ - 1, "http://%s:%s/device", ip, port);
	curl_easy_setopt(curl, CURLOPT_URL, post_address);
	snprintf(post, __POST_BUFFER_SIZE__ - 1, "device=%s_%s", uid, device.uid);
	curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, post);
	res = curl_easy_perform(curl);
	// Check and report errors
	if(res != CURLE_OK) {
		syslog(LOG_ERR, "curl_easy_perform() failed: %s\n",
		curl_easy_strerror(res));
		return -1;
	}
	memset( post, '\0', sizeof(char)*__POST_BUFFER_SIZE__ );
	memset( post_address, '\0', sizeof(char)*__POST_ADDRESS_SIZE__ );

	while (1)
	{
		if ((rd = read(device.handle, device.queue, size * 64)) < size) {  
			syslog (LOG_ERR,"ERROR: device (%s) disconnected \n", device.node);
			rvalue = -1;
			break;
		}

		// Find correct event in queue
		//    Some devices send multiple events.
		//    We are only interested in keypresses (type = 1).
		//      see https://www.kernel.org/doc/Documentation/input/input.txt
		//      for more information on input_event struct.
		//    Uncomment the following printf to debug new devices:
		/*			printf("DEBUG: 0: C[%d] T[%d] V[%d]; 1: Code[%d] Type[%d] V[%d]; 2: Code[%d] Type[%d] V[%d]; 3: Code[%d] Type[%d] V[%d]; 4: Code[%d] Type[%d] V[%d];\n",
						device.queue[0].code, device.queue[0].type, device.queue[0].value,
						device.queue[1].code, device.queue[1].type, device.queue[1].value,
						device.queue[2].code, device.queue[2].type, device.queue[2].value,
						device.queue[3].code, device.queue[3].type, device.queue[3].value,
						device.queue[4].code, device.queue[4].type, device.queue[4].value);
			*/
		int idxOfInterest = 1; //assuming the second event to be the event of index
		if (device.queue[1].code == 69 && device.queue[3].code != 69 && 
				device.queue[1].type == 1 && device.queue[1].value == 1 && 
				device.queue[3].type == 1 && device.queue[3].value == 1) 
		{
			idxOfInterest = 3; //some devices send NUM_LOCK prior to the actual key
		}

		// Check for a valid input
		//   type and value are a key event (which is 1 (EV_KEY) from linux/input.h)
		if (device.queue[idxOfInterest].value == 1 &&
				device.queue[idxOfInterest].type == 1 )
		{
			syslog(LOG_DEBUG,"DEBUG: Code[%d] read from %s\n", device.queue[idxOfInterest].code, device.node);
			// Now we can check which key has been pressed
			// ESC key pressed (KEY_ESC = 1)
			if (device.queue[idxOfInterest].code == KEY_ESC) {
				rvalue = 0;
				break;
			}
			// ENTER key pressed (KEY_ENTER = 28, KEY_KPENTER = 96)
			// This means we should send the input buffer to the server
			if (device.queue[idxOfInterest].code == KEY_ENTER ||
				device.queue[idxOfInterest].code == KEY_KPENTER ) 
			{
				// Translate key events stored in device buffer to chars
				for (j = 0; j < device.filled && j < __OUT_BUFFER_SIZE__; j++) {
					out[j] = charOf(device.buffer[j]);
				}
				syslog(LOG_INFO, "INFO: device %s has emitted %i characters \"%s\" \n",device.node, device.filled,out);
				
				// Determine request type and construct curl address
				//   If output starts with '...' it is a setup.
				//   else it's a regular request (scoreInput).
				if (out[0] == '.' && out[1] == '.' && out[2] == '.') {
					// REST signature of setup requests are
					//   /setDevice/[panel_number]
					snprintf(post_address, __POST_ADDRESS_SIZE__ - 1, "http://%s:%s/setDevice/%c", ip, port, out[3]);
					// remove '...'
					snprintf(out, __OUT_BUFFER_SIZE__ - 1, "%c%c", out[4], out[5]);
				} else {
					// scoreInputs (regular request) are made to
					//   /scoreInput
					snprintf(post_address, __POST_ADDRESS_SIZE__ - 1, "http://%s:%s/scoreInput", ip, port);
				}
				// finally send the request consisting of address and post field
				curl_easy_setopt(curl, CURLOPT_URL, post_address);
				snprintf(post, __POST_BUFFER_SIZE__ - 1, "device=%s_%s&score=%s", uid, device.uid, out);
				curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, post);
				res = curl_easy_perform(curl);
				// Check and report errors
				if(res != CURLE_OK) {
					syslog(LOG_ERR, "curl_easy_perform() failed: %s\n",
					curl_easy_strerror(res));
				}
				//cleanup used variables and buffers
				device.filled = 0;
				memset( out,  '\0', sizeof(char)*__OUT_BUFFER_SIZE__ );
				memset( post, '\0', sizeof(char)*__POST_BUFFER_SIZE__ );
				memset( post_address, '\0', sizeof(char)*__POST_ADDRESS_SIZE__ );
			 } //endif ENTER

			// Any other valid key pressed.
			// Only write valid characters to buffer
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
					device.queue[idxOfInterest].code == 74 || device.queue[idxOfInterest].code == 78 ) 
			{
				// copy key code to device input buffer
				device.buffer[device.filled] = device.queue[idxOfInterest].code;
				// rotate input buffer to prevent buffer overflows and save memory
				// device.filled carries next position in input buffer
				// if input.buffer is full (or would be full after increasing)
				if ( device.filled < ( __OUT_BUFFER_SIZE__ - 1 ) ) {
					device.filled++;
				} else {
					int k;
					for (k = 0; k < ( __OUT_BUFFER_SIZE__ - 1 ); k++) {
						device.buffer[k] = device.buffer[k+1];
					}
					// clean last buffer position to be filled in next iteration
					device.buffer[ __OUT_BUFFER_SIZE__ - 1 ] = 0;
				}
			}
		}
	}
	syslog(LOG_DEBUG,"DEBUG: ended event loop for device (%s)\n", device.node);

	//Send DELETE request to server to set device offline
	snprintf(post_address, __POST_ADDRESS_SIZE__ - 1, "http://%s:%s/device/%s_%s", ip, port,uid, device.uid);
	curl_easy_setopt(curl, CURLOPT_URL, post_address);
	curl_easy_setopt(curl,CURLOPT_CUSTOMREQUEST,"DELETE");
	res = curl_easy_perform(curl);
	// Check and report errors
	if(res != CURLE_OK) {
		syslog(LOG_ERR, "curl_easy_perform() failed: %s\n",
		curl_easy_strerror(res));
	}
	return(rvalue);
}

void printHelp() {
	printf("AeroChimp Score Input Server %s\n\n", __VERSION__);
	printf("Usage: inputserver IP PORT UID PATH DEVID\n\n");
	printf("  IP      server to report score inputs to\n");
	printf("  PORT    port of server to report score inputs to\n");
	printf("  UID     unique ID (max. 8 chars) of the system to identify devices of multiple parallel systems\n");
	printf("  PATH    fully qualified path of the input device node\n");
	printf("  DEVID   unique ID of the input device to identify devices of multiple parallel systems\n");
	printf("\n");
	printf("Example:\n");
	printf("          inputserver 10.0.1.14 5000 PI1 /dev/input/event0 1-1.1-1.3.1:1-0\n");
}

int main(int argc, char* argv[])
{
		pthread_t threads[64];
		int i, nodeCount, argi, copied;
		int *devNum;
		int result = 0;
		int main_return = 0;

		setlogmask (LOG_UPTO (LOG_DEBUG));
		openlog ("inputserver", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);
		if (argc < 6 ) {
			syslog(LOG_ERR,"To few arguments");
			//printHelp();
			exit(-1);
		}

		// Read basic input parameter from command line
		//
		// 1. clean buffers to hold input parameter values
		memset( ip, '\0', sizeof(char)*__IP_ADDRESS_LENGTH__ );
		memset( port,  '\0', sizeof(char)*__PORT_LENGTH__ );
		memset( uid,  '\0', sizeof(char)*__PI_ID_LENGTH__ );
		// 2. guess input parameter order (assuming ip is longer port)
		if (strlen(argv[1]) > strlen(argv[2])) {
			// 3. securely copy IP and port values to buffers
			if ( strlcpy(ip, argv[1], sizeof(char)*__IP_ADDRESS_LENGTH__ ) >= sizeof(char)*__IP_ADDRESS_LENGTH__ ||
					 strlcpy(port, argv[2], sizeof(char)*__PORT_LENGTH__ ) >= sizeof(char)*__PORT_LENGTH__ )
			{
				syslog(LOG_ERR,"ERROR: IP address or port given are to long for input buffers (%s, %s).\n", argv[1], argv[2]);
				perror("IP address or port given are to long");
				return (ENAMETOOLONG);
			}
		} else {
			// 3. securely copy port and IP values to buffers
			if ( strlcpy(ip, argv[2], sizeof(char)*__IP_ADDRESS_LENGTH__ ) >= sizeof(char)*__IP_ADDRESS_LENGTH__ ||
					 strlcpy(port, argv[1], sizeof(char)*__PORT_LENGTH__ ) >= sizeof(char)*__PORT_LENGTH__ )
			{
				syslog(LOG_ERR,"ERROR: IP address or port given are to long for input buffers (%s, %s).\n", argv[2], argv[1]);
				perror("IP address or port given are to long");
				return (ENAMETOOLONG);
			}
		}
		// 4. securely copy PI_ID value to buffer
		if ( strlcpy(uid, argv[3], sizeof(char)*__PI_ID_LENGTH__) >= sizeof(char)*__PI_ID_LENGTH__ )
		{
			syslog(LOG_ERR,"ERROR: PI_ID given is to long for input buffers (%s).\n", argv[3]);
			perror("PI_ID given is to long");
			return (ENAMETOOLONG);
		}
		syslog (LOG_INFO, "server is running with ip %s and port %s on system %s", ip, port, uid);



		// Load device to connect to and grep input from
		//
		// 1. securely copy given device node (=path in /dev) to device struct buffer
		if ( strlcpy(device.node, argv[4], sizeof(char)*__DEVICE_NODE_LENGTH__) >= sizeof(char)*__DEVICE_NODE_LENGTH__ )
		{
			syslog(LOG_ERR,"ERROR: device node given is to long for input buffers (%s).\n", argv[4]);
			perror("device node given is to long");
			return (ENAMETOOLONG);
		}
		// 2. securely copy given device uid (usb bus topology) to device struct buffer
		if ( strlcpy(device.uid, argv[5], sizeof(char)*__DEVICE_UID_LENGTH__) >= sizeof(char)*__DEVICE_UID_LENGTH__ )
		{
			syslog(LOG_ERR,"ERROR: device uid given is to long for input buffers (%s).\n", argv[5]);
			perror("device uid given is to long");
			return (ENAMETOOLONG);
		}
		syslog(LOG_DEBUG,"DEBUG: read address \"%s\" for device\n", device.node);
		// 3. open device by given device address (node)
		device.handle = -1;
		device.handle = open(device.node, O_RDONLY);
		if (device.handle == -1) {
				syslog(LOG_ERR,"ERROR: Failed to open input event device %s.\n", device.node);
				perror("Failed to open input event device");
				exit(1);
		}
		// 4. get read access to input event node
		result = ioctl(device.handle, EVIOCGNAME(sizeof(device.name)), device.handle);
		syslog(LOG_DEBUG,"DEBUG: reading from %s (%s)\n", device.node, device.name);
		syslog(LOG_DEBUG,"DEBUG: getting exclusive access: ");
		// 5. exclusively register device for our process
		result = ioctl(device.handle, EVIOCGRAB, 1);
		if (result == 0) {
				device.filled = 0;
				syslog(LOG_DEBUG,"SUCCESS\n");
		} else {
				syslog(LOG_ERR,"ERROR: Failed to get exclusive access of device %s.\n", device.node);
				perror("Failed to get exclusive access on device");
				exit(errno);
		}

		curl_global_init(CURL_GLOBAL_ALL);
		
		//starting main loop
		
		main_return = main_loop();
		
		curl_global_cleanup();
		
		result = ioctl(device.handle, EVIOCGRAB, 1);
		close(device.handle);
		if (main_return == 0) {
			syslog(LOG_INFO, "INFO: application terminated normally.\n");
		} else {
			syslog(LOG_NOTICE, "INFO: application terminated with errors.\n");
		}
		closelog();
		return main_return;
}
