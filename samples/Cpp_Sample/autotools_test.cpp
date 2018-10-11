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

	// コネクタのストリームを設定
	con->setStream(streamName, sid);

	// proxyと通信
	con->open(SSM_READ);
}
