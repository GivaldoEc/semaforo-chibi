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
#define QUEUE_SIZE 128

/* Definições de pinos*/
// LEDS
#define PEDESTRE_VERDE PAL_LINE(IOPORT2, 3)
#define PEDESTRE_VERMELHO PAL_LINE(IOPORT2, 2)
#define PRIMARIO_VERDE PAL_LINE(IOPORT4, 7)
#define PRIMARIO_AMARELO PAL_LINE(IOPORT4, 4)
#define PRIMARIO_VERMELHO PAL_LINE(IOPORT4, 6)
#define SECUNDARIO_VERDE PAL_LINE(IOPORT2, 1)
#define SECUNDARIO_AMARELO PAL_LINE(IOPORT4, 5)
#define SECUNDARIO_VERMELHO PAL_LINE(IOPORT2, 0)

// Botões
#define AMB_PRIM PAL_LINE(IOPORT3, 3)
#define AMB_SEC PAL_LINE(IOPORT3, 5)
#define SEC_FLAG PAL_LINE(IOPORT3, 4)
#define PED_FLAG PAL_LINE(IOPORT3, 2)

// Buffer
static msg_t queue[QUEUE_SIZE], *rdp, *wrp;
static size_t qsize;
static mutex_t qmtx;
static condition_variable_t qempty;
static condition_variable_t qfull;


typedef struct
{
  uint8_t events[BUFFER_SIZE];
  uint8_t head;
  uint8_t tail;
  uint8_t size;
} EventBuffer;

EventBuffer ev_buffer;

void queueInit(void);
void enqueue(msg_t msg);
msg_t dequeue(void);
void bufferPush(EventBuffer *cb, uint8_t event);
uint8_t bufferPop(EventBuffer *cb);
void vt_cb(void *arg);

enum
{
  SECUNDARIO = 1,
  PEDESTRE,
  AMB_PRIMARIO,
  AMB_SECUNDARIO
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

  msg_t ev;
  while (1)
  {
    ev = dequeue();
    if (ev == AMB_PRIMARIO){
      palSetLine(PRIMARIO_VERDE);
    } else if (ev == AMB_SECUNDARIO){
      palToggleLine(SECUNDARIO_VERDE);
    } else if (ev == SECUNDARIO){
      palSetLine(SECUNDARIO_VERMELHO);
    } else if (ev == PEDESTRE){
      palSetLine(PEDESTRE_VERDE);
    }
    chThdSleepMilliseconds(100);
  }
}

/*
 * Application entry point.
 */
int main(void)
{
  queueInit();
  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /* Configuração dos botões */
  palSetLineMode(PED_FLAG, PAL_MODE_INPUT_PULLUP);
  palSetLineMode(SEC_FLAG, PAL_MODE_INPUT_PULLUP);
  palSetLineMode(AMB_PRIM, PAL_MODE_INPUT_PULLUP);
  palSetLineMode(AMB_SEC, PAL_MODE_INPUT_PULLUP);

  // Pedestre
  palSetLineMode(PEDESTRE_VERDE, PAL_MODE_OUTPUT_PUSHPULL);
  palClearLine(PEDESTRE_VERDE);
  palSetLineMode(PEDESTRE_VERMELHO, PAL_MODE_OUTPUT_PUSHPULL);
  palClearLine(PEDESTRE_VERMELHO);

  // Primário
  palSetLineMode(PRIMARIO_VERDE, PAL_MODE_OUTPUT_PUSHPULL);
  palClearLine(PRIMARIO_VERDE);
  palSetLineMode(PRIMARIO_AMARELO, PAL_MODE_OUTPUT_PUSHPULL);
  palClearLine(PRIMARIO_AMARELO);
  palSetLineMode(PRIMARIO_VERMELHO, PAL_MODE_OUTPUT_PUSHPULL);
  palClearLine(PRIMARIO_VERMELHO);

  // Secundário
  palSetLineMode(SECUNDARIO_VERDE, PAL_MODE_OUTPUT_PUSHPULL);
  palClearLine(SECUNDARIO_VERDE);
  palSetLineMode(SECUNDARIO_AMARELO, PAL_MODE_OUTPUT_PUSHPULL);
  palClearLine(SECUNDARIO_AMARELO);
  palSetLineMode(SECUNDARIO_VERMELHO, PAL_MODE_OUTPUT_PUSHPULL);
  palClearLine(SECUNDARIO_VERMELHO);

  /*
   * Starts the LED blinker thread.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  while (1)
  {
    if (palReadLine(AMB_SEC) == PAL_LOW)
    {
      enqueue(AMB_SECUNDARIO);
    }
    else if (palReadLine(AMB_PRIM) == PAL_LOW)
    {
      enqueue(AMB_PRIMARIO);
    }
    else if (palReadLine(SEC_FLAG) == PAL_LOW)
    {
      enqueue(SECUNDARIO);
    }
    else if (palReadLine(PED_FLAG) == PAL_LOW)
    {
      enqueue(PEDESTRE);
    }
    /* Debouncing. */
    chThdSleepMilliseconds(50);
  }
}

/*
 * Synchronized queue initialization.
 */
void queueInit(void) {
 
  chMtxObjectInit(&qmtx);
  chCondObjectInit(&qempty);
  chCondObjectInit(&qfull);
 
  rdp = wrp = &queue[0];
  qsize = 0;
}

/*
 * Writes a message into the queue, if the queue is full waits
 * for a free slot.
 */
void enqueue(msg_t msg) {
 
  /* Entering monitor.*/
  chMtxLock(&qmtx);
 
  /* Waiting for space in the queue.*/
  while (qsize >= QUEUE_SIZE)
    chCondWait(&qfull);
 
  /* Writing the message in the queue.*/  
  *wrp = msg;
  if (++wrp >= &queue[QUEUE_SIZE])
    wrp = &queue[0];
  qsize++;
 
  /* Signaling that there is at least a message.*/
  chCondSignal(&qempty);
 
  /* Leaving monitor.*/
  chMtxUnlock(&qmtx);
}

/*
 * Reads a message from the queue, if the queue is empty waits
 * for a message.
 */
msg_t dequeue(void) {
  msg_t msg;
 
  /* Entering monitor.*/
  chMtxLock(&qmtx);
 
  /* Waiting for messages in the queue.*/
  while (qsize == 0)
    chCondWait(&qempty);
 
  /* Reading the message from the queue.*/  
  msg = *rdp;
  if (++rdp >= &queue[QUEUE_SIZE])
    rdp = &queue[0];
  qsize--;
 
  /* Signaling that there is at least one free slot.*/
  chCondSignal(&qfull);
 
  /* Leaving monitor.*/
  chMtxUnlock(&qmtx);
 
  return msg;
}

void vt_cb(void *arg)
{
  chSysLockFromISR();
  palTogglePad(IOPORT2, PORTB_LED1);
  chVTSetI((virtual_timer_t *)arg, TIME_MS2I(LED_PERIODO / 2), (vtfunc_t)vt_cb, arg);
  chSysUnlockFromISR();
}
