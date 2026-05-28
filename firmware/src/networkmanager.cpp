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

    while (true) {
        LOG_INFO("Network: Connecting to Wi-Fi (%s)...", WIFI_SSID);
        int err = cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 5000);
        if (err == 0) break;
        LOG_WARN("Network: Wi-Fi connection failed. Retrying in 5s...");
        sleep_ms(5000);
    }

    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    LOG_INFO("Network: Connected. IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

bool NetworkManager::StartSend(const char *data) {
    if (IsBusy() || halted_) return false;

    cyw43_arch_lwip_begin();

    memset(&ctx_, 0, sizeof(ctx_));
    ctx_.self = this;

    snprintf(ctx_.request, sizeof(ctx_.request),
        "POST %s HTTP/1.0\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "X-API-Key: %s\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        HTTP_PATH,
        SERVER_HOST,
        (int)strlen(data),
        API_KEY,
        data
    );

    if (strlen(ctx_.request) >= sizeof(ctx_.request) - 1) {
        LOG_ERROR("Network: request buffer overflow — payload too large");
        state_ = SendState::ERROR;
        cyw43_arch_lwip_end();
        return false;
    }

    if (addr_resolved_) {
        doConnect();
        cyw43_arch_lwip_end();
        return state_ == SendState::CONNECTING;
    }

    if (ip4addr_aton(SERVER_HOST, &server_addr_)) {
        addr_resolved_ = true;
        doConnect();
        cyw43_arch_lwip_end();
        return state_ == SendState::CONNECTING;
    }

    LOG_INFO("Network: Resolving hostname %s...", SERVER_HOST);
    dns_in_flight_ = true;
    dns_result_ok_ = false;
    dns_start_ms_  = to_ms_since_boot(get_absolute_time());

    err_t dns_err = dns_gethostbyname(SERVER_HOST, &server_addr_, onDnsResolved, this);

    if (dns_err == ERR_OK) {
        dns_in_flight_ = false;
        addr_resolved_ = true;
        doConnect();
        cyw43_arch_lwip_end();
        return state_ == SendState::CONNECTING;
    }

    if (dns_err == ERR_INPROGRESS) {
        state_ = SendState::RESOLVING_DNS;
        cyw43_arch_lwip_end();
        return true;
    }

    LOG_ERROR("Network: dns_gethostbyname failed (%d)", (int)dns_err);
    dns_in_flight_ = false;
    state_ = SendState::ERROR;
    cyw43_arch_lwip_end();
    return false;
}

void NetworkManager::doConnect() {
    ctx_.pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (!ctx_.pcb) {
        LOG_ERROR("Network: Failed to create PCB");
        state_ = SendState::ERROR;
        return;
    }

    tcp_arg(ctx_.pcb, &ctx_);
    tcp_recv(ctx_.pcb, onReceive);
    tcp_err (ctx_.pcb, onError);

    err_t err = tcp_connect(ctx_.pcb, &server_addr_, SERVER_PORT, onConnected);
    if (err != ERR_OK) {
        LOG_ERROR("Network: tcp_connect failed (%d)", (int)err);
        tcp_abort(ctx_.pcb);
        ctx_.pcb = nullptr;
        state_ = SendState::ERROR;
        return;
    }

    state_         = SendState::CONNECTING;
    send_start_ms_ = to_ms_since_boot(get_absolute_time());
}

void NetworkManager::Poll() {
    if (halted_) return;

    cyw43_arch_poll();

    int wifi_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    if (state_ == SendState::CONNECTING_WIFI) {
        if (wifi_status == CYW43_LINK_UP) {
            LOG_INFO("Network: Wi-Fi reconnected!");
            wifi_retry_count_ = 0;
            state_ = SendState::IDLE;
        } else if (to_ms_since_boot(get_absolute_time()) - wifi_retry_start_ms_ > 5000) {
            state_ = SendState::IDLE;
        }
        return;
    }

    if (wifi_status != CYW43_LINK_UP) {
        if (wifi_retry_count_ >= 10) {
            LOG_ERROR("Network: Wi-Fi failed 10 times. Halting.");
            halted_ = true;
            return;
        }
        LOG_WARN("Network: Wi-Fi disconnected. Reconnecting (%d/10)...", wifi_retry_count_ + 1);
        cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
        wifi_retry_start_ms_ = to_ms_since_boot(get_absolute_time());
        wifi_retry_count_++;
        state_ = SendState::CONNECTING_WIFI;
        return;
    }

    if (state_ == SendState::RESOLVING_DNS) {
        if (dns_result_ok_) {
            dns_in_flight_ = false;
            addr_resolved_ = true;
            LOG_INFO("Network: DNS resolved");
            cyw43_arch_lwip_begin();
            doConnect();
            cyw43_arch_lwip_end();
            return;
        }
        if (to_ms_since_boot(get_absolute_time()) - dns_start_ms_ > DNS_TIMEOUT_MS) {
            LOG_ERROR("Network: DNS timed out for %s", SERVER_HOST);
            dns_in_flight_ = false;
            state_ = SendState::ERROR;
        }
        return;
    }

    if (state_ == SendState::CONNECTING || state_ == SendState::WAITING_RESPONSE) {
        if (to_ms_since_boot(get_absolute_time()) - send_start_ms_ > SEND_TIMEOUT_MS) {
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

void NetworkManager::onDnsResolved(const char *name, const ip_addr_t *addr, void *arg) {
    NetworkManager *self = static_cast<NetworkManager *>(arg);
    if (addr) {
        self->server_addr_  = *addr;
        self->dns_result_ok_ = true;
    } else {
        LOG_ERROR("Network: DNS failed for %s", name);
        self->dns_in_flight_ = false;
        self->state_ = SendState::ERROR;
    }
}

err_t NetworkManager::onConnected(void *arg, struct tcp_pcb *pcb, err_t err) {
    TcpContext     *ctx  = static_cast<TcpContext *>(arg);
    NetworkManager *self = ctx->self;

    if (err != ERR_OK) {
        LOG_ERROR("Network: Connection failed (%d)", (int)err);
        self->state_ = SendState::ERROR;
        return err;
    }

    err_t write_err = tcp_write(pcb, ctx->request, strlen(ctx->request), TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK) {
        LOG_ERROR("Network: tcp_write failed (%d)", (int)write_err);
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
        ctx->pcb     = nullptr;
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

    LOG_ERROR("Network: Connection error (%d)", (int)err);
    ctx->pcb     = nullptr;
    self->state_ = SendState::ERROR;
}