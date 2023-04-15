/*
 * Button.c
 *
 *  Created on: Apr 13, 2023
 *      Author: mantis
 *
 *  Button library for STM32
 */

#include <string.h>
#include "Button.h"

#if BUTTON_PULLUP == 1
const GPIO_PinState RELEASE_STATE = GPIO_PIN_SET;
#else
const GPIO_PinState RELEASE_STATE = GPIO_PIN_RESET;
#endif

enum ret_codes {
	ok,
    repeat,
	to_very_long_press,
	to_long_press,
	to_short_press,
	to_double_press
};

/* enum state_codes and array state[] must be in sync! */
enum state_codes {
    button_down,
    button_up,
    very_long_press,
    long_press,
    short_press,
    double_press,
    stop
};

struct button_config {
    GPIO_TypeDef* gpio_port;
    uint16_t gpio_pin;
    void (*button_short_callback)(void);
    void (*button_long_callback)(void);
    void (*button_very_long_callback)(void);
    void (*button_double_callback)(void);
    enum state_codes last_state;
    enum state_codes cur_state;
    GPIO_PinState state;
    uint32_t elapsed_time;
    uint32_t debounce_time;
};

static enum ret_codes button_down_handler(struct button_config* button);
static enum ret_codes button_up_handler(struct button_config* button);
static enum ret_codes very_long_press_handler(struct button_config* button);
static enum ret_codes long_press_handler(struct button_config* button);
static enum ret_codes short_press_handler(struct button_config* buttonid);
static enum ret_codes double_press_handler(struct button_config* button);

/* array state[] and enum state_codes must be in sync! */
enum ret_codes (*state[])(struct button_config*) = {
    button_down_handler,
	button_up_handler,
	very_long_press_handler,
    long_press_handler,
	short_press_handler,
    double_press_handler
};

struct transition {
    enum state_codes src_state;
    enum ret_codes ret_code;
    enum state_codes dst_state;
};

struct transition state_transitions[] = {

    { button_down, repeat, button_down },
    { button_down, ok, button_up },

	{ button_up, repeat, button_up },

	{ button_up, to_very_long_press, very_long_press },
	{ button_up, to_long_press, long_press },
	{ button_up, to_short_press, short_press },
	{ button_up, to_double_press, double_press },

    { short_press, ok, stop },

    { double_press, ok, stop },
    { double_press, repeat, double_press },

    { long_press, ok, stop },

    { very_long_press, ok, stop }

};

static unsigned char registered_buttons = 0;
static struct button_config button_configs[MAX_BUTTONS];

/*
 * Library Initialization function
 *
 */
static void init()
{
    memset(button_configs, 0, sizeof(button_configs));
    registered_buttons = 0;
}

static enum state_codes lookup_transitions(enum state_codes cur_state, enum ret_codes rc)
{
    int i;
    int max_items;
    enum state_codes next_state;

    i = 0;
    max_items = sizeof(state_transitions) / sizeof(state_transitions[0]);
    next_state = stop;

    for (i = 0; i < max_items; i++) {
        if ((state_transitions[i].src_state == cur_state) && (state_transitions[i].ret_code == rc)) {
            next_state = state_transitions[i].dst_state;
            break;
        }
    }

    return (next_state);
}

static enum ret_codes button_down_handler(struct button_config* button)
{
	if (button->last_state != button_down)
		button->elapsed_time = HAL_GetTick();
	if (button->state == RELEASE_STATE)
		return ok;
    return repeat;
}

static enum ret_codes button_up_handler(struct button_config* button)
{
	if (HAL_GetTick() - button->elapsed_time > VERY_LONG_PRESS_DELAY)
		return to_very_long_press;
	if (HAL_GetTick() - button->elapsed_time > LONG_PRESS_DELAY)
		return to_long_press;
	if (HAL_GetTick() - button->elapsed_time > DOUBLE_PRESS_MAX_DELAY)
		return to_short_press;
	if (button->state != RELEASE_STATE)
		return to_double_press;
	return repeat;
}

static enum ret_codes very_long_press_handler(struct button_config* button)
{
	if (button->button_very_long_callback != NULL)
		button->button_very_long_callback();
	return ok;
}

static enum ret_codes long_press_handler(struct button_config* button)
{
	if (button->button_long_callback != NULL)
		button->button_long_callback();
	return ok;
}

static enum ret_codes short_press_handler(struct button_config* button)
{
	if (button->button_short_callback != NULL)
		button->button_short_callback();
    return ok;
}

static enum ret_codes double_press_handler(struct button_config* button)
{
	if (button->state != RELEASE_STATE)
		return repeat;
	if (button->button_double_callback != NULL)
		button->button_double_callback();
    return ok;
}

/*
 * Main loop function
 */
static void loop()
{
    unsigned char i = 0;
    for (i = 0; i < registered_buttons; i++) {
        struct button_config* button = &button_configs[i];
        // is debounce over?
        if (button->debounce_time != 0 && HAL_GetTick() > button->debounce_time) {
            button->debounce_time = 0;
            GPIO_PinState state = HAL_GPIO_ReadPin(button->gpio_port, button->gpio_pin);

            // did state change
            if (state != button->state) {
                // update state
                button->state = state;
                // if state is stop and button is down, start fsm
                if (state != RELEASE_STATE && button->cur_state == stop)
                    button->cur_state = button_down;
            }
        }
        // if debounce has elapsed and we're stopped continue
        else if (button->cur_state != stop) {
            enum ret_codes (*state_fun)(struct button_config*) = state[button->cur_state];
            enum ret_codes return_code = state_fun(button);
            button->last_state = button->cur_state;
            button->cur_state = lookup_transitions(button->cur_state, return_code);
        }
    }
}

/*
 * Register button function
 */
static bool register_button(
	GPIO_TypeDef* gpio_port,
	uint16_t gpio_pin,
	void (*button_short_callback)(void),
	void (*button_long_callback)(void),
	void (*button_very_long_callback)(void),
	void (*button_double_callback)(void)
)
{
    if (registered_buttons < MAX_BUTTONS) {
        struct button_config* config = &button_configs[registered_buttons++];
        config->gpio_port = gpio_port;
        config->gpio_pin = gpio_pin;
        config->button_short_callback = button_short_callback;
        config->button_long_callback = button_long_callback;
        config->button_very_long_callback = button_very_long_callback;
        config->button_double_callback = button_double_callback;
        config->last_state = stop;
        config->cur_state = stop;
        config->state = HAL_GPIO_ReadPin(gpio_port, gpio_pin);
        return true;
    }

    return false;
}

int find_button(uint16_t gpio_pin)
{
    unsigned char i = 0;
    for (i = 0; i < registered_buttons; i++) {
        if (button_configs[i].gpio_pin == gpio_pin) {
            return i;
        }
    }
    return -1;
}

/*
 * Interrupt handler for pin state change monitoring
 */
static bool signal_state_change(uint16_t gpio_pin)
{
    int button_index = find_button(gpio_pin);
    if (button_index != -1) {
        struct button_config* signaled_button = &button_configs[button_index];
        signaled_button->debounce_time = HAL_GetTick() + DBNC_COUNTER_MAX;
        return true;
    }
    return false;
}

const struct button Button = {
    .init = init,
    .loop = loop,
    .register_button = register_button,
    .signal_state_change = signal_state_change
};
