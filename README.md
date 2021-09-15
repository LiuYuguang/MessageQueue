msgget/msgrcv function out of block state only in this situation:  
1. success send/recv data;  
2. message queue is deleted;  
3. signal interrupt;  
  
in one thread process, we can use alarm() to interrupt msgget/msgrcv, but it is not suitable for multi-thread process.  
I use rbtree to build a timer thread that calculate time and use pthread_kill() function to interrupt msgget/msgrcv timeout thread.  
线程安全的msgget和msgrcv  
```
.
├── include
│   ├── mq.h
│   └── rbtree.h
├── Makefile
├── README.md
└── src
    ├── mq.c      --- rbtree + pthread_cond + pthread_kill
    ├── mq.c.bak  --- rbtree + pipe + select + pthread_kill
    ├── mq_recv.c
    ├── mq_send.c
    └── rbtree.c
```

