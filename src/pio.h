#pragma once
#include "hardware/pio.h"

void pcm_program_init(PIO pio, uint sm, uint offset, uint base_pin, uint n_bits);
