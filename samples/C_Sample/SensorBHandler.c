#include <stdio.h>
#include <unistd.h>

#include <ssm.h>
#include "SensorB.h"

void receive_sensorB(SensorB *data, double *time) {
	static double a;
	static int b;
	a += 0.1;
	b++;
	data->a = a;
	data->b = b;
	*time = gettimeSSM();
	usleep(50000);
}

int main(int argc, char *argv[]) {
	SensorB sens_data;
	double measured_time;
	SSM_sid sensB_sid;

	initSSM();
	//名前をsensor_Bで、IDを0で、0.05秒おきに来るデータを5秒間保持するよう領域を確保。
	sensB_sid = createSSM_time("sensor_B", 0, sizeof(SensorB), 5, 0.05);

	while (1) {
		//SensorBのデータを何かしらの方法（例えばシリアル通信）で取得し、頑張って観測時刻を推定する関数。
		receive_sensorB(&sens_data, &measured_time);
		//SSMへのデータの書き込み。
		writeSSM(sensB_sid, (char*) &sens_data, measured_time);
	}
}
