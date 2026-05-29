#include "sensors/vma430_gps.h"
#include "logger.h"
#include "hardware/irq.h"
#include <stdio.h>
#include <string.h>

// globals

utc_time_t utc_time = {0, 0, 0, 0, 0, 0, false};
location_t location = {0.0, 0.0};

static ubx_msg_t latest_msg;

#define GPS_RING_SIZE 1024
static volatile uint8_t rx_ring[GPS_RING_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

static void on_uart_rx() {
    while (uart_is_readable(GPS_UART)) {
        rx_ring[rx_head] = uart_getc(GPS_UART);
        rx_head = (rx_head + 1) % GPS_RING_SIZE;
    }
}

static bool has_rx_byte() {
    return rx_head != rx_tail;
}

static uint8_t get_rx_byte() {
    uint8_t c = rx_ring[rx_tail];
    rx_tail = (rx_tail + 1) % GPS_RING_SIZE;
    return c;
}

// helpers

static void calc_checksum(const uint8_t *payload, int len,
                          uint8_t *ck_a, uint8_t *ck_b)
{
    *ck_a = 0;
    *ck_b = 0;
    for (int i = 0; i < len; i++) {
        *ck_a += payload[i];
        *ck_b += *ck_a;
    }
}

static void send_ubx(const uint8_t *msg, size_t len)
{
    uart_write_blocking(GPS_UART, msg, len);
}

static uint8_t get_ubx_ack(uint8_t class_id, uint8_t msg_id)
{
    uint8_t buf[10];
    int i = 0;
    absolute_time_t deadline = make_timeout_time_ms(Config::Gps::ACK_TIMEOUT_MS);

    while (true) {
        if (absolute_time_diff_us(get_absolute_time(), deadline) < 0) {
            LOG_WARN("GPS: ACK timeout");
            return 5;
        }
        if (!has_rx_byte()) continue;

        uint8_t c = get_rx_byte();

        switch (i) {
            case 0:
                if (c == Config::Gps::UBX_SYNC_1) { buf[0] = c; i++; }
                break;
            case 1:
                if (c == Config::Gps::UBX_SYNC_2) { buf[1] = c; i++; }
                else i = 0;
                break;
            case 2:
                if (c == 0x05) { buf[2] = c; i++; }
                else i = 0;
                break;
            default:
                buf[i++] = c;
                break;
        }

        if (i == 10) {
            if (buf[3] == 0x00) {
                LOG_WARN("GPS: NAK received");
                return 1;
            }

            uint8_t ck_a, ck_b;
            calc_checksum(&buf[2], 6, &ck_a, &ck_b);

            if (buf[6] == class_id && buf[7] == msg_id &&
                ck_a == buf[8]     && ck_b == buf[9])
            {
                return 10;
            }

            LOG_WARN("GPS: ACK checksum failure");
            i = 0;
        }
    }
}

static bool send_ubx_cfg(const uint8_t *msg, size_t len)
{
    for (int attempt = 0; attempt < 3; attempt++) {
        send_ubx(msg, len);
        uint8_t result = get_ubx_ack(0x06, 0x01);
        if (result == 10) return true;
    }
    LOG_ERROR("GPS: Config failed");
    return false;
}

// init

bool gps_init(void)
{
    uart_init(GPS_UART, Config::Gps::BAUD_RATE);
    gpio_set_function(Config::Pins::GPS_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(Config::Pins::GPS_UART_RX, GPIO_FUNC_UART);

    int irq_num = (GPS_UART == uart0) ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(irq_num, on_uart_rx);
    irq_set_enabled(irq_num, true);
    uart_set_irq_enables(GPS_UART, true, false);

    sleep_ms(1000);

    // Drain NMEA boot output
    absolute_time_t drain = make_timeout_time_ms(2000);
    int byte_count = 0;
    while (absolute_time_diff_us(get_absolute_time(), drain) > 0) {
        if (has_rx_byte()) {
            get_rx_byte();
            byte_count++;
        }
    }

    if (byte_count == 0) {
        LOG_WARN("  - GPS Module   : No data (Check wiring)");
        return false;
    }

    bool ok = true;

    uint8_t nmea_ids[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
    for (int k = 0; k < 6; k++) {
        uint8_t setNmea[11] = {
            Config::Gps::UBX_SYNC_1, Config::Gps::UBX_SYNC_2,
            0x06, 0x01,
            0x03, 0x00,
            0xF0, nmea_ids[k], 0x00,
            0x00, 0x00
        };
        uint8_t ck_a, ck_b;
        calc_checksum(&setNmea[2], 7, &ck_a, &ck_b);
        setNmea[9]  = ck_a;
        setNmea[10] = ck_b;
        send_ubx_cfg(setNmea, sizeof(setNmea));
    }

    // Enable NAV-TIMEUTC (CFG-MSG: class 0x01, id 0x21, rate 1)
    uint8_t setNavTime[11] = {
        Config::Gps::UBX_SYNC_1, Config::Gps::UBX_SYNC_2,
        0x06, 0x01,       // CFG-MSG
        0x03, 0x00,       // payload length
        Config::Gps::NAV_CLASS, 0x21, 0x01, // class, id, rate
        0x00, 0x00        // checksum placeholder
    };
    uint8_t ck_a, ck_b;
    calc_checksum(&setNavTime[2], 7, &ck_a, &ck_b);
    setNavTime[9]  = ck_a;
    setNavTime[10] = ck_b;
    ok &= send_ubx_cfg(setNavTime, sizeof(setNavTime));

    // Enable NAV-POSLLH (CFG-MSG: class 0x01, id 0x02, rate 1)
    uint8_t setNavPos[11] = {
        Config::Gps::UBX_SYNC_1, Config::Gps::UBX_SYNC_2,
        0x06, 0x01,
        0x03, 0x00,
        Config::Gps::NAV_CLASS, 0x02, 0x01,
        0x00, 0x00
    };
    calc_checksum(&setNavPos[2], 7, &ck_a, &ck_b);
    setNavPos[9]  = ck_a;
    setNavPos[10] = ck_b;
    ok &= send_ubx_cfg(setNavPos, sizeof(setNavPos));

    return ok;
}

// packet reception

bool gps_get_ubx_packet(void)
{
    static int      i              = 0;
    static uint8_t  class_byte     = 0;
    static uint8_t  id_byte        = 0;
    static uint8_t  len_bytes[2]   = {0, 0};
    static uint16_t payload_length = 0;
    static uint8_t  buf[Config::Gps::UBX_MAX_PAYLOAD] = {0};
    static int      checksum_idx   = 0;
    static uint8_t  CK_A = 0, CK_B = 0;

    while (has_rx_byte()) {
        uint8_t c = get_rx_byte();

        // sync detection: must see 0xB5 then 0x62 consecutively
        if (i == 0) {
            if (c == Config::Gps::UBX_SYNC_1) i++;
            continue;
        }
        if (i == 1) {
            if (c == Config::Gps::UBX_SYNC_2) i++;
            else           i = (c == Config::Gps::UBX_SYNC_1) ? 1 : 0; // resync
            continue;
        }

        // class byte: only accept NAV (0x01)
        if (i == 2) {
            if (c == Config::Gps::NAV_CLASS) { class_byte = c; i++; }
            else                { i = 0; } // not a packet we care about, resync
            continue;
        }

        // id byte
        if (i == 3) {
            // Only accept ids we know: 0x21 (TIMEUTC) or 0x02 (POSLLH)
            if (c == 0x21 || c == 0x02) { id_byte = c; i++; }
            else                        { i = 0; }
            continue;
        }

        // length bytes
        if (i == 4) { len_bytes[0] = c; i++; continue; }
        if (i == 5) {
            len_bytes[1]   = c;
            payload_length = (uint16_t)(len_bytes[1] << 8 | len_bytes[0]);
            if (payload_length > Config::Gps::UBX_MAX_PAYLOAD) {
                i = 0; // bad length, resync
                continue;
            }
            // Reset payload state for this packet
            checksum_idx = 0;
            CK_A = 0; CK_B = 0;
            memset(buf, 0, sizeof(buf));
            i++;
            continue;
        }

        // payload + checksum
        int idx = i - 6;
        if (idx < (int)payload_length) {
            buf[idx] = c;
            i++;
        } else if (checksum_idx == 0) {
            CK_A = c;
            checksum_idx = i;
            i++;
        } else {
            // Second checksum byte — packet complete
            CK_B = c;

            uint8_t ck_buf[4 + Config::Gps::UBX_MAX_PAYLOAD];
            ck_buf[0] = class_byte;
            ck_buf[1] = id_byte;
            ck_buf[2] = len_bytes[0];
            ck_buf[3] = len_bytes[1];
            memcpy(&ck_buf[4], buf, payload_length);

            uint8_t expected_ck_a, expected_ck_b;
            calc_checksum(ck_buf, 4 + payload_length, &expected_ck_a, &expected_ck_b);

            if (CK_A == expected_ck_a && CK_B == expected_ck_b) {
                latest_msg.class_byte     = class_byte;
                latest_msg.id_byte        = id_byte;
                latest_msg.payload_length = payload_length;
                latest_msg.CK_A           = CK_A;
                latest_msg.CK_B           = CK_B;
                memcpy(latest_msg.msg, buf, payload_length);
                i = 0;
                return true;
            } else {
                LOG_WARN("GPS: UBX Checksum mismatch, packet dropped");
                i = 0;
            }
        }
    }

    return false;
}

// parsing

static int32_t extract_signed_long(int offset, const uint8_t *data)
{
    uint32_t val = (uint32_t)data[offset]
                 | (uint32_t)data[offset + 1] << 8
                 | (uint32_t)data[offset + 2] << 16
                 | (uint32_t)data[offset + 3] << 24;
    return (int32_t)val;
}

static bool parse_nav_timeutc(void)
{
    if (latest_msg.payload_length != 20) return false;
    const uint8_t *d = latest_msg.msg;

    utc_time.year   = (int)(d[12] | (d[13] << 8));
    utc_time.month  = d[14];
    utc_time.day    = d[15];
    utc_time.hour   = d[16];
    utc_time.minute = d[17];
    utc_time.second = d[18];
    utc_time.valid  = (d[19] & 0x07) == 0x07;
    return true;
}

static bool parse_nav_posllh(void)
{
    if (latest_msg.payload_length != 28) return false;
    const uint8_t *d = latest_msg.msg;

    location.longitude = extract_signed_long(4, d) * 1e-7;
    location.latitude  = extract_signed_long(8, d) * 1e-7;
    return true;
}

bool gps_parse_ubx_data(void)
{
    if (latest_msg.class_byte != Config::Gps::NAV_CLASS) return false;

    switch (latest_msg.id_byte) {
        case 0x21: return parse_nav_timeutc();
        case 0x02: return parse_nav_posllh();
        default:   return false;
    }
}