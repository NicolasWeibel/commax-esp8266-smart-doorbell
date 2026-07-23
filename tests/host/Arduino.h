#pragma once

#include <cstddef>
#include <cstdint>

#define IRAM_ATTR
#define HIGH 0x1
#define LOW 0x0
#define INPUT 0x0
#define OUTPUT 0x1
#define CHANGE 0x3
#define D5 14
#define LED_BUILTIN 2

using byte = uint8_t;
using InterruptHandler = void (*)();

uint32_t millis();
uint32_t micros();
void pinMode(uint8_t pin, uint8_t mode);
int digitalRead(uint8_t pin);
void digitalWrite(uint8_t pin, uint8_t value);
int digitalPinToInterrupt(uint8_t pin);
void attachInterrupt(int interruptNumber, InterruptHandler handler, int mode);
void noInterrupts();
void interrupts();
void yield();
void delay(uint32_t milliseconds);
