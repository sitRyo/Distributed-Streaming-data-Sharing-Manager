//============================================================================
// Name        : ssmWriteSample.cpp
// Author      : NKB,STK
// Version     : 0.0.0
// Compile     : g++ -o odometry-viewer-ssm odometry-viewer-ssm.cpp -Wall -O3 -lssm
// 一秒に一回カウントアップして，ssmに書きこむだけの簡単アルゴリズム
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
	// ここでは同じディレクトリに入れてあるが、オドメトリなど標準的な変数は/usr/local/include/ssmtype/内で定義されている
	// c++に慣れていないと馴染みがない書き方だけれど，ここはパターンで使えれば充分
	SSMApi<intSsm_k> intSsm(SNAME_INT, 0);
	SSMApi<intSsm_k> intSsm2(SNAME_INT, 1);

	// ssm関連の初期化
	if(!initSSM())return 0;
	
	// 共有メモリにSSMで領域を確保
	// create 失敗するとfalseを返す
	//intSsm.create( センサデータ保持時間(sec), おおよそのデータ更新周期(sec) )
	if( !intSsm.create( 5.0, 1.0 ) ){return 1;}
	if( !intSsm2.create( 5.0, 1.0 ) ){return 1;}

	// 安全に終了できるように設定
	setSigInt();

	//---------------------------------------------------
	//ループ処理の開始
	cerr << "start." << endl;
	
	// 書きこむ変数
	int cnt = 0;
	int cnt2 = 100;
	
	while(!gShutOff)
	{
		//データには構造体と同様にアクセスする
		//intSsm.dataの中に変数が入っていると考えれば良い
		intSsm.data.num = cnt;
		intSsm2.data.num = cnt2;
		
		cnt++;
		cnt2+=2;
		
		//データの書き込み、現在時刻をタイムスタンプに
		// write(ssmTimeT time) で時間指定書き込み。時間を省略すると現在時刻を書き込む．普通に使う分には何もなしでok
		intSsm.write();	
		intSsm2.write();
		
		//ssmを使用するプログラムでは，sleepSSM(), usleepSSM() を使用することを推奨
		//ログを再生するときに時間が飛ばないので便利．
		sleepSSM(1);
		
	}
	//---------------------------------------------------
	//終了処理
	// create したら必ず最後にrelease
	intSsm.release(  );
	// SSMからの切断
	endSSM();
	cerr << "end." << endl;
	return 0;
}
