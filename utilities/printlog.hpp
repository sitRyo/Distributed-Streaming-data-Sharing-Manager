#ifndef PRINTLOG_HPP__
#define PRINTLOG_HPP__

#include <iostream>

namespace printlog
{
	extern int logLevel;
	enum
	{
		LOGLEVEL_QUIET = 0,
		LOGLEVEL_ERROR,
		LOGLEVEL_WARNING,
		LOGLEVEL_INFO,
		LOGLEVEL_VERVOSE,
		LOGLEVEL_MAX
	};

	inline void setLogLevel( int logLevel )
	{
		if( logLevel >= printlog::LOGLEVEL_MAX )
			printlog::logLevel = printlog::LOGLEVEL_MAX - 1;
		if( logLevel < printlog::LOGLEVEL_QUIET )
			printlog::logLevel = printlog::LOGLEVEL_QUIET;
		printlog::logLevel = logLevel;
	}


	inline void setLogVerbose(  )
	{
		printlog::setLogLevel( LOGLEVEL_VERVOSE );
	}

	inline void setLogQuiet()
	{
		printlog::setLogLevel( LOGLEVEL_QUIET );
	}

}

#define logError if( printlog::logLevel >= printlog::LOGLEVEL_ERROR ) std::cerr << "ERROR : "
#define logWarning if( printlog::logLevel >= printlog::LOGLEVEL_WARNING ) std::cerr << "WARNING : "
#define logInfo if( printlog::logLevel >= printlog::LOGLEVEL_INFO ) std::cout
#define logVerbose if( printlog::logLevel >= printlog::LOGLEVEL_VERVOSE ) std::cout

#ifdef __MAIN__
namespace printlog
{
	int logLevel = LOGLEVEL_INFO;
}
#endif


#endif /* PRINTLOG__ */
