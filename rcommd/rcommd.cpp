/*
 * rcommc.c
 *
 *  Created on: Mar 1, 2009
 *      Author: ddreggors
*/

/*   A simple server in the internet domain using TCP
 *   The port number is passed as an argument
 *   This version runs forever, forking off a separate
 *   process for each connection
*/

// System Headers
#include <string>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>
// Custom Headers
#include "ConfigFile.h"

// Declaring namespaces we use
using namespace std;


// Setup some global vars to pass between child and parent
bool used[50];
int socknum = 0;


// Prototyping functions for global scope
void WriteLogFile(string szString, string logfile);
bool do_test(const char ipaddr[], const char subnet[], string logfile, bool debug);
string get_time_date(void);
time_t Str2DateTime(string buff);
bool FileExists(string strFilename);
size_t strpos(const string &haystack, const string &needle);
string int2str(int i);
void error(char *msg);
void do_tcmd(string cmd, int cli_sock, string logfile);



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
			printf("ERROR: Could not open logfile (%s) for writing.", log);
			printf("%s\n", szString.c_str());
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
				printf("ERROR: Could not open logfile (%s) for writing.\n", log);
				printf("%s\n", szString.c_str());
			}
		}
	}
}

bool do_test(const char ipaddr[], const char subnet[], string logfile, bool debug) {
	try{
	    char IPAddr[16];
        strcpy(IPAddr, ipaddr);
        char Subnet[19];
        strcpy(Subnet, subnet);
		char * pch;
		string myNet[2];
		//string network, tmpmask;
		pch = strtok (Subnet,"/");
		int ct = 0;
		while (pch != NULL){
			myNet[ct] = pch;
			pch = strtok (NULL, "/");
			ct++;
		}


		// Convert the ipaddress to unsigned long
		struct sockaddr_in sock_lower;
		inet_aton(myNet[0].c_str(),&sock_lower.sin_addr);
		unsigned long mylower = ntohl(sock_lower.sin_addr.s_addr);

		// Convert NetMask to unsigned long
		int imask = atoi(myNet[1].c_str());

		// Grab the upper ip in the range and convert to unsigned long
		struct sockaddr_in sock_upper;
		inet_aton(myNet[0].c_str(),&sock_upper.sin_addr);
		unsigned long myupper = mylower + static_cast<unsigned int>((pow((double)2,32-imask)-1)+0.5);


		// Convert given IP to unsigned long
		struct sockaddr_in sock_ip;
		inet_aton(IPAddr,&sock_ip.sin_addr);
		unsigned long myip = ntohl(sock_ip.sin_addr.s_addr);
		if (debug){
			string msg = "CIDR: ";
			msg.append(myNet[1]);
			msg.append("\n");
			msg.append("Network: ");
			msg.append(myNet[0]);
			msg.append("\n");
			msg.append("IP Address: ");
			msg.append(IPAddr);
			WriteLogFile(msg, logfile);
		}
		if (mylower < myip && myip < myupper){
			if (debug){
				string msg = "IP '";
				msg.append(IPAddr);
				msg.append("' is inside the range '");
				msg.append(myNet[0]);
				msg.append("/");
				msg.append(myNet[1]);
				WriteLogFile(msg, logfile);
			}
			return true;
		}else{
			if (debug){
				string msg = "IP '";
				msg.append(IPAddr);
				msg.append("' is not inside the range '");
				msg.append(myNet[0]);
				msg.append("/");
				msg.append(myNet[1]);
				WriteLogFile(msg, logfile);
			}
			return false;
		}
	}catch (...){
		return false;
	}
}

string get_time_date(void) {
	string currtime;
	struct tm *current;
	time_t now;
	time(&now);
	current = localtime(&now);
	currtime.append("[");
	currtime.append(int2str(current->tm_mon));
	currtime.append("-");
	currtime.append(int2str(current->tm_mday));
	currtime.append("-");
	currtime.append(int2str(current->tm_year));
	currtime.append(" ");
	currtime.append(int2str(current->tm_hour));
	currtime.append(":");
	currtime.append(int2str(current->tm_min));
	currtime.append(":");
	currtime.append(int2str(current->tm_sec));
	currtime.append("]");
	return(currtime);
}

