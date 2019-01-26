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

//#include <openssl/md5.h> // debug

#include <errno.h>
#include <sys/fcntl.h>

#include "libssm.h"
#include "ssm-time.h"
#include "ssm.h"

#include "printlog.hpp"

//#include "ssm-coordinator.h"
#include "libssm.h"
#include "ssm-proxy.hpp"

extern pid_t my_pid; // for debug
/*
 static void CalcurateMD5Hash(unsigned char *buffer, long size, unsigned char* hash)
 {
 MD5_CTX ctx;
 MD5_Init(&ctx);
 MD5_Update(&ctx, buffer, size);
 MD5_Final(hash, &ctx);
 }
 */
DataCommunicator::DataCommunicator(uint16_t nport, char* mData, uint64_t d_size,
		uint64_t t_size, SSMApiBase *pstream, PROXY_open_mode type,
		ProxyServer* proxy) {
	printf("DataCommunicatir new\n");
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

//    printf("deserialize buf = %p\n", this->buf);
	/*
	 printf("msg_type = %d\n", tmsg->msg_type);
	 printf("res_type = %d\n", tmsg->res_type);
	 printf("tid      = %d\n", tmsg->tid);
	 printf("time     = %f\n", tmsg->time);
	 */
	return true;
}

bool DataCommunicator::serializeTmsg(thrd_msg* tmsg) {
	char* p = this->buf;
	proxy->writeLong(&p, tmsg->msg_type);
	proxy->writeLong(&p, tmsg->res_type);
	proxy->writeInt(&p, tmsg->tid);
	proxy->writeDouble(&p, tmsg->time);
	/*
	 for (int i = 0; i < sizeof(thrd_msg); ++i) {
	 if (i % 16 == 0) printf("\n");
	 printf("%02x ", this->buf[i]);
	 }
	 printf("\n");
	 */
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
//        unsigned char hash[MD5_DIGEST_LENGTH];

//        FILE* fp = fopen("recv.log", "w");

	while (true) {
		if (!receiveData()) {
			fprintf(stderr, "receiveData Error happends\n");
			break;
		}
		p = &mData[8];
		time = *(reinterpret_cast<ssmTimeT*>(mData));
		pstream->write(time);
		printf("tid(%d): ", pstream->timeId);
	}
	pstream->showRawData();
}

bool DataCommunicator::sendBulkData(char* buf, uint64_t size) {
	if (send(this->client.data_socket, buf, size, 0) != -1) {
		return true;
	}
	return false;
}

void DataCommunicator::handleRead() {
	thrd_msg tmsg;
	std::cout << "handleRead called" << std::endl;

	while (true) {
		if (receiveTMsg(&tmsg)) {
			switch (tmsg.msg_type) {
			case TOP_TID_REQ: {
				tmsg.tid = getTID_top(pstream->getSSMId());
				tmsg.res_type = TMC_RES;
				sendTMsg(&tmsg);
				break;
			}
			case TIME_ID: {
				SSM_tid req_tid = (SSM_tid) tmsg.tid;
				pstream->read(req_tid);
				tmsg.tid = pstream->timeId;
				tmsg.time = pstream->time;
				tmsg.res_type = TMC_RES;
				if (sendTMsg(&tmsg)) {

					/*printf("mDataSize = %d\n", (int) mDataSize);
					for (int i = 0; i < 8; ++i) {
						printf("%02x ", mData[i + sizeof(ssmTimeT)] & 0xff);
					}*/

					// printf("\n");
					if (!sendBulkData(&mData[sizeof(ssmTimeT)], mDataSize)) {
						perror("send bulk Error");
					}
				}
				break;
			}
				defualt: {
					fprintf(stderr, "NOTICE : unknown msg_type %d",
							tmsg.msg_type);
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
			// std::cout << "write..." << std::endl;
			handleData();
			break;
		}
		case READ_MODE: {
			// std::cout << "...read" << std::endl;
			handleRead();
			break;
		}
		default: {
			perror("no such mode");
		}
		}
		printf("end of thread\n");
	}
	return nullptr;
}

bool DataCommunicator::sopen() {
	this->server.wait_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
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
	printf("Data Communicagtor open!!\n");
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
		printf("wait!!!\n");
		this->client.data_socket = accept(this->server.wait_socket,
				(struct sockaddr*) &this->client.client_addr, &client_addr_len);
		printf("pppppp\n");
		if (this->client.data_socket != -1)
			break;
		if (errno == EINTR)
			continue;
		perror("server open accept");
		return false;
	}
	printf("wait2!!!\n");
	return true;
}

ProxyServer::ProxyServer() {
	printf("Proxy Server created\n");
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
	printf("proxy server deleted\n");
	this->server_close();
	free(mData);
	mData = NULL;
	delete com;
	com = nullptr;
}

