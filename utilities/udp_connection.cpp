#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>

#include <iostream>
#include <string>
#include <vector>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <errno.h>
#include <sys/fcntl.h>

#include "libssm.h"
#include "ssm-time.h"
#include "ssm.h"

#include "printlog.hpp"

#include "libssm.h"
#include "ssm-proxy.hpp"
#include "dssm-utility.hpp"

UDPCommunicator::UDPCommunicator(uint16_t nport, char* mData, uint64_t d_size,
		uint64_t t_size, SSMApiBase *pstream, PROXY_open_mode type,
		ProxyServer* proxy) {
    
    this->mData = mData;
    this->mDataSize = d_size;
    this->ssmTimeSize = t_size;
    this->mFullDataSize = d_size + t_size;
    this->thrdMsgLen = dssm::util::countDssmMsgLength();

    this->pstream = pstream;
    this->mType = type;
    this->proxy = proxy;

    this->buf = (char*)malloc(sizeof(thrd_msg));

    this->udp_server.wait_socket = -1;
    this->udp_server.udpserver_addr.sin_family = AF_INET;
    this->udp_server.udpserver_addr.sin_port = htons(nport);
    this->udp_server.udpserver_addr.sin_addr.s_addr = INADDR_ANY;

    // open udp socket
    if (!this->sopen()) {
        perror("Cannot open UDPSocket\n");
    }
}


bool UDPCommunicator::sopen() {    
    this->udp_server.wait_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (bind(this->udp_server.wait_socket, 
        (struct sockaddr*)&this->udp_server.udpserver_addr,
        sizeof(this->udp_server.udpserver_addr)) == -1) {
        perror("data connection cannot be opened");
        return false;
    }


    return true;
}

UDPCommunicator::~UDPCommunicator() {
    if (this->buf) {
        free(this->buf);
    }
}

void* UDPCommunicator::run(void* args) {

    printf("UDPCommunicator::run\n");

    switch (mType) {
        case WRITE_MODE: {
            printf("write mode\n");
            handleWrite();
            break;
        }
        case READ_MODE: {
            printf("read mode\n");
            break;
        }
        default: {
            perror("no such mode");
        }
    }
    return nullptr;

}

bool UDPCommunicator::receiveData() {    
    int len = 0;    
    socklen_t addrlen = sizeof(this->udp_client.udpclient_addr);
    
    while (len += recvfrom(this->udp_server.wait_socket, &mData[len], mFullDataSize - len, 0,
            (struct sockaddr*)&this->udp_client.udpclient_addr, &addrlen) != mFullDataSize)
    
    return true;
}

void UDPCommunicator::handleWrite() {
    printf("handleWrite\n");
    ssmTimeT time;    
    while (true) {
        if (!receiveData()) {
            fprintf(stderr, "receivedata error happends\n");
            break;
        }
        //printf("write to memory\n");
        time = *(reinterpret_cast<ssmTimeT*>(mData));
		pstream->write(time);
    }
}

void UDPCommunicator::handleRead() {
    printf("handleRead\n");
}
