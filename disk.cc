#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <vector>
#include <ctype.h>
#include <stdio.h>
#include <sstream>
#include <algorithm>
#include <map>
#include <utility>
#include <set>
#include "thread.h"
#include <cstdlib>

using namespace std;

int j = 1;
int numSleeping = 0;
int numExited = 0;
int argSize = 0;
vector<string> argFiles;
vector<int> diskQueue;
vector<int> diskQueueId;
int threadNum = 0; //id of requester
typedef map<int, set<string> >::const_iterator MapIterator;

//parses words in a string delimited by non-alpha characters and stores them in a vector
void tokenize(const string& str, vector<string>& tokens, const string& delimiters = " 1234567890")
{
    // Skip delimiters at beginning.
    string::size_type lastPos = str.find_first_of(delimiters, 0);
    // Find first "non-delimiter".
    string::size_type pos = str.find_first_not_of(delimiters, lastPos);
    while (string::npos != pos || string::npos != lastPos)
    {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_not_of(delimiters, lastPos);
    }
}

void requester(void *a){
	int k = 0; 
	std::string str, strFile, buf; 
	std::vector<string> tempContents;

	struct fileContents {
		int id;
		std::vector<int> tracks;
	} requester;
	
	requester.id = (intptr_t) a; 
	//threadNum++;
	std::ifstream file(argFiles[j].c_str()); //opens the file dictated by the argument given
	j++;
	if (!file.is_open()){
		//cout << "Can not open input file" << endl;
	}
	else {
		//cout << "Opened input file" << endl;
		while (!file.eof()){ //while there are still contents to be read in the input txt file
			//gets the contents from each txt file line by line, delimits the words by spaces, and store in tokens1 vector
			std::getline(file, str);
			tokenize(str, tempContents);
			int match;
			cout << "k: " << k << " tempContents: " << tempContents.size() << endl;
			while (k < tempContents.size()) {
				match = 0;				
				requester.tracks.push_back(atoi(tempContents[k].c_str()));
				cout << "id: " << requester.id << " " << tempContents[k] << endl;
				for (int i = 0; i < diskQueueId.size(); i++) {
					if (requester.id == diskQueueId[i]) {
						match = 1;
						thread_lock(1);
						cout << "requster id: " << requester.id << endl;
						thread_wait(1,(requester.id+1));
						thread_unlock(1);
					}
				}
				thread_lock(1);
				while (diskQueue.size() >= atoi(argFiles[0].c_str())) { //while disk queue is full, wait
					thread_broadcast(1,0);
					cout << "full" << endl;
					thread_wait(1,0);
					//cout << diskQueue.size() << " " << requester.id << " " << atoi(argFiles[0].c_str()) << endl;
				}
				diskQueue.push_back(requester.tracks[k]);
				diskQueueId.push_back(requester.id);
				cout << "requester " << requester.id << " track " << requester.tracks[k] << endl;
				k++;
				thread_broadcast(1,0);
				thread_unlock(1);
			}
			cout << "Hello" << endl;
		}
		thread_lock(1);
		cout << "end id: " << requester.id << endl;
		thread_wait(1,(requester.id+1));
		thread_unlock(1);
		thread_broadcast(1,0);
		//thread_signal(4,0);
		cout << "Thread " << requester.id << " exiting" << endl;
		//numSleeping++;
		argSize--;
		threadNum--;
		numExited++;
	}
}

void servicer(void *a){
	int currentPos = 0;
	while (true) {
		if (atoi(argFiles[0].c_str()) > argSize) {
			stringstream ss;
			ss << argSize;
			string buffer;
			ss >> buffer;
			argFiles[0] = buffer;
		}
		//sleep(1);
		//cout << "# of threads sleeping: " << numSleeping << endl;
		//cout << "Disk Queue size: " << diskQueue.size() << endl;
		//cout << "threadNum: " << threadNum << endl;
		cout << "argFiles: " << argFiles[0] << " disk queue size: " << diskQueue.size() << endl;
		thread_lock(1);
		if (diskQueue.size() == 0 && (atoi(argFiles[0].c_str())) == 0) {
			thread_unlock(1);
			break;
		}
		if (diskQueue.size() != (atoi(argFiles[0].c_str()))) {
			thread_wait(1,0);
		}
		if (diskQueue.size() >= atoi(argFiles[0].c_str())) {
			int smallest = 1000;
			int smallestIndex = 0;
			int temp = 0;
			/*for (int o = 0; o < diskQueue.size(); o++) {
				cout << "Contents: " << diskQueue[o] << endl;
			}*/
			for (int a = 0; a < diskQueue.size(); a++) {
				if (abs(currentPos - diskQueue[a]) <= (double) smallest) {
					//cout << "If statement" << endl;
					smallest = abs(currentPos - diskQueue[a]);
					smallestIndex = a;
					temp = diskQueue[a];
				}
			}
			currentPos = temp;
			cout << "service requester " << diskQueueId[smallestIndex] << " track " << currentPos << endl;
			thread_broadcast(1,0);
			thread_broadcast(1,(diskQueueId[smallestIndex]+1));
			//cout << "broadcast id: " << diskQueueId[smallestIndex] + 1<< endl;
			diskQueue.erase(diskQueue.begin()+(smallestIndex));
			diskQueueId.erase(diskQueueId.begin()+(smallestIndex));			
		}
		while (threadNum == 0 && diskQueue.size() != 0) {
			int smallest = 1000;
			int smallestIndex = 0;
			int temp = 0;
			/*for (int o = 0; o < diskQueue.size(); o++) {
				cout << "Contents: " << diskQueue[o] << endl;
			}*/
			for (int a = 0; a < diskQueue.size(); a++) {
				if (abs(currentPos - diskQueue[a]) <= (double) smallest) {
					smallest = abs(currentPos - diskQueue[a]);
					smallestIndex = a;
					temp = diskQueue[a];
				}
			}
			currentPos = temp;
			thread_broadcast(3,diskQueueId[smallestIndex]);
			diskQueue.erase(diskQueue.begin()+(smallestIndex));
			diskQueueId.erase(diskQueueId.begin()+(smallestIndex));
			cout << "service requester " << diskQueueId[smallestIndex] << " track " << currentPos << endl;
			if (diskQueue.size() == 0) {
				break;
			}
		}
		if (numSleeping == argSize) {
			//cout << "all wake up" << endl;
			thread_broadcast(1,0);
			numSleeping = 0;
		}
		thread_unlock(1);
		
	}
	//cout << "servicer" << endl;
}

void parent(void *argc){
	int fileArg = 2; //2 because the first two arguments are ./disk and max disk queue; 2 is the first input file
	int a = (intptr_t) argc;
	while (fileArg < a){ //creates a thread for every input file
		if (thread_create( (thread_startfunc_t) requester, (void *) (intptr_t) (fileArg - 2))) {
			//cout << "thread_libinit failed\n";
			exit(1);
	    	}
		fileArg++;
	}
	servicer((void *) 5);
}

//input one argument (input file that contains other .txt files)
int main(int argc, char *argv[]){
	start_preemptions(false, true, 123);
	if (argc <= 2) {
		exit(0);
	}
	
	if (argc > 2) {
		argFiles.assign(argv + 1, argv + argc);
	}
	
	argSize = argFiles.size()-1;
	
	threadNum = argSize;

	if (thread_libinit( (thread_startfunc_t) parent, (void *) (intptr_t) argc)) {
		//cout << "thread_libinit failed\n";
		exit(1);
	}
}		
