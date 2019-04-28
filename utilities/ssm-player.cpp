// g++ -o player ssm-player.cpp -I../include -lssm -Wall
#include <iostream>
#include <iomanip>
#include <vector>
#include <list>
#include <string>
#include <stdexcept>

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <cmath>

#include <fcntl.h>
#include <getopt.h>

#define __MAIN__

#include "ssm.hpp"
#include "libssm.h"
#include "ssm-log.hpp"
#include "printlog.hpp"

#include "ssm-proxy-client.hpp"

using namespace std;

bool gShutOff = false;

int write_count = 0;

void ctrlC( int aStatus )
{
	// exit(aStatus); // デストラクタが呼ばれないのでダメ
	gShutOff = true;
	signal( SIGINT, NULL );
}

void setSigInt(  )
{
	signal(SIGINT, ctrlC);
}

int getUnit( double *data, char *unit )
{
	int i = 0;
	static const char table[] = { " kMGTPEZY" };
	static const double base = 1024;

	while( *data >= base && i < 8 )
	{
		*data /= base;
		i++;
	}
	if( i < 0 || i > 8 )
		i = 0;
	*unit = table[i];

	if( i > 0 )
		return 1;
	return 0;
}


class LogPlayer
{
	const char *logName;
	void *data;
	void *property;
	uint64_t propertySize;
	uint64_t dataSize;
	SSMApiBase stream;
	SSMLogBase log;
	void *fulldata;
	
	/* added for network */
	bool useNetwork;
	PConnector *con;

	int readCnt;// 読み込んだ回数
	int writeCnt; // 書きこんだ回数

	bool mIsPlaying;
	void init(  )
	{
		data = NULL;
		property = NULL;
		fulldata = NULL;
		dataSize = 0;
		propertySize = 0;
		readCnt = 0;
		writeCnt = 0;
		mIsPlaying = false;
		useNetwork = false;
		con = NULL;
	}

public:
	// 再生中かどうか
	bool isPlaying(  )
	{
		return mIsPlaying;
	}
	
	void setLog( const char *logName )
	{
		this->logName = logName;
	}
	
	const char *getLogName() const
	{
		return logName;
	}

	LogPlayer(  )
	{
		init(  );
	}
	
	LogPlayer( const char *logName )
	{
		init(  );
		setLog( logName );
	}
	
	~LogPlayer(  )
	{
		free( data );
		free( property );
		free(fulldata);
		data = NULL;
		property = NULL;
		fulldata = NULL;

		delete con;
		con = NULL;
	}
	
	// openしたら必ずcloseすること！
	bool open(  )
	{
		logVerbose << "open : " << logName << endl; 
		if( !log.open( logName ) )
			return false;
		dataSize = log.dataSize(  );
		propertySize = log.propertySize(  );
		data = malloc( dataSize );
		property = malloc( propertySize );
		fulldata = malloc(dataSize + sizeof(ssmTimeT));
		char *p = &((char*)fulldata)[8];
		
                /*
		cout << "\nprint memory (ssm-player)" << endl;
		printf("data: %p\n", data);
		printf("full: %p\n", fulldata);
		printf("pppp: %p\n", p);
		cout << "print end" << endl;
                */

		if( ( data == NULL ) || (fulldata == NULL) || ( propertySize && property == NULL ))
		{
			log.close();
			free( data );
			free( property );
			free(fulldata);
			data = property = fulldata = NULL;
			return false;
		}
		log.setBuffer( data, dataSize, property, propertySize, fulldata ); // fulldata is for network
		stream.setBuffer( data, dataSize, property, propertySize );

		/* For Network */
		if (useNetwork) {
			con->setBuffer(data, dataSize, property, propertySize, fulldata);
		}
		
		log.readProperty(  );		

		ssmTimeT saveTime;
		saveTime = calcSSM_life( log.getBufferNum(  ), log.getCycle(  ) );
		logVerbose << "> "<< log.getStreamName() << ", " << log.getStreamId() << ", " << saveTime << ", " << log.getCycle() << endl;

		/* For Network */
		if (useNetwork) {
			if (!con->create(log.getStreamName(), log.getStreamId(), saveTime, log.getCycle())) {
				return false;
			}

			if (propertySize && !con->setProperty()) {
				fprintf(stderr, "error in setProperty\n");
				return false;
			}
		} else {
			if( !stream.create( log.getStreamName(), log.getStreamId(), saveTime, log.getCycle() ) )
				return false;

			if( propertySize && !stream.setProperty() )
						return false;
		}

		/* open new socket for bulk communication*/
 		if (useNetwork) {
 			if(!con->createDataCon()) {
 				fprintf(stderr, "error in creteDataCon\n");
 				return false;
 			}
 		}
		return true;
	}
	
