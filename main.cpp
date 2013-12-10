// License: GPLv3 or later
// Author: Sasha

#include "functions.h"
#include "cBuild.h"
#include <string>
#include <map>
#include <pwd.h>

int main(int argc, char **argv){
    if (getuid()!=0){
	std::cout << "run only root" << std::endl;
	return 1;
    }
    MYSQL_RES *result;
    MYSQL_ROW row;
    MYSQL *connection, mysql;    
    
    // Защита от двойного запуска
    int fd = -1;
    if ((fd = open(PATHLOCK, O_RDWR| O_CREAT, S_IRUSR| S_IWUSR)) == -1) {
        int myerr = errno;
        printf("ERROR: open errno(%d): %s\n", errno,strerror(myerr));
        return EXIT_FAILURE;
    }
    if (LockFile(fd)==EXIT_FAILURE){
	std::cout << "ERROR: is already running" << std::endl;
	return 1;
    }
    
    
    // Читаем в map параметры командной строки
    map<string,string> cmdParam = loadArgv(argc,argv);
    
    if (cmdParam["--daemon"]=="true") run_as_daemon();
    

    map<string,string> conf = loadConfig();
    //string pathAum=conf["paths/pathAum"];
    string pathUserHome=conf["paths/pathUserHome"];
    //string pathPbs=conf["paths/pathPbs"];
    string pathTemp=conf["paths/pathTemp"];

    string cmdLine="";
    string sqlQuery="";
    int buildError=0;		// Состояние ошибки
    
    mysql_init(&mysql);
    connection = mysql_real_connect(&mysql,conf["mysql/hostname"].c_str(), conf["mysql/username"].c_str(), conf["mysql/password"].c_str(), conf["mysql/database"].c_str(), 0, NULL,0);
    if( connection == NULL ) {
	printf("%s\n",mysql_error(&mysql));
        return 1;
    }
    
    srand (time(NULL));

    // Тут будет основной цикл
    while(1){
	
	int state = mysql_query(connection, 
	    "SELECT t1.id,t1.file_id, t2.filename, t3.user, t4.os, t4.arch "
	    "FROM jobs as t1, tsrc t2, users as t3, platforms as t4 "
	    "WHERE t1.status=0 and t2.id=t1.file_id and t3.id=t2.user_id and t4.id=t1.platform_id ");
	    
	if( state != 0 ) {
	    printf("%s\n",mysql_error(connection));
	    break;
	}

        result = mysql_store_result(connection);    
	while( ( row = mysql_fetch_row(result)) != NULL ) {
    	    printf("%s, %s\n",  row[0],row[1]);
    	
    	    // 
    	    sqlQuery="UPDATE jobs SET status='1' WHERE id='"+string(row[0])+"'";
    	    mysql_query(connection,sqlQuery.c_str());
    	
    	    string dbNSrcFile=string(row[1]);
    	    string dbOsName=string(row[4]);
    	    string dbOsArch=string(row[5]);
	    string username=string(row[3]);
	    string jobFilename=string(row[2]);
	    
	    string aumOsKey=dbOsName+"_"+dbOsArch;
	    string aumLogFile=pathUserHome+"/"+username+"/logs/"+dbNSrcFile+"/"+aumOsKey+".txt";
	    string aumUploadDir=pathTemp+"/files/"+dbNSrcFile;
	    string aumFileName=pathUserHome+"/"+username+"/srpms/"+dbNSrcFile+"/"+jobFilename;

	    std::cout << " " << dbNSrcFile << " " << dbOsName << " " << dbOsArch << " " << username << " " << jobFilename << " " << aumOsKey << std::endl;
	    
	    string aumChroot=conf[aumOsKey+"/chroot"];
	    string aumDistrib=conf[aumOsKey+"/distrib"];
	    
	    string aumPathTemp=pathTemp+"/pbs-chroot-"+dbNSrcFile+"-"+aumOsKey;
	    
	    //Создание временной папки
	    pbsMkdir(aumUploadDir);
	    pbsMkdir(aumPathTemp);

	    map<string,string> options;
	
	    // Формирование строки
	    //cmdLine=pathAum+" ";
	    options["chroot"]=aumChroot;
	    options["distrib"]=aumDistrib;
	    options["log-file"]=aumLogFile;
	    options["upload-dir"]=aumUploadDir;
	    options["tmp-dir"]=aumPathTemp;
	    
	    if (boost::filesystem::exists(pathUserHome+"/"+username+"/.rpmmacros")==true){
		options["rpmmacros"]=pathUserHome+"/"+username+"/.rpmmacros";
	    }
	    options["srpm"]=aumFileName;

	    

	    // Запуск сборки
	    //buildError=system(cmdLine.c_str());
	    cBuild bb;
	    buildError=bb.build(options);
	    if (buildError!=0){
    	        string sqlQuery="UPDATE jobs SET status='11' WHERE id='"+string(row[0])+"'";
    		mysql_query(connection,sqlQuery.c_str());
		continue;
	    }
	    

	    string targetOs = pathUserHome+"/"+username+"/repo/"+dbOsName;
	    int countFile=copyRpmToRepo(aumUploadDir,targetOs);
	    if (countFile==0){
    	        string sqlQuery="UPDATE jobs SET status='11' WHERE id='"+string(row[0])+"'";
    		mysql_query(connection,sqlQuery.c_str());
		continue;
	    }
	    
	    // Удаление временной папки
	    boost::filesystem::remove_all(aumUploadDir);

	    
	    // Пересоздание репозитория
	    exec_cmd("pbs-hdlistgen "+username+ " "+dbOsName);
	    
	    
    	    sqlQuery="UPDATE jobs SET status='10' WHERE id='"+string(row[0])+"'";
	    mysql_query(connection,sqlQuery.c_str());
	    
	    std::cout << "Build end" << std::endl;
	    
	}
	mysql_free_result(result);

	sleep(1);
    }
    
    
    mysql_close(connection);

    return 0;
}
