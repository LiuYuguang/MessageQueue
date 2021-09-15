#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include "mq.h"

#define Q 0x4000
/*
#define QUE_MSGTYPE 30l
typedef struct 
{
  long msgtype;	
  unsigned char buffer[0];
} MSGBUF;

int main(){
    int	msgqid;
	unsigned char* buf = NULL;
	MSGBUF* que_msgbuf;
    int len = 100;
    int retu;

	msgqid=msgget(Q,0666);//https://man7.org/linux/man-pages/man2/msgget.2.html
	assert(msgqid != -1);

    buf = malloc(sizeof(MSGBUF) + len);
    assert(buf!=NULL);

    que_msgbuf = (MSGBUF*)buf;
	que_msgbuf->msgtype = QUE_MSGTYPE;
	sprintf(que_msgbuf->buffer,"hello world");

    for(;;){
        retu=msgsnd(msgqid,buf,len,0);
        printf("send %d\n",retu);
        sleep(1);
    }
}
*/


uint64_t localtime_ms(){
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
}

void *pthread_handler(void* arg){
    int timeout = *(int*)arg+1,ret;
    free(arg);
    // timeout *= 1000;
    unsigned char data[1000];
    sprintf((char*)data,"hello world%lu",pthread_self());
    uint64_t old,now;
    for(;;){
        printf("%lu start read %d\n",pthread_self(),timeout);
        old = localtime_ms();
        ret = send_que_timedwait(Q,data,sizeof(data),timeout);
        now = localtime_ms();
        printf("%lu start done %d, ret=%d ,use time%lu, errno=%d, data%s\n",pthread_self(),timeout,ret,now-old,errno,data);
    }

    pthread_exit(NULL);
}

int main(){
    int i;
    pthread_t tid[10];

    for(i=0;i<sizeof(tid)/sizeof(pthread_t);i++){
        int *j = malloc(sizeof(int));
        *j = i;
        pthread_create(&tid[i],NULL,pthread_handler,j); 
    }

    pause();
    return 0;
}
