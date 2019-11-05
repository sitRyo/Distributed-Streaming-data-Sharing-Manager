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

extern pid_t my_pid; // for debug
DataCommunicator::DataCommunicator(uint16_t nport, char* mData, uint64_t d_size,
		uint64_t t_size, SSMApiBase *pstream, PROXY_open_mode type,
		ProxyServer* proxy) {
	this->mData = mData;
	this->mDataSize = d_size;
	this->ssmTimeSize = t_size;
	this->mFullDataSize = d_size + t_size;

	// streamはコピーされる.
	this->pstream = pstream;
	this->mType = type;
	this->proxy = proxy;

	this->buf = (char*) malloc(sizeof(thrd_msg));

	this->server.wait_socket = -1;
	this->server.server_addr.sin_family = AF_INET;
	this->server.server_addr.sin_addr.s_addr = htonl(SERVER_IP);
	this->server.server_addr.sin_port = htons(nport);

	if (!this->sopen()) {
		perror("errororor\n");
	}
}

DataCommunicator::~DataCommunicator() {
	this->sclose();
	if (this->buf)
		free(this->buf);
}

bool DataCommunicator::receiveData() {
	int len = 0;
	while ((len += recv(this->client.data_socket, &mData[len],
			mFullDataSize - len, 0)) != mFullDataSize)
		;
	return true;
}

bool DataCommunicator::deserializeTmsg(thrd_msg *tmsg) {
	char* p = this->buf;
	tmsg->msg_type = proxy->readLong(&p);
	tmsg->res_type = proxy->readLong(&p);
	tmsg->tid = proxy->readInt(&p);
	tmsg->time = proxy->readDouble(&p);

	return true;
}

bool DataCommunicator::serializeTmsg(thrd_msg* tmsg) {
	char* p = this->buf;
	proxy->writeLong(&p, tmsg->msg_type);
	proxy->writeLong(&p, tmsg->res_type);
	proxy->writeInt(&p, tmsg->tid);
	proxy->writeDouble(&p, tmsg->time);
	return true;
}

bool DataCommunicator::sendTMsg(thrd_msg *tmsg) {
	if (serializeTmsg(tmsg)) {
		if (send(this->client.data_socket, this->buf, sizeof(thrd_msg), 0)
				!= -1) {
			return true;
		}
	}
	return false;
}

bool DataCommunicator::receiveTMsg(thrd_msg *tmsg) {
	int len;
	if ((len = recv(this->client.data_socket, this->buf, sizeof(thrd_msg), 0))
			> 0) {
		return deserializeTmsg(tmsg);
	}
	
	return false;
}

void DataCommunicator::handleData() {
	char *p;
	ssmTimeT time;
	while (true) {
		if (!receiveData()) {
			fprintf(stderr, "receiveData Error happends\n");
			break;
		}
		p = &mData[8];

		time = *(reinterpret_cast<ssmTimeT*>(mData));
		pstream->write(time);
	}
	// pstream->showRawData();
}

bool DataCommunicator::sendBulkData(char* buf, uint64_t size) {
	if (send(this->client.data_socket, buf, size, 0) != -1) {
		return true;
	}
	return false;
}

