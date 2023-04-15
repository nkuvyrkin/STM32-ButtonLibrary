/*
 * Button.h
 *
 *  Created on: Apr 13, 2023
 *      Author: mantis
 *
 *  Button library for STM32
 */

#ifndef INC_BUTTON_H_
#define INC_BUTTON_H_

#include <stdbool.h>
#include "main.h"

// Pressed button pin state configuration: 1 - LOW, 0 - HIGH
#define BUTTON_PULLUP 1

// Max number of button
#define MAX_BUTTONS 3

// Defines the number of ticks before a value is considered legitimate
// Debounce time
#define DBNC_COUNTER_MAX 30
// Long press detected if button pressed longer than this constant
#define LONG_PRESS_DELAY 1000
// Very long press detected if button pressed longer than this constant
#define VERY_LONG_PRESS_DELAY LONG_PRESS_DELAY + 2000
// Double press detection max time
#define DOUBLE_PRESS_MAX_DELAY 600


struct button {
    void (*init)(void);
    bool (*register_button)(
		GPIO_TypeDef* gpio_port,
		uint16_t gpio_pin,
		void (*button_short_callback)(void),
		void (*button_long_callback)(void),
		void (*button_very_long_callback)(void),
		void (*button_double_callback)(void)
    );
    void (*loop)(void);
    bool (*signal_state_change)(uint16_t gpio_pin);
};

extern const struct button Button;

#endif /* INC_BUTTON_H_ */
