// License: GPLv3 or later
// Author: Sasha

#include "functions.h"

#include <sys/mount.h>
#include <stdlib.h>     /* system, NULL, EXIT_FAILURE */

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>


// рекурсивное создание папки
void pbsMkdir(const char *dir_path, mode_t mode){
    boost::filesystem::path dir(dir_path);
    boost::filesystem::create_directories(dir);
}

void pbsMkdir(const char *dir_path){
    mode_t mode = 0755;
    pbsMkdir(dir_path,mode);
}

void pbsMkdir(string dir_path){
    mode_t mode = 0755;
    pbsMkdir(dir_path.c_str(),mode);
}


// Конвертация int в std::string
string IntToString(int x){
    //stringstream r;
    //r << x;
    //return r.str();
    
    char buf[20];
    sprintf(buf,"%d",x);
    return string(buf);
}


// Чтение конфига
map<string,string> loadConfig(){
    map<string,string> conf;
    
    std::ifstream ifs(PATHCFG);
    if (!ifs.is_open()) {
         std::cerr << "Failed to open file" << std::endl;
         return conf;
    }
    
    string razdel="";
    std::string buffer;
    boost::smatch xResults;
    
    while (std::getline(ifs, buffer)) {
	// Если раздел
	if (boost::regex_match(buffer,  xResults, boost::regex("(\\[)([_a-zA-Z0-9]+)(\\])"))==true){
	    razdel=xResults[2];
	    //std::cout << razdel << endl;
	    continue;
	}
	// Если ключ="значение"
	if (boost::regex_match(buffer,  xResults, boost::regex("([_a-zA-Z0-9]+)(=)(\")(.*)(\")"))==true){
	    conf[razdel+"/"+xResults[1]]=xResults[4];
	    //std::cout << razdel << "/" << xResults[1] << "=" << xResults[4] << std::endl;
	}
    }
    
    //Установка умолчаний, если в конфиге нет определений
    if (conf["paths/pathTemp"]=="") conf["paths/pathTemp"]="/tmp";
    
    return conf;
}

// Чтение параметров в массив
map<string,string> loadArgv(int argc, char **argv){
    map<string,string> result;

    for (int i=0;i<argc;i++){
	//std::cout << argv[i] << std::endl;
	result[string(argv[i])]="true";
    }
    
    return result;
}

int copyRpmToRepo(string aumUploadDir,string targetRepoOs){
    int countFile=0;

    for (boost::filesystem::recursive_directory_iterator it(aumUploadDir), end; it != end; ++it) {
	string filename=it->path().filename().string();
	string arch="noarch";
        boost::smatch xResults;
	        
	if (boost::regex_match(filename,  xResults, boost::regex("(.*)(\\.)(src|x86_64|i586)(\\.rpm)"))==true){
	    arch=xResults[3];
	}

	string target = targetRepoOs+"/RPMS/"+arch+"/"+filename;
        if (arch=="src")  target = targetRepoOs+"/SRPMS/"+filename;
        
    	copyFile(aumUploadDir+"/"+filename,target);
        countFile++;
    }

    return countFile;
}

int LockFile(int fd) {
    struct flock fd_flock;
    fd_flock.l_type = F_WRLCK;/*Write lock*/
    fd_flock.l_whence = SEEK_SET;
    fd_flock.l_start = 0;
    fd_flock.l_len = 0;/*Lock whole file*/
                    
                    
    fcntl(fd, F_GETLK,&fd_flock);
    if (fd_flock.l_type!=F_UNLCK){
        return EXIT_FAILURE;
    }

    fd_flock.l_type = F_WRLCK;/*Write lock*/
    if (fcntl(fd, F_SETLK,&fd_flock) == -1) {
	int myerr = errno;
        printf("ERROR: fcntl errno(%d): %s\n", errno,strerror(myerr));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int run_as_daemon(){
    pid_t pid = fork();
    if (pid > 0)
    _exit(0);
    else if (pid < 0)
    return -1;
    setsid();
    close(STDOUT_FILENO);
    close(STDIN_FILENO);
    close(STDERR_FILENO);
    return 0;
}

int pbsMount(string from,string to){
    string cmdLine="mount --bind "+from+" "+to;
    return system(cmdLine.c_str());
}


int exec_cmd(string cmdLine,map<string,string> options){
    string path_log="/dev/null";
    if (options["log-file"] !="") path_log=options["log-file"];
    
    string output="&> /dev/null";
    
    if (options["verbose"]=="true" or options["v"]=="true"){
	cmdLine=cmdLine+" 2>&1 | tee -a "+path_log;
    }else{
	cmdLine=cmdLine+" &>> "+path_log;
    }
    return system(cmdLine.c_str());
}

void logdebug(string message,map<string,string> options){
    if (options["log-file"] !=""){
        std::fstream fs;
	fs.open (options["log-file"].c_str(), std::fstream::in | std::fstream::out | std::fstream::app);
        fs << message << "\n";
	fs.close();
    }
    
    if (options["verbose"]=="true" or options["v"]=="true"){
	std::cout << message << std::endl;
    }

}

int exec_cmd_in_chroot(string cmdLine,string chroot_dir,string user,map<string,string> options){
    string cmd="";
    
    if (user=="" or user=="root"){
        cmd = "LC_ALL=C chroot "+chroot_dir+" "+cmdLine+" ";
    }else{
	cmd = "LC_ALL=C chroot "+chroot_dir+" su -c "+cmdLine+" - "+user+" ";
    }
    
    //std::cout << "exec_cmd_in_chroot: " << cmd << std::endl;
    
    return exec_cmd(cmd,options);
}


int copyFile(string from,string target){

    if (boost::filesystem::exists(target)){
	boost::filesystem::remove(target);
    }
    
    try {
	boost::filesystem::copy_file(from,target);    
    }catch (boost::filesystem::filesystem_error &e){ 
        std::cerr << e.what() << std::endl; 
        return 1;
    } 
    
    return 0;
}

string buildUser(){
    return string(BUILDUSER);
}


string basename(string filepath){
    vector<string> resultSplit;
    boost::split(resultSplit,filepath,boost::is_any_of("/"),boost::token_compress_on);
    string filename=resultSplit[resultSplit.size()-1];

    return filename;
}

vector<string> getRecurciveListFiles(string _folder){
    vector<string> result;
    
    
    try {
        boost::filesystem::path path1(_folder);
        directory_iterator end ;
	for(directory_iterator iter(path1) ; iter != end ; ++iter ){
	    if ( is_directory( *iter ) ){
		vector<string> newList = getRecurciveListFiles(iter->path().string());
		result.insert(result.begin(),newList.begin(),newList.end());
	    }else{
		result.push_back(_folder+"/"+iter->path().filename().string());
	    }
        }
    }catch (boost::filesystem::filesystem_error &e){ 
        std::cerr << e.what() << std::endl; 
    } 

    
    return result;
}

string rndString(int length){
    string result="";
    int s=0;
       
    string abs="1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM";
               
    for (int i=0;i<length;i++){
       s=rand()%abs.length()+1;
       result+=abs.substr(s,1);
    }
    return result;
}

string replaceString(string str,string searchString,string replaceString){
    string::size_type pos = 0;
    while ( (pos = str.find(searchString, pos)) != string::npos ) {
        str.replace( pos, searchString.size(), replaceString );
        pos++;
    }
    return str;
}
