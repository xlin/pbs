// License: GPLv3 or later
// Author: Sasha

#include "cBuild.h"
#include "functions.h"
#include "cChroot.h"

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/impl/io_service.hpp>
#include <boost/thread.hpp>

using namespace boost::filesystem;

cBuild::cBuild(){
    isDeleteTmp=false;
    procBuildPid=0;
    normalExit=false;
}

int cBuild::build(map<string,string> _options){
    options=_options;
    
    // Проверка options
    if (options["upload-dir"]=="") options["upload-dir"]=".";
    if (options["tmp-dir"]=="") options["tmp-dir"]="/tmp/pbs-"+rndString(8);
    if (options["timeout"]=="") options["timeout"]="0";

    options["chroot_dir"]=options["tmp-dir"]+"/chroot";
    
    string urpmi_options=replaceString(options["urpmi-options"],","," ");
    
    // Ставим обработчик прерываний
    boost::asio::io_service service;
    boost::asio::signal_set sig(service, SIGINT, SIGTERM);
    sig.async_wait(boost::bind(&cBuild::signalExit, this));
    boost::thread thr(boost::bind(&boost::asio::io_service::run, &service));

    // Проверяем папку tmp-dir
    if (boost::filesystem::exists(options["tmp-dir"])==false){
	isDeleteTmp=true;
    }
    
    logdebug("Temporary directory: "+options["tmp-dir"],options);
    
    if (options["chroot"]!=""){
        // Проверка существования chroot образа
	if (boost::filesystem::exists(options["chroot"])==false){
	    logdebug(options["chroot"]+" not found",options);
	    return 1;
	}
    }
    
    if (options["log-file"] !=""){
	std::fstream fs;
	fs.open(options["log-file"].c_str(), ios::out );
	fs.close();
    }

    
    cChroot cc(options);

    // Создание chroot
    // Распаковываем архив
    if (options["chroot"]!=""){
        logdebug("Unpacking chroot archive...",options);
	if (cc.unPackArchive()!=0){
	    logdebug("Error when unpacking "+options["chroot"]+ " in "+options["tmp-dir"],options);
    	    clearBuild();
	    return 1;
	}
	logdebug("Unpacking chroot archive was completed",options);
    }
    
    if (boost::filesystem::exists(options["chroot_dir"])==false){
	logdebug(options["chroot_dir"] + " not found",options);
	clearBuild();
	return 1;
    }

    logdebug("Mount dev ",options);
    cc.mountDev();

    
    string chroot_dir=options["chroot_dir"];
    
    // Создание пользователя и его окружение
    logdebug("Create user with name: "+buildUser()+" ",options);
    createUser();

    logdebug("Copy addons ",options);
    copyAddons();
    
    logdebug("Add distrib ",options);
    addDistrib();

    logdebug("Add media ",options);
    addMedia();
    
    
    logdebug("Copying srpm to chroot_dir/home/"+buildUser()+" ",options);
    copySrpm();
    

    //std::cout << "Rebuilding SRPM..." << std::endl;
    //exec_cmd_in_chroot("'rpm -ivh /home/aum/$srpm_basename'",            $chroot_dir, user => 'aum');
    //exec_cmd_in_chroot("'rpmbuild -bs /home/aum/rpmbuild/SPECS/*.spec'", $chroot_dir, user => 'aum');

    exec_cmd_in_chroot("chown "+buildUser()+":"+buildUser()+" /home/"+buildUser()+" -R",chroot_dir,"");
    
    
    string srpm_filepath="/home/"+buildUser()+"/rpmbuild/SRPMS/"+basename(options["srpm"]);
    
    logdebug("Rebuilding RPM database...",options);
    exec_cmd_in_chroot("rpm --rebuilddb", chroot_dir,"",options);

    logdebug("Installing build dependencies...",options);
    exec_cmd_in_chroot("urpmi --buildrequires --debug --no-verify-rpm --nolock --auto --ignoresize --no-suggests "+urpmi_options+" "+srpm_filepath, chroot_dir,"",options);


    // Сборка
    logdebug("Building...",options);
    if (rpmbuild()!=0){
    }
    
    
    // Раскидываем собранные файлы
    logdebug("Copy RPM and SRPM packages in upload-dir...",options);
    
    vector<string> listfiles = getRecurciveListFiles(chroot_dir+"/home/"+buildUser()+"/rpmbuild/RPMS");
    for (unsigned int i=0;i<listfiles.size();i++){
        copyFile(listfiles[i],options["upload-dir"]+"/"+basename(listfiles[i]));
    }
    listfiles = getRecurciveListFiles(chroot_dir+"/home/"+buildUser()+"/rpmbuild/SRPMS");
    for (unsigned int i=0;i<listfiles.size();i++){
        copyFile(listfiles[i],options["upload-dir"]+"/"+basename(listfiles[i]));
    }
    
    
    service.stop();
        
    clearBuild();
    
    
    return 0;
}

void cBuild::signalExit(){
    if (normalExit==true) return;
    std::cout << "Manual exit\n";
    if (procBuildPid>0) kill(procBuildPid,15);

    clearBuild();
    exit(0);
}

