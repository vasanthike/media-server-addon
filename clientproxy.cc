#include "clientproxy.h" 


#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

// MAC
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <gcrypt.h>
#include <assert.h>
#include <stdio.h>


#include "srtp.h"

void usage(int argc, char **argv);
unsigned char *byte_swap(unsigned char *buffer,int len);

#define NUM_THREADS 1 
#define SRTP_PARAM_SETS   8
#define SRTP_VIDEO_IN_DEVICE   0
#define SRTP_AUDIO_IN_DEVICE   1
#define SRTP_AUDIO_IN_A        2
#define SRTP_AUDIO_IN_B        3
#define SRTP_AUDIO_OUT_DEVICE  4
#define SRTP_AUDIO_OUT_A       5
#define SRTP_AUDIO_OUT_B       6
#define SRTP_AUDIO_TEST        7

typedef struct {
  int  in_use;
  char *cmd;
} thread_info;


struct srtp_prms {
  srtp_session_t *s;
  int16_t decoded_packets;
  uint32_t timestamp;
} ;

unsigned char *sdes[16] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
uint32_t ssrc[16];



static struct srtp_prms  srtp_sess[16]; // video in (just proof of concept ), audio in A, audio in B, audio out

void sig_usr(int signo);
int time_count = 0;
pid_t  pid,ppid;
int       base_port = 9000;
int       client_port = 5000;
int       server_port = 0;
char      *server_ip = NULL;
char      *client_ip = NULL;
char      *default_client_ip = "127.0.0.1";
int       port_control = 22220;


void control_msg(int prt,struct sockaddr *client, char *buffer, int len, int client_length);


void cleanupHandler() {
  char buff[80];
  exit(0);
}

typedef struct {
  struct in_addr endpoint[2];
} connPair;

//struct in_addr {
//  unsigned long s_addr;  // load with inet_aton()
//};


int  decrypt_buffer(int sess_nr,unsigned char *buffer,unsigned int  *bytes_received) {
  int ret = 0;
  size_t s = *bytes_received;

  if (srtp_sess[sess_nr].decoded_packets == 0) {
    srtp_init_seq(srtp_sess[sess_nr].s, buffer);
  } 

  //  hexdump(buffer,bytes_received);

  ret = srtp_recv(srtp_sess[sess_nr].s, buffer, &s);
  
  if (ret != 0) {
    //        sprintf(buff, "frame dropped: decoding failed '%s'\n", strerror(ret));
    //       write(1,buff,strlen(buff));
    return(0);
  }
  else
    *bytes_received = s;

  srtp_sess[sess_nr].decoded_packets++;

  return(*bytes_received);

};



int       *initiated;     // max_servers count
struct    sockaddr_in *server_address; // max_servers count
struct    sockaddr_in *client_address; // max_servers count

connPair  *connections;

typedef struct {
  unsigned char key[16], salt[14];
} KEY_SALT;



static const char b64chars[] = 
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned char shiftb64(unsigned char c) {
  const char *p = strchr(b64chars, c);
  assert(p);
  return p-b64chars;
}

static void decode_block(char *in, unsigned char *out) {
  unsigned char shifts[4];
  int i;

  for (i = 0; i < 4; i++) {
    shifts[i] = shiftb64(in[i]);
  }

  out[0] = (shifts[0]<<2)|(shifts[1]>>4);
  out[1] = (shifts[1]<<4)|(shifts[2]>>2);
  out[2] = (shifts[2]<<6)|shifts[3];
}

static void decode_sdes(char *in,
  unsigned char *key, unsigned char *salt) {
  int i;
  size_t len = strlen((char *) in);
  unsigned char raw[30];

  for (i = 0; 4*i < len; i++) {
    decode_block(in+4*i, raw+3*i);
  }

  memcpy(key, raw, 16);
  memcpy(salt, raw+16, 14);
}

int testCall(int p){
  
    return p;
}

#define FIXED_KEY "D9YVck7Gk8ter1CZot4WWyB4L6pWepx5Utm08fVl"

