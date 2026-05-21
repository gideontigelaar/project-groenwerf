#include "networkmanager.h"
#include "credentials.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <cstring>

NetworkManager::NetworkManager()  {}
NetworkManager::~NetworkManager() {}

void NetworkManager::ConnectInitial() {
    sleep_ms(2000);

    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return;
    }

    cyw43_arch_enable_sta_mode();

    while (true) {
        printf("Connecting to Wi-Fi...\n");
        int err = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 5000);
        if (err == 0) {
            break;
        }
        printf("Wi-Fi connection failed. Retrying in 5 seconds...\n");
        sleep_ms(5000);
    }

    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    printf("Connected. IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
}

bool NetworkManager::StartSend(const char *data) {
    if (IsBusy()) {
        return false;
    }

    memset(&ctx_, 0, sizeof(ctx_));
    ctx_.self = this;

    snprintf(ctx_.request, sizeof(ctx_.request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "X-API-Key: %s\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        HTTP_PATH, SERVER_IP, SERVER_PORT,
        (int)strlen(data),
        API_KEY,
        data
    );

    printf("NetworkManager: Initializing connection to server for batch...\n");

    ctx_.pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (!ctx_.pcb) {
        printf("NetworkManager: Failed to create PCB\n");
        state_ = SendState::ERROR;
        return false;
    }

    tcp_arg(ctx_.pcb, &ctx_);
    tcp_recv(ctx_.pcb, onReceive);
    tcp_err (ctx_.pcb, onError);

    ip_addr_t server_addr;
    ip4addr_aton(SERVER_IP, &server_addr);

    err_t err = tcp_connect(ctx_.pcb, &server_addr, SERVER_PORT, onConnected);
    if (err != ERR_OK) {
        printf("NetworkManager: tcp_connect failed: %d\n", err);
        tcp_abort(ctx_.pcb);
        ctx_.pcb = nullptr;
        state_ = SendState::ERROR;
        return false;
    }

    state_         = SendState::CONNECTING;
    send_start_ms_ = to_ms_since_boot(get_absolute_time());
    return true;
}

void NetworkManager::Poll() {
    cyw43_arch_poll();

    // Check Wi-Fi Link status
    int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    if (state_ == SendState::CONNECTING_WIFI) {
        if (status == CYW43_LINK_UP) {
            printf("NetworkManager: Wi-Fi reconnected!\n");
            wifi_retry_count_ = 0;
            state_ = SendState::IDLE;
        } else {
            if (to_ms_since_boot(get_absolute_time()) - wifi_retry_start_ms_ > 5000) {
                state_ = SendState::IDLE; // Timeout, will retry on next poll if still down
            }
        }
    } else if (status != CYW43_LINK_UP) {
        if (wifi_retry_count_ >= 10) {
            printf("NetworkManager: Wi-Fi failed 10 times. Halting completely.\n");
            while(true) { sleep_ms(1000); } // Error out
        }
        printf("NetworkManager: Wi-Fi disconnected. Reconnecting in background (attempt %d/10)...\n", wifi_retry_count_ + 1);
        cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
        wifi_retry_start_ms_ = to_ms_since_boot(get_absolute_time());
        wifi_retry_count_++;
        state_ = SendState::CONNECTING_WIFI;
    }

    // Handle HTTP Timeouts
    if (state_ == SendState::CONNECTING || state_ == SendState::WAITING_RESPONSE) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - send_start_ms_ > SEND_TIMEOUT_MS) {
            printf("NetworkManager: send timeout\n");
            if (ctx_.pcb) {
                tcp_abort(ctx_.pcb);
                ctx_.pcb = nullptr;
            }
            state_ = SendState::ERROR;
        }
    }
}

err_t NetworkManager::onConnected(void *arg, struct tcp_pcb *pcb, err_t err) {
    TcpContext     *ctx  = static_cast<TcpContext *>(arg);
    NetworkManager *self = ctx->self;

    if (err != ERR_OK) {
        printf("NetworkManager: Connection failed: %d\n", err);
        self->state_ = SendState::ERROR;
        return err;
    }

    printf("NetworkManager: Connected to server, posting batch...\n");

    err_t write_err = tcp_write(pcb, ctx->request,
                                strlen(ctx->request),
                                TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK) {
        printf("NetworkManager: tcp_write failed: %d\n", write_err);
        self->state_ = SendState::ERROR;
        return write_err;
    }

    tcp_output(pcb);
    self->state_ = SendState::WAITING_RESPONSE;
    return ERR_OK;
}

err_t NetworkManager::onReceive(void *arg, struct tcp_pcb *pcb,
                                 struct pbuf *p, err_t err) {
    TcpContext     *ctx  = static_cast<TcpContext *>(arg);
    NetworkManager *self = ctx->self;

    if (p == nullptr) {
        printf("NetworkManager: Response received, connection closed.\n");
        tcp_close(pcb);
        ctx->pcb  = nullptr;
        self->state_ = SendState::DONE;
        return ERR_OK;
    }

    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

void NetworkManager::onError(void *arg, err_t err) {
    TcpContext     *ctx  = static_cast<TcpContext *>(arg);
    NetworkManager *self = ctx->self;

    printf("NetworkManager: Connection error %d\n", err);
    ctx->pcb     = nullptr;
    self->state_ = SendState::ERROR;
}