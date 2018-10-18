/*
 * write_proxy_sample_2.cpp
 *
 *  Created on: 2018/10/18
 *      Author: gunjiryouta
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
#include "Dstructure.h"
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

unsigned int sleepSSM(double sec) {
	return usleepSSM(sec * 1000000.0);
}

int usleepSSM(useconds_t usec) {
	double t,speed = 1.0;

	t = (double)usec / speed;
	return usleep((int)t);
}

int main() {
	PConnectorClient<Dstructure> con(SNAME_DS, 2);

	SSM_open_mode SSM_WRITE = SSM_WRITE;

	if (!con.create(5.0,1.0)) {
		con.terminate();
		return 1;
	}

	if (!con.createDataCon()) {
		con.terminate();
		return 1;
	}

	int icnt = 0;
	double dcnt = 0.0;

	// intは1秒に1回，doubleは1秒に0.1ずつカウント
	while (!gShutOff) {
		icnt += 1;
		dcnt += 0.1;

		con.wdata->inum = icnt;
		con.wdata->dnum = dcnt;
		con.write();
		sleepSSM(1);
	}

	con.terminate();
}
