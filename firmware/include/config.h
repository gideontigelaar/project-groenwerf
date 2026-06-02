#pragma once
#include "pico/stdlib.h"
#include "hardware/uart.h"

#define GPS_UART uart0

namespace Config {
    namespace Pins {
        const uint TOF_I2C_SDA       = 20;
        const uint TOF_I2C_SCL       = 21;
        const uint ACCEL_I2C_SDA     = 14;
        const uint ACCEL_I2C_SCL     = 15;
        const uint RCWL_TRIG         = 18;
        const uint RCWL_ECHO         = 19;
        const uint TMP36_PIN         = 28;
        const uint LED_GREEN         = 26;
        const uint LED_YELLOW        = 22;
        const uint GPS_UART_TX       = 12;
        const uint GPS_UART_RX       = 13;

        const uint INTERNAL_TEMP_ADC = 4;
    }

    namespace Timing {
        const uint32_t TOF_INTERVAL_MS   = 20;
        const uint32_t SONIC_INTERVAL_MS = 50;
        const uint32_t ACCEL_INTERVAL_MS = 10;
        const uint32_t PRINT_INTERVAL_MS = 500;
        const uint32_t LOOP_TICK_MS      = 5;
    }

    namespace Network {
        const int SEND_BATCH_SIZE = 10;
    }

    namespace Sensor {
        const float KNOWN_HEIGHT_MM     = 750.0f; // distance from sensor to ground
        const int   CALIBRATION_SAMPLES = 10;

        // hardware calibration offset
        const float TMP36_OFFSET_C      = 0.0f;

        const float VIBRATION_LOW_G     = 0.05f; // idle threshold
        const float VIBRATION_HIGH_G    = 0.15f;
        const size_t WINDOW_NARROW      = 10; // fastest (500ms)
        const size_t WINDOW_MEDIUM      = 18;
        const size_t WINDOW_WIDE        = 25; // max smoothing has 1.25s latency

        const float HPF_ALPHA           = 0.95f; // high-pass filter
        const float RMS_ALPHA           = 0.995f;
        const float SPIKE_THRESHOLD_G   = 0.3f; // spike rejection threshold
    }

    namespace Gps {
        const uint BAUD_RATE       = 9600;
        const uint ACK_TIMEOUT_MS  = 1500;
        const uint UBX_TIMEOUT_MS  = 1100;
        const uint UBX_MAX_PAYLOAD = 128;
        const uint8_t UBX_SYNC_1   = 0xB5;
        const uint8_t UBX_SYNC_2   = 0x62;
        const uint8_t NAV_CLASS    = 0x01;
    }

    const uint I2C_FREQ = 400000;
}