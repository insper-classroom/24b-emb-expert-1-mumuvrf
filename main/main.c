/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/stdlib.h"
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include "pico/binary_info.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"

#define DATA_LENGTH 33000

const int MICROPHONE_PIN = 26;
const int AUDIO_PIN = 15;
const int BTN_PIN = 16;

int audio_data[DATA_LENGTH];

int wav_position = 0;

int play = 0;
int i = 0;

volatile int recording = 0;

bool timer_0_callback(repeating_timer_t *rt) {
    float conversion_factor = 255.0 / ((1 << 12)-1);
    adc_select_input(0);

    int adc_raw = adc_read(); // raw voltage from ADC
    //printf("%.2f\n", adc_raw * ADC_CONVERT);
    audio_data[i%DATA_LENGTH] = adc_raw*conversion_factor;
    //printf("%d\n", audio_data[i%DATA_LENGTH]);
    i++;

    return true; // keep repeating
}

void btn_callback(uint gpio, uint32_t events) {
    if (events == 0x4) { // fall edge
        recording = 1;
    } else if(events == 0x8){
        recording = 0;
        i=0;
    }
}

void button_init(){
    gpio_init(BTN_PIN);
    gpio_set_dir(BTN_PIN, GPIO_IN);
    gpio_pull_up(BTN_PIN);

    gpio_set_irq_enabled_with_callback(
      BTN_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_callback);
}

void pwm_interrupt_handler() {
    pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_PIN));    
    if (wav_position < (DATA_LENGTH<<3) - 1 && play) { 
        // set pwm level 
        // allow the pwm value to repeat for 8 cycles this is >>3 
        pwm_set_gpio_level(AUDIO_PIN, audio_data[wav_position>>3]); 
        wav_position++;
    } else {
        // reset to start
        pwm_set_gpio_level(AUDIO_PIN, 0); 
        play = 0;
        wav_position = 0;
    }
}

void pwm_task(void *p){
    set_sys_clock_khz(176000, true); 
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);

    int audio_pin_slice = pwm_gpio_to_slice_num(AUDIO_PIN);

    // Setup PWM interrupt to fire when PWM cycle is complete
    pwm_clear_irq(audio_pin_slice);
    pwm_set_irq_enabled(audio_pin_slice, true);
    // set the handle function above
    irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_interrupt_handler); 
    irq_set_enabled(PWM_IRQ_WRAP, true);

    // Setup PWM for audio output
    pwm_config config = pwm_get_default_config();
    /* Base clock 176,000,000 Hz divide by wrap 250 then the clock divider further divides
     * to set the interrupt rate. 
     * 
     * 11 KHz is fine for speech. Phone lines generally sample at 8 KHz
     * 
     * 
     * So clkdiv should be as follows for given sample rate
     *  8.0f for 11 KHz
     *  4.0f for 22 KHz
     *  2.0f for 44 KHz etc
     */
    pwm_config_set_clkdiv(&config, 8.0f); 
    pwm_config_set_wrap(&config, 250); 
    pwm_init(audio_pin_slice, &config, true);

    button_init();

    while(1) {
        //__wfi(); // Wait for Interrupt
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void data_init(){
    for(int j=0; j<DATA_LENGTH; ++j){
        audio_data[j] = 0;
    }
}

void microphone_task(void *p){
    adc_init();
    adc_gpio_init(MICROPHONE_PIN);

    button_init();
    data_init();

    int recorded = 0;

    repeating_timer_t timer_0;
    int timer_on = 0;

    while (1) {
        if(recording){
            if(!timer_on){
                play = 0;
                recorded = 1;
                timer_on = 1;
                if (!add_repeating_timer_us(91, 
                                            timer_0_callback,
                                            NULL, 
                                            &timer_0)) {
                    printf("Failed to add timer\n");
                }
            }
        } else if(recorded){
            vTaskDelay(pdMS_TO_TICKS(500));
            timer_on = 0;
            recorded = 0;
            play = 1;
            cancel_repeating_timer(&timer_0);
        }
    }
}

int main() {
    stdio_init_all();
    adc_init();

    xTaskCreate(microphone_task, "Microphone Task", 4095, NULL, 1, NULL);
    xTaskCreate(pwm_task, "PWM Task", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
