/**
 * @ingroup     PO AudioGames SS2016 Haptisches Feedback durch "Datenhandschuh" (am Beispiel von smartuni)
 * @{
 *
 * @file
 * @brief       Implements coap backend
 *
 * @author      smlng <s@mlng.net>, Sebastian Wilkes <sebastian.wilkes@haw-hamburg.de>
 *
 * @}
 */

//#include "net/gnrc/udp.h"
//#include "net/gnrc/ipv6.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "thread.h"

// riot
#include "board.h"
#include "periph/gpio.h"
#include "thread.h"
#include "coap.h"
#include "ps.h"
#include "xtimer.h"
#include "msg.h"

#include "board.h"
#include "saul/periph.h"
#include "timex.h"

#include "coap.h"
#include "periph/gpio.h"

// own
#include "pins.h"

// compatibility
#ifndef LED_ON
#define LED_ON      LED0_ON
#define LED_OFF     LED0_OFF
#define LED_TOGGLE  LED0_TOGGLE
#endif

// parameters
#define COAP_BUF_SIZE           (64)
#define COAP_MSG_QUEUE_SIZE     (8)
#define COAP_REPSONSE_LENGTH    (1500)

// coap_thread
static char coap_thread_stack[THREAD_STACKSIZE_DEFAULT];
static msg_t coap_thread_msg_queue[COAP_MSG_QUEUE_SIZE];
static char endpoints_response[COAP_REPSONSE_LENGTH] = "";

// vibrate thread
static char vibrate_thread_stack[THREAD_STACKSIZE_DEFAULT*6];

// coap endpoints
static const coap_endpoint_path_t path_well_known_core = {2, {".well-known", "core"}};
static const coap_endpoint_path_t path_led = {1, {"led"}};
static const coap_endpoint_path_t path_vibrate = {1, {"vibrate"}};

// udp communication
#define SPORT               (1234)
#define UDP_PORT            (5683)
int seqnum_post = 12345;

// IPC
#define RCV_QUEUE_SIZE  (8)
static msg_t rcv_queue[RCV_QUEUE_SIZE];
static kernel_pid_t vibrate_thread_pid;


//header
static const coap_header_t req_hdr = {
        .version = 1,
        .type    = COAP_TYPE_NONCON,
        .tkllen  = 0,
        .code    = COAP_METHOD_POST,
        .mid     = {5, 57}            // is equivalent to 1337 when converted to uint16_t
};

//prototypes
int interpretPayload(const uint8_t * const, int, int);
int returnFromAsciiToUtf(int);
int findn(int);
int power(int, unsigned int);
int udp_sendto(uint8_t *buf, size_t len, uint8_t *dst, uint16_t dst_port);

/**
 * @brief handle well-known path request
 */
static int handle_get_well_known_core(coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo)
{
    return coap_make_response(scratch, outpkt, (const uint8_t *)endpoints_response, strlen(endpoints_response), id_hi, id_lo, &inpkt->token, COAP_RSPCODE_CONTENT, COAP_CONTENTTYPE_APPLICATION_LINKFORMAT, false);
}

/**
 * @brief handle put led request
 */
static int handle_put_led(coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo)
{
    if (inpkt->payload.len > 0) {

        if (inpkt->payload.p[0] == '1') {
            LED0_ON;
            LED1_ON;
            LED2_ON;
            puts("LED ON!");
        }
        else if (inpkt->payload.p[0] == 'r') {
            LED0_TOGGLE;
            puts("LED TOGGLE red ...");
        }
        else if (inpkt->payload.p[0] == 'g') {
            LED1_TOGGLE;
            puts("LED TOGGLE green ...");
        }
        else if (inpkt->payload.p[0] == 'b') {
            LED2_TOGGLE;
            puts("LED TOGGLE blue ...");
        }
        else {
            LED0_OFF;
            LED1_OFF;
            LED2_OFF;
            puts("LED OFF!");
        }
        return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->token, COAP_RSPCODE_CHANGED, COAP_CONTENTTYPE_TEXT_PLAIN, false);
    }
    else {
        LED_OFF;
        puts("LED OFF");
        return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->token, COAP_RSPCODE_BAD_REQUEST, COAP_CONTENTTYPE_TEXT_PLAIN, false);
    }
}



