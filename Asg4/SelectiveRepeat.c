#include <stdio.h>
#include <stdlib.h>
#include <cnet.h>
#include <string.h>

#define WINDOW_SIZE_N 5

int seq_send=0;
int seq_rec=0;
int flag_sen=1;
int count=0,total_msg=0; 
int flag_sen_recv=0;
int id=0,id1=0;
int w=0,powten=0,flag1=0,N=0;

typedef struct packet{
	int seqno;
	char data[60];
}packet_data;

typedef struct packet_ack{
	int seqno;
	char data[2];
}packet_ack;

static void transmit_frame()
{
  if(strcmp(nodeinfo.nodename,"Dehradun")==0 && flag_sen==1 && total_msg<10000) {
        while(flag_sen==1)
      {
	if(count==N){
	  count=0;
	  flag_sen=0;
	  powten++; 
	  break;
	}
	packet_data *frame;
	size_t length;
	int i=0;
	frame=(packet_data *)malloc(1*sizeof(packet_data));
	frame->seqno = (powten*N)+count;
	for(i=0;i<60;i++) frame->data[i]='c';
	length = sizeof(packet_data);
	printf("Sending :: length=%d frame->seq=%d count=%d\n",length, frame->seqno, count);
	count++;
	CHECK(CNET_write_physical(1, frame, &length));
      }
    flag1=0;
    id = CNET_start_timer(EV_TIMER1, 3600000 , 0);
  }
  else if(strcmp(nodeinfo.nodename,"KGP")==0 && flag_sen_recv==1) {
    packet_ack *ack;
    size_t length;
    ack = (packet_ack *)malloc(1*sizeof(packet_ack));
    length = 1*sizeof(packet_ack);
    if(w==0)ack->seqno = seq_rec;
    else ack->seqno = (-1)*seq_rec;
    printf("ACKNOWLEDGEMENT sent by receiver with sequence no=%d .\n\n",ack->seqno);
    CHECK(CNET_write_physical_reliable(1, ack, &length));
    flag_sen_recv=0;
  }
}

static void frame_arrived(CnetEvent ev, CnetTimerID timer, CnetData data)
{
  if(strcmp(nodeinfo.nodename,"Dehradun")==0) {
    CNET_stop_timer(id);
    packet_ack *ack;
    size_t length;
    int link;
    ack = (packet_ack *)malloc(1*sizeof(packet_ack));
    length = 1*sizeof(packet_ack);
    CHECK(CNET_read_physical(&link, ack, &length));
    printf("Receiving acknowledgement with ack->seqno=%d ,seq_send+1=%d\n",ack->seqno,(seq_send+1));
    if(ack->seqno == (seq_send+1)) {
      printf("Acknowledgement is valid.\n");
      total_msg++;
      seq_send=(seq_send+1);
      if(ack->seqno%N==0) flag_sen=1;
    }
    else if(ack->seqno <0) {
      count = (-1)*ack->seqno;
      total_msg = count-1;
      id1 = CNET_start_timer(EV_TIMER2, 3600000 , 0);
      if(flag1==0)powten--;
      flag1=1;
    }
  }
  else if(strcmp(nodeinfo.nodename,"KGP")==0 ){
    packet_data *frame;
    size_t length=sizeof(packet_data);
    int link,i;
    frame=(packet_data *)malloc(1*sizeof(packet_data));
    CHECK(CNET_read_physical(&link, frame, &length));
    int flag=0;
    for(i=0;i<8;i++) { 
      if(frame->data[i]!='c'){flag=1; break; }
    }
    if(flag==0){
      flag_sen_recv = 1;
      printf("Frame received is valid.\n");
      printf("seq_rec=%d,frame->seqno=%d\n", seq_rec,frame->seqno);
      if(frame->seqno == seq_rec) {
	seq_rec = seq_rec+1;
	total_msg++;
	w = 0;
	transmit_frame();
      }
      else {
	printf("Received Frame is discarded.\n");
	w = 1;
      }		
    }
    else {
      printf("Frame received is Invalid.\n");
      w = 1;
      transmit_frame();
    }
  }
  
}

static EVENT_HANDLER(timer1_out)
{
  printf("\n\nTIMER EXPIRED\n");
  flag_sen = 1;
  transmit_frame();    
}

static EVENT_HANDLER(timer2_out)
{
  printf("\n\nTIMER2 EXPIRED\n");
  flag_sen = 1;
  transmit_frame();    
}


static EVENT_HANDLER(application_ready)
{
  transmit_frame();
  
}



void reboot_node(CnetEvent ev, CnetTimerID timer, CnetData data)
{
  FILE *fp;
  fp=fopen("windowsize.txt","r");
  fscanf(fp,"%d",&N);
  
  CHECK(CNET_set_handler(EV_PHYSICALREADY, frame_arrived, 0));
  
  CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));
  CHECK(CNET_set_handler(EV_TIMER1,timer1_out, 0));	
  CHECK(CNET_set_handler(EV_TIMER2,timer2_out, 0));
  CNET_enable_application(ALLNODES);
  
}
