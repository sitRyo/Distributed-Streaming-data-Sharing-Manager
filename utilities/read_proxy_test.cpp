/*
 *	ネットワークを介したread機能のテスト
 *	1秒に1回書き込まれるデータを取ってきて表示する．
 */


// c++系
#include <iostream>
#include <iomanip>
// c系
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
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

int main() {
	/*
	 * 変数の宣言
	 * PConnectorClient<data型, property型> 変数名(ssm登録名, ssm登録番号, 接続するproxy(server)のipAddress);
	 * property型は省略可能、省略するとpropertyにアクセスできなきなくなるだけ
	 * ssm登録番号は省略可能、省略すると0に設定される
	 * ssm登録名は./intSsm.hに#define SNAME_INT "intSsm"と定義
	 * data型とproperty型は ./intSsm.h に定義
	 * 指定しているIPはループバックアドレス(自分自身)
	 */
	PConnectorClient<intSsm_k, doubleProperty_p> con(SNAME_INT, 1);

	// ssm関連の初期化
	con.initSSM();

	// 共有メモリにすでにある領域を開く
	// 失敗するとfalseを返す
	if (!con.open(SSM_READ)) {
		// terminate()でssm-coordinatorとの接続を切断する
		con.terminate();
		return 1;
	}

	// 指定したプロパティを取得
	con.getProperty();

	// プロパティは 変数名.property.データ でアクセス
	printf("property -> %f\n", con.property.dnum);

	// データ通信路を開く
	// これをしないとデータを取得できない
	if (!con.createDataCon()) {
		con.terminate();
		return 1;
	}

	// 安全に終了できるように設定
	setSigInt();
	double ttime;
	while (!gShutOff) {

		// 最新のデータを取得
		if (con.readNew()) {
			printf("\n");
			printf("now -> %f\n", con.time);
			cout << "NUM = " << con.data.num << endl;
		}

		// 1秒前のデータを取得
		if (con.readTime(con.time - 1)) {
			printf("\n");
			printf("before 1 sec -> %f\n", con.time);
			cout << "old NUM = " << con.data.num << endl;
		}

		sleepSSM(1);
	}

	// プログラム終了後は切断
	con.terminate();
}