/**
 * @brief handle put vibrate request
 */
static int handle_put_vibrate(coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo)
{
    if (inpkt->payload.len > 0)
	 {
      msg_t m;

		m.content.ptr = (char*) inpkt;

		if (msg_try_send(&m, vibrate_thread_pid) == 0)
		{
	 		printf("Receiver queue full.\n");
 	   }

		return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->token, COAP_RSPCODE_CHANGED, COAP_CONTENTTYPE_TEXT_PLAIN, false);
	}
   else
	{
        LED_OFF;
    	  gpio_clear(GPIO_PIN(VBR_PORT, VBR_PIN));
        puts("payloadlength < 1 VIBRATION OFF");
        return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->token, COAP_RSPCODE_BAD_REQUEST, COAP_CONTENTTYPE_TEXT_PLAIN, false);
   }
	return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->token, COAP_RSPCODE_BAD_REQUEST, COAP_CONTENTTYPE_TEXT_PLAIN, false);
}



/**
 * @brief needed for payload interpretation
 *
 * @param[in] int   number of chars
 */
int findn(int num)
{
    char snum[100];
    return  sprintf(snum, "%d", num);
}

int findnInString(uint8_t* string)
{
    char snum[100];
    return  sprintf(snum, "%s", string);
}


/**
 * @brief vibrate handler thread function
 *
 * @param[in] arg   unused
 */
void *vibrate_thread(void *arg)
{
  (void) arg;
  msg_init_queue(rcv_queue, RCV_QUEUE_SIZE);
  printf("vibrate thread started, pid: %" PRIkernel_pid "\n", thread_getpid());

  msg_t m;
  m.content.ptr = 0;

  while (1) {
		msg_receive(&m); // blocking
		if(NULL != m.content.ptr){

			const coap_packet_t* inpkt = (const coap_packet_t *)  m.content.ptr;

			int payloadLength =(int) inpkt->payload.len;

			int period = 0;
			int pause  = 0;
			int count  = 0;
			int pos = payloadLength - 1;

			//interpret
			count = interpretPayload(inpkt->payload.p, payloadLength, pos);
			pos = pos - (findn(count)+1);
			pause  = interpretPayload(inpkt->payload.p, payloadLength, pos);
			pos = pos - (findn(pause)+1);
			period  = interpretPayload(inpkt->payload.p, payloadLength, pos);

			//console
			printf("\npayloadLength2: %d / period: %d / pause: %d / count: %d \n", payloadLength, period, pause, count);

			int i;
			for(i=0; i<count; i = i+1){
				gpio_set(GPIO_PIN(VBR_PORT, VBR_PIN));
				LED0_ON;
				LED1_ON;
				LED2_ON;
				puts("VIBRATION ON");
				usleep(period*1000);
				gpio_clear(GPIO_PIN(VBR_PORT, VBR_PIN));
				LED0_OFF;
				LED1_OFF;
				LED2_OFF;
				puts("VIBRATION OFF");
				usleep(pause*1000);
			}

		}else{
			printf("Received Null pointer.. something went wrong!\n");
		}

  }
  return NULL;
}



int returnFromAsciiToUtf(int value) {
	return (value - 48);
}

int power(int base, unsigned int exp) {
    int i, result = 1;
    for (i = 0; i < exp; i++)
        result *= base;
    return result;
 }


int interpretPayload(const uint8_t * const payload, int payloadlength, int pos) {
	int i = 0;
	int value = 0;
	int convertedValue = 0;
	while(pos < payloadlength && pos >= 0 && payload[pos] != 47)
	{
		convertedValue = returnFromAsciiToUtf(payload[pos]);
		value = value + (convertedValue * power(10,i));
		pos=pos-1;
		i=i+1;
	}
	return value;
}

