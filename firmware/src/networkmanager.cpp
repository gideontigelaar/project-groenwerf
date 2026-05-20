#include "networkmanager.h"
#include "credentials.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

NetworkManager::NetworkManager()  {}
NetworkManager::~NetworkManager() {}

// ---------------------------------------------------------------------------
// Init — connect to Wi-Fi (blocking once at startup, that's fine)
// ---------------------------------------------------------------------------

int NetworkManager::Init() {
    sleep_ms(2000);

    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Connecting to Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Failed to connect.\n");
        return -1;
    }

    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    printf("Connected. IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    return 1;
}

// ---------------------------------------------------------------------------
// StartSend — build request and kick off tcp_connect (non-blocking)
// ---------------------------------------------------------------------------

bool NetworkManager::StartSend(const char *data) {
    if (IsBusy()) {
        printf("NetworkManager: send already in progress, dropping batch\n");
        return false;
    }

    // Reset context
    memset(&ctx_, 0, sizeof(ctx_));
    ctx_.self = this;

    // Build the HTTP POST request
    snprintf(ctx_.request, sizeof(ctx_.request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        HTTP_PATH, SERVER_IP, SERVER_PORT,
        (int)strlen(data),
        data
    );

    printf("StartSend: queuing %d bytes\n", (int)strlen(data));

    ctx_.pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (!ctx_.pcb) {
        printf("StartSend: failed to create PCB\n");
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
        printf("StartSend: tcp_connect failed: %d\n", err);
        tcp_abort(ctx_.pcb);
        ctx_.pcb = nullptr;
        state_ = SendState::ERROR;
        return false;
    }

    state_         = SendState::CONNECTING;
    send_start_ms_ = to_ms_since_boot(get_absolute_time());
    return true;
}

// ---------------------------------------------------------------------------
// Poll — call every main-loop iteration; handles timeout and lwIP ticks
// ---------------------------------------------------------------------------

void NetworkManager::Poll() {
    // Always let lwIP do its work
    cyw43_arch_poll();

    if (!IsBusy()) return;

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

// ---------------------------------------------------------------------------
// lwIP callbacks
// ---------------------------------------------------------------------------

err_t NetworkManager::onConnected(void *arg, struct tcp_pcb *pcb, err_t err) {
    TcpContext     *ctx  = static_cast<TcpContext *>(arg);
    NetworkManager *self = ctx->self;

    if (err != ERR_OK) {
        printf("onConnected: connection failed: %d\n", err);
        self->state_ = SendState::ERROR;
        return err;
    }

    printf("onConnected: sending HTTP POST...\n");

    err_t write_err = tcp_write(pcb, ctx->request,
                                strlen(ctx->request),
                                TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK) {
        printf("onConnected: tcp_write failed: %d\n", write_err);
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
        // Server closed connection — we're done
        printf("onReceive: connection closed by server, send complete\n");
        tcp_close(pcb);
        ctx->pcb  = nullptr;
        self->state_ = SendState::DONE;
        return ERR_OK;
    }

    printf("onReceive: got %u bytes\n", p->tot_len);
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

void NetworkManager::onError(void *arg, err_t err) {
    TcpContext     *ctx  = static_cast<TcpContext *>(arg);
    NetworkManager *self = ctx->self;

    // lwIP has already freed the PCB when this fires
    printf("onError: TCP error %d\n", err);
    ctx->pcb     = nullptr;
    self->state_ = SendState::ERROR;
}