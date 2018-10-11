/**
 * @brief SSMのプロセス同士のつながりを表示するツール
 */


#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "libssm.h"

#define __MAIN__

#include "printlog.hpp"

using namespace std;

int main( int aArgc, char **aArgv )
{
	if( !initSSM(  ) )
		logError << "cannot open ssm." << endl;
		
	int nodeNum, edgeNum;
	int nodeId;
	string nodeName;
	//char nodeName[128];
	char edgeName[SSM_SNAME_MAX];
	int edgeId;
	int node1, node2;
	int dir;
	
	nodeNum = getSSM_node_num() - 1;
	edgeNum = getSSM_edge_num();


	cout
		<< "digraph ssm" << endl << "{" << endl
		<< "\tgraph [ rankdir = LR ];" << endl
		<< "\tnode [ shape = box, style = filled, fillcolor=lightskyblue1 ];" << endl
		<< endl;
	
	for( int i = 0; i < nodeNum; i++ )
	{
		getSSM_node_info( i, &nodeId );
		stringstream str;
		cout
			<< '\t' << nodeId;
		str << "/proc/" << nodeId << "/cmdline";
		ifstream file( str.str(  ).c_str(  ) );
		if( file.is_open() )
		{
			file >> nodeName;
			cout << " [ label = \"" << nodeName.c_str() << "\"]";
		}
		cout << ';' << endl;
				
	}
	cout << endl;
	
	for( int i = 0; i < edgeNum; i++ )
	{
		if( !getSSM_edge_info( i, edgeName, sizeof( edgeName ), &edgeId, &node1, &node2, &dir ) )
			continue;
		cout
			<< '\t' << node1 << " -> " << node2
			<< " [ label = \"" << edgeName << '[' << edgeId << "]\"];"
			<< endl;
	}
	
	cout << "}" << endl << endl;
	
	endSSM(  );
	
	
	return 0;
}
