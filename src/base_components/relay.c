#include "relay.h"
#include "tl_common.h"
#include "millis.h"

#define MAX_RELAY_PULSES 4  // Number of pulses allowed at the same time
#define RELAY_PULSE_DURATION 125 // bistable relay pulse duration
#define RELAY_PULSE_RETRY 50 // bistable relay pulse retry timeout

typedef struct {
  u32 pin;
  u8 value;
} relay_pulse_t;

relay_pulse_t pulse_pool[MAX_RELAY_PULSES];
u8 pulse_pool_in_use[MAX_RELAY_PULSES];
u8 pulse_active;
s32 try_pulse_on(void *arg);
void relay_pulse(u32 pin, u8 value);
relay_pulse_t* pulse_alloc(void);
void pulse_free(relay_pulse_t* p);
void deactivate_pin(u32 pin, u8 turn_off);
s32 schedule_pin_clear(void *arg);

void relay_init(relay_t *relay)
{
  pulse_active = 0;
  relay_off(relay);
}

void relay_on(relay_t *relay)
{
  printf("relay_on\r\n");

  if (relay->off_pin)
  {
    // Bi-stable relay: pulse the ON pin
    if (!relay_pulse(relay->pin, relay->on_high)) return;
  } else {
    // Normal relay: drive continuously
    drv_gpio_write(relay->pin, relay->on_high);
  }

  relay->on = 1;
  if (relay->on_change != NULL)
  {
    relay->on_change(relay->callback_param, 1);
  }
}

void relay_off(relay_t *relay)
{
  printf("relay_off\r\n");

  if (relay->off_pin)
  {
    // Bi-stable relay: pulse the OFF pin
    if (!relay_pulse(relay->off_pin, relay->on_high)) return;
  } else {
    // Normal relay: turn OFF
    drv_gpio_write(relay->pin, !relay->on_high);
  }

  relay->on = 0;
  if (relay->on_change != NULL)
  {
    relay->on_change(relay->callback_param, 0);
  }
}

void relay_toggle(relay_t *relay)
{
  printf("relay_toggle\r\n");
  if (relay->on)
  {
    relay_off(relay);
  }
  else
  {
    relay_on(relay);
  }
}

s32 try_pulse_on(void *arg) {
  printf("try switch on pin\r\n");

  // check if an other pin is switched on
  if (pulse_active) {
    // if an other pin is switched on reschedule try_pulse_on
    return 0;
  }

  pulse_active = 1;
  relay_pulse_t *pulse = (relay_pulse_t *)arg;
  drv_gpio_write(pulse->pin, pulse->value);

  // schedule pin on
  TL_ZB_TIMER_SCHEDULE(schedule_pin_clear, pulse, RELAY_PULSE_DURATION);
  return -1;
}

void relay_pulse(u32 pin, u8 value) {
  relay_pulse_t *pulse = pulse_alloc();
  if (!pulse) return false;

  pulse->pin = pin;
  pulse->value = value;

  // try to switch on pin
  if (try_pulse_on(pulse) >= 0) {
    // if return is not -1 schedule switch on try
    printf("schedule pulse\r\n");
    TL_ZB_TIMER_SCHEDULE(try_pulse_on, pulse, RELAY_PULSE_RETRY);
  }

  return true;
}

s32 schedule_pin_clear(void *arg)
{
  printf("schedule_pin clear\r\n");
  relay_pulse_t *pulse = (relay_pulse_t *)arg;
  drv_gpio_write(pulse->pin, !pulse->value);
  pulse_free(pulse);
  pulse_active = 0;
  return -1;
}

relay_pulse_t* pulse_alloc(void) {
  for (int i = 0; i < MAX_RELAY_PULSES; ++i) {
    if (!pulse_pool_in_use[i]) {
      pulse_pool_in_use[i] = 1;
      return &pulse_pool[i];
    }
  }
  return NULL;
}

void pulse_free(relay_pulse_t* p) {
  for (int i = 0; i < MAX_RELAY_PULSES; ++i) {
    if (&pulse_pool[i] == p) {
      pulse_pool_in_use[i] = 0;
      return;
    }
  }
}
