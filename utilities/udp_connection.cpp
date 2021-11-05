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

    //printf("UDPCommunicator::run\n");

    switch (mType) {
        case WRITE_MODE: {
            //printf("write mode\n");
            handleWrite();
            break;
        }
        case READ_MODE: {
            //printf("read mode\n");
            handleRead();
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
    //printf("handleWrite\n");
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

bool UDPCommunicator::serializeTmsg(thrd_msg* tmsg) {
    char* p = this->buf;
	proxy->writeLong(&p, tmsg->msg_type);
	proxy->writeLong(&p, tmsg->res_type);
	proxy->writeInt(&p, tmsg->tid);
	proxy->writeDouble(&p, tmsg->time);
	return true;    
}

bool UDPCommunicator::deserializeTmsg(thrd_msg* tmsg) {
    //printf("deserialize tmsg\n");
    memset((char*)tmsg, 0, sizeof(thrd_msg));
    char* p = this->buf;
	tmsg->msg_type = proxy->readLong(&p);
	tmsg->res_type = proxy->readLong(&p);
	tmsg->tid = proxy->readInt(&p);
	tmsg->time = proxy->readDouble(&p);

    //printf("msg_type = %d\n", tmsg->msg_type);    

	return true;
}

bool UDPCommunicator::sendTmsg(thrd_msg* tmsg) {
    if(serializeTmsg(tmsg)) {
        // send udp
        if(sendto(this->udp_server.wait_socket, this->buf, this->thrdMsgLen, 0, 
            (struct sockaddr*)&this->udp_client.udpclient_addr, sizeof(this->udp_client.udpclient_addr)) != -1) {
                return true;
        }
    } 
    return false;
}

bool UDPCommunicator::sendBulkData(char* bdata, uint64_t size) {
    /*
    printf("send bulk data\n");    
    for (int i = 0; i < size; ++i) {
        if (i % 16 == 0) printf("\n");
        printf("%02x ", bdata[i]);
    }
    printf("\n");
    */
    
    if (sendto(this->udp_server.wait_socket, bdata, size, 0,
        (struct sockaddr*)&this->udp_client.udpclient_addr, sizeof(this->udp_client.udpclient_addr)) != -1) {
        return true;
    }
    return false;
}

bool UDPCommunicator::receiveTmsg(thrd_msg* tmsg) {
    int len = 0;
    socklen_t addrlen = sizeof(this->udp_client.udpclient_addr);
    
    //printf("thrdMsgLen = %d\n", thrdMsgLen);
   
    if ((len = recvfrom(this->udp_server.wait_socket, this->buf, this->thrdMsgLen, 0,
        (struct sockaddr*)&this->udp_client.udpclient_addr, &addrlen)) > 0) {
            // deserialize
            //printf("received len = %d\n", len);
            /*
            for (int i = 0; i < this->thrdMsgLen; ++i) {
                if (i % 16 == 0) printf("\n");
                printf("%02x ", buf[i] & 0xff);
            }
            printf("\n");
            */

            return deserializeTmsg(tmsg);            
    }
    
    return true;
}

void UDPCommunicator::handleRead() {
    printf("handleRead\n");
    thrd_msg tmsg;
    while (true) {
        if(receiveTmsg(&tmsg)) {
            switch (tmsg.msg_type) {
                case TIME_ID: {
                    SSM_tid req_tid = (SSM_tid)tmsg.tid;                    
                    if (!pstream->read(req_tid)) {
						fprintf(stderr, "[%s] SSMApi::read error.\n", pstream->getStreamName());
					}
					tmsg.tid = pstream->timeId;
					tmsg.time = pstream->time;
					tmsg.res_type = TMC_RES;
                    
                    if(sendTmsg(&tmsg)) {
                        //printf("sent tmsg!\n");
                        if (!sendBulkData(&mData[sizeof(ssmTimeT)], mDataSize)) {
							perror("send bulk Error");
						}
                    }
                    break;
                }
                case REAL_TIME: {                    
                    ssmTimeT t = tmsg.time;
                    if (!pstream->readTime(t)) {
                        fprintf(stderr, "[%s] SSMApi::readTime error.\n", pstream->getStreamName());
                    }
                    tmsg.tid = pstream->timeId;
                    tmsg.time = pstream->time;
                    tmsg.res_type = TMC_RES;
                    if (sendTmsg(&tmsg)) {
                        if (!sendBulkData(&mData[sizeof(ssmTimeT)], mDataSize)) {
                            perror("send bulk Error");
                        }
                    }                    
                    break;
                }
                default: {
                    fprintf(stderr, "unrecognized msg_type %d\n", tmsg.msg_type);
                    break;
                }
            }

        } else {
            fprintf(stderr, "receiveTmsg is not valid\n");
            break;
        }
    }
}
