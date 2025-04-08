#include "hardware/pio.h"

#include "pio.h"

// Assembled program
#include "pcm.pio.h"

/**
 * @brief Initialize PIO for parallel PCM output
 * @param pio PIO instance (pio0 or pio1)
 * @param sm State machine index (0-3)
 * @param offset Location in PIO memory where program is loaded
 * @param base_pin First GPIO pin for parallel output
 * @param n_bits Number of parallel bits (1-32)
 * @param clk_div Clock divider (1.0 for full speed)
 */

void pcm_program_init(PIO pio, uint sm, uint offset, uint base_pin, uint n_bits) {

    // Validate parameters
    if (n_bits < 1 || n_bits > 32) {
        panic("Invalid number of bits (1-32 allowed)");
    }
    if (base_pin + n_bits > 30) {
        panic("GPIO pins would exceed maximum");
    }

    // Initialize GPIO pins
    for (uint pin = base_pin; pin < base_pin + n_bits; pin++) {
        pio_gpio_init(pio, pin);
        gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_8MA); // Increased drive strength
        gpio_set_slew_rate(pin, GPIO_SLEW_RATE_FAST);          // Fast slew rate
    }

    pio_sm_set_consecutive_pindirs(pio, sm, base_pin, n_bits, true);

    // Get default config
    pio_sm_config c = pcm_program_get_default_config(offset);

    // Configure output pins
    sm_config_set_out_pins(&c, base_pin, n_bits);

    //set up autopull
    sm_config_set_out_shift(&c, true, true, 32);

    // Initialize the state machine
    pio_sm_init(pio, sm, offset, &c);

    // Enable the state machine
    pio_sm_set_enabled(pio, sm, true);
}
