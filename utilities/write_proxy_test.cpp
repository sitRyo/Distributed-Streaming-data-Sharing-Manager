/*
 * write_proxy_test.cpp
 *
 *  Created on: 2018/09/22
 *      Author: gunjiryouta
 */

/*
 * writeのテスト. intSsmを使って, 1秒に1回カウントアップ変数を書き込む.
 */

// c++系
#include <iostream>
#include <iomanip>
// c系
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
// その他
#include <ssm.hpp>
// データ構造
#include "intSsm.h"
// クライアント側
#include "ssm-proxy-client-child.hpp"
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

/*
unsigned int sleepSSM(double sec) {
	return usleepSSM(sec * 1000000.0);
}

int usleepSSM(useconds_t usec) {
	double t,speed = 1.0;

	t = (double)usec / speed;
	return usleep((int)t);
}
*/

int main(int aArgc, char **aArgv) {
	// サーバとのコネクタを設定
	// PConnector *con = new PConnector();
	// doubleProperty_p -> double 1個
	PConnectorClient<intSsm_k, doubleProperty_p> con(SNAME_INT, 1);
	// initSSM
	con.initSSM();

	// openmodeを設定
	SSM_open_mode SSM_WRITE = SSM_WRITE;

	// stream creates!
	if (!con.create(5.0, 1.0)) {
		con.terminate();
		return 1;
	}

	con.property.dnum = 1.5;
	con.setProperty();

	// データの送受信路を開く
	if (!con.createDataCon()) {
		con.terminate();
		return 1;
	}
	// 書き込む変数
	int cnt = 0;

	// 1秒に1回インクリメント
	while (!gShutOff) {
		cnt += 1;
		//((intSsm_k*)(&data[8]))->num = cnt;
		con.wdata->num = cnt;
		printf("write %d\n", cnt);
		con.write();
		sleepSSM(1);
	}

	con.terminate();

	/*
	// メモリの解放を忘れずに
	free(data);
	free(rcvbuf);
	free(property);
	*/
}
