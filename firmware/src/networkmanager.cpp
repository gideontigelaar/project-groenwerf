#include "networkmanager.h"
#include <stdio.h>
#include <string.h>

NetworkManager::NetworkManager() {}
NetworkManager::~NetworkManager() {}

typedef struct {
    struct tcp_pcb *pcb;
    bool complete;
    bool error;
    char request[512];
} TCP_STATE_T;

// --- Callbacks (must be static) ---

err_t NetworkManager::onConnected(void *arg, struct tcp_pcb *pcb, err_t err) {
    TCP_STATE_T *state = (TCP_STATE_T *)arg;

    if (err != ERR_OK) {
        printf("Connection failed: %d\n", err);
        state->error = true;
        state->complete = true;
        return err;
    }

    printf("Connected to server, sending data...\n");

    // Build and send the request
    const char *body = (const char *)state; // we'll fix this below
    // NOTE: see SendData for how we pass the request in
    return ERR_OK;
}

err_t NetworkManager::onReceive(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    TCP_STATE_T *state = (TCP_STATE_T *)arg;
    if (p == NULL) {
        // Server closed the connection
        state->complete = true;
        tcp_close(pcb);
        return ERR_OK;
    }
    printf("Response received\n");
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

void NetworkManager::onError(void *arg, err_t err) {
    TCP_STATE_T *state = (TCP_STATE_T *)arg;
    printf("TCP error: %d\n", err);
    state->error = true;
    state->complete = true;
}

// --- Extended state to carry the request string ---


static err_t onConnectedFull(void *arg, struct tcp_pcb *pcb, err_t err) {
    TCP_STATE_T *state = (TCP_STATE_T *)arg;

    if (err != ERR_OK) {
        printf("Connection failed: %d\n", err);
        state->error = true;
        state->complete = true;
        return err;
    }

    printf("Connected! Sending HTTP POST...\n");
    err_t write_err = tcp_write(pcb, state->request, strlen(state->request), TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK) {
        printf("tcp_write failed: %d\n", write_err);
        state->error = true;
        state->complete = true;
        return write_err;
    }

    tcp_output(pcb);
    return ERR_OK;
}

static err_t onReceiveFull(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    TCP_STATE_T *state = (TCP_STATE_T *)arg;
    if (p == NULL) {
        state->complete = true;
        tcp_close(pcb);
        return ERR_OK;
    }
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void onErrorFull(void *arg, err_t err) {
    TCP_STATE_T *state = (TCP_STATE_T *)arg;
    printf("TCP error: %d\n", err);
    state->error = true;
    state->complete = true;
}

// --- Public methods ---

int NetworkManager::Init() {
    sleep_ms(2000);

    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Connecting to Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms("SSID", "Password",
            CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Failed to connect.\n");
        return -1;
    }

    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    printf("Connected. IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    return 1;
}

int NetworkManager::SendData(char *data) {
    static TCP_STATE_T state;
    memset(&state, 0, sizeof(state));

    // Build the HTTP request
    snprintf(state.request, sizeof(state.request),
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

    printf("Request:\n%s\n", state.request);

    state.pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (!state.pcb) {
        printf("Failed to create PCB\n");
        return -1;
    }

    tcp_arg(state.pcb, &state);
    tcp_recv(state.pcb, onReceiveFull);
    tcp_err(state.pcb, onErrorFull);

    ip_addr_t server_addr;
    ip4addr_aton(SERVER_IP, &server_addr);

    err_t err = tcp_connect(state.pcb, &server_addr, SERVER_PORT, onConnectedFull);
    if (err != ERR_OK) {
        printf("tcp_connect call failed: %d\n", err);
        return -1;
    }

    printf("Waiting for connection...\n");

    // Wait with timeout
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (!state.complete) {
        cyw43_arch_poll();
        sleep_ms(1);
        if (to_ms_since_boot(get_absolute_time()) - start > 5000) {
            printf("Timeout! Server not reachable.\n");
            tcp_abort(state.pcb);
            return -1;
        }
    }

    return state.error ? -1 : 1;
}