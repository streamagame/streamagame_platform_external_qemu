#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
  #include <windows.h>
  #include <tchar.h>
#else
  #include <time.h>
  #include <errno.h>

  #include <pthread.h>
#endif


/**** How to execute a command and get output of command within C++
 **** http://stackoverflow.com/questions/478898/how-to-execute-a-command-and-get-output-of-command-within-c
 ****/
#define VBOXMANAGE_START_VM_CMD "VBoxManage startvm androidx86 --type headless"
#define VBOXMANAGE_STOP_VM_CMD "VBoxManage controlvm androidx86 poweroff"
#define VBOXMANAGE_GET_MANAGEMENT_IP_CMD "VBoxManage guestproperty get androidx86 androvm_ip_management"
#define VBOXMANAGE_START_VM_CMD_EXPECTED_OUTPUT "has been successfully started"
#define VBOXMANAGE_STOP_VM_CMD_EXPECTED_OUTPUT "0%...10%...20%...30%...40%...50%...60%...70%...80%...90%...100%"
#define VBOXMANAGE_GET_MANAGEMENT_IP_KEY "Value: "
#define VBOXMANAGE_STOP_VM_CMD "VBoxManage controlvm androidx86 poweroff"

static void my_sleep(unsigned msec) {

#ifdef WIN32
                     Sleep(msec);
#else

    struct timespec req, rem;
    int err;
    req.tv_sec = msec / 1000;
    req.tv_nsec = (msec % 1000) * 1000000;
    while ((req.tv_sec != 0) || (req.tv_nsec != 0)) {
        if (nanosleep(&req, &rem) == 0)
            break;
        err = errno;
        // Interrupted; continue
        if (err == EINTR) {
            req.tv_sec = rem.tv_sec;
            req.tv_nsec = rem.tv_nsec;
        }
        // Unhandleable error (EFAULT (bad pointer), EINVAL (bad timeval in tv_nsec), or ENOSYS (function not supported))
        break;
    }
#endif
}

/** reads string value from key-value pair */
bool strValFromKey(std::string & val, const std::string key, const std::string src)
{
        std::size_t startPos;
        std::size_t endPos;
        printf("strValFromKey key %s str %s...\n", key.c_str(), src.c_str());
        if((startPos = src.find(key)) != std::string::npos)
        {
            startPos += key.length();
            endPos = src.length();
            val = std::string(src, startPos, endPos).c_str();
            printf("startPos %d endPos %d val %s \n", startPos, endPos, val.c_str());
            return true;
        }
        return false;
}

std::string exec(char* cmd) {
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "ERROR";
    char buffer[128];
    std::string result = "";
    while(!feof(pipe)) {
        if(fgets(buffer, 128, pipe) != NULL)
                result += buffer;
    }
    pclose(pipe);
    return result;
}

extern "C" const char * get_vm_ip(){
    std::string result;
    std::string vm_ip_str;
        while(1){
		result = exec(VBOXMANAGE_GET_MANAGEMENT_IP_CMD);

        	printf("VBOXMANAGE_GET_MANAGEMENT_IP_CMD return %s \n", result.c_str());
        	if(strValFromKey(vm_ip_str, VBOXMANAGE_GET_MANAGEMENT_IP_KEY, result)){
            		printf("Got Management IP Address %s ...\n", vm_ip_str.c_str());
			break;
       		}
		else{
			my_sleep(1000);		
		}
	}	

    printf("get_vm_ip returning IP Address %s ...\n", vm_ip_str.c_str());
    return vm_ip_str.c_str();
}

extern "C" void stop_vm(){
    exec(VBOXMANAGE_STOP_VM_CMD);
}

extern "C" void start_vm(){
    std::string result;
    result = exec(VBOXMANAGE_START_VM_CMD);
    printf("VBOXMANAGE_START_VM_CMD return %s \n", result.c_str());
    if(result.find(VBOXMANAGE_START_VM_CMD_EXPECTED_OUTPUT) != std::string::npos){
   	printf("VM started successfully \n"); 
    }
    else{
        printf("VM failed to start \n");
	exit(1);
    }
}

