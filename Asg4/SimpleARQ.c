#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define ACK_SIZE  6
#define DATA_TRANS_TIME     50
#define ACK_TRANS_TIME      5
#define DATA_FRAME_SIZE_MAX 64
#define DATA_FRAME_H_SIZE   (sizeof(uint16_t) + sizeof(int) + sizeof(size_t))
#define DATA_SIZE_MAX       (DATA_FRAME_SIZE_MAX - DATA_FRAME_H_SIZE)
#define DATA_FRAME_SIZE(f)  (DATA_FRAME_H_SIZE + f.len)

#define TIMEOUT     3600
#define CHECK_BYTES 8 

typedef enum {
    TRANSMITTER,
    RECEIVER
} NODE_TYPE;

typedef struct {
    char        data[DATA_SIZE_MAX];
} MSG;

typedef struct {
    int         seq;            // Seq Number of the Frame
    int         len;
    int         checksum;       // Checksum of the frame (first 64 bits)
    MSG         msg;            // Message sent by the transmitter
} DATA_FRAME;

typedef struct {
    char        ack[ACK_SIZE];  // Acknowledgement sent by the receiver
} ACK_FRAME;



NODE_TYPE nodetype;

// Details of last message sent
static  MSG         *last_msg;
static  size_t       last_len;
static  int          last_seq;
static  CnetTimerID last_timer = NULLTIMER;

static  int next_frame = 0;     // Next Frame to be sent
static  int frame_exp = 0;      // Frame expected by a node


static  void send_data_frame(MSG *msg, int len, int seq)
{
    if(len > DATA_SIZE_MAX)
        len = DATA_SIZE_MAX;
    
    DATA_FRAME f;
    f.seq = seq;        
    f.len = len;
    f.checksum = 0;
    memcpy(&f.msg, msg, len);
    
    printf("DATA transmitted, seq = %d\n", seq);

    // Setting timeout to wait for ack frame
    last_timer = CNET_start_timer(EV_TIMER1, 3*TIMEOUT, 0);

    size_t size = DATA_FRAME_SIZE(f);

    f.checksum  = CNET_ccitt((unsigned char *) &f, CHECK_BYTES);
    
    CHECK(CNET_write_physical(1, (char *) &f, &size));
}

static  void send_ack_frame()
{
    ACK_FRAME f;
    strcpy(f.ack, "Ack_S");
    
    printf("ACK transmitted\n");
    
    size_t size = sizeof(ACK_FRAME);
    CHECK(CNET_write_physical(1, (char *) &f, &size));
}

static EVENT_HANDLER(app_ready)
{
    CnetAddr dest;
    
    last_len = MAX_MESSAGE_SIZE;
    CHECK(CNET_read_application(&dest, (char*) last_msg, &last_len));
    CNET_disable_application(ALLNODES);
    
    last_seq = next_frame;
    send_data_frame(last_msg, last_len, last_seq);
    next_frame = next_frame + 1;
}

static EVENT_HANDLER(physical_ready)
{
    if(nodetype == RECEIVER)
    {
        DATA_FRAME f;
        size_t len;
        int link;
        int checksum;

        len = sizeof(DATA_FRAME);
        CHECK(CNET_read_physical(&link, (char*) &f, &len));

        checksum = f.checksum;
        f.checksum  = 0;

        if(CNET_ccitt((unsigned char*) &f, CHECK_BYTES) != checksum)
        {
            // bad checksum, ignore frame
            printf("\t\t\t\tBAD checksum - frame ignored\n");
            return;           
        }

        printf("\t\t\t\tDATA received, seq=%d, ", f.seq);
        if(f.seq == frame_exp)
        {
            printf("Success\n");
            len = f.len;
            //CHECK(CNET_write_application((char *)&f.msg, &len));
            frame_exp = frame_exp + 1;
        }
        else
            printf("Ignored\n");
        send_ack_frame();
    }
    else
    {
        ACK_FRAME f;
        int link;
        size_t len;

        len = sizeof(ACK_FRAME);
        CHECK(CNET_read_physical(&link, (char*) &f, &len));

        CNET_stop_timer(last_timer);
        CNET_enable_application(ALLNODES);
    }
    
}

static EVENT_HANDLER(wait)
{
    printf("Timeout. Resending\n");
    
    // Acknowledgement not received. Resend last frame
    send_data_frame(last_msg, last_len, last_seq);
}

EVENT_HANDLER(reboot_node)
{
    if(nodeinfo.nodenumber > 1)
    {
        fprintf(stderr,"This is not a 2-node network!\n");
        exit(1);
    }

    last_msg = calloc(1, MAX_MESSAGE_SIZE);

    CHECK(CNET_set_handler(EV_APPLICATIONREADY, app_ready, 0));
    CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
    CHECK(CNET_set_handler(EV_TIMER1, wait, 0));

    // Only transmitter should send messages
    if(nodeinfo.nodenumber == 0)
    {
        nodetype = TRANSMITTER;
        CNET_enable_application(ALLNODES);
    }
    else
        nodetype = RECEIVER;
}

