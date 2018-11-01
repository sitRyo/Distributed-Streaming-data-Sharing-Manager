

#ifndef _SSM_PROXY_H_
#define _SSM_PROXY_H_

#include "ssm.hpp"

void test();

#define SERVER_PORT     8080            /* サーバ用PORT */
#define SERVER_IP       0x00000000UL    /* サーバ用待ち受けIP */
#define BUFFER_SIZE     1024            /* バッファバイト数 */


/* クライアントからの接続を待つサーバの情報を表現する構造体 */
typedef struct {
    int                 wait_socket;    /* サーバ待ち受け用ソケット */
    struct sockaddr_in  server_addr;    /* サーバ待ち受け用アドレス */
} TCPSERVER_INFO;

/* クライアントとの接続に関する情報を保存する構造体 */
typedef struct {
    int                 data_socket;    /* クライアントとの通信用ソケット */
    struct sockaddr_in  client_addr;    /* クライアントのアドレス */
} TCPCLIENT_INFO;


#include "Thread.hpp"

class DataCommunicator : public Thread {
private:
	TCPSERVER_INFO server;
	TCPCLIENT_INFO client;

	char* mData;
	size_t mDataSize;
	size_t ssmTimeSize;
	size_t mFullDataSize;
	PROXY_open_mode mType;

	SSMApiBase *pstream;

	bool sopen();
	bool rwait();
	bool sclose();
public:
	DataCommunicator() = delete;
	DataCommunicator(uint16_t nport, char* mData, size_t d_size, size_t t_size, SSMApiBase *pstream, PROXY_open_mode type);
	~DataCommunicator();
	void* run(void *args);

	void handleData();
	void readData();

	bool receiveData();
	READ_packet_type receiveTimeIdData(double* buf);

	bool sendToReadData();

	SSM_open_mode switchMode();
};


class ProxyServer {
private:
	TCPSERVER_INFO server;
	TCPCLIENT_INFO client;
	uint16_t nport;     // センサデータ受信用のポート番号.子プロセスが生成されるたびにインクリメントしていく

	char* mData;        // データ用
	size_t mDataSize;   // データサイズ
	size_t ssmTimeSize; // ssmTimeTのサイズ
	size_t mFullDataSize;  // mDataSize + ssmTimeSize
	char *mProperty;
	size_t mPropertySize;
	PROXY_open_mode mType;

	DataCommunicator *com;

	SSMApiBase stream; // real stream

	bool open();
	bool wait();

	void serializeMessage(ssm_msg *msg, char *buf);

	int  readInt(char **p);
	uint64_t readLong(char **p);
	double readDouble(char **p);
	void readRawData(char **p, char *d, int len);

	void writeInt(char **p, int v);
	void writeLong(char **p, uint64_t v);
	void writeDouble(char **p, double v);
	void writeRawData(char **p, char *d, int len);

	void setSSMType(PROXY_open_mode mode);

	int receiveMsg(ssm_msg *msg, char *buf);
	int sendMsg(int cmd_type, ssm_msg *msg);

	bool receiveData();
	void handleData();

	void setupSigHandler();
	static void catchSignal(int signo);

public:
	ProxyServer();
	~ProxyServer();
	bool init();
	bool run();
	bool server_close();
	bool client_close();
	void handleCommand();




};

#endif
