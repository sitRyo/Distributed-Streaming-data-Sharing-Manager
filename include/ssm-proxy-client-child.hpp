// テンプレートを利用した実装はヘッダに書くこと(リンカに怒られる)
/*
 * SSMの宣言に仕様する
 * ほとんどSSMApiと同じように使用できる
 */

#ifndef _SSM_PROXY_CLIENT_CHILD_
#define _SSM_PROXY_CLIENT_CHILD_

#include "ssm-proxy-client.hpp"

template < typename T, typename P = DSSMDummy >
class PConnectorClient : public PConnector {
private:
	void initApi(char *ipAddr) {
		fulldata = malloc(sizeof(T) + sizeof(ssmTimeT)); // メモリの開放はどうする？ -> とりあえずデストラクタで対応
		wdata = (T *)&(((char *)fulldata)[8]);
		PConnector::setBuffer(&data, sizeof(T), &property, sizeof(P), fulldata);
		PConnector::setIpAddress(ipAddr);
	}

	void initApi() {
		fulldata = malloc(sizeof(T) + sizeof(ssmTimeT)); // メモリの開放はどうする？ -> とりあえずデストラクタで対応
		wdata = (T *)&(((char *)fulldata)[8]);
		PConnector::setBuffer(&data, sizeof(T), &property, sizeof(P), fulldata);
	}

protected:
	void setBuffer(void *data, uint64_t dataSize, void *property, uint64_t propertySize, void *fulldata);

public:
	T data;
	T* wdata;
	P property;
	void* fulldata;

	PConnectorClient() {
		initApi();
	}
	// 委譲
	PConnectorClient(const char *streamName, int streamId = 0, char* ipAddr = "127.0.0.1") :  PConnector::PConnector(streamName, streamId) {
		initApi(ipAddr);
	}

	~PConnectorClient() {
		//std::cout << __PRETTY_FUNCTION__ << std::endl;
		free(fulldata);
		free(wdata);
	}
};

#endif
