#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <fstream>

#define BUFF_SZ sizeof ( int ) * 10
const int SHMKEY1 = 4201069;
const int SHMKEY2 = 4201070;
const int PERMS = 0644;
using namespace std;

int shmid1;
int shmid2; 
int *nano;
int *sec;

struct PCB{

  int occupied; // either true or false
  pid_t pid; // process id of this child
  int startSeconds; // time when it was forked
  int startNano; // time when it was forked
};

//message buffer
struct msgbuffer {
   long mtype; //needed
   int sec; //stores time to kill sec
   int nano; //stores time to kill nano sec
};

void myTimerHandler(int dummy) {
  
  shmctl( shmid1, IPC_RMID, NULL ); // Free shared memory segment shm_id
  shmctl( shmid2, IPC_RMID, NULL ); 
  cout << "Oss has been running for 60 seconds! Freeing shared memory before exiting" << endl;
  cout << "Shared memory detached" << endl;
  kill(0, SIGKILL);
  exit(1);

}

static int setupinterrupt(void) { /* set up myhandler for SIGPROF */
  struct sigaction act;
  act.sa_handler = myTimerHandler;
  act.sa_flags = 0;
  return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

static int setupitimer(void) { /* set ITIMER_PROF for 60-second intervals */
  struct itimerval value;
  value.it_interval.tv_sec = 60;
  value.it_interval.tv_usec = 0;
  value.it_value = value.it_interval;
  return (setitimer(ITIMER_PROF, &value, NULL));  
}



void myHandler(int dummy) {
    shmctl( shmid1, IPC_RMID, NULL ); // Free shared memory segment shm_id
    shmctl( shmid2, IPC_RMID, NULL ); 
    cout << "Ctrl-C detected! Freeing shared memory before exiting" << endl;
    cout << "Shared memory detached" << endl;
    exit(1);
}

void initClock(int check) {

  
  if (check == 1) {
    shmid1 = shmget ( SHMKEY1, BUFF_SZ, 0777 | IPC_CREAT );
    if ( shmid1 == -1 ) {
      fprintf(stderr,"Error in shmget ...\n");
      exit (1);
    }
    shmid2 = shmget ( SHMKEY2, BUFF_SZ, 0777 | IPC_CREAT );
    if ( shmid2 == -1 ) {
      fprintf(stderr,"Error in shmget ...\n");
      exit (1);
    }
    cout << "Shared memory created" << endl;
    // Get the pointer to shared block
    sec = ( int * )( shmat ( shmid1, 0, 0 ) );
    nano = ( int * )( shmat ( shmid2, 0, 0 ) );
    *sec = 0;
    *nano = 0;
    return;
  }
//detaches shared memory
  else {
    shmdt(sec);    // Detach from the shared memory segment
    shmdt(nano);
    shmctl( shmid1, IPC_RMID, NULL ); // Free shared memory segment shm_id
    shmctl( shmid2, IPC_RMID, NULL ); 
    cout << "Shared memory detached" << endl;
    return;
  }
  
}

void incrementClock() {

  int * clockSec = ( int * )( shmat ( shmid1, 0, 0 ) );
  int * clockNano = ( int * )( shmat ( shmid2, 0, 0 ) );
  
  *clockNano = *clockNano + 10000;
  if (*clockNano >= 1000000000) {
    *clockNano = *clockNano - 1000000000;
    *clockSec = *clockSec + 1;
  }
  shmdt(clockSec);
  shmdt(clockNano);
  return;
  
}

int main(int argc, char *argv[])  {

//Ctrl-C handler
  signal(SIGINT, myHandler);
  if (setupinterrupt() == -1) {
    perror("Failed to set up handler for SIGPROF");
    return 1;  
  }
  if (setupitimer() == -1) {
    perror("Failed to set up the ITIMER_PROF interval timer");
    return 1;
  }


//Had issues with default selection in switch decided to have an argc catch at the beginning to insure that more than one option is given
  if (argc == 1) {
  
    cout << "Error! No parameters given, enter ./oss -h for how to operate this program" << endl;
    exit(1);

  }
  
  int opt, optCounter = 0;
  int nValue, sValue, tValue, status;
  string fValue;

//opt function to collect command line params  
  while ((opt = getopt ( argc, argv, "hr:n:s:t:f:" ) ) != -1) {
    
    optCounter++;
    
    switch(opt) {
    
      case 'h':
        cout << "Usage: ./oss -n proc -s simul -t time" << endl;
        cout << "-n: The number of child processes to be made." << endl;
        cout << "-s: The number of simultaneous child processes. This number cannot be higher than 18." << endl;
        cout << "-t: The upper bound of the random number to be generated per child. [i.e. 10 would give you and number between 0 and 9]." << endl;
        cout << "-f: The name of the file the program will write to for logging." << endl;
        exit(1);
      
      case 'n':
        nValue = atoi(optarg);
        if (nValue < 0) {
          while(nValue < 0) {
            cout << "Error! -n value was a negative number! Please provide a number equal to or greater than 0: ";
            cin >> nValue;
          }
        }
        break;
        
      case 's':
        sValue = atoi(optarg);
        if (sValue < 0 || sValue > 18) {
          while(sValue < 0 || sValue > 18) {
            cout << "Error! -s value was a negative number or greater than 18! Please provide a number equal to or greater than 0 and less than 19: ";
            cin >> sValue;
          }
        }
        break;
      
      case 't':
        tValue = atoi(optarg);
        if (tValue < 0) {
          while(tValue < 0) {
            cout << "Error! -t value was a negative number! Please provide a number equal to or greater than 0: ";
            cin >> tValue;
          }
        }
        break;
        
        case 'f':
          fValue = optarg;
          break;
    }
  
  }
  if (( argc > 1) && (optCounter < (4))) { 
  
        cout << "Error! There are no defaults for this program! One of the parameters was not set! Please use -h for proper usage of .oss" << endl;
        exit(1);
        
    } 
    
//setup logfile
  ofstream logFile(fValue.c_str());
    
//Creates seed for random gen
    int sec = 0, nano = 0;
    srand((unsigned) time(NULL));
    initClock(1);
    
//create Message queue
  struct msgbuffer buf;
  int msqid;
  key_t key;
  
  if ((key = ftok("oss.cpp", 'B')) == -1) {
      perror("ftok");
      exit(1);
   }
   
   if ((msqid = msgget(key, PERMS | IPC_CREAT)) == -1) {
      perror("msgget");
      exit(1);
   }
   
     buf.mtype = 1;

//startWhile var makes sure the while loop creates the first children before doing the first waitpid
  int waitCheck, tableIndex = 0, startWhile = 0, halfSecCheck = 0, counter = 0;
  struct PCB processTable[nValue];
//init all PCB values to 0 for better looking output
  for (int i = 0; i < (*(&processTable + 1) - processTable); i++) {
    processTable[i].occupied = 0;
    processTable[i].pid = 0;
    processTable[i].startSeconds = 0;
    processTable[i].startNano = 0;
  }
  int * clockSec = ( int * )( shmat ( shmid1, 0, 0 ) );
  int * clockNano = ( int * )( shmat ( shmid2, 0, 0 ) );
  pid_t pid;
  
  while(nValue > 0) {
    incrementClock();
    if (*clockNano == 500000000) {
      halfSecCheck = 1;
    }
    if (halfSecCheck == 1) {
      cout << "OSS PID: " << getpid() << " SysClockS: " << *clockSec << " SysClockNano: " << *clockNano << endl;
      logFile << "OSS PID: " << getpid() << " SysClockS: " << *clockSec << " SysClockNano: " << *clockNano << endl;
      cout << "Process Table: " << endl;
      logFile << "Process Table: " << endl;
      cout << "Entry Occupied PID   StartS StartN" << endl;
      logFile << "Entry Occupied PID   StartS StartN" << endl;
      for (int i = 0; i < (*(&processTable + 1) - processTable); i++) {
        cout << i << "     " << processTable[i].occupied << "        " << processTable[i].pid << " " << processTable[i].startSeconds << "      " << processTable[i].startNano << endl;
        logFile << i << "     " << processTable[i].occupied << "        " << processTable[i].pid << " " << processTable[i].startSeconds << "      " << processTable[i].startNano << endl;
      }
      halfSecCheck = 0;
    }
    if(startWhile > 0) {
      waitCheck = waitpid(-1, &status, WNOHANG);

    }
    
    if(tableIndex < sValue) {
      startWhile++;
      sec = sec + rand() % (tValue - 1);
      nano = rand() % 1000000000;
      buf.sec = sec;
      buf.nano = nano;
      char *args[]={"./worker", NULL};
      nValue--;
      pid = fork();
      if(pid == 0) {
        if (msgsnd(msqid, &buf, 20, 0) == -1)
          perror("msgsnd");
        execvp(args[0],args);
        cout << "Exec failed! Terminating" << endl;
        logFile << "Exec failed! Terminating" << endl;   
        exit(1);   
      }
      else {
        processTable[tableIndex].occupied = 1;
        processTable[tableIndex].pid = pid;
        processTable[tableIndex].startSeconds = *clockSec;
        processTable[tableIndex].startNano = *clockNano;
        tableIndex++;
        }
    }
    
    if (waitCheck > 0) {
      for(int i = 0; i < (*(&processTable + 1) - processTable); i++) {
        if( processTable[i].pid == waitCheck) {
          processTable[i].occupied = 0;
        }
      }
      if (sValue = nValue) {
        sec = sec + rand() % (tValue - 1);
        nano = rand() % 1000000000;
        buf.sec = sec;
        buf.nano = nano;
        char *args[]={"./worker", NULL};
        nValue--;
        sValue--;
        pid = fork();
        if(pid == 0) {
          if (msgsnd(msqid, &buf, 20, 0) == -1)
            perror("msgsnd");
          execvp(args[0],args);
          cout << "Exec failed! Terminating" << endl;
          logFile << "Exec failed! Terminating" << endl;
          exit(1);   
      }
      else {
        processTable[tableIndex].occupied = 1;
        processTable[tableIndex].pid = pid;
        processTable[tableIndex].startSeconds = *clockSec;
        processTable[tableIndex].startNano = *clockNano;
        tableIndex++;
        }
      }
    }
  }

//secondary waitpid loop to catch final child. Without this I got stuck *shrugs*
//occCheck insures that the leftover workers are unoccupied before closing
    int occCheck = 0;
    for(int i = 0; i < (*(&processTable + 1) - processTable); i++) {
        if (processTable[i].occupied == 1) {
          occCheck++;
        }
    }
    
  while(1) {
    incrementClock();
    if (*clockNano == 500000000) {
      halfSecCheck = 1;
    }
    if (halfSecCheck == 1) {
      cout << "OSS PID: " << getpid() << " SysClockS: " << *clockSec << " SysClockNano: " << *clockNano << endl;
      logFile << "OSS PID: " << getpid() << " SysClockS: " << *clockSec << " SysClockNano: " << *clockNano << endl;
      cout << "Process Table: " << endl;
      logFile << "Process Table: " << endl;
      cout << "Entry Occupied PID   StartS StartN" << endl;
      logFile << "Entry Occupied PID   StartS StartN" << endl;
      for (int i = 0; i < (*(&processTable + 1) - processTable); i++) {
        cout << i << "     " << processTable[i].occupied << "        " << processTable[i].pid << " " << processTable[i].startSeconds << "      " << processTable[i].startNano << endl;
        logFile << i << "     " << processTable[i].occupied << "        " << processTable[i].pid << " " << processTable[i].startSeconds << "      " << processTable[i].startNano << endl;
      }
      halfSecCheck = 0;
    }
    waitCheck = waitpid(-1, &status, WNOHANG);
    if (waitCheck > 0) {
      for(int i = 0; i < (*(&processTable + 1) - processTable); i++) {
        if (processTable[i].pid == waitCheck) {
          processTable[i].occupied = 0;
          occCheck--;
        }
      }
      if (occCheck == 0) {
        break;
      }
    }
  
  }
  
  //print PCB one more time to see that everything is not occupied
  cout << "OSS PID: " << getpid() << " SysClockS: " << *clockSec << " SysClockNano: " << *clockNano << endl;
  logFile << "OSS PID: " << getpid() << " SysClockS: " << *clockSec << " SysClockNano: " << *clockNano << endl;
  cout << "Process Table: " << endl;
  logFile << "Process Table: " << endl;
  cout << "Entry Occupied PID   StartS StartN" << endl;
  logFile << "Entry Occupied PID   StartS StartN" << endl;
  for (int i = 0; i < (*(&processTable + 1) - processTable); i++) {
    cout << i << "     " << processTable[i].occupied << "        " << processTable[i].pid << " " << processTable[i].startSeconds << "      " << processTable[i].startNano << endl;
    logFile << i << "     " << processTable[i].occupied << "        " << processTable[i].pid << " " << processTable[i].startSeconds << "      " << processTable[i].startNano << endl;
  }
  
//original solution did NOT work correctly sadge
  /*for (int i = 0; i < nValue; i++) {
    sec = sec + rand() % (tValue - 1);
    nano = rand() % 1000000000;
    char strSec[50];
    snprintf(strSec, sizeof(strSec), "%d", sec);
    char strNano[50];
    snprintf(strNano, sizeof(strNano), "%d", nano);
    char *args[]={"./worker", strSec, strNano, NULL};
    pid_t pid = fork();
    if(pid == 0) {
      processTable[i].occupied = 1;
      processTable[i].pid = pid;
      processTable[i].startSeconds = *clockSec;
      processTable[i].startNano = *clockNano;
      execvp(args[0],args);
      cout << "Exec failed! Terminating" << endl;
      exit(1);
    } 
    else  {
      while (1) {
        incrementClock();
        waitCheck = waitpid(-1, &status, WNOHANG);
        if(waitCheck > 0) {  
          if (sValue < nValue) {
              sValue++;
              break;
          }
         } 
        if(sValue == nValue) {
          break;
        }
      }
    }
  }
  */
//catch all wait() for when -s > -n and to ensure that all children still have a parent id
//while(wait(NULL) > 0);
  cout << "Oss finished" << endl;
  logFile << "Oss finished" << endl;
  shmdt(clockSec);
  shmdt(clockNano);
  if (msgctl(msqid, IPC_RMID, NULL) == -1) {
      perror("msgctl");
      exit(1);
   }
  initClock(0);
  return 0;
  
}