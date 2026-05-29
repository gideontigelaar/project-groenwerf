#include "networkmanager.h"
#include "credentials.h"
#include "logger.h"
#include "pico/stdlib.h"
#include <cstring>

NetworkManager::NetworkManager()  {}
NetworkManager::~NetworkManager() {}

void NetworkManager::ConnectInitial() {
    sleep_ms(2000);

    if (cyw43_arch_init()) {
        LOG_ERROR("Network: Wi-Fi init failed");
        return;
    }

    cyw43_arch_enable_sta_mode();

    LOG_INFO("Network: Connecting to Wi-Fi (%s)...", WIFI_SSID);
    // don't block the system, connect async
    cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
    wifi_retry_start_ms_ = to_ms_since_boot(get_absolute_time());
    state_ = SendState::CONNECTING_WIFI;
}

bool NetworkManager::StartSend(const uint8_t *data, size_t length) {
    if (IsBusy() || halted_) {
        return false;
    }

    cyw43_arch_lwip_begin();

    if (state_ != SendState::CONNECTED_IDLE) {
        memset(&ctx_, 0, sizeof(ctx_));
        ctx_.self = this;
    }

    const char* active_host = (SERVER_HOST[0] != '\0') ? SERVER_HOST : SERVER_IP;

    int header_len = snprintf(ctx_.request, sizeof(ctx_.request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %d\r\n"
        "X-API-Key: %s\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        HTTP_PATH, active_host, (int)length, API_KEY);

    if (header_len < 0 || header_len + length >= sizeof(ctx_.request)) {
        LOG_ERROR("Network: WARNING - request truncated! Buffer too small.");
        state_ = SendState::ERROR;
        cyw43_arch_lwip_end();
        return false;
    }

    memcpy(ctx_.request + header_len, data, length);
    ctx_.pending_write_len = header_len + length;

    if (state_ == SendState::CONNECTED_IDLE && ctx_.pcb != nullptr) {
        err_t write_err = tcp_write(ctx_.pcb, ctx_.request, ctx_.pending_write_len, 0);
        if (write_err == ERR_OK) {
            tcp_output(ctx_.pcb);
            state_ = SendState::WAITING_RESPONSE;
            send_start_ms_ = to_ms_since_boot(get_absolute_time());
            cyw43_arch_lwip_end();
            return true;
        } else {
            tcp_close(ctx_.pcb);
            ctx_.pcb = nullptr;
            state_ = SendState::IDLE;
        }
    }

    state_ = SendState::CONNECTING;
    send_start_ms_ = to_ms_since_boot(get_absolute_time());

    // use dns if hostname is provided, otherwise fallback to ip
    if (SERVER_HOST[0] != '\0') {
        ip_addr_t server_addr;
        err_t dns_err = dns_gethostbyname(SERVER_HOST, &server_addr, onDnsFound, this);
        if (dns_err == ERR_OK) {
            ConnectTcp(&server_addr);
        } else if (dns_err != ERR_INPROGRESS) {
            LOG_ERROR("Network: DNS request failed");
            state_ = SendState::ERROR;
        }
    } else {
        ip_addr_t server_addr;
        ip4addr_aton(SERVER_IP, &server_addr);
        ConnectTcp(&server_addr);
    }

    cyw43_arch_lwip_end();

    return true;
}

void NetworkManager::onDnsFound(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    NetworkManager *self = static_cast<NetworkManager *>(callback_arg);
    if (ipaddr) {
        self->ConnectTcp(ipaddr);
    } else {
        LOG_ERROR("Network: DNS resolution failed for %s", name);
        self->state_ = SendState::ERROR;
    }
}

bool NetworkManager::ConnectTcp(const ip_addr_t *addr) {
    ctx_.pcb = tcp_new_ip_type(IP_GET_TYPE(addr));
    if (!ctx_.pcb) {
        LOG_ERROR("Network: Failed to create PCB");
        state_ = SendState::ERROR;
        return false;
    }

    tcp_arg(ctx_.pcb, &ctx_);
    tcp_recv(ctx_.pcb, onReceive);
    tcp_err(ctx_.pcb, onError);

    err_t err = tcp_connect(ctx_.pcb, addr, SERVER_PORT, onConnected);
    if (err != ERR_OK) {
        LOG_ERROR("Network: tcp_connect failed (%d)", err);
        tcp_abort(ctx_.pcb);
        ctx_.pcb = nullptr;
        state_ = SendState::ERROR;
        return false;
    }
    return true;
}

void NetworkManager::Poll() {
    if (halted_) return;

    cyw43_arch_poll();

    int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    if (state_ == SendState::CONNECTING_WIFI) {
        if (status == CYW43_LINK_UP) {
            uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
            LOG_INFO("Network: Connected. IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            wifi_retry_count_ = 0;
            state_ = SendState::IDLE;
        } else {
            if (to_ms_since_boot(get_absolute_time()) - wifi_retry_start_ms_ > 10000) {
                state_ = SendState::IDLE;
            }
        }
    } else if (status != CYW43_LINK_UP && state_ != SendState::CONNECTING_WIFI) {
        if (wifi_retry_count_ >= 30) {
            LOG_ERROR("Network: Wi-Fi failed 30 times. Halting.");
            halted_ = true;
            return;
        }
        LOG_WARN("Network: Wi-Fi disconnected. Reconnecting (%d/10)...", wifi_retry_count_ + 1);
        cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
        wifi_retry_start_ms_ = to_ms_since_boot(get_absolute_time());
        wifi_retry_count_++;
        state_ = SendState::CONNECTING_WIFI;
    }

    if (state_ == SendState::CONNECTING || state_ == SendState::WAITING_RESPONSE) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - send_start_ms_ > SEND_TIMEOUT_MS) {
            LOG_ERROR("Network: Send timeout");

            cyw43_arch_lwip_begin();
            if (ctx_.pcb) {
                tcp_abort(ctx_.pcb);
                ctx_.pcb = nullptr;
            }
            cyw43_arch_lwip_end();

            state_ = SendState::ERROR;
        }
    }
}

err_t NetworkManager::onConnected(void *arg, struct tcp_pcb *pcb, err_t err) {
    TcpContext     *ctx  = static_cast<TcpContext *>(arg);
    NetworkManager *self = ctx->self;

    if (err != ERR_OK) {
        LOG_ERROR("Network: Connection failed (%d)", err);
        self->state_ = SendState::ERROR;
        return err;
    }

    err_t write_err = tcp_write(pcb, ctx->request, ctx->pending_write_len, 0);
    if (write_err != ERR_OK) {
        LOG_ERROR("Network: tcp_write failed (%d)", write_err);
        self->state_ = SendState::ERROR;
        return write_err;
    }

    tcp_output(pcb);
    self->state_ = SendState::WAITING_RESPONSE;
    return ERR_OK;
}

err_t NetworkManager::onReceive(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    TcpContext     *ctx  = static_cast<TcpContext *>(arg);
    NetworkManager *self = ctx->self;

    if (p == nullptr) {
        tcp_close(pcb);
        ctx->pcb  = nullptr;
        self->state_ = SendState::IDLE;
        return ERR_OK;
    }

    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    self->state_ = SendState::CONNECTED_IDLE;
    return ERR_OK;
}

void NetworkManager::onError(void *arg, err_t err) {
    TcpContext     *ctx  = static_cast<TcpContext *>(arg);
    NetworkManager *self = ctx->self;

    LOG_ERROR("Network: Connection error (%d)", err);
    ctx->pcb     = nullptr;
    self->state_ = SendState::ERROR;
}