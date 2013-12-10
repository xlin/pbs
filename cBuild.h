// License: GPLv3 or later
// Author: Sasha

#ifndef CBUILD_H
#define CBUILD_H

#include <string>
#include <map>

using namespace std;

class cBuild{
    public:
	cBuild(); 
	int build(map<string,string> _options);
	
    private:
	int createUser();
	int copyAddons();
	int addDistrib();
	int addMedia();
	int rpmbuild();
	int copySrpm();
	void clearBuild();
	void signalExit();

	
        map<string,string> options;
        
        bool isDeleteTmp;
        pid_t procBuildPid;
        bool normalExit;
        
	
};

#endif