#include <stdio.h>
#include "pico/stdlib.h"
#include <stdlib.h>
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "pio.h"
#include "pcm.pio.h"
#include <math.h>

#define BASE_PIN 2        // Starting GPIO pin
#define N_BITS 12         // Number of parallel bits
#define MAXNSAMP 4096

uint16_t *buffer;
uint16_t nsamp;
float clk_div = 1.0f;

// DMA and PIO handles
int dma_chan_0;
PIO pio = pio0;
uint sm;

// DMA interrupt handler for continuous operation
void __isr __not_in_flash_func(dma_handler)() {
    if (dma_channel_is_busy(dma_chan_0)) return;

    dma_hw->ints0 = 1u << dma_chan_0; // Clear interrupt
    
    // Restart DMA with the new buffer
    dma_channel_set_read_addr(dma_chan_0, buffer, true);
}

void dma_init(PIO pio, uint sm) {
    dma_chan_0 = dma_claim_unused_channel(true);

    dma_channel_config c0 = dma_channel_get_default_config(dma_chan_0);
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_16);  // Change to 16-bit transfers
    channel_config_set_read_increment(&c0, true);
    channel_config_set_write_increment(&c0, false);
    channel_config_set_dreq(&c0, pio_get_dreq(pio, sm, true));

    dma_channel_configure(
        dma_chan_0, &c0,
        &pio0_hw->txf[sm],           // Write to PIO TX FIFO
        buffer,                       // Initial read address
        nsamp,                        // Transfer count
        false                         // Don't start immediately
    );

    // Setup interrupt for buffer switching
    dma_channel_set_irq0_enabled(dma_chan_0, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_start(dma_chan_0);
}

float sine(float phase) {
    return sinf(2 * M_PI * phase);
}

void set_f(float freq) {
    // Free old buffer if exists and reallocate
    if (buffer) free(buffer);

    // Get system clock
    float sysclk = (float)clock_get_hz(clk_sys);

    // Calculate the number of samples based on the system clock and desired frequency
    nsamp = (uint16_t)(sysclk / (freq * 8.0f));
    nsamp = (nsamp / 4) * 4;  // Ensure nsamp is divisible by 4 for DMA alignment

    // Apply boundaries
    if (nsamp < 8) nsamp = 8;  // Ensure at least 4 samples
    if (nsamp > MAXNSAMP) nsamp = MAXNSAMP;  // Limit to MAXNSAMP

    // Calculate clock divider for the PIO
    clk_div = sysclk / (freq * nsamp * 2);  // Adjust clock divider based on the sample count and frequency
    if (clk_div < 1.0f) clk_div = 1.0f; // Ensure the divider doesn't go below 1

    // Allocate buffer for samples
    buffer = malloc(nsamp * sizeof(uint16_t));  // Use uint16_t to match the data size
    if (!buffer) {
        panic("Buffer allocation failed!\n");
        return;
    }

    // Fill buffer with sine wave data
    for (uint16_t i = 0; i < nsamp; ++i) {
        float phase = (float)i / nsamp;
        float value = sine(phase);  // Generate sine wave

        // Map to DAC range
        uint16_t sample = (uint16_t)(((value + 1.0f) / 2.0f) * ((1 << N_BITS) - 1)); 
        buffer[i] = sample;
    }

    // Apply clock divider to PIO state machine
    pio_sm_set_clkdiv(pio, sm, clk_div);

    // Set up DMA for the new buffer and sample count
    dma_channel_set_read_addr(dma_chan_0, buffer, false);
    dma_channel_set_trans_count(dma_chan_0, nsamp, true);
    dma_channel_start(dma_chan_0);

    // Calculate the actual output frequency
    float actual_freq = sysclk / (clk_div * nsamp) / 2;
    printf("Actual frequency: %.2f Hz\n", actual_freq);
}

int main() {
    stdio_init_all();

    set_sys_clock_hz(300e6, true);  // Set system clock to 300 MHz

    // Initialize PIO
    sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &pcm_program);
    pcm_program_init(pio, sm, offset, BASE_PIN, N_BITS);

    // Initialize DMA
    dma_init(pio, sm);

    // Initialize buffer
    buffer = malloc(MAXNSAMP * sizeof(uint16_t));  // Allocate the buffer once
    set_f(4e6);  // Set initial frequency

    while (1) {
        tight_loop_contents();  // Do nothing but keep the loop running
    }
}
