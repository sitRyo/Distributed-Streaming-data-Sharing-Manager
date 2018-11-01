
#ifndef _SSM_PROXY_CLIENT_H_
#define _SSM_PROXY_CLIENT_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "libssm.h"

/**
 * Dummy Class
 */
class DSSMDummy
{
};

class PConnector {
private:
	struct sockaddr_in server;
	struct sockaddr_in dserver;
	int sock;
	int dsock;                              // データ送信用
	PROXY_open_mode openMode;

	const char *streamName;
	int streamId;
	void *mData;								///< データのポインタ
	size_t mDataSize;						///< データ構造体のサイズ
	void *mProperty;							///< プロパティのポインタ
	size_t mPropertySize;					///< プロパティサイズ
	void *mFullData;
	size_t mFullDataSize;
	ssmtime *timecontrol;					///< for get real time
	char *ipaddr;

	void writeInt(char **p, int v);
	void writeLong(char **p, long v);
	void writeDouble(char **p, double v);
	void writeRawData(char **p, char *d, int len);

	int  readInt(char **p);
	long readLong(char **p);
	double readDouble(char **p);
	void readRawData(char **p, char *d, int len);

	void serializeMessage(ssm_msg *msg, char *buf);
	bool createRemoteSSM( const char *name, int stream_id, size_t ssm_size, ssmTimeT life, ssmTimeT cycle );
	bool setPropertyRemoteSSM(const char *name, int sensor_uid, const void *adata, size_t size);
	bool getPropertyRemoteSSM(const char *name, int sensor_uid, const void *adata);

	bool sendData(const char *data, size_t size);

public:
	SSM_tid timeId; // データのTimeID (SSM_tid == int)
	ssmTimeT time = 0;  // データのタイムスタンプ (ssmTimeT == double)


	PConnector();
	~PConnector();
	PConnector(const char *streamName, int streamId = 0);

	void initPConnector();

	bool connectToServer(const char* serverName, int port);
	bool sendMsgToServer(int cmd_type, ssm_msg *msg);
	bool recvMsgFromServer(ssm_msg *msg, char *buf);

	bool connectToDataServer(const char* serverName, int port);

	bool isOpen();
	void* getData();

	bool initRemote();
	bool initSSM();
	void setStream(const char *streamName, int streamId);
	void setBuffer(void *data, size_t dataSize, void *property, size_t propertySize, void *fulldata);
	void setIpAddress(char *address);
	bool create(const char *streamName, int streamId, double saveTime, double cycle);
	bool create(double saveTime, double cycle);
	bool open(SSM_open_mode openMode = SSM_READ);
	bool open(const char *streamNane, int streamId, SSM_open_mode openMode);
	bool setProperty();
	bool getProperty();
	void setOffset(ssmTimeT offset);
	bool createDataCon();
	bool terminate();

	double timettof( struct timespec t ); // 使わないかも
	static ssmTimeT getRealTime(); // 現在時刻の取得

	bool write( ssmTimeT time = gettimeSSM()); // write bulkdata with time

	bool read( SSM_tid tmid = -1, READ_packet_type type = TIME_ID ); // read
	bool readNew(); // 最新であり、前回読み込んだデータと違うデータのときに読み込む
	bool readNext(int dt); // 前回読み込んだデータの次のデータを読み込む dt -> 移動量
	bool readBack(int dt); // 前回のデータの1つ(以上)前のデータを読み込む dt -> 移動量
	bool readLast(); // 最新データの読み込み
	bool readTime(ssmTimeT t); // 時間指定, packet control

	bool recvData(); // read data recv
};

#endif
