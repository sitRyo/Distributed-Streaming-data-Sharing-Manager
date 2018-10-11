#include <stdio.h>
#include <unistd.h>

#include <ssm.h>
#include "SensorA.h"

int main(int argc, char *argv[]) {
	SensorA sens_data;
	SSM_sid sensA_sid;
	double measured_time;
	int tid;
	double now_time;

	initSSM();
	sensA_sid = openSSM("sensor_A", 0, 0);

	while (1) {
		now_time = gettimeSSM(); //現在時刻を取得

		//現在時刻の1秒前のデータを取得
		tid = readSSM_time(sensA_sid, (char*) &sens_data, now_time - 1,
				&measured_time);
		printf("nowtime=%f\n   time=%f tid=%d a=%f b=%f c=%d\n", gettimeSSM()
				- 1, measured_time, tid, sens_data.a, sens_data.b, sens_data.c);
		//その一つ後のデータを取得
		//tid++;
		tid = readSSM(sensA_sid, (char*) &sens_data, &measured_time, tid + 1);
		printf("   time=%f tid=%d a=%f b=%f c=%d\n\n", measured_time, tid,
				sens_data.a, sens_data.b, sens_data.c);

		sleep(1);
	}
}

