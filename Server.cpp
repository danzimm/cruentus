#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <fstream>
#include <time.h>
#include "Socket.h"
#include "Server.h"

using namespace std;

static bool utility_ = false;
static bool crux_ = false;
static bool logging_ = false;

extension extensions[] = {
        {(char *)".j", (char *)"text/javascript"},
        {(char*)".gif", (char*)"image/gif" },
        {(char*)".jpg", (char*)"image/jpeg"},
        {(char*)".jpeg",(char*)"image/jpeg"},
        {(char*)".png", (char*)"image/png" },
        {(char*)".zip", (char*)"image/zip" },
        {(char*)".gz",  (char*)"image/gz"  },
        {(char*)".tar", (char*)"image/tar" },
        {(char*)".htm", (char*)"text/html" },
        {(char*)".html",(char*)"text/html" },
        {(char *)".css", (char *)"text/css"},
        {(char *)".js", (char *)"text/javascript"},
        {(char *)".mp4", (char *)"video/mp4"},
        {(char *)".tiff", (char *)"image/tiff"},
        {(char *)".pdf", (char *)"application/pdf"},
        {0,0} };

unsigned int HTMLCRUX = 0;
unsigned int SOCKCRUX = 1;

cruxExt cruxExtenstions[]= {
	{(char *)".html", HTMLCRUX},
	{(char *)".hcrux", HTMLCRUX},
	{(char *)".crux", SOCKCRUX},
	{0,0} };


static vector<serverPath> *servers = new vector<serverPath>();

void server_setUtility(bool ut) {
	utility_ = ut;
}
void server_setCrux(bool crux) {
	crux_ = crux;
}

void server_setLogging(bool log) {
	logging_ = log;
}

void server_readConf(const char *path) {
	
	ifstream in(path);
	char *data = new char[1024];
	
	while (in) {
		in.getline(data, 1024);
		if (!in || data[0] == 0)
			continue;
		string dd(data);
		size_t first = dd.find("<*server*>");
		size_t second = dd.find("<*server*>", first+11/*lengthof <*server*> ++*/);
		string stuff = dd.substr(first+10, (second - (first+10)));
		size_t sppl = stuff.find(" ");
		string domain = stuff.substr(0, sppl);
		string path = stuff.substr(sppl+1);
		serverPath server;
		server.domain = domain;	
		server.path = path;
		servers->push_back(server);
#ifdef DEBUG
		cout << "Found server " << server.domain << " with path " << server.path << endl;
#endif	
	}
	delete data;
	in.close();
}


void replace(string &str, const string &find_what, const string &replace_with)
{
	string::size_type pos=0;
	while((pos=str.find(find_what, pos))!=string::npos)
	{
		str.erase(pos, find_what.length());
		str.insert(pos, replace_with);
		pos+=replace_with.length();
	}
}