void DataCommunicator::handleRead() {
	thrd_msg tmsg;

	while (true) {
		if (receiveTMsg(&tmsg)) {
			switch (tmsg.msg_type) {
				case TID_REQ: {
					tmsg.tid = getTID(pstream->getSSMId(), tmsg.time);
					tmsg.res_type = TMC_RES;
					sendTMsg(&tmsg);
					break;
				}
				case TOP_TID_REQ: {
					tmsg.tid = getTID_top(pstream->getSSMId());
					tmsg.res_type = TMC_RES;
					sendTMsg(&tmsg);
					break;
				}
				case BOTTOM_TID_REQ: {
					tmsg.tid = getTID_bottom(pstream->getSSMId());
					tmsg.res_type = TMC_RES;
					sendTMsg(&tmsg);
					break;
				}
				case READ_NEXT: {
					int dt = tmsg.tid;
					pstream->readNext(dt);
					tmsg.tid = pstream->timeId;
					tmsg.time = pstream->time;
					tmsg.res_type = TMC_RES;
					if (sendTMsg(&tmsg)) {
						if (!sendBulkData(&mData[sizeof(ssmTimeT)], mDataSize)) {
							perror("send bulk Error");
						}
					}
					break;
				}
				case TIME_ID: {
					SSM_tid req_tid = (SSM_tid) tmsg.tid;
					pstream->read(req_tid);
					tmsg.tid = pstream->timeId;
					tmsg.time = pstream->time;
					tmsg.res_type = TMC_RES;
					if (sendTMsg(&tmsg)) {
						if (!sendBulkData(&mData[sizeof(ssmTimeT)], mDataSize)) {
							perror("send bulk Error");
						}
					}
					break;
				}
				case REAL_TIME: {
					ssmTimeT t = tmsg.time;
					pstream->readTime(t);
					tmsg.tid = pstream->timeId;
					tmsg.time = pstream->time;
					tmsg.res_type = TMC_RES;
					if (sendTMsg(&tmsg)) {
					  if (!sendBulkData(&mData[sizeof(ssmTimeT)], mDataSize)) {
					    perror("send bulk Error");
					  }
					}
					break;
				}
				default: {
					//thrd_msg* ptr = &tmsg;                                        
					break;
				}
			}
		} else {
			break;
		}
	}
}

void* DataCommunicator::run(void* args) {

	if (rwait()) {
		switch (mType) {
		case WRITE_MODE: {
			handleData();
			break;
		}
		case READ_MODE: {
			handleRead();
			break;
		}
		default: {
			perror("no such mode");
		}
		}
//		printf("end of thread\n");
	}
	return nullptr;
}

