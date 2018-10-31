#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include <iostream>
#include <string>

#include "ssm.h"
#include "ssm-proxy-client.hpp"
#include "ssm-proxy-client-child.hpp"


#define FOR_DEBUG 0

PConnector::PConnector() {
	initPConnector();
}

PConnector::PConnector(const char *streamName, int streamId) {
	initPConnector();
	setStream(streamName, streamId);
}

PConnector::~PConnector() {
	printf("PConnector destructor\n");
	if (sock != -1) {
		close(sock);
	}
}

void PConnector::initPConnector() {
	printf("PConnector constructor\n");
	sock = -1;
	dsock = -1;
	mDataSize = 0;
	streamId = 0;
	mData = NULL;
	mFullData = NULL;
	mProperty = NULL;
	mPropertySize = 0;
	streamName = "";
	mFullData = NULL;
	mFullDataSize = 0;
	openMode = PROXY_INIT;
	timeId = -1;
	timecontrol = NULL;
}

int PConnector::readInt(char **p) {
	uint8_t v1 = **p; (*p)++;
	uint8_t v2 = **p; (*p)++;
	uint8_t v3 = **p; (*p)++;
	uint8_t v4 = **p; (*p)++;

	int v =(int)( v1 << 24 | v2 << 16 | v3 << 8 | v4);
	return v;
}

long PConnector::readLong(char **p) {
	uint8_t v1 = **p; (*p)++;
	uint8_t v2 = **p; (*p)++;
	uint8_t v3 = **p; (*p)++;
	uint8_t v4 = **p; (*p)++;
	uint8_t v5 = **p; (*p)++;
	uint8_t v6 = **p; (*p)++;
	uint8_t v7 = **p; (*p)++;
	uint8_t v8 = **p; (*p)++;

	long lv = (long)((long)v1 << 56 | (long)v2 << 48 | (long)v3 << 40 | (long)v4 << 32
			| (long)v5 << 24 | (long)v6 << 16 | (long)v7 << 8 | (long)v8);
	return lv;
}

double PConnector::readDouble(char **p) {
	char buf[8];
	for (int i = 0; i < 8; ++i, (*p)++) {
		buf[7-i] = **p;
	}
	return *(double*)buf;
}

void PConnector::readRawData(char **p, char *d, int len) {
	for (int i = 0; i < len; ++i, (*p)++) {
		d[i] = **p;
	}
}

void PConnector::writeInt(char **p, int v) {
	**p = (v >> 24) & 0xff; (*p)++;
	**p = (v >> 16) & 0xff; (*p)++;
	**p = (v >> 8)  & 0xff; (*p)++;
	**p = (v >> 0)  & 0xff; (*p)++;
}

void PConnector::writeLong(char **p, long v) {
	**p = (v >> 56) & 0xff; (*p)++;
	**p = (v >> 48) & 0xff; (*p)++;
	**p = (v >> 40)  & 0xff; (*p)++;
	**p = (v >> 32)  & 0xff; (*p)++;
	this->writeInt(p, v);
}

void PConnector::writeDouble(char **p, double v) {
	char *dp = (char*)&v;
	for (int i = 0; i < 8; ++i, (*p)++) {
		**p = dp[7 - i] & 0xff;
	}
}

void PConnector::writeRawData(char **p, char *d, int len) {
	for (int i = 0; i < len; ++i, (*p)++) **p = d[i];
}

void* PConnector::getData() {
	// mDataのゲッタ. 使用するときはTにキャスト. 返り値はポインタであることに注意.
	return mData;
}

bool PConnector::isOpen() {
	// streamは開いているか？
	return (streamId != 0) ? true : false;
}

bool PConnector::connectToServer(const char* serverName, int port) {
	printf("connect to server\n");

	sock = socket(AF_INET, SOCK_STREAM, 0);

	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = inet_addr(serverName);
	if(connect(sock, (struct sockaddr *)&server, sizeof(server))) {
		fprintf(stderr, "connection error\n");
		return false;
	}
	printf("sock = %d\n", sock);
	printf("connect to server2	\n");
	return true;
}


