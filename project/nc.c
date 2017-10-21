/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define A 0x03
#define SET_C 0x03
#define SET_BCC A^SET_C
#define UA_C 0x07
#define UA_BCC A^UA_C
#define C10 0x00
#define C11 0x40
#define Escape 0x7D
#define escapeFlag 0x5E
#define escapeEscape 0x5D
#define RR_C0 0x05
#define RR_C1 0x85
#define REJ_C0 0x01
#define REJ_C1 0x81
#define DISC_C 0x0B
#define C2End 0x03

volatile int STOP=FALSE;
int esperado = 0;

void LLOPEN(int fd);
unsigned char * LLREAD(int fd, int* sizeMessage);
int checkBCC2(unsigned char* message, int sizeMessage);
int readControlMessage(int fd, unsigned char C);
void sendControlMessage(int fd, unsigned char C);
unsigned char * removeHeader(unsigned char * toRemove,int sizeToRemove,int * sizeRemoved);
off_t sizeOfFileFromStart(unsigned char * start);
unsigned char* nameOfFileFromStart(unsigned char * start);
int isEndMessage(unsigned char * start,int sizeStart,unsigned char * end, int sizeEnd);
void createFile(unsigned char * mensagem, off_t* sizeFile,unsigned char * filename);
void LLCLOSE (int fd);

int main(int argc, char** argv){
    int fd;
    struct termios oldtio,newtio;
    int sizeMessage=0;
    unsigned char* mensagemPronta;
    int sizeOfStart = 0;
    unsigned char * start;
    off_t sizeOfGiant = 0;
    unsigned char * giant;
    off_t index = 0;

    if ( (argc < 2) ||
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) &&
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }
  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 1;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 0;   /* blocking read until 5 chars received */

  /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)
  */

    tcflush(fd, TCIOFLUSH);

    printf("New termios structure set\n");

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    LLOPEN(fd);
    start = LLREAD(fd,&sizeOfStart);

		unsigned char* nameOfFile=nameOfFileFromStart(start);
    sizeOfGiant = sizeOfFileFromStart(start);

    giant = (unsigned char*)malloc(sizeOfGiant);

    while(TRUE){
      mensagemPronta = LLREAD(fd, &sizeMessage);
			if(sizeMessage == 0)
			continue;
      if(isEndMessage(start,sizeOfStart,mensagemPronta,sizeMessage)){
					printf("End message received\n");
	 		break;
			}

      int sizeWithoutHeader = 0;

      mensagemPronta = removeHeader(mensagemPronta,sizeMessage,&sizeWithoutHeader);

      memcpy(giant+index,mensagemPronta,sizeWithoutHeader);
      index += sizeWithoutHeader;
    }

		printf("Mensagem: \n");
  	int i = 0;
		for(; i < sizeOfGiant;i++){
		printf("%x",giant[i]);
	}

	createFile(giant,&sizeOfGiant,nameOfFile);

	LLCLOSE(fd);

    sleep(1);
    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}

//Data linhk layer
void LLCLOSE (int fd){
	readControlMessage(fd, DISC_C);
	printf("Recebeu DISC\n");
	sendControlMessage(fd, DISC_C);
	printf("Mandou DISC\n");
	readControlMessage(fd, UA_C);
	printf("Recebeu UA\n");
}

//lê trama de controlo SET e manda trama UA
//Data link layer
void LLOPEN(int fd){
  if(readControlMessage(fd,SET_C))
  {
      printf("Recebeu SET\n");
      sendControlMessage(fd,UA_C);
      printf("Mandou UA\n");
  }
  //TESTAR EM CASO DE ERRO
 }

