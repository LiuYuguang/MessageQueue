msgget/msgrcv function out of block state only in this situation:  
1. success send/recv data;  
2. message queue is deleted;  
3. signal interrupt;  
  
in one thread process, we can use alarm() to interrupt msgget/msgrcv, but it is not suitable for multi-thread process.  
I use rbtree to build a timer thread that calculate time and use pthread_kill() function to interrupt msgget/msgrcv timeout thread.  
