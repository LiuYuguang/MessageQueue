#include "mq.h"
#include <string.h>                //for memcpy
#include <stdlib.h>                //for malloc free
#include <sys/time.h>              //for struct timeval gettimeofday
#include <signal.h>                //for signal sigset_t sigfillset pthread_sigmask SIG*
#include <sys/msg.h>               //for msgget msgsnd msgrcv
#include <unistd.h>                //for pipe write read close
#include <pthread.h>               //for pthread*
#include <errno.h>                 //for errno E*
#include "rbtree.h"

#define QUE_MSGTYPE    1l

static int timer_alive = 0;
static pthread_t timer_tid;
static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t timer_cond = PTHREAD_COND_INITIALIZER;
static rbtree_t timer_root;

typedef struct timer_node_s timer_node_t;
struct timer_node_s{
	pthread_t tid;
	uint64_t expire;
	int status;//0 default, 1 timeout
	rbtree_node_t rbtree_node;
};

int is_thread_alive(pthread_t tid){
	int kill_rc = pthread_kill(tid,0);//0 success, ESRCH 线程不存在, EINVAL 信号不合法 
	int ret = 1;//alive
	if(kill_rc == ESRCH){
		ret = 0;
	}
	return ret;
}

static void* timer_loop(void*arg);

static int timer_create_new(){
	if(timer_alive == 1){
		return 0;
	}

	pthread_mutex_lock(&timer_mutex);
	if(timer_alive == 1){
		pthread_mutex_unlock(&timer_mutex);
		return 0;
	}

	rbtree_init(&timer_root);
	pthread_create(&timer_tid,NULL,timer_loop,NULL);
	timer_alive = 1;
	pthread_detach(timer_tid);//分离线程

	pthread_mutex_unlock(&timer_mutex);
	return 0;
}

static void* timer_loop(void*arg){
	sigset_t sigset;
    sigfillset(&sigset);
	pthread_sigmask(SIG_SETMASK,&sigset,NULL);//当前线程屏蔽所有信号

	uint64_t now;
	struct timespec now_ts;
	struct timespec timeout_ts;
	rbtree_node_t *rbtree_node;
	timer_node_t *timer_node=NULL,*timer_node_will_expire=NULL;

	for(;;){
		//加锁
		pthread_mutex_lock(&timer_mutex);
		clock_gettime(CLOCK_REALTIME, &now_ts);
		now = now_ts.tv_sec*1000000000 + now_ts.tv_nsec;

		timer_node = NULL;
		timer_node_will_expire = NULL;
		for(rbtree_node = rbtree_min(&timer_root,timer_root.root);
			rbtree_node!=NULL;
			rbtree_node = rbtree_next(&timer_root,rbtree_node)
		){
			timer_node = rbtree_data(rbtree_node,timer_node_t,rbtree_node);
			if(timer_node->expire <= now){
				//超时, 则信号通知
				timer_node->status = 1;
				pthread_kill(timer_node->tid,SIGUSR1);
				continue;
			}
			timer_node_will_expire = timer_node;
			break;
		}

		//条件等待
		if(timer_node_will_expire == NULL){
			pthread_cond_wait(&timer_cond,&timer_mutex);
		}else{
			timeout_ts.tv_sec = timer_node_will_expire->expire/1000000000;
			timeout_ts.tv_nsec = timer_node_will_expire->expire%1000000000;
			pthread_cond_timedwait(&timer_cond,&timer_mutex,&timeout_ts);//注意, 时间不能是NULL
		}
		
		//解锁
		pthread_mutex_unlock(&timer_mutex);
	}
	timer_alive = 0;
	pthread_exit(NULL);
}

int add_timer(timer_node_t *node,size_t timeout){//单位ms
	node->tid = pthread_self();
	node->status = 0;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	node->expire = (ts.tv_sec+timeout)*1000000000 + ts.tv_nsec;
	node->rbtree_node.key = node->expire;

	pthread_mutex_lock(&timer_mutex);
	rbtree_insert(&timer_root,&node->rbtree_node);
	pthread_cond_signal(&timer_cond);
	pthread_mutex_unlock(&timer_mutex);
	//pthread_cond_signal(&timer_cond);
	return 0;
}

int delete_timer(timer_node_t *node){
	pthread_mutex_lock(&timer_mutex);
	rbtree_delete(&timer_root,&node->rbtree_node);
	pthread_mutex_unlock(&timer_mutex);
	return 0;
}

int _signal(int signo, void (*sighandler)(int))
{
    struct sigaction sa;
    sigset_t set;
    //sigemptyset(&set);//当进入sighandler时,不屏蔽所有信号,可被其他信号中断
	sigfillset(&set);//当进入sighandler时,屏蔽所有信号
    sa.sa_mask = set;
    sa.sa_handler = sighandler;
    sa.sa_restorer = NULL;
    sa.sa_flags = 0;
    return sigaction(signo, &sa, NULL);
}

void timer_sighandler(int signo){
    return;
}

