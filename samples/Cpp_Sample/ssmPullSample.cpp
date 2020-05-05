//============================================================================
// Name        : ssmReadSample.cpp
// Author      : NKB, STK
// Version     : 0.0.0
// Compile     : g++ -o ssmReadSample ssmReadSample.cpp -Wall -O3 -lssm
// ssmWriteSampleで書き込んだデータを，ssmから読み出すだけの簡単アルゴリズム
//============================================================================
// c++系
#include <iostream>
#include <iomanip>
// c系
#include <unistd.h>
#include <signal.h>
// その他
#include <ssm.hpp>
#include "intSsm.h"
#include "SSMpull.hpp"

// クラス名はメンバと同じようにならないように調整
struct intSSM_k {
	intSsm_k data;
	SSMDummy property;
};

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

// メイン
int main(int aArgc, char **aArgv)
{

	if(!initSSM()) {
		return 0;
	}
	
	SSMApiInfo ssmApiInfo;
	void* buf = new char(sizeof(intSSM_k));
	ssmApiInfo.add(SNAME_INT, buf);
	ssmApiInfo.assign<intSSM_k>(SNAME_INT);
	
	auto apiInfo = ssmApiInfo.getSSMApi(SNAME_INT);
	apiInfo.open(SSM_READ);

	setSigInt();
	cerr << "start." << endl;

	intSSM_k* intSsm_k = reinterpret_cast<intSSM_k*>(apiInfo.storage);

	while(!gShutOff)
	{
		if(apiInfo.ssmApiBase->readNew())
		{
			// 出力
			cout << "NUM = " << intSsm_k->data.num << endl;
		}
		
		//1秒前のデータを読み込みたい場合
		
		if(apiInfo.ssmApiBase->readTime(apiInfo.ssmApiBase->time - 1 ))
		{
			// 出力
			cout << "old NUM = " << intSsm_k->data.num << endl;
		}
		
		sleepSSM( 1 ); 				// SSM時間にあわせたsleep。playerなどで倍速再生している場合にsleep時間が変化する。
	}
	//---------------------------------------------------
	//終了処理
	// openしたら、必ず最後にclose
	apiInfo.ssmApiBase->close(  );
	// SSMからの切断
	endSSM();
	cerr << "end." << endl;
	return 0;
}
