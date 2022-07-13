/******************************************************************************/
/*                                                                            */
/* main.c -- Example program using the PmodMAXSONAR IP                        */
/*                                                                            */
/******************************************************************************/
/* Author: Arvin Tang                                                         */
/*                                                                            */
/******************************************************************************/
/* File Description:                                                          */
/*                                                                            */
/* This demo continuously polls the PmodMAXSONAR for distance and prints the  */
/* distance.                                                                  */
/*                                                                            */
/******************************************************************************/
/* Revision History:                                                          */
/*                                                                            */
/*    10/18/2017(atangzwj): Created                                           */
/*    01/20/2018(atangzwj): Validated for Vivado 2017.4                       */
/*                                                                            */
/******************************************************************************/

/************ Include Files ************/

#include "PmodMAXSONAR.h"
#include "sleep.h"
#include "xil_cache.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xtime_l.h"
#include "stdbool.h"

/************ Macro Definitions ************/

#define PMOD_MAXSONAR_BASEADDR XPAR_PMODMAXSONAR_0_AXI_LITE_GPIO_BASEADDR

#ifdef __MICROBLAZE__
#define CLK_FREQ XPAR_CPU_M_AXI_DP_FREQ_HZ
#else
#define CLK_FREQ 100000000 // FCLK0 frequency not found in xparameters.h
#endif

#define LED 0x04   /* Assumes bit 0 of GPIO is connected to an LED  */

#define GPIO_EXAMPLE_DEVICE_ID  XPAR_GPIO_0_DEVICE_ID

#define LED_CHANNEL 1


/************ Global Variables ************/

PmodMAXSONAR myDevice;


/************ Function Prototypes ************/

void DemoInitialize();

void DemoRun();

void DemoCleanup();

void EnableCaches();

void DisableCaches();

u64 now() {
	XTime cur;
	XTime_GetTime(&cur);
	return cur / COUNTS_PER_SECOND;
}


/************ Function Definitions ************/

int main(void) {
   DemoInitialize();
   DemoRun();
   DemoCleanup();
   return 0;
}

XGpio Gpio;

typedef enum DeskState {
	Sitting = 0,
	Standing = 1,
} DeskState_t;
DeskState_t currentDeskState = Sitting;

bool isStanding(u64 dist) {
	const u64 standingMin = 25;

	return dist <= standingMin;
}

typedef enum led {
	RED =1,
	GREEN,
	BLUE,
}led_t;

void setLED(led_t color) {
	  XGpio_DiscreteWrite(&Gpio, LED_CHANNEL, color);
}

void DemoInitialize() {
   EnableCaches();
   MAXSONAR_begin(&myDevice, PMOD_MAXSONAR_BASEADDR, CLK_FREQ);
   int Status;

	/* Initialize the GPIO driver */
	Status = XGpio_Initialize(&Gpio, GPIO_EXAMPLE_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("Gpio Initialization Failed\r\n");
		return;
	}

	/* Set the direction for all signals as inputs except the LED output */
	XGpio_SetDataDirection(&Gpio, LED_CHANNEL, ~(0b11));
	setLED(RED);

	xil_printf("\r\nstarting\r\n");
}

void DemoRun() {
   u32 dist;
   u64 cur;
   const u64 graceSeconds = 1500; // 25 minutes
   const u64 standingSeconds = 300; // 5 minutes
   u64 startedSitting = 0, startedStanding = 0;
   bool notified = false, standingMet = false;

   while (1) {
      dist = MAXSONAR_getDistance(&myDevice);
	  cur = now();

      DeskState_t measuredDeskState = isStanding(dist) ? Standing : Sitting;

      if (measuredDeskState != currentDeskState) {
    	  xil_printf("transitioned: %s\r\n", measuredDeskState == Sitting ? "sitting":"standing");
    	  xil_printf("%d\r\n", dist);
    	  notified = false;
    	  cur = now();
    	  switch(measuredDeskState) {
    	  case Sitting:
    		  if (standingMet == false) {
    			  setLED(RED);
    			  notified = true;
    		  } else {
    			  standingMet = false;
    		  }
    		  startedSitting = now();
    		  break;
    	  case Standing:
    		  startedStanding = now();
    		  break;
    	  default:
    		  break;
    	  }

    	  currentDeskState = measuredDeskState;

    	  continue;
      } else if (measuredDeskState == Sitting && !notified) {
    	  xil_printf("sitting: %u\r\n", (int)(cur-startedSitting));
    	  if (cur - startedSitting > graceSeconds) {
			  setLED(RED);
			  xil_printf("notified\r\n");
			  notified = true;
    	  }
      } else if (measuredDeskState == Standing && !standingMet ) {
		  if (cur - startedStanding > standingSeconds) {
    		  standingMet = true;
    		  setLED(GREEN);
    	  }
      }

      // need to stand for at least 5 minutes, otherwise
      // sitting down immediately after standing causes
      // the led to go red immediately as well

      sleep(1);
   }
}

void DemoCleanup() {
   DisableCaches();
}

void EnableCaches() {
#ifdef __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_ICACHE
   Xil_ICacheEnable();
#endif
#ifdef XPAR_MICROBLAZE_USE_DCACHE
   Xil_DCacheEnable();
#endif
#endif
}

void DisableCaches() {
#ifdef __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_DCACHE
   Xil_DCacheDisable();
#endif
#ifdef XPAR_MICROBLAZE_USE_ICACHE
   Xil_ICacheDisable();
#endif
#endif
}