int getMediaProxy(int bp, int cp, int vp,  char *server , char *key){
  

  KEY_SALT key_salt[16];

  int       max_servers = 8 + 1;  // no of connections + controll channel
  int       *server_handle; // max_servers count
  int       max_server_handle = 0;
  unsigned char      buffer[1500];
  char  keybuff[128];
  socklen_t client_length;
  fd_set    read_handles;
  struct    timeval timeout_interval;
  int taglen = 10;
  int       bytes_received;
  int       retval;
  int       i;
  int aot = 2; // 
  int vbr = 0;
  int bitrate = 32000;
  int input_size;
  uint8_t* input_buf;
  int16_t* convert_buf;
  int afterburner = 1;
  int format, sample_rate = 8000, channels = 1, bits_per_sample = 16;

  printf("Media Proxy.\n");

  // init stuff

   
  base_port = bp;
  port_control = base_port + 8;

  client_port = cp;

  server_port = vp;

  server_ip = server;

  sprintf(keybuff,"%s",key);


  for(i=0;i<16;i++)
    decode_sdes(&keybuff[0], key_salt[i].key, key_salt[i].salt);
  
  for (i=0;i<16;i++) {
    srtp_sess[i].s = srtp_create(SRTP_ENCR_AES_CM, SRTP_AUTH_HMAC_SHA1, taglen, SRTP_PRF_AES_CM, 0);
    assert(srtp_sess[i].s != NULL);
    srtp_setkey(srtp_sess[i].s, key_salt[i].key, sizeof(key_salt[i].key), key_salt[i].salt, sizeof(key_salt[i].salt));
    srtp_sess[i].decoded_packets = 0;
    srtp_sess[i].timestamp = 0;
  };

  if (client_ip == NULL)
    client_ip = default_client_ip;

 /* if (server_ip == NULL) 
    usage(argc,argv);
  
  if (server_port<1)
    usage(argc,argv);*/

  ppid = getpid();
  
  pid = fork ();  // fork right away before anything is malloced

  if (pid != 0)  { // parent , go ahead and malloc 

    time_t epoch = time(NULL);
    struct sigaction sig;      

    sigemptyset(&sig.sa_mask);          
    sig.sa_flags = 0;                   
    sig.sa_handler = sig_usr;  

    sigaction(SIGUSR2,&sig,NULL);
    //signal(SIGINT, cleanupHandler);
    
    server_address = (struct sockaddr_in *) malloc(max_servers*sizeof( server_address[0]));
    client_address = (struct sockaddr_in *) malloc(max_servers*sizeof( client_address[0]));
    connections = (connPair *) malloc(max_servers*sizeof( connections[0]));

    server_handle = (int *) malloc(max_servers*sizeof( server_handle[0]));
    initiated = (int *) malloc(max_servers*sizeof( initiated[0]));


	input_size = 1024;
	input_buf = (uint8_t*) malloc(input_size);
	convert_buf = (int16_t*) malloc(input_size);

    
    for (i = 0; i < max_servers; i++) // max_servers = 8 + 1 = 9
      {
        if (i == ( max_servers - 1) )
          printf("Creating control socket %d on port: %d\n", i, port_control );
        else
          printf("Creating socket %d on port: %d\n", i, base_port + i);
        
        initiated[i] = 0;
        server_handle[i] = socket(AF_INET, SOCK_DGRAM, 0);
        if (server_handle[i] < 0)
          {
            perror("Unable to create socket.");
            return -1;
          }

        int optval = 1;
        setsockopt(server_handle[i], SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int));
        
        if (server_handle[i] > max_server_handle)
          max_server_handle = server_handle[i];

        memset( &server_address[i], 0, sizeof( server_address[i] ) );
        server_address[i].sin_family = AF_INET;
        server_address[i].sin_addr.s_addr = htonl(INADDR_ANY);;
        
        if (i == ( max_servers - 1) )
          server_address[i].sin_port = htons( (unsigned short) (port_control) );
        else
          server_address[i].sin_port = htons( (unsigned short) (base_port + i) );
        
        if (bind( server_handle[i], (struct sockaddr *)&server_address[i], sizeof( server_address[i] )) < 0)
          {
            perror("Unable to bind.");
            return -1;
          }
        printf("Bind %d successful.\n", i);
        memset( &client_address[i], 0, sizeof( client_address[i] ) );
        client_address[i].sin_family = AF_INET;
        if (i<4) {
          int l;
          client_address[i].sin_addr.s_addr = inet_addr(server_ip);
          client_address[i].sin_port = htons( (unsigned short) (server_port  + i) );
          for(l=0;l<13;l++)
            sendto(server_handle[i],"ping ping",9,0,
                   (struct sockaddr *)&client_address[i], sizeof( client_address[i] ));

        } else
          if (i<8) {
            client_address[i].sin_addr.s_addr = inet_addr(client_ip);
            client_address[i].sin_port = htons( (unsigned short) (client_port  - 4 + i) );
          };
        initiated[i] = 1;


      }

        
    while (1)
      {
        FD_ZERO(&read_handles);
        for (i = 0; i < max_servers; i++)
          FD_SET(server_handle[i], &read_handles);
        
        timeout_interval.tv_sec = 50; //15; VASANTHI
        timeout_interval.tv_usec = 500000;
        
        retval = select(max_server_handle + 1, &read_handles, NULL, NULL, &timeout_interval);
        if (retval == -1)
          {
            printf("Select error\n");
            //error
          }
        else if (retval == 0)
          {
            printf("timeout\n");
            cleanupHandler();
          }
        else
          {
            //good
            for (i = 0; i < max_servers; i++)
              {
                if (FD_ISSET(server_handle[i], &read_handles))
                  {
                    client_length = sizeof(client_address[i]);
                    initiated[i] = 1;
                    if ((bytes_received = recvfrom(server_handle[i], buffer, sizeof(buffer), 0, (struct sockaddr *)&client_address[i], &client_length)) < 0)
                      {
                        perror("Error in recvfrom.");
                        break;
                      }

                    if (i == ( max_servers - 1 )) { // CONTROL channel
                      control_msg(server_handle[i],(struct sockaddr *)&client_address[i],(char *)&buffer,bytes_received,client_length) ;
                    } else {
		      int other_end;
		      if ((i==0)||(i==2)) {
			int ret = decrypt_buffer(i,buffer,(unsigned int*)&bytes_received);
			printf("%d\n",ret);
		      };
                      if ( i > 4 ) {
                        other_end = i - 4;
                      } else {
                        other_end = i + 4;
                      };
                      
		      if (i == 2) {
		      };
                      int n = sendto(server_handle[other_end],buffer,bytes_received,0,
                                     (struct sockaddr *)&client_address[other_end], client_length);
                        //                      };
                    };
		  }
	      }
	  }
      }
  } else { // CHILD
    while(1) {
      sleep(1);
      //kill(ppid, SIGUSR2);
    };    
  }
  return 1;
}

unsigned char *byte_swap(unsigned char *buffer,int len) {
  int i;
  unsigned char c;
  for (i=0;i<len;i+=4) {
    c=buffer[i];
    buffer[i]=buffer[i+1];
    buffer[i+2]=buffer[i+1];
    buffer[i+1]=c;
    buffer[i+3]=c;
  };
  return(buffer);    
};

void control_msg(int prt,struct sockaddr *client, char *buffer, int len, int client_length) {
  printf("\ncommand error\n");
};

void sig_usr(int signo){
  if(signo == SIGUSR2) {
    time_count++;
    /*
    if(time_count >10) {
      kill(pid, SIGINT);
      kill(ppid, SIGINT);
    };
    */
  }
}



void usage(int argc, char **argv) {
  printf("usage: %s --base_port <listen_on_port> --client_port <vlc start port> --server_port <server start port>  --server_address <server ip address> [ --client_address <client ip address> ]\n",argv[0]);
  exit(1);
};
