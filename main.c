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
#include "chprintf.h"

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


void queueInit(void);
void enqueue(msg_t msg);
msg_t dequeue(void);
uint8_t button_check(ioline_t line, uint8_t button);
void vt_cb(void *arg);

enum
{
  SECUNDARIO = 1,
  PEDESTRE,
  AMB_PRIMARIO,
  AMB_SECUNDARIO,
  PRIMARIO
};

enum
{
  VERDE_AMB_PRIM,
  VERDE_LOCKED_PRIM,
  VERDE_IDLE_PRIM,
  AMARELO_PED_PRIM,
  AMARELO_SEC_PRIM,
  VERDE_AMB_SEC,
  VERDE_LOCKED_SEC,
  AMARELO_PED_SEC,
  AMARELO_PRIM_SEC,
  VERDE_AMB_PED,
  VERDE_LOCKED_PED,
  PISCANDO_SEC,
  PISCANDO_PRIM
};

/* Estado inicial */
uint8_t g_state = VERDE_LOCKED_PRIM;

/* Flags timers */
uint8_t main_vt_flag = 0;

/* Flags ambulancias */
uint8_t flag_amb_prim, flag_amb_sec;
uint8_t prev_state = PEDESTRE;

/*
 * LED blinker thread, times are in milliseconds.
 */