	// デストラクタへの登録禁止
	void close(  )
	{
		stream.release(  );
		log.close(  );
	}
	bool seek( ssmTimeT time )
	{
		// logからの読み込み
		if( !log.readTime( time ) )
			return false;
		// log時間でのstreamへの書き込み
		if( !stream.write( log.time() ) )
			return false;
		writeCnt = ++readCnt;
		return true;
	}
	
	bool play( ssmTimeT time = gettimeSSM(  ) )
	{
		if( readCnt == writeCnt )
		{
			if (useNetwork) {
				if ((mIsPlaying = log.readFull()))
					readCnt++;
			} else {
				if( ( mIsPlaying = log.readNext(  ) )  )
					readCnt++;
			}
		}

		if( log.time(  ) <= time && writeCnt < readCnt )
		{
			write_count++; // for test
			// log時間でのstreamへの書き込み
			if (useNetwork) {
				con->write(log.time());

			} else {
				stream.write( log.time(  ) );
			}
			writeCnt = readCnt;
			return true;
		}
		return false;
	}
	
	bool playBack( ssmTimeT time = gettimeSSM(  ) )
	{
		if( readCnt == writeCnt )
		{
			if( ( mIsPlaying = log.readBack(  ) )  )
				readCnt++;
		}

		if( log.time(  ) >= time && writeCnt < readCnt )
		{
			// log時間でのstreamへの書き込み
			stream.write( log.time(  ) );
			writeCnt = readCnt;
			return true;
		}
		
		return false;
	}
	
	const ssmTimeT &getStartTime() const
	{
		return log.getStartTime();
	}

	void setUseNetwork(bool useNetwork) {
		this->useNetwork = useNetwork;
		con = new PConnector();
	}

	bool initRemote() {
		if (useNetwork) {
			return con->initRemote();
		}
		return false;
	}

	void setOffset(ssmTimeT offset) {
		if (useNetwork) {
			con->setOffset(offset);
		}
	}

	void showRawData() {
		stream.showRawData();
	}
};

typedef std::list<LogPlayer> LogPlayerArray;


class MyParam
{
	void init()
	{
		startTime = -1;
		endTime = -1;
		startOffsetTime = 0.0;
		endOffsetTime = 0.0;
		speed = 1.0;
		isLoop = false;
		useNetwork = false;
	}
public:
	
	LogPlayerArray logArray;
	ssmTimeT startTime, endTime, startOffsetTime, endOffsetTime;
	bool isLoop;
	double speed;
	bool useNetwork;

	MyParam(){init();}
	
	void printHelp( int aArgc, char **aArgv )
	{
		cerr
			<< "HELP" << endl
			<< "\t-x | --speed <SPEED>            : set playing speed to SPEED." << endl
			<< "\t-s | --start-time <OFFSET TIME> : start playing log at TIME." << endl
			<< "\t-e | --end-time <OFFSET TIME>   : end playing log at TIME." << endl
			<< "\t-l | --loop                     : looping mode." << endl
			<< "\t-S | --start-ssmtime <SSMTIME>  : start playing log at SSMTIME." << endl
			<< "\t-E | --end-ssmtime <SSMTIME>    : end playing log at SSMTIME." << endl
			<< "\t-v | --verbose                  : verbose." << endl
			<< "\t-q | --quiet                    : quiet." << endl
			<< "\t-h | --help                     : print help" << endl
			<< "ex)" << endl << "\t$ "<< aArgv[0] << " hoge1.log hoge2.log" << endl
			//<< "\t" << endl
			<< endl;
	}	

