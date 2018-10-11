#include <stdio.h>
#include <unistd.h>

#include <ssm.h>
#include "SensorA.h"

SensorA sens_data;

void receive_sensorA(SensorA *data, double *time) {
	static double a, b;
	static int c;
	a += 0.1;
	b += 0.01;
	c++;
	data->a = a;
	data->b = b;
	data->c = c;
	*time = gettimeSSM();
	usleep(100000);
}

int main(int argc, char *argv[]) {

	double measured_time;
	SSM_sid sensA_sid;
	SensorA_Property sens_prop;

	initSSM();
	//名前をsensor_Aで、IDを0で、0.1秒おきに来るデータを10秒間保持するよう領域を確保。
	sensA_sid = createSSM("sensor_A", 0, sizeof(SensorA), 10, 0.1);

	/*propertyの設定*/
	sens_prop.a = 100;
	sprintf(sens_prop.name, "sensorA");
	set_propertySSM("sensor_A", 0, (char*) &sens_prop, sizeof(SensorA_Property));

	while (1) {
		usleep(100);

		//SensorAのデータを何かしらの方法（例えばシリアル通信）で取得し、頑張って観測時刻を推定する関数。
		receive_sensorA(&sens_data, &measured_time);
		//SSMへのデータの書き込み。
		printf("%d\n", writeSSM(sensA_sid, (char*) &sens_data, measured_time));
	}
}