time_t Str2DateTime(string buff) {
	int yyyy, mm, dd, hour, min, sec;
	struct tm *when;
	time_t tme;
	char tmp_time[20];
	strcpy(tmp_time, buff.c_str());

	sscanf(tmp_time, "%d-%d-%d %d:%d:%d", &mm, &dd, &yyyy, &hour, &min, &sec);

	time(&tme);
	when = localtime(&tme);
	when->tm_year = yyyy - 1900;
	when->tm_mon = mm-1;
	when->tm_mday = dd;
	when->tm_hour = hour;
	when->tm_min = min;
	when->tm_sec = sec;

	return( mktime(when) );
}

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

size_t strpos(const string &haystack, const string &needle) {
    int sleng = haystack.length();
    int nleng = needle.length();

    if (sleng==0 || nleng==0)
        return string::npos;

    for(int i=0, j=0; i<sleng; j=0, i++ )
    {
        while (i+j<sleng && j<nleng && haystack[i+j]==needle[j])
            j++;
        if (j==nleng)
            return i;
    }
    return string::npos;
}

string int2str(int i) {
	std::string str_return;
	std::stringstream strconvert;
	strconvert << i;
	str_return = strconvert.str();
	return(str_return);
}

void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[]) {
	daemon(0,0);
	bool cfile = false, debug = false;

	int ct = 0, portno = 18889, MAXRECV = 256, listener, cli_sock, n;
	char * pch, allowed[255], cmd_buf[MAXRECV + 1];
	struct sockaddr_in serv_addr, cli_addr;
	socklen_t clilen;
	string config_path, app = argv[0], app_home, logfile, strallow = "127.0.0.1", cli_cmd, IPADD, cmd_reply;
	vector<string> IPACL;
	try{
		if (((string)argv[1] == "-c" || (string)argv[1] == "-C") && (string)argv[2] != ""){
			config_path.append(argv[2]);
			cfile = FileExists(config_path);
		}else{
			config_path = "/etc/rcomm.conf";
			cfile = FileExists(config_path);
		}

		if (cfile){
			//Start using config options
			ConfigFile config( config_path );
			// We initialized portno to 18889 above so
			// we will safely fail to that port
			// if we cannot read config file!
			portno = config.read<int>( "lport" );
			strallow = config.read<string>( "allowed" );
			strcpy(allowed,strallow.c_str());
			debug = config.read<bool>( "server_debug" );
			logfile = config.read<string>( "server_log" );
			app_home = config.read<string>( "app_home" );
			string pos1 = logfile.substr(0,1);
			if (pos1 != "/"){
				string tmp = app_home;
				tmp.append("/");
				tmp.append(logfile);
				logfile = tmp;
			}
		}else{
			portno = 18889;
			strallow = "127.0.0.1";
			app_home = "/tmp";
		}
		// We initialized strallow to 127.0.0.1
		// above so we will safely fail to that
		// address ACL if we cannot read config file!
		strcpy(allowed,strallow.c_str());

	}catch(...){
		if (debug){
			string msg = get_time_date();
			msg.append("- Error reading config file!");
			WriteLogFile(msg, logfile);
		}
	}

	if (debug){
		string msg = get_time_date();
		msg.append(" - Config file: ");
		msg.append(config_path);
		msg.append("\n");
		msg.append(get_time_date());
		msg.append(" - IP Address ACL: ");
		msg.append(allowed);
		msg.append("\n");
		msg.append(get_time_date());
		msg.append(" - Listening port: ");
		string strport = int2str(portno);
		msg.append(strport.c_str());
		WriteLogFile(msg, logfile);
	}

	pch = strtok (allowed,",");
	while (pch != NULL){
		IPACL.resize(ct +1);
		IPACL[ct] = pch;
		pch = strtok (NULL, ",");
		ct++;
	}
	// Create the sockets
	try {
        // This is the listener
        listener = socket(AF_INET, SOCK_STREAM, 0);
        if (listener < 0) {
            error((char*)"ERROR opening socket");
        }
        bzero((char *) &serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(portno);

        if (bind(listener, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
              error((char*)"ERROR on binding");
        }
        // Start listening and allow up to 5 connections at once.
        listen(listener,5);

		// We want to continue running until closed manually!
		while ( true ){
			try {

                clilen = sizeof(cli_addr);
                // Pass the connection to an open client socket
                cli_sock = accept(listener, (struct sockaddr *) &cli_addr, &clilen);
                if (cli_sock < 0) {
                    error((char*)"ERROR on accept");
                } else {
                    // Find out who connected
                    IPADD = inet_ntoa(cli_addr.sin_addr);
                }
                n = 0;
				memset(cmd_buf, 0, MAXRECV + 1);
                n = read( cli_sock, cmd_buf, MAXRECV );

                cli_cmd = "";
                cli_cmd.append(cmd_buf);
                memset(cmd_buf, 0, MAXRECV + 1);
                string msg = "";
                msg.append(get_time_date());
                msg.append(" - Incoming Command from: ");
                msg.append(IPADD);
                WriteLogFile(msg, logfile);
                if (n < 0) {
                    string msg = "";
                    msg.append(get_time_date());
                    msg.append(" - Error reading from socket.");
                    WriteLogFile(msg, logfile);
                }
				bool isAllowed = false;
				unsigned int ctr = 0;
                while (ctr < IPACL.size()){
					string::size_type found = IPACL[ctr].find( "/", 0 );
					if (found != string::npos){
                        isAllowed = do_test(IPADD.c_str(), IPACL[ctr].c_str(), logfile, debug);
						if (isAllowed)
							break;
					}else{
						if (IPADD == IPACL[ctr].c_str()){
							isAllowed = true;
								break;
						}
					}
					ctr++;
				}

				if (isAllowed){
					// Update the log
					string msg = "";
					msg.append(get_time_date());
					msg.append(" - Command Sent: ");
					msg.append(cli_cmd);
					WriteLogFile(msg, logfile);
					pid_t cpid = fork();
					switch(cpid){
						case -1:
							break;
						case 0:
						{
							pid_t gcpid = fork();
							switch(gcpid){
								case -1:
									break;
								case 0:
                                    do_tcmd(cli_cmd, cli_sock, logfile);
									break;
								default:
									exit(0);
									break;
							}
						}
							break;
						default:
							waitpid(cpid, NULL, 0);
							break;
					}
				}else{
					string msg = get_time_date();
					msg.append(" - This host (");
					msg.append(IPADD);
					msg.append(") is not authorized.\n");
					WriteLogFile(msg, logfile);
					msg = "Current ACL's prohibit this client (";
					msg.append(IPADD);
					msg.append(") from accessing rcommd!");
					memset ( cmd_buf, 0, MAXRECV + 1);
					strcpy(cmd_buf,msg.c_str());
                    n = write(cli_sock,cmd_buf,strlen(cmd_buf));
                    if (n < 0) {
                        error((char *)"ERROR writing to socket");
                    }
                    memset ( cmd_buf, 0, MAXRECV + 1);
				}
			}catch ( ... ) {}

		}
	}catch ( ... ){
		if (debug){
			string msg = get_time_date();
			msg.append(" - Unknown exception was caught");
			WriteLogFile(msg, logfile);
		}
	}
	return 0;
}

void do_tcmd(string cmd, int cli_sock, string logfile) {
	// Start processing the command
	string send_data = "", start_time = get_time_date();
	int MAXBUF = 256;
	size_t n = 0;
	FILE *stream;
	char cmd_buff[MAXBUF];
	cmd.append(" 2>&1");
	time_t tm_start = Str2DateTime(start_time);
	stream = popen(cmd.c_str(), "r");
    pid_t proc_pid = getpid();
	if (!stream){
		exit(1);
	}
	string curr_time;
	double curr_dif = 0.00;
	memset(cmd_buff, 0, MAXBUF + 1);
	while ((fgets(cmd_buff, MAXBUF, stream) != NULL) && (curr_dif < 10.00)){
		curr_time = get_time_date();
		time_t tm_curr = Str2DateTime(curr_time);
		curr_dif = difftime (tm_curr,tm_start);
		n = write( cli_sock, cmd_buff, MAXBUF );
		if (n < 0) {
            string msg = "";
            msg.append(get_time_date());
            msg.append(" - ERROR writing to socket.");
            WriteLogFile(msg, logfile);
        }

	}
	pclose(stream);
    shutdown(cli_sock, 2);
    close(cli_sock);
    // Now kill the child process
	waitpid(proc_pid, NULL, 0);
	kill (proc_pid, 9);

}