bool PConnector::connectToDataServer(const char* serverName, int port) {

	dsock = socket(AF_INET, SOCK_STREAM, 0);
	dserver.sin_family = AF_INET;
	dserver.sin_port = htons(port);
	dserver.sin_addr.s_addr = inet_addr(serverName);
	if(connect(dsock, (struct sockaddr *)&dserver, sizeof(dserver))) {
		fprintf(stderr, "connection error\n");
		return false;
	}
	printf("dsock = %d\n", sock);
	printf("connect to dataserver	\n");
	return true;
}



bool PConnector::initRemote() {
	bool r = true;
	ssm_msg msg;
	char *msg_buf = (char*)malloc(sizeof(ssm_msg));
	// Todo change IPAddress
	connectToServer("127.0.0.1", 8080);
	if(!sendMsgToServer(MC_INITIALIZE, NULL)) {
		fprintf(stderr, "error in initRemote\n");
	}
	if (recvMsgFromServer(&msg, msg_buf)) {
		printf("msg = %d\n", (int)msg.cmd_type);
		if (msg.cmd_type != MC_RES) {
			r = false;
		}
	} else {
		fprintf(stderr, "fail recvMsg\n");
		r = false;
	}
	free(msg_buf);
	return r;

}

bool PConnector::initSSM() {
	return initRemote();
}