typedef struct {
	long msgtype;	
	unsigned char buffer[0];
} MSGBUF;

ssize_t send_que_timedwait(key_t qid,void* pdata,size_t len,int timeout){
	int	msgqid,retu;
	timer_node_t node = {0};
	unsigned char* buf = NULL;
	MSGBUF* que_msgbuf;

	//https://man7.org/linux/man-pages/man2/msgget.2.html
	msgqid=msgget(qid,0666);
	if(msgqid == -1){
		if(errno == ENOENT){
			msgqid=msgget(qid,IPC_CREAT|0666);
		}
		if(msgqid == -1)
			return -1;
	}

	buf = malloc(sizeof(MSGBUF) + len);
	if(buf == NULL)
		return -1;
	
	que_msgbuf = (MSGBUF*)buf;
	que_msgbuf->msgtype = QUE_MSGTYPE;
	memcpy(que_msgbuf->buffer,pdata,len);

	// int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
	// msgflag
	// 0：当消息队列满时，msgsnd将会阻塞，直到消息能写进消息队列
	// IPC_NOWAIT：当消息队列已满的时候，msgsnd函数不等待立即返回
	// IPC_NOERROR：若发送的消息大于size字节，则把该消息截断，截断部分将被丢弃，且不通知发送进程。
	// 返回值
	// 成功：0
	// 出错：-1，错误原因存于error中
	// EAGAIN：参数msgflg设为IPC_NOWAIT，而消息队列已满
	// EIDRM：标识符为msqid的消息队列已被删除
	// EACCESS：无权限写入消息队列
	// EFAULT：参数msgp指向无效的内存地址
	// EINTR：队列已满而处于等待情况下被信号中断
	// EINVAL：无效的参数msqid、msgsz或参数消息类型type小于0

	if(timeout == -1){
		retu=msgsnd(msgqid,buf,len,0);
	}
	else if(timeout == 0){
		retu=msgsnd(msgqid,buf,len,IPC_NOWAIT);
	}else{
		timer_create_new();
		
		_signal(SIGUSR1,timer_sighandler);
		add_timer(&node,timeout);
		retu=msgsnd(msgqid,buf,len,0);
		delete_timer(&node);
		if(errno == EINTR && node.status == 1){
			errno = ETIMEDOUT;
		}
	}

	free(buf);
	return retu;
}

ssize_t read_que_timedwait(key_t qid,void* pdata,size_t len,int timeout){
	int	msgqid,retu;
	timer_node_t node = {0};
	unsigned char* buf = NULL;
	MSGBUF* que_msgbuf = NULL;

	msgqid=msgget(qid,0666);//https://man7.org/linux/man-pages/man2/msgget.2.html
	if(msgqid == -1){
		if(errno == ENOENT){
			msgqid=msgget(qid,IPC_CREAT|0666);
		}
		if(msgqid == -1)
			return -1;
	}

	buf = malloc(sizeof(MSGBUF) + len);
	if(buf == NULL){
		return -1;
	}

	// ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp,int msgflg);
	// msgtyp
	// 0：接收第一个消息
	// >0：接收类型等于msgtyp的第一个消息
	// <0：接收类型等于或者小于msgtyp绝对值的第一个消息
	// msgflg
	// 0: 阻塞式接收消息，没有该类型的消息msgrcv函数一直阻塞等待
	// IPC_NOWAIT：如果没有返回条件的消息调用立即返回，此时错误码为ENOMSG
	// IPC_EXCEPT：与msgtype配合使用返回队列中第一个类型不为msgtype的消息
	// IPC_NOERROR：如果队列中满足条件的消息内容大于所请求的size字节，则把该消息截断，截断部分将被丢弃
	// 函数返回值
	// 成功：实际读取到的消息数据长度
	// 出错：-1，错误原因存于error中
	// E2BIG：消息数据长度大于msgsz而msgflag没有设置IPC_NOERROR
	// EIDRM：标识符为msqid的消息队列已被删除
	// EACCESS：无权限读取该消息队列
	// EFAULT：参数msgp指向无效的内存地址
	// ENOMSG：参数msgflg设为IPC_NOWAIT，而消息队列中无消息可读
	// EINTR：等待读取队列内的消息情况下被信号中断


	if(timeout == -1){
		retu=msgrcv(msgqid,buf,len,0,0);
	}
	else if(timeout == 0){
		retu=msgrcv(msgqid,buf,len,0,IPC_NOWAIT);
	}else{
		timer_create_new();
		
		_signal(SIGUSR1,timer_sighandler);
		add_timer(&node,timeout);
		retu=msgrcv(msgqid,buf,len,0,0);
		delete_timer(&node);
		if(errno == EINTR && node.status == 1){
			errno = ETIMEDOUT;
		}	
	}

	que_msgbuf = (MSGBUF*)buf;
	if(retu > 0){
		memcpy(pdata,que_msgbuf->buffer,retu);
	}

	free(buf);
	return retu;
}