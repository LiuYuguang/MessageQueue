#ifndef _QUE_H_
#define _QUE_H_

#include <sys/types.h> //for key_t
#include <stddef.h>    //for size_t
#include <sys/types.h> //for ssize_t


/**
 * @param[in] qid
 * @param[in] pdata
 * @param[in] len
 * @param[in] timeout ms
 * @return len if successful, -1 error
*/
ssize_t send_que_timedwait(key_t qid,void* pdata,size_t len,int timeout);

/**
 * @param[in] qid
 * @param[in] pdata
 * @param[in] len
 * @param[in] timeout ms
 * @return >0 if successful, -1 error
*/
ssize_t read_que_timedwait(key_t qid,void* pdata,size_t len,int timeout);

#endif