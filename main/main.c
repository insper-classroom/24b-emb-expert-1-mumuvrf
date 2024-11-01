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

void microphone_task(void *p){
    while(true){
        printf("Hello World!!!\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main() {
    stdio_init_all();

    xTaskCreate(microphone_task, "Microphone Task", 8192, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
