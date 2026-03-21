/**
 * @file pinout.h
 * @brief Hardware pin mapping for the VESPA Motion Node (Arduino/SimpleFOC compatible).
 */

#ifndef PINOUT_H
#define PINOUT_H

#include <Arduino.h>

/* ========================================================================= */
/* MOTOR 1 (PAN)                                                             */
/* ========================================================================= */
#define M1_INU PA8
#define M1_INV PA9
#define M1_INW PA10
#define M1_EN_FAULT PA11
#define POT_PAN PB14      // Fine-tuning potentiometer (Moved from PA2 for Serial TX)

// Current & Voltage Sensing (For future activation)
#define M1_SENSEU PA1
#define M1_SENSEV PB1
#define M1_SENSEW PA4     // WARNING: PA4 currently repurposed for SPI Hardware NSS Safety
#define M1_VBUS PA0


/* ========================================================================= */
/* MOTOR 2 (TILT)                                                            */
/* ========================================================================= */
#define M2_INU PC6
#define M2_INV PC7
#define M2_INW PC8
#define M2_EN_FAULT PB12
#define POT_TILT PB0      // Fine-tuning potentiometer

// Current & Voltage Sensing (For future activation)
#define M2_SENSEU PC0
#define M2_SENSEV PC1
#define M2_SENSEW PC2
#define M2_VBUS PC3


/* ========================================================================= */
/* AS5048A ABSOLUTE ENCODERS - SPI1                                          */
/* ========================================================================= */
#define SPI1_SCK PA5
#define SPI1_MISO PA6
#define SPI1_MOSI PA7
#define SPI1_NSS PA4      // Hardware NSS pin (Pulled HIGH for safety)
#define ENC1_CS PB6       // Pan Encoder CS
#define ENC2_CS PB7       // Tilt Encoder CS

/* ========================================================================= */
/* WCMCU-230 CAN TRANSCEIVER - FDCAN1                                        */
/* ========================================================================= */
#define CAN_RX PB8
#define CAN_TX PB9

/* ========================================================================= */
/* SYSTEM & SAFETY                                                           */
/* ========================================================================= */
#define USER_BUTTON PC13  // Hardware E-Stop trigger

#endif // PINOUT_H