bool DataCommunicator::sopen() {
	this->server.wait_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	int flag = 1;
	int ret = setsockopt(this->server.wait_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	if (ret == -1) {
		fprintf(stderr, "server setsocket");
		exit(1);
	}
	if (this->server.wait_socket == -1) {
		perror("open socket error");
		return false;
	}
	if (bind(this->server.wait_socket,
			(struct sockaddr*) &this->server.server_addr,
			sizeof(this->server.server_addr)) == -1) {
		perror("data com bind");
		return false;
	}
	if (listen(this->server.wait_socket, 5) == -1) {
		perror("data com open");
		return false;
	}
//	printf("Data Communicator open!!\n");
	return true;
}

bool DataCommunicator::sclose() {
	if (this->client.data_socket != -1) {
		close(this->client.data_socket);
		this->client.data_socket = -1;
	}
	if (this->server.wait_socket != -1) {
		close(this->server.wait_socket);
		this->server.wait_socket = -1;
	}

	return true;
}

bool DataCommunicator::rwait() {
	memset(&this->client, 0, sizeof(this->client));
	this->client.data_socket = -1;
	for (;;) {
		socklen_t client_addr_len = sizeof(this->client.client_addr);
		this->client.data_socket = accept(this->server.wait_socket,
				(struct sockaddr*) &this->client.client_addr, &client_addr_len);
		if (this->client.data_socket != -1)
			break;
		if (errno == EINTR)
			continue;
		perror("server open accept");
		return false;
	}
	return true;
}

ProxyServer::ProxyServer() {
	nport = SERVER_PORT;
	mData = NULL;
	mDataSize = 0;
	ssmTimeSize = sizeof(ssmTimeT);
	mFullDataSize = 0;
	mProperty = NULL;
	mPropertySize = 0;
	com = nullptr;
	mType = WRITE_MODE;
}

ProxyServer::~ProxyServer() {
	this->server_close();
	free(mData);
	mData = NULL;
	delete com;
	com = nullptr;
}

bool ProxyServer::init() {
	setupSigHandler();
	memset(&this->server, 0, sizeof(this->server));
	this->server.wait_socket = -1;
	this->server.server_addr.sin_family = AF_INET;
	this->server.server_addr.sin_addr.s_addr = htonl(SERVER_IP);
	this->server.server_addr.sin_port = htons(SERVER_PORT);

	return this->open();
}

bool ProxyServer::open() {
	this->server.wait_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	int flag = 1;
	int ret = setsockopt(this->server.wait_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	if (ret == -1) {
		perror("proxy setsockopt");
		exit(1);
	}
	
	if (this->server.wait_socket == -1) {
		perror("open socket error");
		return false;
	}
	if (bind(this->server.wait_socket,
			(struct sockaddr*) &this->server.server_addr,
			sizeof(this->server.server_addr)) == -1) {
		perror("server bind");
		return false;
	}
	if (listen(this->server.wait_socket, 5) == -1) {
		perror("server open");
		return false;
	}

	return true;
}

bool ProxyServer::wait() {
	memset(&this->client, 0, sizeof(this->client));
	this->client.data_socket = -1;
	for (;;) {
		socklen_t client_addr_len = sizeof(this->client.client_addr);
		this->client.data_socket = accept(this->server.wait_socket,
				(struct sockaddr*) &this->client.client_addr, &client_addr_len);

		if (this->client.data_socket != -1)
			break;
		if (errno == EINTR)
			continue;
		perror("server open accept");
		return false;
	}
	return true;
}

bool ProxyServer::server_close() {
	if (this->server.wait_socket != -1) {
		close(this->server.wait_socket);
		this->server.wait_socket = -1;
	}
	return true;
}

bool ProxyServer::client_close() {
	if (this->client.data_socket != -1) {
		close(this->client.data_socket);
		this->client.data_socket = -1;
	}
	return true;
}

int ProxyServer::readInt(char **p) {
	uint8_t v1 = **p;
	(*p)++;
	uint8_t v2 = **p;
	(*p)++;
	uint8_t v3 = **p;
	(*p)++;
	uint8_t v4 = **p;
	(*p)++;

	int v = (int) (v1 << 24 | v2 << 16 | v3 << 8 | v4);
	return v;
}

uint64_t ProxyServer::readLong(char **p) {
	uint8_t v1 = **p;
	(*p)++;
	uint8_t v2 = **p;
	(*p)++;
	uint8_t v3 = **p;
	(*p)++;
	uint8_t v4 = **p;
	(*p)++;
	uint8_t v5 = **p;
	(*p)++;
	uint8_t v6 = **p;
	(*p)++;
	uint8_t v7 = **p;
	(*p)++;
	uint8_t v8 = **p;
	(*p)++;

	uint64_t lv = (uint64_t) ((uint64_t) v1 << 56 | (uint64_t) v2 << 48
			| (uint64_t) v3 << 40 | (uint64_t) v4 << 32 | (uint64_t) v5 << 24
			| (uint64_t) v6 << 16 | (uint64_t) v7 << 8 | (uint64_t) v8);
	return lv;
}

double ProxyServer::readDouble(char **p) {
	char buf[8];
	for (int i = 0; i < 8; ++i, (*p)++) {
		buf[7 - i] = **p;
	}
	return *(double*) buf;
}

void ProxyServer::readRawData(char **p, char *d, int len) {
	for (int i = 0; i < len; ++i, (*p)++) {
		d[i] = **p;
	}
}

void ProxyServer::writeInt(char **p, int v) {
	**p = (v >> 24) & 0xff;
	(*p)++;
	**p = (v >> 16) & 0xff;
	(*p)++;
	**p = (v >> 8) & 0xff;
	(*p)++;
	**p = (v >> 0) & 0xff;
	(*p)++;
}

void ProxyServer::writeLong(char **p, uint64_t v) {
	**p = (v >> 56) & 0xff;
	(*p)++;
	**p = (v >> 48) & 0xff;
	(*p)++;
	**p = (v >> 40) & 0xff;
	(*p)++;
	**p = (v >> 32) & 0xff;
	(*p)++;
	this->writeInt(p, v);
}

void ProxyServer::writeDouble(char **p, double v) {
	char *dp = (char*) &v;
	for (int i = 0; i < 8; ++i, (*p)++) {
		**p = dp[7 - i] & 0xff;
	}
}

void ProxyServer::writeRawData(char **p, char *d, int len) {
	for (int i = 0; i < len; ++i, (*p)++)
		**p = d[i];
}

void ProxyServer::deserializeMessage(ssm_msg *msg, char *buf) {
	msg->msg_type = readLong(&buf);
	msg->res_type = readLong(&buf);
	msg->cmd_type = readInt(&buf);
	readRawData(&buf, msg->name, 32);
	msg->suid = readInt(&buf);
	msg->ssize = readLong(&buf);
	msg->hsize = readLong(&buf);
	msg->time = readDouble(&buf);
	msg->saveTime = readDouble(&buf);
}

int ProxyServer::receiveMsg(ssm_msg *msg, char *buf) {
	int len = recv(this->client.data_socket, buf, sizeof(ssm_msg), 0);
	if (len > 0) {
		deserializeMessage(msg, buf);
	}
	return len;
}

int ProxyServer::sendMsg(int cmd_type, ssm_msg *msg) {
	ssm_msg msgbuf;
	uint64_t len;
	char *buf, *p;
	if (msg == NULL) {
		msg = &msgbuf;
	}
	msg->cmd_type = cmd_type;
	buf = (char*) malloc(sizeof(ssm_msg));
	p = buf;
	writeLong(&p, msg->msg_type);
	writeLong(&p, msg->res_type);
	writeInt(&p, msg->cmd_type);
	writeRawData(&p, msg->name, 32);
	writeInt(&p, msg->suid);
	writeLong(&p, msg->ssize);
	writeLong(&p, msg->hsize);
	writeDouble(&p, msg->time);
	writeDouble(&p, msg->saveTime);

	if ((len = send(this->client.data_socket, buf, sizeof(ssm_msg), 0)) == -1) {
		fprintf(stderr, "error happens\n");
	}

	free(buf);
	return len;
}

void ProxyServer::handleCommand() {
	ssm_msg msg;
	char *buf = (char*) malloc(sizeof(ssm_msg));
	while (true) {
		int len = receiveMsg(&msg, buf);
		if (len == 0)
			break;
		switch (msg.cmd_type & 0x1f) {
		case MC_NULL: {
			break;
		}
		case MC_INITIALIZE: {
			if (!initSSM()) {
				fprintf(stderr, "init ssm error in ssm-proxy\n");
				sendMsg(MC_FAIL, &msg);
				break;
			} else {
				sendMsg(MC_RES, &msg);
			}
			break;
		}
		case MC_CREATE: {
			setSSMType(WRITE_MODE);
			mDataSize = msg.ssize;
			mFullDataSize = mDataSize + sizeof(ssmTimeT);
			if (mData) {
				free(mData);
			}
			mData = (char*) malloc(mFullDataSize);


			if (mData == NULL) {
				fprintf(stderr, "fail to create mData\n");
				sendMsg(MC_FAIL, &msg);
			} else {

				stream.setDataBuffer(&mData[sizeof(ssmTimeT)], mDataSize);
				if (!stream.create(msg.name, msg.suid, msg.saveTime,
						msg.time)) {
					sendMsg(MC_FAIL, &msg);
					break;
				}
				sendMsg(MC_RES, &msg);
			}

			break;
		}
		case MC_OPEN: {
			mDataSize = msg.ssize;
			mFullDataSize = mDataSize + sizeof(ssmTimeT);
			if (mData) {
				free(mData);
			}
			mData = (char*) malloc(mFullDataSize);

			SSM_open_mode openMode = (SSM_open_mode) (msg.cmd_type & SSM_MODE_MASK);

			switch (openMode) {
				case SSM_READ: {
					setSSMType(READ_MODE);
					break;
				}
				case SSM_WRITE: {
					setSSMType(WRITE_MODE);
					break;
				}
				default: {
					fprintf(stderr, "unknown ssm_open_mode\n");
				}
			}

			if (mData == NULL) {
				fprintf(stderr, "fail to create mData");
				sendMsg(MC_FAIL, &msg);
			} else {
				stream.setDataBuffer(&mData[sizeof(ssmTimeT)], mDataSize);
				if (!stream.open(msg.name, msg.suid)) {
					fprintf(stderr, "stream open failed");
					endSSM();
					sendMsg(MC_FAIL, &msg);
				} else {
					sendMsg(MC_RES, &msg);
				}
			}
			break;
		}
		case MC_STREAM_PROPERTY_SET: {
			mPropertySize = msg.ssize;
			if (mProperty) {
				free(mProperty);
			}
			mProperty = (char*) malloc(mPropertySize);
			if (mProperty == NULL) {
				sendMsg(MC_FAIL, &msg);
				break;
			}
			stream.setPropertyBuffer(mProperty, mPropertySize);
			sendMsg(MC_RES, &msg);
			uint64_t len = 0;
			while ((len += recv(this->client.data_socket, &mProperty[len],
					mPropertySize - len, 0)) != mPropertySize)
				;

			if (len == mPropertySize) {
//				printf("receive property\n");
				if (!stream.setProperty()) {
					sendMsg(MC_FAIL, &msg);
					break;
				}
                                /*
				if (mProperty != NULL) {
					printf("mProperty is not null\n");
				} else {
					printf("mProperty is null\n");
				}
                                 */
				sendMsg(MC_RES, &msg);
			} else {
				sendMsg(MC_FAIL, &msg);
			}
			break;
		}
		case MC_STREAM_PROPERTY_GET: {
			if (mProperty) {
				free(mProperty);
			}
			mPropertySize = msg.ssize;
			mProperty = (char*) malloc(mPropertySize);
			if (mProperty == NULL) {
				sendMsg(MC_FAIL, &msg);
				break;
			}
			stream.setPropertyBuffer(mProperty, mPropertySize);

			if (!stream.getProperty()) {
				fprintf(stderr, "can't get property on SSM\n");
				sendMsg(MC_FAIL, &msg);
				break;
			}

			sendMsg(MC_RES, &msg);
			if (send(this->client.data_socket, mProperty, mPropertySize, 0)
					== -1) {
				fprintf(stderr, "packet send error\n");
			}
			break;
		}
		case MC_OFFSET: {
			ssmTimeT offset = msg.time;
			settimeOffset(offset);
			sendMsg(MC_RES, &msg);
			break;
		}
		case MC_CONNECTION: {
			msg.suid = nport;
			com = new DataCommunicator(nport, mData, mDataSize, ssmTimeSize,
					&stream, mType, this);
			com->start(nullptr);
			sendMsg(MC_RES, &msg);
			break;
		}
		case MC_TERMINATE: {
			sendMsg(MC_RES, &msg);
			goto END_PROC;
			break;
		}
		default: {
			fprintf(stderr, "NOTICE : unknown msg %d", msg.cmd_type);
			break;
		}
		}

	}

	END_PROC: free(buf);
	if (com) {
		com->wait();
	}
	free(com);

	inittimeSSM();
	endSSM();
}

void ProxyServer::setSSMType(PROXY_open_mode mode) {
	mType = mode;
}

bool ProxyServer::run() {
//	printf("run\n");
	while (wait()) {
		++nport;
		pid_t child_pid = fork();
		if (child_pid == -1) { // fork failed
			break;
		} else if (child_pid == 0) { // child
			this->server_close();
			this->handleCommand();
			this->client_close();
			//fprintf(stderr, "end of process");
			exit(1);
		} else { // parent
			this->client_close();
		}
	}
	server_close();
	return true;
}

void ProxyServer::setupSigHandler() {
	struct sigaction act;
	memset(&act, 0, sizeof(act)); /* sigaction構造体をとりあえずクリア */
	act.sa_handler = &ProxyServer::catchSignal; /* SIGCHLD発生時にcatch_SIGCHLD()を実行 */
	sigemptyset(&act.sa_mask); /* catch_SIGCHLD()中の追加シグナルマスクなし */
	act.sa_flags = SA_NOCLDSTOP | SA_RESTART;
	sigaction(SIGCHLD, &act, NULL);
}

void ProxyServer::catchSignal(int signo) {
	//printf("catch signal!!!!");
	pid_t child_pid = 0;
	/* すべての終了している子プロセスに対してwaitpid()を呼ぶ */
	do {
		int child_ret;
		child_pid = waitpid(-1, &child_ret, WNOHANG);
		/* すべての終了している子プロセスへwaitpid()を呼ぶと
		 WNOHANGオプションによりwaitpid()は0を返す */
	} while (child_pid > 0);
}

int main(void) {
	ProxyServer server;
	server.init();
	server.run();

	return 0;
}
