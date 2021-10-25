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

}

UDPCommunicator::~UDPCommunicator() {    
}

void* UDPCommunicator::run(void* args) {
    
}
