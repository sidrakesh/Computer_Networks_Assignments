#include <cnet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define	MESSAGES (sizeof(messages) / sizeof(messages[0]))
#define MAX 1000;

static char *check;
static char* messages[] = { "Hello!!", "Bye!!"};
extern int random(void);
int SN=0;
int RN=0;
int count=0;
int flag_sen=1;
int flag_rec=0;
int id=0;
static void transmit_frame(){
	if(strcmp(nodeinfo.nodename,"Dehradun")==0 && flag_sen == 1 && count < 1000 ){
		char *frame=(char *)malloc(64*sizeof(char));
		size_t length=64;
		int i=0;


		if(SN==0){ 
		sprintf(frame, "%dmessage %d is %s",SN, count+1, messages[0]);
		}
		else{ 
		sprintf(frame, "%dmessage %d is %s",SN, count+1, messages[1]);
		}
		printf("\nsending %u bytes of ' %s ': ", length, frame);
		for(i=1;i<9;i++){
			check[i]=frame[i];
		}
		CHECK(CNET_write_physical(1, frame, &length));
		id=CNET_start_timer(EV_TIMER1,3600,0);
		flag_sen=0;
	}
	else if(strcmp(nodeinfo.nodename,"KGP")==0 && flag_rec == 1){
		char *ack=(char *)malloc(48*sizeof(char));
		size_t length=48;
		sprintf(ack, "%d",RN);
		CHECK(CNET_write_physical(1, ack, &length));
		flag_rec =0;
		printf("\n Acknowledgement send ");
	}
}

static void frame_arrived(CnetEvent ev, CnetTimerID timer, CnetData data)
{
	if(strcmp(nodeinfo.nodename,"Dehradun")==0 ){
		char *ack=(char *)malloc(48*sizeof(char));
		int link;
		int seq_check;
		size_t length=48;
		CNET_stop_timer(id);
		CHECK(CNET_read_physical(&link, ack, &length));
		seq_check = ((int)(ack[0]))-48;
		if(seq_check == ((SN+1)%2)){
			printf("\n Acknowledgement received ");
			flag_sen=1;
			count++;
			SN= count%2;		
		}
	}
	else if(strcmp(nodeinfo.nodename,"KGP")==0 ){
		char *frame=(char *)malloc(64*sizeof(char));
		char *tobchecked=(char *)calloc(9,sizeof(char));
		size_t length=64;
		int link;
		int i=0;
		printf("\n Receiving.....");
		CHECK(CNET_read_physical(&link, frame, &length));
		for(i=1;i<9;i++){
			tobchecked[i]=frame[i];
		}
		if((strcmp(check,tobchecked))==0){
			flag_rec = 1;
			if(RN==((int)frame[0])-48){		
				printf("\nSUCCESSFULLY RECEIVED %u bytes", length);
				RN=(RN+1)%2;
				flag_sen=1;	
			}
			else{
				 printf("\n The SN mismatches !");
			}
			transmit_frame();	//for the acknowledgement
			printf("\n The frame is valid");
		}
		else {
			printf("\n The frame is either lost or error");
		}	
	}
}

static EVENT_HANDLER(resend_frame){
	flag_sen=1;
	printf("Timer expired");
	transmit_frame();
}
static EVENT_HANDLER(application_ready)
{
	transmit_frame();
}
void reboot_node(CnetEvent event){
	check = (char *)malloc(9*sizeof(char));
	CHECK(CNET_set_handler(EV_APPLICATIONREADY,application_ready, 0));
	CHECK(CNET_set_handler(EV_PHYSICALREADY,frame_arrived, 0));
	CHECK(CNET_set_handler(EV_TIMER1,resend_frame, 0));
	CNET_enable_application(ALLNODES);

}