	bool optAnalyze(int aArgc, char **aArgv)
	{
		int opt, optIndex = 0;
		struct option longOpt[] = {
			{"speed", 1, 0, 'x'},
			{"start-time", 1, 0, 's'},
			{"end-time", 1, 0, 'e'},
			{"loop", 0, 0, 'l'},
			{"network", 0, 0, 'n'},
			{"start-ssmtime", 1, 0, 'S'},
			{"end-ssmtime", 1, 0, 'E'},
			{"verbose", 0, 0, 'v'},
			{"quiet", 0, 0, 'q'},
			{"help", 0, 0, 'h'},
			{0, 0, 0, 0}
		};
		while( ( opt = getopt_long( aArgc, aArgv, "x:s:lS:e:E:vqhn", longOpt, &optIndex)) != -1)
		{
			switch(opt)
			{
				case 'x' : speed = atof( optarg ); break;
				case 's' : startOffsetTime = atof(optarg); break;
				case 'e' : endOffsetTime = atof(optarg); break;
				case 'l' : isLoop = true; break;
				case 'n' : useNetwork = true; break;
				case 'S' : startTime = atof(optarg); break;
				case 'E' : endTime = atof(optarg); break;
				case 'v' : printlog::setLogVerbose(  ); break;
				case 'q' : printlog::setLogQuiet(  ); break;
				case 'h' : printHelp( aArgc, aArgv ); return false; break;
				default : { logError << "help : " << aArgv[0] << " -h" << endl; return false;}break;
			}
		}

		// 残りは全部ログファイル
		for( ; optind < aArgc; optind++ )
		{
			logArray.push_back( LogPlayer( aArgv[optind] ) );
		}
		
		if( !logArray.size() )
		{
			logError << "USAGE : this program needs <LOGFILE> of ssm." << endl;
			logError << "help : " << aArgv[0] << " -h" << endl;
			return false;
		}		
		return true;
	}
	
	bool logOpen()
	{
		LogPlayerArray::iterator log;
		ssmTimeT time = INFINITY;

		// 設定中は一時停止
		settimeSSM_is_pause( 1 );
		// 設定
		settimeSSM_speed( speed );

		// ログファイルの展開とストリームの作成
		log = logArray.begin(  );
		while( log != logArray.end(  ) )
		{
			if( !log->open(  ) )
			{
				log = logArray.erase( log );
				continue;
			}
			if( log->getStartTime(  ) < time )
				time = log->getStartTime(  );
			log++;
		}
		
		// ssm時間の設定
		if( startTime > 0 || startOffsetTime > 0 )
		{
			if( startTime <= 0 )
				startTime = time;
			startTime += startOffsetTime;
			
			// シーク
			seek( startTime );
		}
		else
		{
			startTime = time;
		}
		
		if( endTime > 0 || endOffsetTime > 0 )
		{
			if( endTime <= 0 )
				endTime = time;
			endTime += endOffsetTime;
		}
		else
		{
			endTime = INFINITY;
		}
		
		logVerbose << "startTime : " << startTime << endl;
		settimeSSM( startTime );
		settimeSSM_is_pause( 0 );
		
		return true;
	}
	bool seek( ssmTimeT time )
	{
		int isPause = gettimeSSM_is_pause();
		settimeSSM_is_pause( 1 );
		LogPlayerArray::iterator log;
		// シーク
		log = logArray.begin(  );
		while( log != logArray.end(  ) )
		{
			log->seek( time );
			log++;
		}
		settimeSSM( time );
		settimeSSM_is_pause( isPause );
		return true;
	}
	
