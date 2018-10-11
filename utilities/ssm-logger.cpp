// g++ -o logger ssm-logger.cpp -I../include -lssm
#include <iostream>
#include <string>
#include <stdexcept>

#include <cstdlib>
#include <csignal>
#include <cstring>

#include <getopt.h>

#define __MAIN__
#include "printlog.hpp"
#include "ssm-log.hpp"

using namespace std;

static const ssmTimeT WAITTIME = 60; //[s] streamが開くまで待つ時間。これを超えると開くことを諦める

bool gShutOff = false;

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

class MyParam
{
	void init()
	{
		logName = NULL;
		streamName = NULL;
		streamId = 0;
	}
public:
	char *logName;
	char *streamName;
	int streamId;

	MyParam(){init();}
	
	void printHelp( int aArgc, char **aArgv )
	{
		cerr
			<< "HELP" << endl
			<< "\t-l | --log-name <LOGFILE> : set name of logfile to LOGFILE." << endl
			<< "\t-n | --stream-name <NAME> : set stream-name of ssm to NAME." << endl
			<< "\t-i | --stream-id <ID>     : set stream-id of ssm to ID." << endl
			<< "\t-v | --verbose            : verbose." << endl
			<< "\t-q | --quiet              : quiet." << endl
			<< "\t-h | --help               : print help" << endl
			<< "ex)" << endl << "\t$ "<< aArgv[0] << " -l hoge.log -n hogehoge -i 0" << endl
			//<< "\t" << endl
			<< endl;
	}	

	bool optAnalyze(int aArgc, char **aArgv)
	{
		int opt, optIndex = 0;
		struct option longOpt[] = {
			{"log-name", 1, 0, 'l'},
			{"stream-id", 1, 0, 'i'},
			{"stream-name", 1, 0, 'n'},
			{"verbose", 0, 0, 'v'},
			{"quiet", 0, 0, 'q'},
			{"help", 0, 0, 'h'},
			{0, 0, 0, 0}
		};
		while( ( opt = getopt_long( aArgc, aArgv, "l:i:n:vh", longOpt, &optIndex)) != -1)
		{
			switch(opt)
			{
				case 'l' : logName = optarg; break;
				case 'n' : streamName = optarg; break;
				case 'i' : streamId = atoi(optarg); break;
				case 'v' : printlog::setLogVerbose(  ); break;
				case 'q' : printlog::setLogQuiet(  ); break;
				case 'h' : printHelp( aArgc, aArgv ); return false; break;
				default : {cerr << "help : " << aArgv[0] << " -h" << endl; return false;}break;
			}
		}

		if( ( !logName ) || ( !streamName ) )
		{
			cerr << "USAGE : this program need <LOGNAME> and <NAME> of stream." << endl;
			cerr << "help : " << aArgv[0] << " -h" << endl;
			return false;
		}

		return true;
	}
};


// SSMのログ保存
int main( int aArgc, char **aArgv )
{
	SSMLogBase log;
	SSMApiBase stream;
	MyParam param;
	double cycle;
	int num;
	void *data = NULL;
	void *property = NULL;
	size_t propertySize = 0;
	size_t dataSize = 0;
	
	// オプション解析
	if( !param.optAnalyze( aArgc, aArgv ) )
		return -1;

	// SSMの初期化
	if( !initSSM(  ) )
	{
		logError << "ERROR: ssm init error." << endl;
		return -1;
	}

	setSigInt();
	
	// SSMに問い合わせ
	ssmTimeT startTime = gettimeSSM_real(  );
	while( getSSM_info( param.streamName, param.streamId, &dataSize, &num, &cycle, &propertySize ) <= 0 )
	{
		if( gShutOff )
		{
			endSSM();
			return -1;
		}
		
		if( gettimeSSM_real() - startTime > WAITTIME )
		{
			logError << "ERROR: cannot get ssm info of '"<< param.streamName << " ["<<  param.streamId<< "]." << endl;
			endSSM();
			return -1;
		}
		usleep( 100000 );
	}
	
	try
	{
		// メモリ確保
		if( ( data = malloc( dataSize ) ) == NULL )
			throw runtime_error("memory allocation of data");
		if( ( property = malloc( propertySize ) ) == NULL )
			throw runtime_error("memory allocation of property");

		log.setBuffer( data, dataSize, property, propertySize, NULL );
		stream.setBuffer( data, dataSize, property, propertySize );
				
		// ストリームを開く
		if( !stream.open( param.streamName, param.streamId ) )
			throw runtime_error( "stream is not exist." );
		if( propertySize > 0 && ( !stream.getProperty(  ) ) )
			throw runtime_error( "cannot get property" );
		logInfo << "open stream '" << param.streamName << "' [" << param.streamId << "]." << endl;

		stream.readLast(  );
		stream.setBlocking( true );

		// ログファイルを作成
		if( !log.create( param.streamName, param.streamId, num, cycle, param.logName ) )
			throw runtime_error( "cannot create logfile" );
			
		logVerbose << "create log '" << param.logName << "'." << endl;
		
		// メインループ
		while( !gShutOff )
		{
			// ストリームから読み込み
			if( !stream.readNext(  ) )
				continue;

			// ファイルへ書き込み
			if( !log.write( stream.time ) )
			{
				throw runtime_error( "cannot write log." );
			}
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
	
	log.close(  );
	// SSMからの切断
	stream.close(  );
	endSSM(  );

	// メモリの解放
	free( data );
	free( property );

	return 0;
}

