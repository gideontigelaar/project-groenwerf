#pragma once
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"

#define SERVER_IP     "172.20.10.10"   // Your Flask server IP
#define SERVER_PORT   5002
#define HTTP_PATH     "/message"

class NetworkManager
{
public:
    NetworkManager();
    virtual ~NetworkManager();

    int Init();
    int SendData(char * data);
private:
    static err_t onConnected(void *arg, struct tcp_pcb *pcb, err_t err);
    static err_t onReceive(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
    static void  onError(void *arg, err_t err);
};