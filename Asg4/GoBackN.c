#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define ACK_FRAME_SIZE      6
#define DATA_TRANS_TIME     50
#define ACK_TRANS_TIME      5
#define DATA_FRAME_SIZE_MAX 64
#define DATA_FRAME_H_SIZE   (sizeof(uint16_t) + sizeof(int) + sizeof(size_t))
#define DATA_SIZE_MAX       (DATA_FRAME_SIZE_MAX - DATA_FRAME_H_SIZE)
#define DATA_FRAME_SIZE(f)  (DATA_FRAME_H_SIZE + f.len)

#define TIMEOUT     3600
#define CHECK_BYTES 8
#define MAXSEQ      2
#define ACK_RECV    1
#define ACK_WAIT    2
#define RR          1
#define REJ         2


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
    int         seq;            // Seq Number of the Frame
                                // (if +ve RR frame, if non +ve REJ frame)
    uint16_t    check;
} ACK_FRAME;



NODE_TYPE nodetype;

static  CnetTimerID wait_timer = NULLTIMER;
static  MSG         *last_msg;
static  int         last_ack = -1;   // Seq Number of last ack frame
static  int next_frame = 0;     // Next Frame to be sent
static  int packets_sent = 0;
static  DATA_FRAME t_window[MAXSEQ + 1];    // Storing Frames in window
static  int        status[MAXSEQ + 1];      // Storing Status of Frames in Window

long  start_time = 0, end_time = 0;

static  void send_data_frame(MSG *msg, int len, int seq)
{
    if(len > DATA_SIZE_MAX)
        len = DATA_SIZE_MAX;
    
    DATA_FRAME f;
    f.seq = seq;        
    f.len = len;
    f.checksum = 0;
    memcpy(&f.msg, msg, len);
    
    f.checksum  = CNET_ccitt((unsigned char *) &f, CHECK_BYTES);
    
    printf("DATA transmitted, seq = %d\n", seq);
    
    t_window[seq] = f;
    
    size_t size = DATA_FRAME_SIZE(f);
    CHECK(CNET_write_physical(1, (char *) &f, &size));
}

static  void send_ack_frame(int seq, int type)
{
    ACK_FRAME f;
    f.check = 0;
    if(type == REJ)
        f.seq = -seq;
    else
        f.seq = seq;
    
    size_t size = sizeof(ACK_FRAME);
    f.check = CNET_ccitt((unsigned char *) &f, size);

    printf("ACK transmitted\n");

    CHECK(CNET_write_physical(1, (char *) &f, &size));
}

static EVENT_HANDLER(app_ready)
{
    if(status[next_frame] == ACK_WAIT)
    {
        wait_timer = CNET_start_timer(EV_TIMER1, TIMEOUT, 0);
        CNET_disable_application(ALLNODES);
        return;
    }
    status[next_frame] = ACK_WAIT;
    
    CnetAddr dest;
    
    size_t len = MAX_MESSAGE_SIZE;
    CHECK(CNET_read_application(&dest, (char*) last_msg, &len));
    
    send_data_frame(last_msg, len, next_frame);
    next_frame = (next_frame + 1) % (MAXSEQ + 1);
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
            if(f.seq >= 0 && f.seq <= MAXSEQ)
                send_ack_frame(f.seq, REJ);
            return;           
        }
        
        printf("\t\t\t\tDATA received, seq=%d\n", f.seq);
        
        send_ack_frame(f.seq + 1, RR);
    }
    else
    {
        ACK_FRAME f;
        int link;
        size_t len;

        len = sizeof(ACK_FRAME);
        CHECK(CNET_read_physical(&link, (char*) &f, &len));

        uint16_t check = f.check;
        f.check = 0;
        if(CNET_ccitt((unsigned char*) &f, sizeof(ACK_FRAME)) != check)
            return;
        
        if(f.seq > 0)
        {
            // RR FRAME
            printf("\t\t\t\tACK received, seq=%d\n", f.seq);
            
            int i;
            int c = 0;
            for(i = (last_ack + 1) % (MAXSEQ + 1); i != (f.seq - 1) % (MAXSEQ + 1); i = (i + 1) % (MAXSEQ + 1))
            {
                status[i] = ACK_RECV;
                c++;
            }
            
            last_ack = (f.seq - 1) % (MAXSEQ + 1);
            status[last_ack] = ACK_RECV;
            
            CNET_stop_timer(wait_timer);

            if(packets_sent < 10000)
            {
                packets_sent += (c+1);
                CNET_enable_application(ALLNODES);
            }
            else
            {
                if(end_time == 0)
                    end_time = nodeinfo.time_of_day.sec;
            }
        }
        else
        {
            // REJ FRAME
            f.seq = -f.seq;
            printf("\t\t\t\tREJ received, seq=%d\n", f.seq);
            last_ack = (f.seq - 1) % (MAXSEQ + 1);
            CNET_stop_timer(timer);
            int i;
            for(i = f.seq; i != next_frame; i = (i + 1) % (MAXSEQ + 1))
            {
                status[i] = ACK_WAIT;
                send_data_frame(&(t_window[i].msg), t_window[i].len, i);
            }
        }
    }    
}

static EVENT_HANDLER(wait)
{
    printf("Timeout. Resending\n");
    
    // Acknowledgement not received. Resend from last unacknowledged frame
    
    int i;
    for(i = (last_ack + 1) % (MAXSEQ + 1); i != next_frame; i = (i + 1) % (MAXSEQ + 1))
        send_data_frame(&(t_window[i].msg), t_window[i].len, i);
    send_data_frame(&(t_window[next_frame].msg), t_window[next_frame].len, i);
}

static EVENT_HANDLER(record_result)
{
    if(nodetype == TRANSMITTER)
    {
        if(end_time == 0)
            end_time = nodeinfo.time_of_day.sec;
        FILE *f = fopen("results_goBack1R", "a");
        fprintf(f, "%d %f\n", MAXSEQ, packets_sent*1.0*DATA_FRAME_SIZE_MAX*8/(end_time - start_time));
        fprintf(stderr, "%d %ld \n", packets_sent, (end_time - start_time));
        fclose(f);
    }
}

EVENT_HANDLER(reboot_node)
{
    if(nodeinfo.nodenumber > 1)
    {
        fprintf(stderr,"This is not a 2-node network!\n");
        exit(1);
    }

    last_msg = calloc(1, MAX_MESSAGE_SIZE);
    wait_timer = NULLTIMER;
    int i;
    for(i=0; i<=MAXSEQ; i++)
        status[i] = 0;

    CHECK(CNET_set_handler(EV_APPLICATIONREADY, app_ready, 0));
    CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
    CHECK(CNET_set_handler(EV_TIMER1, wait, 0));
    CHECK(CNET_set_handler(EV_SHUTDOWN, record_result, 0));

    start_time = nodeinfo.time_of_day.sec;
    // Only transmitter should send messages
    if(nodeinfo.nodenumber == 0)
    {
        nodetype = TRANSMITTER;
        CNET_enable_application(ALLNODES);
    }
    else
        nodetype = RECEIVER;
}