static THD_WORKING_AREA(waThread1, 32);
static THD_FUNCTION(Thread1, arg)
{
  msg_t ev;
  virtual_timer_t main_vt;

  chVTObjectInit(&main_vt);
  
  while (1)
  {
    switch (g_state) {
      case VERDE_LOCKED_PRIM:
        palClearLine(PRIMARIO_VERMELHO);
        palSetLine(PRIMARIO_VERDE);
        palSetLine(SECUNDARIO_VERMELHO);
        palSetLine(PEDESTRE_VERMELHO);
        if (prev_state == SECUNDARIO) {
          chVTSet(&main_vt, TIME_MS2I(5000), (vtfunc_t)vt_cb, (void *)&main_vt);
          while (!main_vt_flag) {
            if ((*rdp == AMB_PRIMARIO)) {
              flag_amb_prim = !flag_amb_prim;
            }
            chThdSleepMilliseconds(100);
          }
        }
        main_vt_flag = 0;
        chVTSet(&main_vt, TIME_MS2I(prev_state == SECUNDARIO ? 5000 : 10000), (vtfunc_t)vt_cb, (void *)&main_vt);
        while (!main_vt_flag) {
          if ((*rdp == AMB_SECUNDARIO)) {
            chVTReset(&main_vt);
            main_vt_flag = 1;
          }
          chThdSleepMilliseconds(100);
        }
        main_vt_flag = 0;
        g_state = VERDE_IDLE_PRIM;
        break;
      case VERDE_IDLE_PRIM:
        ev = dequeue();
        if (ev == AMB_PRIMARIO) {
          flag_amb_prim = !flag_amb_prim;
        }
        if ((ev == SECUNDARIO || ev == AMB_SECUNDARIO) &! flag_amb_prim) {
          if (ev == AMB_SECUNDARIO) {
            flag_amb_sec = 1;
          }
          g_state = AMARELO_SEC_PRIM;
          palClearLine(PRIMARIO_VERDE);
        } else if (ev == PEDESTRE &! flag_amb_prim) {
          g_state = AMARELO_PED_PRIM;
          palClearLine(PRIMARIO_VERDE);
        }
        break;
      case AMARELO_SEC_PRIM:
        palSetLine(PRIMARIO_AMARELO);
        chVTSet(&main_vt, TIME_MS2I(2000), (vtfunc_t)vt_cb, (void *)&main_vt);
        while (!main_vt_flag) {
          chThdSleepMilliseconds(100);
        }
        main_vt_flag = 0;
        g_state = VERDE_LOCKED_SEC;
        palClearLine(PRIMARIO_AMARELO);
        palSetLine(PRIMARIO_VERMELHO);
        prev_state = PRIMARIO;
        break;
      case AMARELO_PED_PRIM:
        palSetLine(PRIMARIO_AMARELO);
        chVTSet(&main_vt, TIME_MS2I(2000), (vtfunc_t)vt_cb, (void *)&main_vt);
        while (!main_vt_flag) {
          chThdSleepMilliseconds(100);
        }
        main_vt_flag = 0;
        g_state = VERDE_LOCKED_PED;
        palClearLine(PRIMARIO_AMARELO);
        palSetLine(PRIMARIO_VERMELHO);
        prev_state = PRIMARIO;
        break;
      case VERDE_LOCKED_SEC:
        palClearLine(SECUNDARIO_VERMELHO);
        palSetLine(SECUNDARIO_VERDE);
        if (prev_state == PRIMARIO) {
          chVTSet(&main_vt, TIME_MS2I(5000), (vtfunc_t)vt_cb, (void *)&main_vt);
          while (!main_vt_flag) {
            if ((*rdp == AMB_SECUNDARIO) && flag_amb_sec == 0) {
              dequeue();
              flag_amb_sec = 1;
            } else if ((*rdp == AMB_PRIMARIO) && flag_amb_prim == 0) {
              dequeue();
              flag_amb_prim = 1;
            }
            chThdSleepMilliseconds(100);
          }
        }
        main_vt_flag = 0;
        chVTSet(&main_vt, TIME_MS2I((prev_state == PRIMARIO) ? 1000 : 6000), (vtfunc_t)vt_cb, (void *)&main_vt);
        while (!main_vt_flag) {
          if ((*rdp == AMB_PRIMARIO)) {
            flag_amb_prim = 1;
            chVTReset(&main_vt);
            main_vt_flag = 1;
          } else if ((*rdp == AMB_SECUNDARIO) && flag_amb_sec == 0) {
            dequeue();
            flag_amb_sec = 1;
          }
          chThdSleepMilliseconds(100);
        }
        main_vt_flag = 0;
        while (flag_amb_sec) {
          ev = dequeue();
          if (ev == AMB_SECUNDARIO) {
            flag_amb_sec = 0;
          }
        }
        if (qsize > 0) {
          if (*rdp != SECUNDARIO) {
            ev = dequeue(); /* Caso esse evento nao seja um dos esperados por esse estado, será perdido (CONFERIR SE É ASSIM) */
          }
        }
        if (ev == PEDESTRE &! flag_amb_prim) {
          g_state = AMARELO_PED_SEC;
          palClearLine(SECUNDARIO_VERDE);
        } else {
          g_state = AMARELO_PRIM_SEC;
          palClearLine(SECUNDARIO_VERDE);
        }
        break;
      case AMARELO_PRIM_SEC:
        palSetLine(SECUNDARIO_AMARELO);
        chVTSet(&main_vt, TIME_MS2I(2000), (vtfunc_t)vt_cb, (void *)&main_vt);
        while (!main_vt_flag) {
          chThdSleepMilliseconds(100);
        }
        main_vt_flag = 0;
        g_state = VERDE_LOCKED_PRIM;
        palClearLine(SECUNDARIO_AMARELO);
        palSetLine(SECUNDARIO_VERMELHO);
        prev_state = SECUNDARIO;
        break;
      case AMARELO_PED_SEC:
        palSetLine(SECUNDARIO_AMARELO);
        chVTSet(&main_vt, TIME_MS2I(2000), (vtfunc_t)vt_cb, (void *)&main_vt);
        while (!main_vt_flag) {
          chThdSleepMilliseconds(100);
        }
        main_vt_flag = 0;
        g_state = VERDE_LOCKED_PED;
        palClearLine(SECUNDARIO_AMARELO);
        palSetLine(SECUNDARIO_VERMELHO);
        prev_state = SECUNDARIO;
        break;
      case VERDE_LOCKED_PED:
        palClearLine(PEDESTRE_VERMELHO);
        palSetLine(PEDESTRE_VERDE);
        chVTSet(&main_vt, TIME_MS2I(3000), (vtfunc_t)vt_cb, (void *)&main_vt);
        while (!main_vt_flag) {
          if ((*rdp == AMB_PRIMARIO)) {
            flag_amb_prim = 1;
            //chVTReset(&main_vt);
            //main_vt_flag = 1;
          } else if ((*rdp == AMB_SECUNDARIO)) {
            flag_amb_sec = 1;
            //chVTReset(&main_vt);
            //main_vt_flag = 1;
          }
          chThdSleepMilliseconds(100);
        }
        main_vt_flag = 0;
        if (qsize > 0) {
          if (*rdp != PEDESTRE) {
            ev = dequeue(); /* Caso esse evento nao seja um dos esperados por esse estado, será perdido (CONFERIR SE É ASSIM) */
          }
        }
        if (ev == SECUNDARIO || ev == AMB_SECUNDARIO) {
          g_state = PISCANDO_SEC;
          palClearLine(PEDESTRE_VERDE);
        } else {
          g_state = PISCANDO_PRIM;
          palClearLine(PEDESTRE_VERDE);
        }
        break;
      case PISCANDO_SEC:
        chVTSet(&main_vt, TIME_MS2I(2000), (vtfunc_t)vt_cb, (void *)&main_vt);
        while (!main_vt_flag) {
          palToggleLine(PEDESTRE_VERMELHO);
          chThdSleepMilliseconds(125);
        }
        main_vt_flag = 0;
        g_state = VERDE_LOCKED_SEC;
        palSetLine(PEDESTRE_VERMELHO);
        prev_state = PEDESTRE;
        break;
      case PISCANDO_PRIM:
        chVTSet(&main_vt, TIME_MS2I(2000), (vtfunc_t)vt_cb, (void *)&main_vt);
        while (!main_vt_flag) {
          palToggleLine(PEDESTRE_VERMELHO);
          chThdSleepMilliseconds(125);
        }
        main_vt_flag = 0;
        g_state = VERDE_LOCKED_PRIM;
        palSetLine(PEDESTRE_VERMELHO);
        prev_state = PEDESTRE;
        break;
      default:
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
  SerialConfig config = {.sc_brr = UBRR2x(9600),
                         .sc_bits_per_char = USART_CHAR_SIZE_8
  };

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

  sdStart(&SD1, &config);

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
    if (button_check(AMB_SEC, 1))
    {
      enqueue(AMB_SECUNDARIO);
    }
    else if (button_check(AMB_PRIM, 2))
    {
      enqueue(AMB_PRIMARIO);
    }
    else if (button_check(SEC_FLAG, 3))
    {
      enqueue(SECUNDARIO);
    }
    else if (button_check(PED_FLAG, 4))
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

/* Ficou muito hard coded */
uint8_t button_check(ioline_t line, uint8_t button) {
  static uint8_t x1, old_x1, x2, old_x2, x3, old_x3, x4, old_x4;
  uint8_t w, x;

  if (button == 1) {
    x1 = palReadLine(line);
    w = x1^old_x1;
    old_x1 = x1;
    x = x1;
  } else if (button == 2) {
    x2 = palReadLine(line);
    w = x2^old_x2;
    old_x2 = x2;
    x = x2;
  } else if (button == 3) {
    x3 = palReadLine(line);
    w = x3^old_x3;
    old_x3 = x3;
    x = x3;
  } else if (button == 4) {
    x4 = palReadLine(line);
    w = x4^old_x4;
    old_x4 = x4;
    x = x4;
  }
  
  return w &! x;
}

void vt_cb(void *arg)
{
  chSysLockFromISR();
  main_vt_flag = 1;
  chSysUnlockFromISR();
}
