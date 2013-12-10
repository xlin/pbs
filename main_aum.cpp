// License: GPLv3 or later
// Author: Sasha

#include "functions.h"
#include "cBuild.h"
#include <string>
#include <map>
#include <pwd.h>

void show_usage() {
std::cout << "\
================================================================================ \n\
    aum++ ver. (0.3.0) - RPM build tool for building in chroot \n\
================================================================================ \n\
\n\
    Usage: aum++ [OPTIONS...] SRPM \n\
\n\
Options: \n \
\n\
General: \n\
\n\
    --help or -h       Show help \n\
    --chroot=PATH      Specify chroot tarball \n\
\n\
    Distribs and medias: \n\
\n\
        --distrib=PATH     Use specified distrib \n\
        --add-media=PATH   Add additional media \n\
\n\
    Building: \n\
\n\
        --arch=ARCH        Specify arch \n\
        --log-file=FILE    Save logs in specified log file \n\
        --noclear          Don\'t remove build directory on exit \n\
        --timeout=SECONDS  Limit build process time (time for rpmbuild) \n\
        --tmp-dir=DIR      Build package in specified temp directory \n\
        --upload-dir=DIR       Upload dir \n\
        --add-rpmmacros=FILE   Change rpmmacros \n\
        --urpmi-options=WORD   Options for urpmi (see urpmi --help) \n\
\n\
    Debugging: \n\
\n\
	--verbose or -v    Be more verbose \n\
\n\
    " << std::endl;
}

map<string,string> loadArgvAum(int argc, char **argv){
    map<string,string> result;

    for (int i=1;i<argc-1;i++){
	string arg=string(argv[i]);

        boost::smatch xResults;	        
	if (boost::regex_match(arg,  xResults, boost::regex("(--)([-a-zA-Z0-9_]+)(=)(.*)"))==true){

	    //for (unsigned int a=0;a<xResults.size();a++) std::cout << xResults[a] << endl;

	    if (result[xResults[2]]==""){
		result[xResults[2]]=xResults[4];
	    }else{
		result[xResults[2]]+=";"+xResults[4];
	    }
	    continue;
	}
	if (boost::regex_match(arg,  xResults, boost::regex("([-]+)([-a-zA-Z0-9_]+)"))==true){
	    result[xResults[2]]="true";
	}
    }
    result["srpm"]=string(argv[argc-1]);
    
    
    // Проверка на обязательные параметры
    
    // Установка умолчаний
    
    
    return result;
}

int main(int argc, char **argv){
    if (getuid()!=0){
	std::cout << "You must be root" << std::endl;
	return 1;
    }

    if (argc<2){
	show_usage();
	return 1;
    }
    if (strcmp(argv[1],"--help")==0 or strcmp(argv[1],"-h")==0){
	show_usage();
	return 1;
    }
    srand (time(NULL));

    map<string,string> options=loadArgvAum(argc,argv);

    cBuild bb;
    bb.build(options);

    return 1;
}