	bool printProgressInit()
	{
		for( int i=0; i< 5; i++ )
		{
			logInfo << endl;
		}
		return true;
	}
	bool printProgress( ssmTimeT time )
	{
		char unit;
		double total = 0;

		char day[64];
		time_t t = time;
		strftime( day, sizeof ( day ), "%Y/%m/%d(%a) %H:%M:%S %Z", localtime( &t ) );

		LogPlayerArray::iterator log;

		getUnit( &total, &unit );
		
		logInfo
			<< "\033[s"							// 現在位置を記憶
			<< "\033[" << 6 << "A"				// 移動
			<< "\r\033[K"						// 左端へ移動後、右を全消去
			;
		logInfo
			<< "\033[K------[ "<< day <<" ]-------------" << endl
			<< "\033[Kssm time  : " << time << endl
			<< "\033[Klog time  : " << time - startTime + startOffsetTime << endl
			//<< "\033[Kdata rate : " << total << unit << endl
			<< "\033[Kspeed rate: " << gettimeSSM_speed(  ) << endl
			<< "\033[K" << endl;
			
		logInfo << "\033[u" << flush;						// 保存した位置に戻る
		return true;
	}
	
	bool commandAnalyze( const char *aCommand )
	{
		std::istringstream data( aCommand );
		std::string command;
		
		data >> command;
		if( !data.fail() )
		{
			if( command.compare( "speed" ) == 0 || command.compare( "x" ) == 0 )
			{
				double speed = 1.0;
				data >> speed;
				settimeSSM_speed( speed );
			}
			else if( command.compare( "skip" ) == 0 )
			{
				double skip = 0.1;
				data >> skip;
				settimeSSM( gettimeSSM(  ) + skip );
			}
			else if( command.compare( "seek" ) == 0 )
			{
				ssmTimeT offset = 0.0;
				data >> offset;
				seek( gettimeSSM(  ) + offset );
			}
			else if( command.compare( "pause") == 0 || command.compare( "p" ) == 0  )
			{
				settimeSSM_is_pause( 1 );
			}
			else if( command.compare( "start") == 0 || command.compare( "s" ) == 0 )
			{
				settimeSSM_is_pause( 0 );
			}
			else if( command.compare( "reverse") == 0 || command.compare( "r" ) == 0  )
			{
				settimeSSM_speed( -gettimeSSM_speed(  ) );
			}
			else if( command.compare( "quit") == 0 || command.compare( "q" ) == 0 )
			{
				gShutOff = true;
			}
			else
			{
				int c;
				for ( unsigned int i = 0; i < command.length(); i++ )
				{
					c = command[i];
					if( c == '+' )
					{
						settimeSSM_speed( gettimeSSM_speed(  ) * 1.2 );
					}
					else if( c == '-' )
					{
						settimeSSM_speed( gettimeSSM_speed(  ) / 1.2 );
					}
					else if( c == '>' )
					{
						settimeSSM_is_pause( 1 );
						settimeSSM( gettimeSSM(  ) + 0.1 );
					}
				}
			}
		}
		logInfo << "\033[2A\033[1M\r \n\r\033[K> " << flush;	// 2つ上へ移動して下の行を削除して下の行に移動 
		
		return true;
	}
};


void nproc_start(MyParam& param) {
	LogPlayerArray::iterator log;
	char command[128];

	if(!opentimeSSM() ){
		fprintf(stderr, "opentimeSSM error!\n");
	} 

	log = param.logArray.begin();
	while (log != param.logArray.end()) {
		log->setUseNetwork(true);
		if (!log->initRemote()) {
			fprintf(stderr, "initRemote error!\n");
			return; // init fail
		}
		// logの中のPConnectorからMC_INITIALIZEを発行する
		log++;
	}


	setSigInt();

	// ログファイルの展開とストリームの作成
	// MC_CREATEとMC_PROPERTY_SETを実行
	param.logOpen();
	param.printProgressInit();



	log = param.logArray.begin(  );
	log->setOffset(gettimeOffset()); // リモートにオフセットを設定．MC_OFFSETを実行

	logInfo << "  start" << endl << endl;
	logInfo << "\033[1A" << "> " << flush;

	// メインループ


	ssmTimeT time, printTime;
	bool isWorking;
	int playCnt; // 再生中のログの個数
	printTime = gettimeSSM_real(  );
//	int count = 0;  // test
	while( !gShutOff ) {
//		++count;
//		if (write_count >= 31) {
//			printf("end of log play\n");
//			printf("count = %d\n", count);
//			ctrlC(1);
//		}

		isWorking = false;
		playCnt = 0;
		// 現在時刻の取得
		time = gettimeSSM(  );
		// ログの再生
		log = param.logArray.begin(  );
		while( log != param.logArray.end(  ) ) {
			isWorking |= ( gettimeSSM_speed(  ) >= 0 ? log->play( time ) : log->playBack( time ) );
			if( log->isPlaying(  ) )
				playCnt++;
			log++;
		}
		// 終了判定
		if( !playCnt || time > param.endTime )
		{
			// ループするかどうか
			if( param.isLoop )
			{
				param.seek( param.startTime );
			}
			else
			{
				gShutOff = true;
				break;
			}
		}
		// ステータスの表示
		if( gettimeSSM_real(  ) >= printTime )
		{
			printTime += 1.0;
			param.printProgress( time );
		}

		// コマンド解析
		{
			// cinがノンブロッキングできないのでしょうがなくstdinを使うことにする。
			// 時間があればマルチスレッドとかにしても良いけど、同期が面倒
			if( fgets(command, sizeof(command), stdin ) )
				param.commandAnalyze( command );
		}
		// wait
		if( !isWorking )
			usleepSSM( 1000 ); // 1msスリープ
	}



}

