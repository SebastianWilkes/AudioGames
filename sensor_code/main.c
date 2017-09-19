/**
 * @ingroup     PO AudioGames SS2016 Haptisches Feedback durch "Datenhandschuh" (am Beispiel von smartuni)
 * @{
 *
 * @file
 * @brief       Main control loop
 *
 * @author      smlng <s@mlng.net>, Sebastian Wilkes <sebastian.wilkes@haw-hamburg.de>
 *
 * @}
 */



// standard
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "thread.h"


// riot
#include "board.h"
#include <net/af.h>
#include "net/gnrc.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/udp.h"
#include "net/gnrc/ipv6.h"

#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "net/gnrc/pktdump.h"
#include "timex.h"
#include "xtimer.h"

#include "net/uhcp.h"
#include "board.h"
#include "saul/periph.h"

#include "periph/gpio.h"
#include "shell.h"
#include "coap.h"

// own
#include "pins.h"

// lowpan configuration
#define COMM_PAN           (0x2804) // lowpan ID
#define COMM_CHAN          (16U)  	// channel

// compatibility
#ifndef LED_ON
#define LED_ON      LED0_ON
#define LED_OFF     LED0_OFF
#define LED_TOGGLE  LED0_TOGGLE
#endif

// udp communication
#define SPORT               (1234)
#define UDP_PORT            (5683)

// board usage
#ifndef USAGE
#define USAGE 	"LEFTHAND"
#endif

int vibrate_pid = -1;
int coap_pid = -1;
char c;

long long current_time = 0;
long long old_time = 0;
long long debounce_delay = 200;

extern int coap_start_thread(void);
extern int vibrate_start_thread(void);
extern int coap_ext_build_PUT(uint8_t *buf, size_t *buflen, const char *payload, const coap_endpoint_path_t *path);
extern int send_coap_put(uint8_t*, size_t);
static int comm_init(void)
{
    kernel_pid_t ifs[GNRC_NETIF_NUMOF];
    uint16_t pan = COMM_PAN;
    uint16_t chan = COMM_CHAN;

    /* get the PID of the first radio */
    if (gnrc_netif_get(ifs) <= 0) {
        puts("ERROR: comm init, not radio found!\n");
        return (-1);
    }

	/* initialize the vibration pin */
	if (gpio_init(GPIO_PIN(0, 1), GPIO_OUT) < 0) {
        printf("ERROR: comm_init, cannot initialize GPIO_PIN(%i, %02i)\n", VBR_PORT, VBR_PIN);
        return (-1);
    }

    /* initialize the radio */
    gnrc_netapi_set(ifs[0], NETOPT_NID, 0, &pan, 2);
    gnrc_netapi_set(ifs[0], NETOPT_CHANNEL, 0, &chan, 2);
    return 0;
}


/**
 * @brief getter for current timestamp
 *
 * @return current timestamp in milliseconds
 */
long long current_timestamp_ms(void) {
    struct timeval te;
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // caculate milliseconds
    return milliseconds;
}


/**
 * @brief the callback function for a button click event
 *
 * @return void
 */
static void cb(void *arg)
{
        puts("callback");

        current_time = current_timestamp_ms();
        if((current_time - old_time) > debounce_delay){
                int buflen = sizeof("{\"type\":\"button\",\"id\":}") + sizeof(USAGE);
                char buf[buflen];
                strcpy(buf,"{\"type\":\"button\",\"id\":\"");
                strcat(buf,USAGE);
                strcat(buf,"\"}");

                send_coap_put((uint8_t *)buf, sizeof(buf));
        }else{
                puts("debounced");
        }
        old_time = current_time;
}


/**
 * @brief the main programm loop
 *
 * @return non zero on error
 */
int main(void)
{
    // some initial infos
    puts("=========================");
    puts("|| PO Audio Games 2016 ||");
    puts("|| Haptisches Feedback ||");
    puts("=========================");
    printf("You are running RIOT on a(n) %s board.\n", RIOT_BOARD);
    printf("This board features a(n) %s MCU.\n", RIOT_MCU);
    puts("=========================");

    // init 6lowpan interface
    puts(". init network");
    if (comm_init()!=0) {
        return 1;
    }

	// clear signals
    LED0_OFF;
    LED1_OFF;
    LED2_OFF;

    // start coap receiver
    puts(".. init coap handler");
    coap_pid = coap_start_thread();

    // start vibrate handler
	 puts("... init vibrate handler");
    vibrate_pid = vibrate_start_thread();

	 // init onboard button
	 gpio_init_int(BTN0_PIN, GPIO_IN_PU, GPIO_FALLING, cb, NULL);

	 // init external button
	 gpio_init_int(GPIO_PIN(BTN_PORT, BTN_PIN), GPIO_IN_PU, GPIO_FALLING, cb, NULL);

	 while(1) {
	 	 fflush(stdin);
		 // wait for something to avoid polling
		 c = getchar();
	 }
    // should be never reached
    return 0;

}
