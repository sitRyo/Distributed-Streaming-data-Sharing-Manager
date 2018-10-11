/*
 * read_proxy_test.cpp
 *
 *  Created on: 2018/09/19
 *      Author: gunjiryouta
 */

/* MC_OPENテスト
 * ssm-proxy-clientからMC_OPENを発行し、proxyからの応答があることを確認する。
 */

// c++系
#include <iostream>
#include <iomanip>
// c系
#include <unistd.h>
#include <signal.h>
// その他
#include <ssm.hpp>
// データ構造
#include "intSsm.h"
// クライアント側
#include "ssm-proxy-client.hpp"
// おまじない
using namespace std;
// 終了するかどうかの判定用変数
bool gShutOff = false;
// シグナルハンドラー
// この関数を直接呼び出すことはない
void ctrlC(int aStatus)
{
	signal(SIGINT, NULL);
	gShutOff = true;
}
// Ctrl-C による正常終了を設定
inline void setSigInt(){ signal(SIGINT, ctrlC); }

unsigned int sleepSSM(double sec) {
	return usleepSSM(sec * 1000000.0);
}

int usleepSSM(useconds_t usec) {
	double t,speed = 1.0;

	t = (double)usec / speed;
	return usleep((int)t);
}

int main() {
	// サーバとのコネクタを設定
	PConnector *con = new PConnector();

	// サーバと接続、サーバ側はプロセスをフォーク
	// MC_INITIALIZEを発行
	con->initRemote();

	// openmode, streamName, sidを設定
	SSM_open_mode SSM_READ = SSM_READ;
	char *streamName = SNAME_INT;
	int sid = 1;

	// データ構造体のサイズ, プロパティサイズ
	size_t dataSize = sizeof(intSsm_k);
	size_t propertySize = sizeof(SSMDummy);
	void* data = malloc(dataSize + sizeof(ssmTimeT));
	void* property = malloc(propertySize);
	void* fulldata = malloc(dataSize + sizeof(ssmTimeT));

	// コネクタのバッファ設定
	con->setBuffer(data, dataSize, property, propertySize, fulldata);

	// open!
	con->open(streamName, sid, SSM_READ);

	// データ通信路を開く
	if (!con->createDataCon()) {
		// endSSM() -> MC_TERMINATE
		con->terminate();
		return 1;
	}

	while (!gShutOff) {
		// 最新のデータを取得
		if (con->readNew()) {
			printf("\n");
			printf("now -> %f\n", con->time);
			cout << "NUM = " << ((intSsm_k *)data)->num << endl;
			printf("timestamp -> %f\n", con->time);
			printf("timeid -> %d\n", con->timeId);
		}

		// 1秒前のデータを取得
		if (con->readTime(con->time - 1)) {
			printf("\n");
			printf("before 1 sec -> %f\n", con->time);
			cout << "old NUM = " << ((intSsm_k *)data)->num << endl;
			printf("timestamp -> %f\n", con->time);
			printf("timeid -> %d\n", con->timeId);
		}

		sleepSSM(1);
	}

	con->terminate();

	free(data);
	free(property);
	free(fulldata);
}



