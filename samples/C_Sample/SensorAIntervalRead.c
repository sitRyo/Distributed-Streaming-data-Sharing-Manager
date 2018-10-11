#include <stdio.h>
#include <unistd.h>

#include <ssm.h>
#include "SensorA.h"

int main(int argc, char *argv[]) {
	SensorA sens_data; /*sensor_Aのデータ取得用*/
	SSM_sid sensA_sid; /*sensor_Aのアクセス用*/
	double measured_time; /*sensor_Aの計測時刻取得用*/
	SensorA_Property sens_prop; /*sensor_Aのプロパティ取得用*/

	/*初期化*/
	initSSM();

	/*sensor_Aのオープン*/
	sensA_sid = openSSM("sensor_A", 0, 0);

	/*プロパティの取得*/
	get_propertySSM("sensor_A", 0, (char*) &sens_prop);
	printf("[%s %f]\n", sens_prop.name, sens_prop.a);

	/*1秒おきにデータを取得*/
	while (1) {
		/*最新のデータの読み込み*/
		readSSM(sensA_sid, (char*) &sens_data, &measured_time, -1);

		printf("time=%f a=%f b=%f c=%d\n", measured_time, sens_data.a,
				sens_data.b, sens_data.c);

		sleep(1);
	}
}

