// License: GPLv3 or later
// Author: Sasha

#ifndef CCHROOT_H
#define CCHROOT_H

#include <string>
#include <map>

using namespace std;

class cChroot{
    public:
	cChroot(map<string,string> options); 
	int unPackArchive();
	int mountDev();
	int umountDev();
	
    private:
	map<string,string> options;
	
};

#endif