// SSMのログ再生
int main( int aArgc, char **aArgv )
{
	MyParam param;
	LogPlayerArray::iterator log;
	char command[128];
	try
	{
		std::cout.setf(std::ios::fixed);
		std::cout << setprecision(3);
		
		// キーボートをノンブロッキングにしてみる
		fcntl( fileno( stdin ), F_SETFL, O_NONBLOCK );

		// オプション解析
		if( !param.optAnalyze( aArgc, aArgv ) )
			return -1;

		if (param.useNetwork) {
			nproc_start(param);
			exit(1);
		}

		// SSMの初期化
		if( !initSSM(  ) )
		{
			logError << "ssm init error." << endl;
			return -1;
		}

		setSigInt();
	
		// ログファイルの展開とストリームの作成
		log = param.logArray.begin(  );
		
		param.logOpen();
		param.printProgressInit();

		logInfo << "  start" << endl << endl;
		logInfo << "\033[1A" << "> " << flush;
		// メインループ
		ssmTimeT time, printTime;
		bool isWorking;
		int playCnt; // 再生中のログの個数
		printTime = gettimeSSM_real(  );
		while( !gShutOff )
		{
			isWorking = false;
			playCnt = 0;
			// 現在時刻の取得
			time = gettimeSSM(  );
			
			// ログの再生
			log = param.logArray.begin(  );
			while( log != param.logArray.end(  ) )
			{
				isWorking |= ( gettimeSSM_speed(  ) >= 0 ? log->play( time ) : log->playBack( time ) );
				if( log->isPlaying(  ) )
					playCnt++;
				log++;
			}

			// 終了判定
			//printf("time:endTime = %f:%f\n", time, param.endTime);
			if( !playCnt || time > param.endTime )
			{
				// ループするかどうか
				if( param.isLoop )
				{
					param.seek( param.startTime );
				}
				else
				{
					gShutOff = true;
					break;
				}
			}
			// ステータスの表示
			if( gettimeSSM_real(  ) >= printTime )
			{
				printTime += 1.0;
				param.printProgress( time );
			}
			
			// コマンド解析
			{
				// cinがノンブロッキングできないのでしょうがなくstdinを使うことにする。
				// 時間があればマルチスレッドとかにしても良いけど、同期が面倒
				if( fgets(command, sizeof(command), stdin ) )
					param.commandAnalyze( command );

			}
			// wait
			if( !isWorking )
				usleepSSM( 1000 ); // 1msスリープ
		}
	}
	catch(std::exception &exception)
	{
		cerr << endl << "EXCEPTION : " << exception.what() << endl;
	}
	catch(...)
	{
		cerr << endl << "EXCEPTION : unknown exception." << endl;
	}
	
	logInfo << endl;
	//　ログを閉じる
	logInfo << "finalize log data" << endl;
	log = param.logArray.begin(  );
	while( log != param.logArray.end(  ) )
	{
		log->close(  );
		log++;
	}
	
	// 時間の初期化
	logInfo << "ssm time init" << endl;
	inittimeSSM(  );

	logInfo << "end" << endl;
	endSSM(  );

	return 0;
}
