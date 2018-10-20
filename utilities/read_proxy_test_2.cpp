/*
 * read_proxy_test_2.cpp
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

	con.initRemote();

	SSM_open_mode SSM_READ = SSM_READ;
	con.open(SSM_READ);

	if (!con.createDataCon()) {
		con.terminate();
		return 1;
	}

	while (!gShutOff) {
		if (con.readNew()) {
			printf("\n");
			printf("%%%%%%%%%%%%%%%%%%%%%\n");
			printf("read\n");
			printf("time -> %f", con.time);
			printf(" icnt -> %d, dcnt -> %f\n", con.data.inum, con.data.dnum);
			printf("%%%%%%%%%%%%%%%%%%%%%\n");
		}
		sleepSSM(1);
	}
	con.terminate();
}