//lê uma mensagem, faz destuffing
//Data link layer
unsigned char* LLREAD(int fd, int* sizeMessage){
  unsigned char* message = (unsigned char *)malloc(0);

	*sizeMessage = 0;
  unsigned char c_read;
  int trama=0;
  int mandarDados=FALSE;
    unsigned char c;
    int state=0;
    while(state!=6){

      read(fd,&c,1);
	     //printf("%x\n",c);
      switch(state){
        //recebe flag
        case 0:
          if(c==FLAG)
            state=1;
          break;
        //recebe A
        case 1:
  	     //printf("1state\n");
          if(c==A)
            state=2;
          else
            {
              if(c==FLAG)
                state=1;
              else
                state = 0;
            }
        break;
        //recebe C
        case 2:
  	     	//printf("2state\n");
          if(c==C10){
            trama = 0;
            c_read=c;
            state = 3;
          }else
          if(c==C11){
            trama=1;
            c_read=c;
            state=3;
          }
          else{
            if(c==FLAG)
              state=1;
            else
              state = 0;
          }
        break;
        //recebe BCC
        case 3:
  	     //printf("3state\n");
          if(c==(A ^ c_read))
            state = 4;
          else
            state=0;
        break;
        //recebe FLAG final
        case 4:
  	     //printf("4state\n");
          if(c==FLAG){
		          if(checkBCC2(message,*sizeMessage)){
			            	 if(trama == 0)
    	  					       sendControlMessage(fd,RR_C1);
    	 				       else
           					     sendControlMessage(fd,RR_C0);

				             state=6;
                     mandarDados=TRUE;
				             printf("Enviou RR, T: %d\n", trama);
			         }

             else
              {
                if(trama == 0)
			               sendControlMessage(fd,REJ_C1);
    	 	        else
           		       sendControlMessage(fd,REJ_C0);
			          state=6;
                mandarDados=FALSE;
			          printf("Enviou REJ, T: %d\n", trama);

              }
          }else
          if(c == Escape){
            state = 5;
          }
          else{
            message = (unsigned char  *)realloc(message, ++(*sizeMessage));
            message[*sizeMessage-1] = c;
          }
        break;
        case 5:
        //printf("5state\n");
        if(c==escapeFlag){
          message = (unsigned char  *)realloc(message, ++(*sizeMessage));
          message[*sizeMessage-1] = FLAG;
        }
        else{
          if (c==escapeEscape){
            message = (unsigned char  *)realloc(message, ++(*sizeMessage));
            message[*sizeMessage-1] = Escape;
          }
          else
            {
              perror("Non valid character after escape character");
              exit(-1);
            }
        }
        state=4;
        break;

      }

  }
	printf("Message size: %d\n", *sizeMessage);
    //message tem BCC2 no fim
    message = (unsigned char*)realloc(message,*sizeMessage-1);

		*sizeMessage = *sizeMessage - 1;
    if(mandarDados){
      if(trama==esperado){
        esperado^=1;
      }
      else
        *sizeMessage=0;
    }
    else
      *sizeMessage=0;
    return message;
}

//vê se o BCC2 recebido está certo
//Data link layer
int checkBCC2(unsigned char* message, int sizeMessage){
    int i =1;
    unsigned char BCC2=message[0];
    for(; i < sizeMessage-1;i++){
      BCC2 ^= message[i];
    }
    if (BCC2 == message[sizeMessage - 1]) {
			return TRUE;
    }
    else
      return FALSE;
}

//manda uma trama de controlo
//Application Layer
void sendControlMessage(int fd,unsigned char C){
    unsigned char message[5];
    message[0]=FLAG;
    message[1]= A;
    message[2]=C;
    message[3]=message[1]^message[2];
    message[4]=FLAG;
    write(fd,message,5);
}

//lê uma trama de controlo
//Application Layer
int readControlMessage(int fd, unsigned char C){
    int state=0;
    unsigned char c;

    while(state!=5){
      read(fd,&c,1);
      switch(state){
        //recebe flag
        case 0:
          if(c==FLAG)
            state=1;
            break;
        //recebe A
        case 1:
          if(c==A)
            state=2;
          else
            {
              if(c==FLAG)
                state=1;
              else
                state = 0;
            }
        break;
        //recebe C
        case 2:
          if(c==C)
            state=3;
          else{
            if(c==FLAG)
              state=1;
            else
              state = 0;
          }
        break;
        //recebe BCC
        case 3:
          if(c==(A^C))
            state = 4;
          else
            state=0;
        break;
        //recebe FLAG final
        case 4:
          if(c==FLAG){
  	         //printf("Recebeu mensagem\n");
  	         state=5;
            }
          else
            state = 0;
        break;
      }
    }
      return TRUE;
}

//cabeçalho das tramas I
//Application Layer
unsigned char * removeHeader(unsigned char * toRemove,int sizeToRemove,int * sizeRemoved){
  int i = 0;
  int j = 4;
  unsigned char * messageRemovedHeader = (unsigned char *)malloc(sizeToRemove - 4);
  for (; i < sizeToRemove; i++,j++) {
    messageRemovedHeader[i]=toRemove[j];
  }
  *sizeRemoved = sizeToRemove - 4;
  return messageRemovedHeader;
}

//Application Layer
int isEndMessage(unsigned char * start,int sizeStart,unsigned char * end, int sizeEnd){
  int s = 1;
  int e=1;
  if(sizeStart != sizeEnd)
    return FALSE;
  else{
    if(end[0] == C2End){
      for(;s < sizeStart; s++,e++){
        if(start[s] != end[e])
          return FALSE;
      }
      return TRUE;
    }
    else{
      return FALSE;
    }
  }
}

//Application Layer
off_t sizeOfFileFromStart(unsigned char * start){
  return (start[3] << 24 ) | (start[4]<<16) | (start[5]<<8) | (start[6]);
}

//Application Layer
unsigned char* nameOfFileFromStart(unsigned char * start){

  int L2 = (int)start[8];

  unsigned char * name = (unsigned char*)malloc(L2+1);

  int i;
  for (i=0; i < L2; i++){
    name[i]=start[9+i];
  }

  name[L2]='\0';

  return name;
}

//cria ficheiro
void createFile(unsigned char * mensagem, off_t* sizeFile,unsigned char filename[]){
	FILE* file = fopen((char*)filename, "wb+");

	fwrite((void *)mensagem, 1, *sizeFile, file);
	printf("%zd\n",*sizeFile);
	printf("New file created\n");
	fclose(file);
}