bool ProxyServer::init() {
	printf("init\n");
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
//	printf("handlecommand\n");
	fprintf(stderr, "nport = %d\n", nport);
	ssm_msg msg;
	// SSM_List *slist;
	char *buf = (char*) malloc(sizeof(ssm_msg));
	while (true) {
		printf("wait recv\n");
		int len = receiveMsg(&msg, buf);
		printf("len in process = %d\n", len);
		if (len == 0)
			break;
		switch (msg.cmd_type & 0x1f) {
		case MC_NULL: {
			break;
		}
		case MC_INITIALIZE: {
			printf("MC_INITIALIZE\n");

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
			printf("MC_CREATE\n");

			setSSMType(WRITE_MODE);

			std::cout << "********************************" << std::endl;
			printf("strean name = %s\n", msg.name);
			printf("stream_id = %d\n", msg.suid);
			printf("ssm_size = %d\n", (int) msg.ssize);
			printf("hsize =  %d\n", (int) msg.hsize);
			printf("msg.time(cycle) = %f\n", msg.time);
			printf("msg.saveTime = %f\n", msg.saveTime);
			std::cout << "********************************" << std::endl;
			mDataSize = msg.ssize;
			mFullDataSize = mDataSize + sizeof(ssmTimeT);
			if (mData) {
				free(mData);
			}
			mData = (char*) malloc(mFullDataSize);

			// メモリ番地を調べる
			printf("mData's pointer %p\n", mData);
			printf("sizeof(ssmTimeT): %d\n", sizeof(ssmTimeT));
			printf("mData[sizeof(ssmTimeT)] is %p\n", &mData[sizeof(ssmTimeT)]);
			std::cout << "*******************************" << std::endl;

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
				printf("stream is created\n");
				sendMsg(MC_RES, &msg);
			}
			break;
		}
		case MC_OPEN: {
			printf("MC_OPEN\n");

			setSSMType(READ_MODE);

			mDataSize = msg.ssize;
			mFullDataSize = mDataSize + sizeof(ssmTimeT);
			if (mData) {
				free(mData);
			}
			mData = (char*) malloc(mFullDataSize);
			printf("MC_OPEN mData -> %p\n", mData);
			if (mData == NULL) {
				std::cout << "fail to create mData" << std::endl;
				sendMsg(MC_FAIL, &msg);
			} else {
				stream.setDataBuffer(&mData[sizeof(ssmTimeT)], mDataSize);
				printf("?mData pointer: %p\n", stream.data());
				if (!stream.open(msg.name, msg.suid)) {
					std::cout << "stream open failed" << std::endl;
					endSSM();
					sendMsg(MC_FAIL, &msg);
				} else {
					printf("stream open\n");
					sendMsg(MC_RES, &msg);
				}
			}
			break;
		}
		case MC_STREAM_PROPERTY_SET: {
			printf("MC_STREAM_PROPERTY_SET\n");
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

			printf("mProperty size: %ld\n", mPropertySize);
			printf("ready to receive property\n");


			sendMsg(MC_RES, &msg);
			uint64_t len = 0;
			while ((len += recv(this->client.data_socket, &mProperty[len], mPropertySize - len, 0)) != mPropertySize) ;
			// int len = recv(this->client.data_socket, mProperty, msg.ssize, 0);

			if (len == mPropertySize) {
				printf("receive property\n");
				if (!stream.setProperty()) {
					sendMsg(MC_FAIL, &msg);
					break;
				}
				printf("set property\n");

				/* memory dump 
				for (int i = 0; i < 8; ++i) {
					printf("%02x ", ((char*)mProperty)[i] & 0xff);
				}
				printf("\n"); */

				if (mProperty != NULL) {
					printf("mProperty is not null\n");
				} else {
					printf("mProperty is null\n");
				}
				sendMsg(MC_RES, &msg);
			} else {
				sendMsg(MC_FAIL, &msg);
			}
			break;
		}
		case MC_STREAM_PROPERTY_GET: {
			printf("MC_STREAM_PROPERTY_GET\n");
			if (mProperty) {
				free(mProperty);
			}
			mPropertySize = msg.ssize;
			mProperty = (char*) malloc(mPropertySize);
			if (mProperty == NULL) {
				sendMsg(MC_FAIL, &msg);
				break;
			}

			// propertyを取得
			stream.setPropertyBuffer(mProperty, mPropertySize);

			if (!stream.getProperty()) {
				printf("can't get property on SSM\n");
				sendMsg(MC_FAIL, &msg);
				break;
			}

			/* memory dump
			for (int i = 0; i < 8; ++i) {
				printf("%02x ", ((char*)mProperty)[i] & 0xff);
			}
			printf("\n"); */

			sendMsg(MC_RES, &msg);
			printf("send property\n");
			if (send(this->client.data_socket, mProperty, mPropertySize, 0)
					== -1) {
				fprintf(stderr, "packet send error\n");
			}
			break;
		}
		case MC_OFFSET: {
			printf("MC_OFFSET\n");
			ssmTimeT offset = msg.time;
//			printf("time  = %f\n", offset);
			settimeOffset(offset);
//			printf("time2 = %f\n", gettimeOffset());
			sendMsg(MC_RES, &msg);
			break;
		}
		case MC_CONNECTION: {
			printf("MC_CONNECTION\n");
			msg.suid = nport;
			// DataCommunicatorはThreadを継承
			com = new DataCommunicator(nport, mData, mDataSize, ssmTimeSize,
					&stream, mType, this);
			com->start(nullptr);
			sendMsg(MC_RES, &msg);
			break;
		}
		case MC_TERMINATE: {
			printf("MC_TERMINATE\n");
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

	// SSMの終了
	// 時間の初期化
	inittimeSSM();
	endSSM();
}

void ProxyServer::setSSMType(PROXY_open_mode mode) {
	mType = mode;
}

bool ProxyServer::run() {
	printf("run\n");
	std::cout << "runrun" << std::endl;
	while (wait()) {
		++nport;
		pid_t child_pid = fork();
		if (child_pid == -1) { // fork failed
			break;
		} else if (child_pid == 0) { // child
			this->server_close();
			this->handleCommand();
			this->client_close();
			printf("end of process");
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
	printf("uint64_t size = %d\n", (int) sizeof(uint64_t));
	server.init();
	server.run();
	test();

	return 0;
}
