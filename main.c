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
#define BUFFER_SIZE 8

typedef struct {
    uint8_t events[BUFFER_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t size;
} EventBuffer;

EventBuffer ev_buffer;

void bufferInit(EventBuffer *cb);
bool isBufferEmpty(EventBuffer *buffer);
bool isBufferFull(EventBuffer *buffer);
void bufferPush(EventBuffer *cb, uint8_t event);
uint8_t bufferPop(EventBuffer *cb);
void vt_cb(void *arg);

enum {
  SECUNDARIO=1, PEDESTRE, AMB_PRIMARIO, AMB_SECUNDARIO
};

/*
 * LED blinker thread, times are in milliseconds.
 */
static THD_WORKING_AREA(waThread1, 32);
static THD_FUNCTION(Thread1, arg)
{
  virtual_timer_t vt;

  chVTObjectInit(&vt);
  chVTSet(&vt, TIME_MS2I(LED_PERIODO / 2), (vtfunc_t)vt_cb, (void *)&vt);

  while (1)
  {
  }
}

/*
 * Application entry point.
 */
int main(void)
{
  bufferInit(&ev_buffer);
  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /* Configuração dos pinos */
  // Semáforo primário: Vermelho, Verde
  palSetPadMode(IOPORT4, 7, PAL_MODE_OUTPUT_PUSHPULL);
  palClearPad(IOPORT4, 7);
  palSetPadMode(IOPORT4, 6, PAL_MODE_OUTPUT_PUSHPULL);
  palClearPad(IOPORT4, 6);

  // Semáforo secundário: Vermelho, Verde
  palSetPadMode(IOPORT2, 1, PAL_MODE_OUTPUT_PUSHPULL);
  palClearPad(IOPORT2, 1);
  palSetPadMode(IOPORT2, 0, PAL_MODE_OUTPUT_PUSHPULL);
  palClearPad(IOPORT2, 0);

  // Pedestre: Vermelho, Verde
  palSetPadMode(IOPORT2, 3, PAL_MODE_OUTPUT_PUSHPULL);
  palClearPad(IOPORT2, 3);
  palSetPadMode(IOPORT2, 2, PAL_MODE_OUTPUT_PUSHPULL);
  palClearPad(IOPORT2, 2);

  // LED Interno:
  palSetPadMode(IOPORT2, PORTB_LED1, PAL_MODE_OUTPUT_PUSHPULL);
  palClearPad(IOPORT2, PORTB_LED1);

  /* Configuração dos botões */

  /*
   * Starts the LED blinker thread.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO + 1, Thread1, NULL);

  while (1)
  {
  }
}

void bufferInit(EventBuffer *buffer) {
    buffer->head = 0;
    buffer->tail = 0;
    buffer->size = 0;
}

bool isBufferEmpty(EventBuffer *buffer) {
    return buffer->size == 0;
}

bool isBufferFull(EventBuffer *buffer) {
    return buffer->size == BUFFER_SIZE;
}

void bufferPush(EventBuffer *buffer, uint8_t event) {
    buffer->events[buffer->tail] = event;
    buffer->tail = (buffer->tail + 1) % BUFFER_SIZE;
    buffer->size++;
}

uint8_t bufferPop(EventBuffer *buffer) {
    if (isBufferEmpty(buffer)) {
        return false; // Buffer vazio
    }

    uint8_t event = buffer->events[buffer->head];
    buffer->head = (buffer->head + 1) % BUFFER_SIZE;
    buffer->size--;
    return event;
}

void vt_cb(void *arg) {
  chSysLockFromISR();
  palTogglePad(IOPORT2, PORTB_LED1);
  chVTSetI((virtual_timer_t *)arg, TIME_MS2I(LED_PERIODO / 2), (vtfunc_t)vt_cb, arg);
  chSysUnlockFromISR();
}