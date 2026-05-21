#pragma once

#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include <string>

class NetworkManager {
public:
    enum class SendState {
        IDLE,
        CONNECTING_WIFI,
        CONNECTING,
        WAITING_RESPONSE,
        DONE,
        ERROR
    };

    NetworkManager();
    ~NetworkManager();

    void ConnectInitial();               // Blocking indefinite connect loop
    bool StartSend(const char *data);   // non-blocking kick-off; returns false if busy
    void Poll();                         // call every main-loop iteration
    bool IsBusy()  const { return state_ == SendState::CONNECTING || state_ == SendState::WAITING_RESPONSE || state_ == SendState::CONNECTING_WIFI; }
    bool HasError() const { return state_ == SendState::ERROR; }
    bool IsDone()   const { return state_ == SendState::DONE; }
    void ResetState() { state_ = SendState::IDLE; }

private:
    // ---- internal TCP state ----
    struct TcpContext {
        struct tcp_pcb *pcb   = nullptr;
        NetworkManager *self  = nullptr;
        char request[2048]    = {};
    };

    TcpContext   ctx_;
    SendState    state_  = SendState::IDLE;

    uint32_t     send_start_ms_ = 0;
    static constexpr uint32_t SEND_TIMEOUT_MS = 5000;

    int          wifi_retry_count_ = 0;
    uint32_t     wifi_retry_start_ms_ = 0;

    // lwIP callbacks — must be static
    static err_t onConnected(void *arg, struct tcp_pcb *pcb, err_t err);
    static err_t onReceive  (void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
    static void  onError    (void *arg, err_t err);
};