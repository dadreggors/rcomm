//      rcomm-client.cpp
//
//      Copyright 2009 David Dreggors <ddreggors@cfl.rr.com>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.


// System Headers
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <cstring>
#include <cstdlib>
#include <stdio.h>
// Custom Headers
#include "ConfigFile.h"


// Declaring namespaces we use
using namespace std;

// Prototyping functions for global scope
bool FileExists(string strFilename);
string get_time_date(void);
string int2str(int i);
void WriteLogFile(string szString, string logfile);
string name2ip(char *hostname, bool debug, bool lfile, string logfile);
void error(char *msg);
void usage(void);
void print_output(string reply);



bool FileExists(string strFilename) {
  struct stat stFileInfo;
  bool blnReturn;
  int intStat;

  // Attempt to get the file attributes
  intStat = stat(strFilename.c_str(),&stFileInfo);
  if(intStat == 0) {
    // We were able to get the file attributes
    // so the file obviously exists.
    blnReturn = true;
  } else {
    // We were not able to get the file attributes.
    // This may mean that we don't have permission to
    // access the folder which contains this file. If you
    // need to do that level of checking, lookup the
    // return values of stat which will give you
    // more details on why stat failed.
    blnReturn = false;
  }

  return(blnReturn);
}

inline const char * const BoolToString(bool b) {
  return b ? "true" : "false";
}

string get_time_date(void) {
	string currtime;
	struct tm *current;
	time_t now;
	time(&now);
	current = localtime(&now);
	currtime.append(int2str(current->tm_hour));
	currtime.append(":");
	currtime.append(int2str(current->tm_min));
	currtime.append(":");
	currtime.append(int2str(current->tm_sec));
	currtime.append(" ");
	currtime.append(int2str(current->tm_mon));
	currtime.append("/");
	currtime.append(int2str(current->tm_mday));
	currtime.append("/");
	currtime.append(int2str(current->tm_year));
	return(currtime);
}

string int2str(int i) {
	std::string str_return;
	std::stringstream strconvert;
	strconvert << i;
	str_return = strconvert.str();
	return(str_return);
}

void WriteLogFile(string szString, string logfile) {
	size_t pos = logfile.find_last_of("/\\");
	string path = logfile.substr(0, pos);
	if( chdir(path.c_str()) == 0 ){
		char log[logfile.length()];
		strcpy(log, logfile.c_str());
		try{
			FILE* pFile = fopen(log, "a");
			fprintf(pFile, "%s\n",szString.c_str());
			fclose(pFile);
		}catch(...){
			printf("ERROR: Could not open logfile (%s) for writing.\n%s\n", log, szString.c_str());
		}
	}else{
		int tmp = mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
		if ( tmp == 0 ){
			char log[logfile.length()];
			strcpy(log, logfile.c_str());
			try{
				FILE* pFile = fopen(log, "a");
				fprintf(pFile, "%s\n",szString.c_str());
				fclose(pFile);
			}catch(...){
				printf("ERROR: Could not open logfile (%s) for writing.\n%s\n", log, szString.c_str());
			}
		}
	}
}

string name2ip(char *hostname, bool debug, bool lfile, string logfile) {
	char *ip;
	string ipaddr;
	struct hostent *HostInfo;

	HostInfo = gethostbyname(hostname);
	if (HostInfo == NULL)
	{
		if (debug){
			string msg = get_time_date();
			msg.append(" - ");
			msg.append("ERROR: Unable to resolve hostname (");
			msg.append(hostname);
			msg.append(")");
			WriteLogFile(msg, logfile);
		}
		fprintf(stderr,"rcomm: ERROR: Unable to resolve hostname (%s)\n", hostname);
		exit(0);
	}else{
		int nCount = 0;
		while(HostInfo->h_addr_list[nCount])
		{
		 ip = inet_ntoa(*(struct in_addr *)HostInfo->h_addr_list[nCount]);
		 nCount++;
		}

	}
	ipaddr.append(ip);
	return(ipaddr);
}

void error(char *msg) {
    perror(msg);
    exit(0);
}

void usage(void) {
	printf("You must specify a host and a command!\n");
	printf("rcomm <remote-host> <command>\n");
	exit(1);
}


void print_output(string reply, string host_passed) {
    size_t newl, prev=0, endline;
    newl=reply.find_first_of("\n", 0);
    endline=reply.size();
    while (newl<=endline) {
        cout << host_passed << ": " << reply.substr(prev, newl-prev) << endl;
        prev = newl+1;
        newl=reply.find_first_of("\n", newl+1);
    }

}

int main(int argc, char *argv[]) {
	bool cfile = false, debug = false, lfile = false;
    int sockfd, portno, n, i, MAXRECV = 256;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[MAXRECV + 1];
    string host_passed = argv[1], app, app_home, conf_file, logfile;
    app = argv[0];
	conf_file = "/etc/rcomm.conf";
    if ( argc < 3 ){
		usage();
	}
    // Set the default port here
    portno = 18889;

    // Read the config file and get needed vals
    try{
		cfile = FileExists(conf_file);
		// Set the port with the value in the config file
		// If the config file does not have that set then use a default value.
		if (cfile){
			//Start using config options
			ConfigFile config( conf_file );
			// We initialized portno to 18889 above so
			// we will safely fail to that port
			// if we cannot read config file!
			portno = config.read<int>( "rport" );
			debug = config.read<bool>( "client_debug" );
			logfile = config.read<string>( "client_log" );
			app_home = config.read<string>( "app_home" );
			string pos1 = logfile.substr(0,1);
			if (pos1 != "/"){
				string tmp = app_home;
				tmp.append("/");
				tmp.append(logfile);
				logfile = tmp;
			}
		}

	}catch(...){}

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error((char *)"ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,( sockaddr * ) &serv_addr,sizeof(serv_addr)) < 0)
        error((char *)"ERROR connecting");
    std::string cmdbuf;
    argv[1] = NULL;
    for(i = 2; i < argc; i++) {
		string tmpstr = (std::string)argv[i];
		cmdbuf.append(" ");
		cmdbuf.append(tmpstr.c_str());
	}
	strcpy(buffer,cmdbuf.c_str());
    n = write(sockfd,buffer,strlen(buffer));
    if (n < 0) {
         error((char *)"ERROR writing to socket");
    }
	memset ( buffer, 0, MAXRECV + 1);
	string reply;
	bool hasoutput = false;
	while (n = read( sockfd, buffer, MAXRECV ) > 0) {
        hasoutput = true;
		reply.append(buffer);
        memset ( buffer, 0, MAXRECV + 1);
	}
	shutdown(sockfd, 0);
	close(sockfd);
	if ( (n < 1) && (hasoutput) ) {
            reply.append("No reply from host.");
	}
	print_output(reply, host_passed);
    return 0;
}

