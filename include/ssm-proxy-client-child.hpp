/*
 * ssm-proxy-client-child.hpp
 *
 *  Created on: 2018/10/17
 *      Author: gunjiryouta
 */

// テンプレートを利用した実装はヘッダに書くこと(リンカに怒られる)

#ifndef _SSM_PROXY_CLIENT_CHILD_
#define _SSM_PROXY_CLIENT_CHILD_

#include "ssm-proxy-client.hpp"

template < typename T, typename P = DSSMDummy >
class PConnectorClient : public PConnector {
private:
	void initApi() {
		fulldata = malloc(sizeof(T) + sizeof(ssmTimeT)); // メモリの開放はどうする？ -> とりあえずデストラクタで対応
		wdata = (T *)&(((char *)fulldata)[8]);
		PConnector::setBuffer(&data, sizeof(T), &property, sizeof(P), fulldata);
	}

protected:
	void setBuffer(void *data, size_t dataSize, void *property, size_t propertySize, void *fulldata);

public:
	T data;
	T* wdata;
	P property;
	void* fulldata;

	PConnectorClient() {
		initApi();
	}
	// 委譲
	PConnectorClient(const char *streamName, int streamId = 0) :  PConnector::PConnector(streamName, streamId) {
		initApi();
	}

	~PConnectorClient() {
		//std::cout << __PRETTY_FUNCTION__ << std::endl;
		free(fulldata); // いやな予感がする
	}
};

#endif