void cBuild::clearBuild(){
    //Размонтируем /dev
    //cc.umountDev();
    
    string chroot_dir=options["chroot_dir"];

    exec_cmd("umount "+chroot_dir+"/dev/pts");
    exec_cmd("umount "+chroot_dir+"/dev");
    exec_cmd("umount "+chroot_dir+"/sys");
    exec_cmd("umount "+chroot_dir+"/proc");
    
    // удаляем временную папку
    if (options["noclear"]!="true"){
	try {
	    logdebug("Delete directory "+options["tmp-dir"]+"/chroot",options);
	    boost::filesystem::remove_all(options["tmp-dir"]+"/chroot");
	    if (isDeleteTmp==true){
		logdebug("Delete directory "+options["tmp-dir"],options);
		boost::filesystem::remove_all(options["tmp-dir"]);
	    }
        }catch (boost::filesystem::filesystem_error &e){ 
	    std::cerr << e.what() << std::endl; 
	} 
    }
    
    normalExit=true;
}


int cBuild::createUser(){
    string chroot_dir=options["chroot_dir"];

    exec_cmd_in_chroot("userdel -r "+buildUser(),chroot_dir,"");
    exec_cmd_in_chroot("useradd "+buildUser(),chroot_dir,"");
    exec_cmd_in_chroot("chown -R "+buildUser()+":"+buildUser()+" /home/"+buildUser(),chroot_dir,"");

    return 0;
}

int cBuild::copyAddons(){
    string chroot_dir=options["chroot_dir"];

    if (copyFile("/etc/resolv.conf",chroot_dir+"/etc/resolv.conf")!=0){
        std::cout << "Can't copy /etc/resolv.conf into chroot" << std::endl;
    }

    if (options["add-rpmmacros"] != ""){
	copyFile(options["add-rpmmacros"],chroot_dir+"/home/"+buildUser()+"/.rpmmacros");
    }

    return 0;
}

int cBuild::addDistrib(){
    string chroot_dir=options["chroot_dir"];
    
    if (options["distrib"]=="") return 0;
    
    vector<string> resultSplit;
    boost::split(resultSplit,options["distrib"],boost::is_any_of(";"),boost::token_compress_on);
    for (unsigned int i=0;i<resultSplit.size();i++){
	string distrib=resultSplit[i];
	if (distrib=="") continue;
	std::cout << "Add distrib: " << distrib << std::endl;
	exec_cmd_in_chroot("urpmi.addmedia --distrib "+distrib, chroot_dir,"",options);
    }

    return 0;
}

int cBuild::addMedia(){
    string chroot_dir=options["chroot_dir"];
    
    if (options["add-media"]=="") return 0;
    
    vector<string> resultSplit;
    boost::split(resultSplit,options["add-media"],boost::is_any_of(";"),boost::token_compress_on);
    for (unsigned int i=0;i<resultSplit.size();i++){
	string media=resultSplit[i];
	if (media=="") continue;
	std::cout << "Add media: " << media << std::endl;
        exec_cmd_in_chroot("urpmi.addmedia media_"+IntToString(i)+" "+media, chroot_dir,"",options);
    }

    return 0;
}

int cBuild::copySrpm(){
    string chroot_dir=options["chroot_dir"];
    
    if (options["srpm"]==""){
	std::cout << "srpm not found" << std::endl;
	return 1;
    }
    
    string srpm_filename=basename(options["srpm"]);
    pbsMkdir(chroot_dir+"/home/"+buildUser()+"/rpmbuild/SRPMS/");
    copyFile(options["srpm"],chroot_dir+"/home/"+buildUser()+"/rpmbuild/SRPMS/"+srpm_filename);
    
    return 0;
}


int cBuild::rpmbuild(){
    string srpm_filepath="/home/"+buildUser()+"/rpmbuild/SRPMS/"+basename(options["srpm"]);
    string chroot_dir=options["chroot_dir"];
    int timeout=atoi(options["timeout"].c_str());
    int status=0;

    // Сборка
    procBuildPid = fork();
    if (procBuildPid==-1){
	std::cout << "/* Handle the error (you can perror(\"fork\") and exit) */" << std::endl;
        clearBuild();
	return 1;
    }
    if (procBuildPid==0){
        std::cout << "Building RPM packages..." << std::endl;
	if (options["arch"] == "i586") {
    	    exec_cmd_in_chroot("'linux32 rpmbuild --target i586 --rebuild "+srpm_filepath+"'", chroot_dir, buildUser(),options);
	}
	else {
    	    exec_cmd_in_chroot("'rpmbuild --rebuild "+srpm_filepath+"'", chroot_dir, buildUser(),options);
	}
	exit(0);
    }

    double starttime=time(NULL);
    int npid=0;
    while (npid != procBuildPid){
	sleep(1);
        npid = waitpid(procBuildPid, &status,WNOHANG);
        if (npid == -1) {
            perror("waitpid");
            break;
        }
	if (time(NULL)-starttime>timeout and timeout!=0){
	    logdebug("\nError: stopped by timeout",options);
	    kill(procBuildPid,15);
	}
    }
    procBuildPid=0;
    if (npid == -1) return EXIT_FAILURE;
    

    if (WIFEXITED(status)) {
	printf("exited, status=%d\n", WEXITSTATUS(status));
	status=WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
	printf("killed by signal %d\n", WTERMSIG(status));
	status=WTERMSIG(status);
    } else if (WIFSTOPPED(status)) {
        printf("stopped by signal %d\n", WSTOPSIG(status));
        status=WSTOPSIG(status);
    } else if (WIFCONTINUED(status)) {
	printf("continued\n");
	status=1;
    }

    return status;
}
