#pragma once

// FreeRTOS xTaskCreate odottaa signatuuria: void (*)(void*)
void ui_task(void *params);

// Apufunktiot
static void say_hi_and_go_idle(void);