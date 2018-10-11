#include <stdio.h>
#include <unistd.h>

#include <ssm.h>
#include "SensorA.h"
#include "SensorB.h"

int main(int argc, char *argv[]) {
	SensorA sens_dataA;
	SensorB sens_dataB;
	double measured_time;
	SSM_sid sensA_sid, sensB_sid;
	int tid;
	double time;

	initSSM();

	sensA_sid = openSSM("sensor_A", 0, 0);
	sensB_sid = openSSM("sensor_B", 0, 0);

	while (1) {
		//センサAの最新データを取得
		tid = readSSM(sensA_sid, (char*) &sens_dataA, &measured_time, -1);
		printf("hoge\n");
		//同時刻のデータ（もっとも近く、古いもの）を取得
		readSSM_time(sensB_sid, (char*) &sens_dataB, measured_time, &time);
		printf("timeA=%f timeB=%f A.a=%f B.a=%f\n", measured_time, time,
				sens_dataA.a, sens_dataB.a);
		sleep(1);
	}
}

