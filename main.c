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
#define AMB_PRIM
#define AMB_SEC
#define SEC_FLAG
#define PEDESTRE

typedef struct
{
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
void vt_cb2(void *arg);

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

  /* Configuração dos botões */

  /*
   * Starts the LED blinker thread.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  palSetLine(PEDESTRE_VERDE);

  // virtual_timer_t vt2;

  // chVTObjectInit(&vt2);
  // chVTSet(&vt2, TIME_MS2I(LED_PERIODO / 2), (vtfunc_t)vt_cb2, (void *)&vt2);

  while (1)
  {
    palToggleLine(SECUNDARIO_VERDE);
    chThdSleepMicroseconds(10000);
  }
}

void bufferInit(EventBuffer *buffer)
{
  buffer->head = 0;
  buffer->tail = 0;
  buffer->size = 0;
}

bool isBufferEmpty(EventBuffer *buffer)
{
  return buffer->size == 0;
}

bool isBufferFull(EventBuffer *buffer)
{
  return buffer->size == BUFFER_SIZE;
}

void bufferPush(EventBuffer *buffer, uint8_t event)
{
  buffer->events[buffer->tail] = event;
  buffer->tail = (buffer->tail + 1) % BUFFER_SIZE;
  buffer->size++;
}

uint8_t bufferPop(EventBuffer *buffer)
{
  if (isBufferEmpty(buffer))
  {
    return false; // Buffer vazio
  }

  uint8_t event = buffer->events[buffer->head];
  buffer->head = (buffer->head + 1) % BUFFER_SIZE;
  buffer->size--;
  return event;
}

void vt_cb(void *arg)
{
  chSysLockFromISR();
  palTogglePad(IOPORT2, PORTB_LED1);
  chVTSetI((virtual_timer_t *)arg, TIME_MS2I(LED_PERIODO / 2), (vtfunc_t)vt_cb, arg);
  chSysUnlockFromISR();
}

void vt_cb2(void *arg)
{
  chSysLockFromISR();
  palTogglePad(IOPORT2, 3);
  chVTSetI((virtual_timer_t *)arg, TIME_MS2I(LED_PERIODO / 2), (vtfunc_t)vt_cb2, arg);
  chSysUnlockFromISR();
}