void *server(void *socket) {
	int sock = *((int*)socket);
	Socket *asock_= new Socket(sock);
#ifdef DEBUGSOCK
	cout << pthread_self() << ": Got socket " << sock << endl;
#endif
	int bufsize=1024;
	char *buffer = (char *)calloc(1,bufsize);
	recv(sock,buffer,bufsize,0);
	if (logging_) {
		time_t rtime;
		time(&rtime);
		string ts = string(ctime(&rtime));
		cout << "[" << ts.substr(0,ts.length()-1) << "] Request:" << endl << buffer << endl << endl;
	}
#ifdef DEBUG
	cout << "Request:" << endl << buffer << endl<<"Done"<<endl;
#endif
	//get the url they want.....
	if (strncmp(buffer, "GET ", 4) && strncmp(buffer,"get ",4)) {
		asock_->send(string("Sorry only GET is allowed atm"));
		close(sock);
		delete asock_;
		free(buffer);
		return NULL;
	}		
	
	string request(buffer);

	if (servers->size() > 0) {
		string aserver(request);
		size_t hostp = aserver.find("Host:");
		size_t newlp = aserver.find("\n", hostp);
		if (hostp == string::npos || newlp == string::npos) 
			goto skipWho;
		aserver = aserver.substr(hostp, newlp-hostp);
		aserver = aserver.substr(5);
		if (aserver[0] == ' ')
			aserver = aserver.substr(1);
		if (aserver.find("aboutcrux") == 0) {
			free(buffer);
			asock_->send(string("HTTP/1.1 404 Not Found\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n<html><body>Cruentus:<br/>The bloodiest web server out there<br/>Version 0.1a</body></html>"));
			close(sock);
			delete asock_;
			return NULL;
		}	
		size_t matches = 0;
		string path("./");
		for (unsigned int i = 0; i < servers->size(); ++i) {
			size_t matchest = aserver.find((*servers)[i].domain);
			if (matchest == string::npos && (*servers)[i].domain.compare("*") != 0)
				continue;
			size_t matchl = (*servers)[i].domain.length();
			if (matchl > matches) {
				path = (*servers)[i].path;
				matches = matchl;
			}
		}
		if (chdir(path.c_str()) != 0) {
			cout << "Failed to chdir(" << path << ")" << endl;
		}
	}
skipWho:
	request = request.substr(4);
	size_t pos = request.find(" HTTP/1");
	//NOTE MUST ADD IS NPOS
	request = request.substr(0,pos);
	
#ifdef DEBUGFILE
	cout << "Requesting " << request << endl;
#endif
	if (!utility_ && strcmp(request.c_str(), "/") == 0)
		request = "/index.html";
	else if (!utility_ && strncmp(request.c_str(), "/?", 2) == 0)
		request.insert(1, string("index.html"));
#ifdef DEBUG
	cout << "Trying to get " << request << endl;
#endif 
	struct stat st;
	string requestp = "";
	if (crux_) {
		size_t posp = request.find("?");
		if (posp == string::npos)
			goto skipcrux;
		string requestparam = request.substr(posp+1);
		size_t space = 0;
		while (space != string::npos) {
			space = requestparam.find("%20");
			if (space != string::npos)
				requestparam.replace(space, 3, " ");
		}
		requestp = requestparam;
		request.erase(posp);
	}
skipcrux:
	request.insert(0,string("."));

	stat(request.c_str(), &st);

	if (!utility_ && S_ISDIR(st.st_mode) && !(crux_ && request.find(".crux")!=string::npos)) {
		if (request[request.length()-1] == '/')
			request += "index.html";
		else
			request += "/index.html";
	}

	
	if ((stat(request.c_str(), &st) != 0 || S_ISDIR(st.st_mode)) && !(crux_ && request.find(".crux")!=string::npos)) {
#ifdef DEBUGFAIL
		cout << "failed to get resource " << request << endl;
#endif
		if (!utility_) {
			asock_->send(string("HTTP/1.1 404 Not Found\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n<html><body>NotFound<br/>Goodbye World<br/></body></html>"));
		} else {
		
			DIR *dp;
			struct dirent *ep;     
			dp = opendir (request.c_str());
			if (dp != NULL) {
				asock_->send(string("HTTP/1.1 200 OK\r\n"));
				asock_->send(string("Content-Type: text/html; charset=UTF-8\r\n\r\n"));
				asock_->send(string("<html><body>"));
				while ((ep = readdir (dp)) != NULL) {
					asock_->send(string("<a href=./"));
					asock_->send(string(ep->d_name));
					if (ep->d_type == DT_DIR) {
						asock_->send(string("/"));
					}
					asock_->send(string(">"));
					asock_->send(string(ep->d_name));
					asock_->send(string("</a><br/>"));
				}	
				asock_->send(string("</body></html>"));
				closedir (dp);
			} else {
#ifdef DEBUGFAIL
				cout << "Failed to get resource dir" << endl;
#endif
				asock_->send(string("HTTP/1.1 404 Not Found\r\n"));
				asock_->send(string("Content-Type: text/html; charset=UTF-8\r\n\r\n"));
				asock_->send(string("<html><body>Goodbye World</body></html>"));
			}	
		}
	
	} else {
		size_t leng = request.length();
		char *fstr = (char *)0;
		for(int i=0;extensions[i].ext != 0;i++) {
			size_t len = strlen(extensions[i].ext);
			if (!strncmp(request.substr(leng-len).c_str(), extensions[i].ext, len)) {
				fstr =extensions[i].filetype;
				break;
			}
		}
			
		string header("HTTP/1.1 200 OK\r\n");
		
		time_t rawtime;
		time(&rawtime);
		header += "Date: ";
		header += ctime(&rawtime);

		if (fstr == 0) {
#ifdef DEBUG
			cout << "Didn't find a file type.... " << endl;
#endif
			header += "Content-Type: text/plain; charset=UTF-8\r\n";
		} else {
			header += "Content-Type: ";
			header += fstr;
			header += "; charset=UTF-8\r\n";
			if (strcmp(fstr, "text/html") && strcmp(fstr, "text/plain")) {
				header += "Content-Disposition: attachment\r\n";
			}
		}
		size_t currentCrux = -1;
		if (crux_) {
			int cruxType = HTMLCRUX;
			for (int i=0;cruxExtenstions[i].ext != 0;i++) {
				size_t len = strlen(cruxExtenstions[i].ext);
				if (!strncmp(request.substr(leng-len).c_str(), cruxExtenstions[i].ext, len)) {
                                	cruxType =cruxExtenstions[i].type;
                                	break;
                        	}
			}
			switch (cruxType) {
				case 1:{
					Socket *unSock = new Socket(AF_UNIX, SOCK_STREAM, 0);
					string tmpr(request);
					if (tmpr[tmpr.length()-1]=='/')
						tmpr+="sock";
					else
						tmpr+="/sock";
					unSock->connect((char *)tmpr.c_str());
					unSock->send(requestp);
					int bs = 1024;
					char *abs = (char *)calloc(1,bs);
					int rt = recv(*(unSock->socket_),abs,bs,0);
					if (rt <= 0) {
						asock_->send(string("HTTP/1.1 404 Not Found\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n<html><body>Goodbye World</body></html>"));
					} else {
						asock_->send(string(abs));
					}
					delete unSock;
					free(abs);
					break;
				}
				default:
				case 0:{
					header+= "\r\n\r\n";
					asock_->send(header);
					ifstream fstream(request.c_str(),ifstream::in);
					size_t place = 0;
					char ptest[255];
					while (!fstream.eof()) {
						fstream.getline(ptest, 255);
						if (!fstream || ptest[0]==0)
							continue;
						string sptest(ptest);
						size_t aplace = 0;
						while (aplace != string::npos) {
							place = aplace = sptest.find("<*", aplace);
							if (place != string::npos) {
								size_t place2 = sptest.find("*>",aplace);
								if (place2 != string::npos) {
									int dp = (place2) - (place + 2);
									size_t crpl = requestp.find("?", ++currentCrux);
									string arequestp("");
									if (crpl != string::npos) {
										arequestp = requestp.substr(currentCrux, crpl-currentCrux);
										currentCrux = crpl;
									} else if (requestp.find("?", currentCrux+1) == string::npos) {
										arequestp = requestp.substr(currentCrux);
										currentCrux = requestp.length()-1;
									}
									if (dp > 0) {
										string def = sptest.substr(place+2,dp);
#ifdef DEBUG
										cout << "Default is " <<def << endl;
#endif	
										sptest.erase(place, (place2+2)-place);
										sptest.insert(place, (arequestp.length()>0 ? arequestp : def));
									} else {
										sptest.erase(place, 3);
										sptest.insert(place, arequestp);
									}
								}
							}
						}
						asock_->send(sptest);
					}
					fstream.close();
					break;
				}
			};
		} else {
			header += "Content-Length: ";
		
			ostringstream result;
			result << st.st_size;
			header += result.str();
	
			header += "\r\n\r\n";
#ifdef DEBUGFILE
			cout << "Sending " << header << " for " << request << endl;
#endif
		
			asock_->send(header);
	
			asock_->sendFile(request);

		}
	
	}
#ifdef DEBUGSOCK
	cout << pthread_self() << ": Closing socket" << sock << endl;
#endif
#ifndef TESTSOCK
	close(sock);
#endif
	delete asock_;
	free(buffer);
	return NULL;
}


