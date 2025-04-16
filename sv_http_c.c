#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <sys/types.h>
#include <microhttpd.h>

#define PORT 8080
#define MAX_RESPONSE_SIZE 1048576

//get process
char* getAllProcess(){
	char* response = malloc(MAX_RESPONSE_SIZE);
    strcpy(response, "List of processes:\n");

    DIR* dir = opendir("/proc");
    if (!dir) {
        strcat(response, "Cannot open /proc");
        return response;
    }	
	struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0) {
            char path[512];
            snprintf(path, sizeof(path), "/proc/%.200s/comm", entry->d_name);
            FILE* fp = fopen(path, "r");
            if (fp) {
                char comm[512];
                if (fgets(comm, sizeof(comm), fp)) {
                    comm[strcspn(comm, "\n")] = 0; // Xóa ký tự xuống dòng
                    char line[512];
                    snprintf(line, sizeof(line), "PID: %.200s, Name: %.200s\n", entry->d_name, comm);
                    strcat(response, line);
                }
                fclose(fp);
            }
        }
    }
    closedir(dir);
	return response;
}

// request handle
static enum MHD_Result requestHandle(void* cls, struct MHD_Connection* connection, const char* url,
                   const char* method, const char* version, const char* upload_data,
                   size_t* upload_data_size, void** ptr)
{
	static int dummy;
	if(&dummy != *ptr){
		*ptr = &dummy;
		return MHD_YES;
	}
	
	struct MHD_Response* response;
	enum MHD_Result ret;
	char* content;
	
	//route show process
	if(strcmp(url,"/show_all_process") == 0 && strcmp(method,"GET") == 0){
		content = getAllProcess();
		response = MHD_create_response_from_buffer(strlen(content),content,MHD_RESPMEM_MUST_FREE);
		ret = MHD_queue_response(connection,MHD_HTTP_OK,response);
		MHD_destroy_response(response);
		return ret;
	}
	
	//route kill process(id)
	if(strncmp(url,"/kill_process/",14) == 0 && strcmp(method,"POST") == 0){
		const char* pid_str = url+14;
		pid_t pid = atoi(pid_str);
		if(pid <=0){
			content = strdup("invalid");
			response = MHD_create_response_from_buffer(strlen(content),content,MHD_RESPMEM_MUST_FREE);
			ret = MHD_queue_response(connection,MHD_HTTP_BAD_REQUEST,response);
			MHD_destroy_response(response);
			return ret;
		}
		if(kill(pid,SIGTERM) == 0){
			content = malloc(256);
			snprintf(content,256,"Process %d terminated successfully",pid);
			response = MHD_create_response_from_buffer(strlen(content),content,MHD_RESPMEM_MUST_FREE);
			ret = MHD_queue_response(connection,MHD_HTTP_OK,response);
		}
		else{
			content = strdup("fail to terminate process");
			response = MHD_create_response_from_buffer(strlen(content),content,MHD_RESPMEM_MUST_FREE);
			ret = MHD_queue_response(connection,MHD_HTTP_INTERNAL_SERVER_ERROR,response);
		}
		MHD_destroy_response(response);
		return ret;
	}
	content = strdup("Not found");
    response = MHD_create_response_from_buffer(strlen(content), content, MHD_RESPMEM_MUST_FREE);
    ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);
    printf("a");
    return ret;
	
}


int main(){
	struct MHD_Daemon* daemon;
	daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY,PORT,NULL,NULL,&requestHandle,NULL,MHD_OPTION_END);
	if(!daemon){
		fprintf(stderr,"failed to start");
		return 1;
	}
	printf("sv run on port %d \n",PORT);
	printf("Try: curl http://localhost:%d/show_all_process\n", PORT);
    printf("Try: curl -X POST http://localhost:%d/kill_process/<pid>\n", PORT);
    getchar();
    MHD_stop_daemon(daemon);
	return 0;
}