const coap_endpoint_t endpoints[] =
{
    {COAP_METHOD_GET, handle_get_well_known_core, &path_well_known_core, "ct=40"},
    {COAP_METHOD_PUT, handle_put_led, &path_led, NULL},
    {COAP_METHOD_PUT, handle_put_vibrate, &path_vibrate, NULL},
    {(coap_method_t)0, NULL, NULL, NULL}
};


void send_coap_put(uint8_t *data, size_t len)
{
    puts("send_coap_put");

    uint8_t  snd_buf[128];
    size_t   req_pkt_sz;

    coap_header_t req_hdr = {
      .version = 1,
      .type = COAP_TYPE_NONCON,
      .tkllen = 0,
      .code = MAKE_RSPCODE(0, COAP_METHOD_PUT),
      .mid = {seqnum_post,22}
    };
    seqnum_post++;

    coap_buffer_t payload = {
            .p   = data,
            .len = len
    };

    coap_packet_t req_pkt = {
            .header  = req_hdr,
            .token   = (coap_buffer_t) { 0 },
            .numopts = 1,
            .opts    = {{{(uint8_t *)"trigger", 7}, (uint8_t)COAP_OPTION_URI_PATH}},
            .payload = payload
    };

    req_pkt_sz = sizeof(req_pkt);

    // To something with declared variables as long coap_build is commented out...

    snd_buf[0]++;
    snd_buf[0]--;
    req_pkt_sz++;
    req_pkt_sz--;

    /*
    * FIXME HERE IS THE PROBLEM!
    * Note: worked on earlier version of riot-os.
    * Even though the coap.h header is there,
    * and the coap_build method is for testing supposed to do nothing (code commented out)
    * it throws this error when the coap_build method is called:
          2017-09-19 12:40:37,643 - INFO # Stack pointer corrupted, reset to top of stack
          2017-09-19 12:40:37,643 - INFO # FSR/FAR:
          2017-09-19 12:40:37,645 - INFO #  CFSR: 0x00009200
          2017-09-19 12:40:37,647 - INFO #  HFSR: 0x40000000
          2017-09-19 12:40:37,648 - INFO #  DFSR: 0x00000000
          2017-09-19 12:40:37,650 - INFO #  AFSR: 0x00000000
          2017-09-19 12:40:37,652 - INFO #  BFAR: 0x1fffbfa8
          2017-09-19 12:40:37,652 - INFO # Misc
          2017-09-19 12:40:37,654 - INFO # EXC_RET: 0xfffffff1

    if (coap_build(snd_buf, &req_pkt_sz, &req_pkt) != 0) {
            printf("CoAP build failed :\n");
            return;
    }
    */

    puts("before send");

    /* TODO just sending a udp message when the internal button is clicked,
    * here instead of data, the coap package should be send.
    */

	  const char * addr_str = "fd9a:620d:c050:0:1ac0:ffee:1ac0:ffee";

    struct sockaddr_in6 src, dst;
    size_t data_len = strlen((char*)data);
    int s;
    uint16_t port;

    data_len++;
    data_len--;
    s = 0;
    s++;
    s--;
    port = UDP_PORT;

    src.sin6_family = AF_INET6;
    dst.sin6_family = AF_INET6;
    memset(&src.sin6_addr, 0, sizeof(src.sin6_addr));

    if (inet_pton(AF_INET6, addr_str, &dst.sin6_addr) != 1) {
        puts("Error: unable to parse destination address");
        return;
    }

    dst.sin6_port = htons(port);
    src.sin6_port = htons(port);

    s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        puts("error initializing socket");
        return;
    }

    if (sendto(s, data, data_len, 0, (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        puts("could not send");
    }
    else {
        printf("Success: send\n");
    }

    //usleep(delay);

    close(s);

    puts(addr_str);
}

/**
 * @brief generate well known resource description
 */
static void setup_endpoints(void)
{
    uint16_t len = COAP_REPSONSE_LENGTH;
    const coap_endpoint_t *ep = endpoints;
    int i;

    len--; // Null-terminated string

    while(NULL != ep->handler)
    {
        if (NULL == ep->core_attr) {
            ep++;
            continue;
        }

        if (0 < strlen(endpoints_response)) {
            strncat(endpoints_response, ",", len);
            len--;
        }

        strncat(endpoints_response, "<", len);
        len--;

        for (i = 0; i < ep->path->count; i++) {
            strncat(endpoints_response, "/", len);
            len--;

            strncat(endpoints_response, ep->path->elems[i], len);
            len -= strlen(ep->path->elems[i]);
        }

        strncat(endpoints_response, ">;", len);
        len -= 2;

        strncat(endpoints_response, ep->core_attr, len);
        len -= strlen(ep->core_attr);

        ep++;
    }
}


/**
 * @brief udp receiver thread function
 *
 * @param[in] arg   unused
 */
static void *coap_thread(void *arg)
{
    (void) arg;
    // start coap listener
    struct sockaddr_in6 server_addr;
    char src_addr_str[IPV6_ADDR_MAX_STR_LEN];
    uint16_t port;
    static int sock = -1;
    static uint8_t buf[COAP_BUF_SIZE];
    uint8_t scratch_raw[COAP_BUF_SIZE];
    coap_rw_buffer_t scratch_buf = {scratch_raw, sizeof(scratch_raw)};

    msg_init_queue(coap_thread_msg_queue, COAP_MSG_QUEUE_SIZE);
    sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

    /* parse port */
    port = (uint16_t)COAP_PORT;
    if (port == 0) {
        puts("ERROR: invalid port specified");
        return NULL;
    }
    server_addr.sin6_family = AF_INET6;
    memset(&server_addr.sin6_addr, 0, sizeof(server_addr.sin6_addr));
    server_addr.sin6_port = htons(port);
    if (sock < 0) {
        puts("ERROR: initializing socket");
        sock = 0;
        return NULL;
    }
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        sock = -1;
        puts("ERROR: binding socket");
        return NULL;
    }
    while (1) {
        int res,rc;
        struct sockaddr_in6 src;
        socklen_t src_len = sizeof(struct sockaddr_in6);
        coap_packet_t pkt;
        // blocking receive, waiting for data
        if ((res = recvfrom(sock, buf, sizeof(buf), 0,
                            (struct sockaddr *)&src, &src_len)) < 0) {
            puts("ERROR: on receive");
        }
        else if (res == 0) {
            puts("WARN: Peer did shut down");
        }
        else { // check for PING or PONG
            if (0 != (rc = coap_parse(&pkt, buf, res)))
                printf("WARN: Bad packet rc=%d\n", rc);
            else
            {
                inet_ntop(AF_INET6, &(src.sin6_addr),
                          src_addr_str, sizeof(src_addr_str));
                printf(". received COAP message from [%s].\n", src_addr_str);
                size_t rsplen = sizeof(buf);
                coap_packet_t rsppkt;

					 coap_handle_req(&scratch_buf, &pkt, &rsppkt, true, false);

                if (0 != (rc = coap_build(buf, &rsplen, &rsppkt))) {
                    printf("WARN: coap_build failed rc=%d\n", rc);
                }
                else {
                    sendto(sock, buf, rsplen, 0, (struct sockaddr *)&src, src_len);
                }
            }
        }
    }

    puts("Running coap thread");

    return NULL;
}


/**
 * @brief start vibrate thread
 *
 * @return PID of vibrate thread
 */


int vibrate_start_thread(void)
{
	 // start thread
	 vibrate_thread_pid = thread_create(vibrate_thread_stack, sizeof(vibrate_thread_stack),
				                          THREAD_PRIORITY_MAIN-1, THREAD_CREATE_STACKTEST,
				                          vibrate_thread, NULL, "vibrate_thread");
    return vibrate_thread_pid;
}


/**
 * @brief start udp receiver thread
 *
 * @return PID of coap thread
 */
int coap_start_thread(void)
{
    // init coap endpoints
    setup_endpoints();
    // start thread
    return thread_create(coap_thread_stack, sizeof(coap_thread_stack),
                         THREAD_PRIORITY_MAIN, THREAD_CREATE_STACKTEST,
                         coap_thread, NULL, "coap_thread");
}
