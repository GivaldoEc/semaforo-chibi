/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "ch.h"
#include "hal.h"

#define LED_PERIODO 10000

void vt_cb(void *arg) {
  chSysLockFromISR();
  palTogglePad(IOPORT2, PORTB_LED1);
  chVTSetI((virtual_timer_t*) arg, TIME_MS2I(LED_PERIODO/2), (vtfunc_t) vt_cb, arg);
  chSysUnlockFromISR();
}

/*
 * LED blinker thread, times are in milliseconds.
 */
static THD_WORKING_AREA(waThread1, 32);
static THD_FUNCTION(Thread1, arg) {
  virtual_timer_t vt;

  chVTObjectInit(&vt);
  chVTSet(&vt, TIME_MS2I(LED_PERIODO/2), (vtfunc_t) vt_cb, (void*) &vt);

  while (1) {

  }
}

/*
 * Application entry point.
 */
int main(void) {
  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  palSetPadMode(IOPORT2, PORTB_LED1, PAL_MODE_OUTPUT_PUSHPULL);
  palClearPad(IOPORT2, PORTB_LED1);

  /*
   * Starts the LED blinker thread. 
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO+1, Thread1, NULL);

  while (1) {}
}