void PConnector::serializeMessage(ssm_msg *msg, char *buf) {
	printf("serialize message\n");
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

bool PConnector::recvMsgFromServer(ssm_msg *msg, char *buf) {
	printf("ready to receive\n");
	int len = recv(sock, buf, sizeof(ssm_msg), 0);
	if (len > 0) {
		serializeMessage(msg, buf);
		return true;
	}
	return false;
}

/* read */

// 前回のデータの1つ(以上)前のデータを読み込む
bool PConnector::readBack(int dt = 1) {
	return (dt <= timeId ? read(timeId - dt) : false);
}

// 新しいデータを読み込む
bool PConnector::readLast() {
	return read(-1);
}

// 前回のデータの1つ(以上)あとのデータを読み込む
bool PConnector::readNext(int dt = 1) {
	if (!isOpen()) {
		return false;
	}
	SSM_tid tid,rtid;
	if (timeId < 0) {
		return read(-1);
	}
	rtid = timeId + dt;
	return read(rtid, READ_NEXT);
}

bool PConnector::readNew() {
	// isBlockingは考えなくて良い(proxy側でやるから)
	return (isOpen() ? read(-1) : false);
}

// 時間で取得
bool PConnector::readTime(ssmTimeT t) {
	READ_packet_type type = REAL_TIME; // 時間を送る -> libssm.h
	size_t len = sizeof(int) + sizeof(ssmTimeT);
	void* buf = malloc(len);
	((READ_packet_type *)buf)[0] = type;
	*((ssmTimeT *)&(((READ_packet_type *)buf)[1])) = t;

	if (send(dsock, buf, len, 0) == -1) {
		std::cout << "error : PConnector::readTime" << std::endl;
		free(buf);
		return false;
	}

	free(buf);
	return recvData();
}

// timeidをproxyに送信 -> proxy側でtimeidを更新してもらう
bool PConnector::read(SSM_tid tmid, READ_packet_type type) {
	size_t len = sizeof(int) + sizeof(ssmTimeT);
	void* buf = malloc(len);
	((int *)buf)[0] = type;
	((int *)buf)[1] = tmid;

	printf("type -> %d\n", ((int *)buf)[0]);
	printf("client time id -> %d\n", ((int *)buf)[1]);

	// パケットをまとめて送信.
	// buffer size -> 8(int 2つ分), (buf + 4) -> type, (buf + 8) -> timeid
	if (send(dsock, buf, len, 0) == -1) {
		free(buf);
		std::cout << "error happen : PConnector::read" << std::endl;
		return false;
	}

	free(buf);
	return recvData();
}

/* read ここまで　*/


bool PConnector::recvData() {
	// 受信するデータ, mData, timestamp, timeid
	// サイズ: mDataSize + sizeof(ssmTimeT) + sizeof(SSM_tid)
	// mDataを更新 <- こいつはポインタ

	size_t size = mDataSize + sizeof(ssmTimeT) + sizeof(SSM_tid);
	void* buf = malloc(size);

	int len = recv(dsock, buf, size, 0);

	printf("len -> %d\n", len);

	if (len != size) {
		perror("recv");
		free(buf);
		std::cout << "error : receive mData" << std::endl;
		return false;
	}

	// mDataをコピー、やっぱり型情報が欲しい...
	for (int i = 0; i < mDataSize; ++i) {
		((char *)mData)[i] = ((char *)buf)[i];
	}

	time = *((ssmTimeT *)&(((char *)buf)[mDataSize]));
	timeId = *((SSM_tid *)&(((char *)buf)[mDataSize + sizeof(ssmTimeT)]));

	/* recv data print */
	printf("\nfull size = %d\n", mFullDataSize);
	char *p = (char*)buf;

	for (int i = 0; i < size; ++i) {
		printf("%02x ", p[i] & 0xff);
	}
	printf("\n");

	free(buf);
	return true;
}

ssmTimeT PConnector::getRealTime() {
	struct timeval current;
	gettimeofday( &current, NULL );
	return current.tv_sec + current.tv_sec / 1000000.0;
}

// prototype time = getTimeSSM()
bool PConnector::write( ssmTimeT time ) {
	/*
	printf("\nfull size = %d\n", mFullDataSize);
	char *p = &((char*)mFullData)[8];

	for (int i = 0; i < 8; ++i) {
		printf("%02x ", p[i] & 0xff);
	}
	printf("\n");
	*/
	/*
	if (time == 0) {
		struct timeval current;
		gettimeofday( &current, NULL );
		time = current.tv_sec + current.tv_sec / 1000000.0;;
	}
	*/

	// 先頭にアドレスを入れたい
	*((ssmTimeT*)mFullData) = time;
	printf("mFullData: %p\n", (ssmTimeT *)mFullData);

	printf("%f\n", *(ssmTimeT *)mFullData);
	std::cout << "write!" << std::endl;

	//if (send(sock, mFullData, mFullDataSize, 0) == -1) {
	if (send(dsock, mFullData, mFullDataSize, 0) == -1) { // データ送信用経路を使う
		// fprintf(stderr, "write data send error happen!!!!\n");
		perror("send");
		return false;
	}

	return true;
}

bool PConnector::sendMsgToServer(int cmd_type, ssm_msg *msg) {
	ssm_msg msgbuf;
	char *buf, *p;
	if (msg == NULL) {
		msg = &msgbuf;
	}
	msg->msg_type = 1; // dummy
	msg->res_type = 8;
	msg->cmd_type = cmd_type;

	buf = (char*)malloc(sizeof(ssm_msg));
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

	if (send(sock, buf, sizeof(ssm_msg), 0) == -1) {
		fprintf(stderr, "error happens\n");
		free(buf);
		return false;
	}
	free(buf);
	return true;
}

bool PConnector::sendData(const char *data, size_t size) {
	if (send(sock, data, size, 0) == -1) {
		fprintf(stderr, "error in sendData\n");
		return false;
	}
	return true;
}

// mDataにfulldataのポインタをセット
void PConnector::setBuffer(void *data, size_t dataSize, void *property, size_t propertySize, void* fulldata) {
	mData = data;
	mDataSize = dataSize;
	mProperty = property;
	mPropertySize = propertySize;
	mFullData = fulldata;
	mFullDataSize = mDataSize + sizeof(ssmTimeT);
}

void PConnector::setStream(const char *streamName, int streamId = 0) {
	this->streamName = streamName;
	this->streamId = streamId;
}

bool PConnector::createRemoteSSM( const char *name, int stream_id, size_t ssm_size, ssmTimeT life, ssmTimeT cycle ) {
	ssm_msg msg;
	int open_mode = SSM_READ | SSM_WRITE;
	size_t len;

	/* initialize check */
	if( !name ) {
		fprintf( stderr, "SSM ERROR : create : stream name is NOT defined, err.\n" );
		return false;
	}
	len = strlen( name );
	if( len == 0 || len >= SSM_SNAME_MAX ) {
		fprintf( stderr, "SSM ERROR : create : stream name length of '%s' err.\n", name );
		return false;
	}

	if( stream_id < 0 ) {
		fprintf( stderr, "SSM ERROR : create : stream id err.\n" );
		return false;
	}

	if( life <= 0.0 ) {
		fprintf( stderr, "SSM ERROR : create : stream life time err.\n" );
		return false;
	}

	if( cycle <= 0.0 ) {
		fprintf( stderr, "SSM ERROR : create : stream cycle err.\n" );
		return false;
	}

	if( life < cycle ) {
		fprintf( stderr, "SSM ERROR : create : stream saveTime MUST be larger than stream cycle.\n" );
		return false;
	}

	strncpy( msg.name, name, SSM_SNAME_MAX );
	msg.suid = stream_id;
	msg.ssize = ssm_size;
	msg.hsize = calcSSM_table( life, cycle ) ;	/* table size */
	msg.time = cycle;
	msg.saveTime = life;

	printf("msg.suid  = %d\n", msg.suid);
	printf("msg.ssize = %d\n", msg.ssize);
	printf("msg.hsize  = %d\n", msg.hsize);
	printf("msg.time  = %f\n", msg.time);

	bool r = true;
	char *msg_buf = (char*)malloc(sizeof(ssm_msg));
	printf("msg_buf: %p\n", msg_buf);
	if (!sendMsgToServer(MC_CREATE | open_mode, &msg)) {
		fprintf(stderr, "error in createRemoteSSM\n");
		r = false;
	}
	if (recvMsgFromServer(&msg, msg_buf)) {
		printf("msg %d\n", (int)msg.cmd_type);
		if (msg.cmd_type != MC_RES) {
			fprintf(stderr, "MC_CREATE Remote RES is not MC_RES\n");
			r = false;
		}
	} else {
		fprintf(stderr, "fail recvMsg\n");
		r = false;
	}
	free(msg_buf);
	return r;
}

bool PConnector::open(SSM_open_mode openMode) {
	ssm_msg msg;

	if (!mDataSize) {
		std::cerr << "ssm-proxy-client: data buffer of" << streamName << "', id = " << streamId << " is not allocked." << std::endl;
		return false;
	}

	// メッセージにストリームIDとストリームネームをセット
	msg.suid = streamId;
	strncpy( msg.name, streamName, SSM_SNAME_MAX);
	msg.ssize = mDataSize;

	// ssmOpen( streamName, streamId, openMode )の実装
	// 内部のcommunicatorでMC_OPENを発行。
	// PConnector::sendMsgToServer(int cmd_type, ssm_msg *msg)を実装
	if (!sendMsgToServer(MC_OPEN | (int) openMode, &msg)) {
		std::cout << "PConnector::open: error" << std::endl;
		return false;
	}

	// proxyからのメッセージを受信
	char *msg_buf = (char*)malloc(sizeof(ssm_msg));
	if (recvMsgFromServer(&msg, msg_buf)) {
		printf("msg %d\n", (int)msg.cmd_type);
		if (!(msg.cmd_type == MC_RES)) {
			std::cout << "PConnector::open : error" << std::endl;
			return false;
		}
	}

	return true;
}

bool PConnector::open(const char *streamName, int streamId = 0, SSM_open_mode openMode = SSM_READ) {
	setStream(streamName, streamId);
	return open(openMode);
}

bool PConnector::create(double saveTime, double cycle) {
	if( !mDataSize ){
		std::cerr << "SSM::create() : data buffer of ''" << streamName << "', id = " << streamId << " is not allocked." << std::endl;
		return false;
	}
	return this->createRemoteSSM(streamName, streamId, mDataSize, saveTime, cycle);
}
bool PConnector::create(const char *streamName, int streamId, double saveTime, double cycle) {
	setStream(streamName, streamId);
	return create(saveTime, cycle);
}

bool PConnector::setProperty() {
	if (mPropertySize > 0) {
		return setPropertyRemoteSSM(streamName, streamId, mProperty, mPropertySize);
	}
	return false;
}

bool PConnector::setPropertyRemoteSSM(const char *name, int sensor_uid, const void *adata, size_t size) {
	ssm_msg msg;
	char *ptr;
	const char *data = ( char * )adata;
	if( strlen( name ) > SSM_SNAME_MAX ) {
		fprintf(stderr, "name length error\n");
		return 0;
	}

	/* メッセージをセット */
	strncpy( msg.name, name, SSM_SNAME_MAX );
	msg.suid = sensor_uid;
	msg.ssize = size;
	msg.hsize = 0;
	msg.time = 0;

	bool r = true;
	char *msg_buf = (char*)malloc(sizeof(ssm_msg));
	if (!sendMsgToServer(MC_STREAM_PROPERTY_SET, &msg)) {
		fprintf(stderr, "error in setPropertyRemoteSSM\n");
		r = false;
	}
	if (recvMsgFromServer(&msg, msg_buf)) {
		printf("msg %d\n", (int)msg.cmd_type);
		if (msg.cmd_type != MC_RES) {
			//r = false;
			return false;
		}

		/*
		for (int i = 0; i < 16; ++i) {
			printf("%02x ", data[i]);
		}
		printf("\n");
		*/
		if (!sendData(data, size)) {
			r = false;
		}
		if (recvMsgFromServer(&msg, msg_buf)) {
			printf("sendData ack msg %d\n", (int)msg.cmd_type);
			if (msg.cmd_type != MC_RES) {
				r = false;
			}
		}

	} else {
		fprintf(stderr, "fail recvMsg\n");
		r = false;
	}
	free(msg_buf);
	return r;
}

bool PConnector::getProperty() {
	if (mPropertySize > 0) {
		return getPropertyRemoteSSM(streamName, streamId, mProperty);
	}
	return false;
}

bool PConnector::getPropertyRemoteSSM(const char *name, int sensor_uid, const void *adata) {
	ssm_msg msg;
	int size;
	char *data = ( char * )adata;
	if( strlen( name ) > SSM_SNAME_MAX ) {
		fprintf(stderr, "name length error\n");
		return 0;
	}

	strncpy( msg.name, name, SSM_SNAME_MAX );
	msg.suid = sensor_uid;
	msg.ssize = mPropertySize;
	msg.hsize = 0;
	msg.time = 0;

	bool r = true;
	char *msg_buf = (char*)malloc(sizeof(ssm_msg));
	if (!sendMsgToServer(MC_STREAM_PROPERTY_GET, &msg)) {
		fprintf(stderr, "error in getPropertyRemoteSSM send\n");
		return false;
	}

	if (recvMsgFromServer(&msg, msg_buf)) {
		if (msg.cmd_type != MC_RES) {
			fprintf(stderr, "error in getPropertyRemoteSSM recv\n");
			return false;
		}
		// propertyのサイズ
		int size = msg.ssize;
		// property取得
		int len = recv(sock, data, size, 0);
		if (len != size) {
			fprintf(stderr, "fail recv property\n");
			r = false;
		}
	} else {
		fprintf(stderr, "fail recvMsg\n");
		r = false;
	}
	free(msg_buf);
	return r;
}

void PConnector::setOffset(ssmTimeT offset) {

	printf("offset = %f\n", offset);
	ssm_msg msg;
	msg.hsize = 0;
	msg.ssize = 0;
	msg.suid = 0;
	msg.time = offset;
	char *msg_buf = (char*)malloc(sizeof(ssm_msg));
	if (!sendMsgToServer(MC_OFFSET, &msg)) {
		fprintf(stderr, "error in setOffset\n");
	}
	if (recvMsgFromServer(&msg, msg_buf)) {
		printf("msg res offset %d\n", (int)msg.cmd_type);
	}
	free(msg_buf);
}

bool PConnector::createDataCon() {
	ssm_msg msg;
	msg.hsize = 0;
	msg.ssize = 0;
	msg.suid = 0;
	msg.time = 0;
	char *msg_buf = (char*)malloc(sizeof(ssm_msg));
	if (!sendMsgToServer(MC_CONNECTION, &msg)) {
		fprintf(stderr, "error in createDataCon\n");
	}
	if (recvMsgFromServer(&msg, msg_buf)) {
		printf("msg res offset %d\n", (int)msg.cmd_type);
		printf("msg res suid(port) %d\n", (int)msg.suid);
	}
	free(msg_buf);

	printf("connectToDataServer!\n");
	// Todo: change IPAddress
	connectToDataServer("127.0.0.1", msg.suid);

	return true;
}

bool PConnector::terminate() {
	ssm_msg msg;
	msg.hsize = 0;
	char *msg_buf = (char*)malloc(sizeof(ssm_msg));
	if (!sendMsgToServer(MC_TERMINATE, &msg)) {
		fprintf(stderr, "error in setOffset\n");
	}
	if (recvMsgFromServer(&msg, msg_buf)) {
		printf("msg terminate %d\n", (int)msg.cmd_type);
	}
	free(msg_buf);

	return true;
}
