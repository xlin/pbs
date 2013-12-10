// License: GPLv3 or later
// Author: Sasha

#include "cChroot.h"
#include "functions.h"

cChroot::cChroot(map<string,string> options){
    this->options=options;
    
    // Проверка обязательных параметров
    if (options["chroot_dir"]==""){
	std::cout << "no param chroot_dir" << std::endl;
	exit(0);
    }
}

int cChroot::unPackArchive(){
    string tmp_dir=options["tmp-dir"];
    string chroot=options["chroot"];
    boost::smatch xResults;
    int result=0;
    
    if (boost::filesystem::exists(tmp_dir)==false){
	pbsMkdir(tmp_dir.c_str());
    }
    
    if (boost::regex_match(chroot, xResults, boost::regex("(.*)(\\.tar\\.gz)$"))==true){
	result=exec_cmd("tar zxf "+chroot+" -C "+tmp_dir,options);
    }
    if (boost::regex_match(chroot, xResults, boost::regex("(.*)(\\.tar\\.bz2)$"))==true){
	result=exec_cmd("tar jxf "+chroot+" -C "+tmp_dir,options);
    }
    if (boost::regex_match(chroot, xResults, boost::regex("(.*)(\\.tar\\.xz)$"))==true){
	result=exec_cmd("tar Jxf "+chroot+" -C "+tmp_dir,options);
    }

    return result;
}

int cChroot::mountDev(){
    string chroot_dir=options["chroot_dir"];

    pbsMount("/proc",chroot_dir+"/proc");
    pbsMount("/sys",chroot_dir+"/sys");
    pbsMount("/dev",chroot_dir+"/dev");
    pbsMount("/dev/pts",chroot_dir+"/dev/pts");

    return 0;
}

int cChroot::umountDev(){
    string chroot_dir=options["chroot_dir"];

    exec_cmd("umount "+chroot_dir+"/dev/pts");
    exec_cmd("umount "+chroot_dir+"/dev");
    exec_cmd("umount "+chroot_dir+"/sys");
    exec_cmd("umount "+chroot_dir+"/proc");

    return 0;
}
