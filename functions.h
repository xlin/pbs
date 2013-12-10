// License: GPLv3 or later
// Author: Sasha

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <stdio.h>      /* printf */
#include <stdlib.h>     /* system, NULL, EXIT_FAILURE */
#include <unistd.h>
#include <iostream>
#include <fstream>

#include <string>
#include <vector>
#include <map>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>

#include <mysql/mysql.h>

using namespace std;
using namespace boost::filesystem;

#define PATHCFG "/etc/pbs.conf"
#define PATHLOCK "/var/run/pbs.lock"
#define BUILDUSER "user"

struct sPlatform {
    string osName;
    string osArch;
    string chroot;
    string distrib;
};

void pbsMkdir(const char *dir_path, mode_t mode);
void pbsMkdir(const char *dir_path);
void pbsMkdir(string dir_path);
string IntToString(int x);
map<string,string> loadConfig();
map<string,string> loadArgv(int argc, char **argv);
int copyRpmToRepo(string aumUploadDir,string targetRepoOs);
int LockFile(int fd);
int run_as_daemon();
int pbsMount(string from,string to);
int exec_cmd(string cmdLine,map<string,string> options =map<string,string>());
int exec_cmd_in_chroot(string cmdLine,string chroot_dir,string user,map<string,string> options =map<string,string>());
int copyFile(string from,string target);
string buildUser();
string basename(string filepath);
vector<string> getRecurciveListFiles(string _folder);
string rndString(int length);
string replaceString(string str,string from,string to);
void logdebug(string message,map<string,string> options);

#endif