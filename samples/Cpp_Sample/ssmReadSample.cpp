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
	// 変数の宣言
	// SSMApi<data型, property型> 変数名(ssm登録名, ssm登録番号);
	// property型は省略可能、省略するとpropertyにアクセスできなきなくなるだけ
	// ssm登録番号は省略可能、省略すると0に設定される
	// ssm登録名は./intSsm.hに#define SNAME_INT "intSsm"と定義
	// ここでは同じディレクトリに入れてあるが、オドメトリなど標準的な変数は/user/local/include/ssmtype/内で定義されている
	// c++に慣れていないと馴染みがない書き方だけれど，ここはパターンで使えれば充分
	// readとwriteで同じ宣言ができていればOK
	SSMApi<intSsm_k> intSsm(SNAME_INT, 1);

	// ssm関連の初期化
	if(!initSSM()) { // @suppress("Invalid arguments")
		return 0;
	}
	
	SSM_open_mode SSM_READ = SSM_READ;
	// 共有メモリにすでにある領域を開く
	// open 失敗するとfalseを返す
	if(!(intSsm.open( SSM_READ ))){ endSSM(); return 1; } // @suppress("Invalid arguments")


	// 安全に終了できるように設定
	setSigInt();
	//---------------------------------------------------
	//ループ処理の開始
	cerr << "start." << endl;
	while(!gShutOff)
	{
		// reat(tid)  tid指定読み込み
		// readNew()  最も新しいデータを読み込む。ただし前回から更新されていなければfalseを返す
		// readLast() 最も新しいデータを読み込む。更新されていなくてもtrueを返す
		// readNext() 前回読み込んだデータの次のデータを読み込む。次のデータがなければfalse
		// readBack() 前回読み込んだデータの前のデータを読み込む。前のデータがなければfalse
		// readTime(ssmTimeT time) 時間を指定して読み込み
		
		// 最も新しいデータが更新されていたら
		if(intSsm.readNew())
		{
			// 出力
			cout << "NUM = " << intSsm.data.num << endl;
		}
		
		//1秒前のデータを読み込みたい場合
		
		if(intSsm.readTime(intSsm.time - 1 ))
		{
			// 出力
			cout << "old NUM = " << intSsm.data.num << endl;
		}
		
		sleepSSM( 1 ); 				// SSM時間にあわせたsleep。playerなどで倍速再生している場合にsleep時間が変化する。
	}
	//---------------------------------------------------
	//終了処理
	// openしたら、必ず最後にclose
	intSsm.close(  );
	// SSMからの切断
	endSSM();
	cerr << "end." << endl;
	return 0;
}
