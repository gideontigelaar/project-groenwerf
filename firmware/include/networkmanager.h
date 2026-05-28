#pragma once

#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include <string>

class NetworkManager {
public:
    enum class SendState {
        IDLE,
        CONNECTING_WIFI,
        RESOLVING_DNS,
        CONNECTING,
        WAITING_RESPONSE,
        DONE,
        ERROR
    };

    NetworkManager();
    ~NetworkManager();

    void ConnectInitial();
    bool StartSend(const char *data);
    void Poll();

    bool IsBusy()    const {
        return state_ == SendState::RESOLVING_DNS
            || state_ == SendState::CONNECTING
            || state_ == SendState::WAITING_RESPONSE
            || state_ == SendState::CONNECTING_WIFI;
    }
    bool HasError()  const { return state_ == SendState::ERROR; }
    bool IsDone()    const { return state_ == SendState::DONE; }
    bool IsHalted()  const { return halted_; }
    void ResetState()      { state_ = SendState::IDLE; }

private:
    struct TcpContext {
        struct tcp_pcb *pcb   = nullptr;
        NetworkManager *self  = nullptr;
        char request[8192]    = {};
    };

    TcpContext ctx_;
    SendState  state_ = SendState::IDLE;

    ip_addr_t server_addr_;
    bool      addr_resolved_  = false;
    bool      dns_in_flight_  = false;
    bool      dns_result_ok_  = false;
    uint32_t  dns_start_ms_   = 0;
    static constexpr uint32_t DNS_TIMEOUT_MS  = 6000;

    uint32_t send_start_ms_ = 0;
    static constexpr uint32_t SEND_TIMEOUT_MS = 5000;

    int      wifi_retry_count_    = 0;
    uint32_t wifi_retry_start_ms_ = 0;
    bool     halted_              = false;

    void doConnect();

    static void  onDnsResolved(const char *name, const ip_addr_t *addr, void *arg);
    static err_t onConnected  (void *arg, struct tcp_pcb *pcb, err_t err);
    static err_t onReceive    (void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
    static void  onError      (void *arg, err_t err);
};