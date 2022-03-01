/**
 * @file ssl.h
 * @brief SSL module 
 * @author	Wan Wenkai (wanwenkai@ict.ac.cn)
 * 			Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2020.8.25
 * @version 0.1 
 * @detail Function list: \n
 *   1. \n
 *   2. \n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2020.8.25 
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __SSL_H_
#define __SSL_H_
/******************************************************************************/
struct q_SSL_CONTEX;
typedef struct q_SSL_CONTEXT q_SSL_CTX;
struct q_SSL_STREAM;
typedef struct q_SSL_STREAM q_SSL;
struct ssl_mgt;
typedef struct ssl_mgt *ssl_mgt_t;
#if INSTACK_TLS
extern ssl_mgt_t q_ssl_mgt;
#endif
typedef struct q_ssl_method_st q_SSL_METHOD;

/******************************************************************************/
#include "universal.h"
#include "qstack.h"
#include "runtime_mgt.h"
//#include "api.h"

#include <openssl/engine.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <arpa/inet.h>
/******************************************************************************/
/* global macros */
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* data structures */
struct ssl_config_t {
	int port;
	char *cacert;
	char *servcert;
	char *serpkey;
};

#define SSL_MGT_NUM 10
struct ssl_mgt {
#if INSTACK_TLS
	int ctx_num;
	struct ssl_config_t ssl_config[SSL_MGT_NUM];
#endif
};

struct q_SSL_CONTEXT
{
#if INSTACK_TLS
	SSL_CTX *ssl_ctx;
#endif
};

struct q_SSL_STREAM 
{
	SSL *ssl;
	/* SSLv3 */
	const q_SSL_METHOD *method;
	tcp_stream_t cur_stream;
#define HANDSHAKE_UNINITED 0
#define HANDSHAKE_WAITING 1
#define HANDSHAKE_COMPLETE 2
#define CLOSE_TLS_WAITING 3
#define CLOSE_TLS 4
	int stream_state;
    BIO *source;
    BIO *sink;
    int priority;//wanwenkai for testing    
    struct timespec begin_handshake;
    struct timespec end_handshake;
	int low_count;
	int high_count;

    int state;
};

/******************************************************************************/
/* Used to hold SSL/TLS functions */
struct q_ssl_method_st {
	int version;
	int (*q_ssl_accept) (q_SSL *q_ssl);
	int (*q_ssl_connect) (q_SSL *q_ssl);
};

/******************************************************************************/
/* Handshake statem */
typedef enum {
    SSL_EARLY_DATA_NONE = 0,
    SSL_EARLY_DATA_CONNECT_RETRY,
    SSL_EARLY_DATA_CONNECTING,
    SSL_EARLY_DATA_WRITE_RETRY,
    SSL_EARLY_DATA_WRITING,
    SSL_EARLY_DATA_WRITE_FLUSH,
    SSL_EARLY_DATA_UNAUTH_WRITING,
    SSL_EARLY_DATA_FINISHED_WRITING,
    SSL_EARLY_DATA_ACCEPT_RETRY,
    SSL_EARLY_DATA_ACCEPTING,
    SSL_EARLY_DATA_READ_RETRY,
    SSL_EARLY_DATA_READING,
    SSL_EARLY_DATA_FINISHED_READING
} SSL_EARLY_DATA_STATE;

/******************************************************************************/
/* function declarations */
void init_ssl_mgt();

int ssl_parse_conf(FILE *);

int set_ssl_listener(struct tcp_listener *);

int set_ssl_stream(tcp_stream_t, q_SSL_CTX *);

int process_ssl_packet(qstack_t, tcp_stream_t, mbuf_t, char *, int);

void handle_cryption_rsp(qstack_t);

int qssl_close_wait(int, q_SSL *);

int ssl_write(q_SSL *, int, mbuf_t, char *, int);
/******************************************************************************/
/* inline functions */
static inline int
get_cipher_len(mbuf_t mbuf, int plain_len)
{
	return mbuf->pkt_len - ETHERNET_HEADER_LEN - IP_HEADER_LEN - mbuf->l4_len;
//	return ((plain_len >> 7) + 1) << 7;
}
/******************************************************************************/
/*----------------------------------------------------------------------------*/
/**
 * 
 *
 * @param 
 * @param[out]
 * @return
 * 	
 * @ref 
 * @see
 * @note
 */
#endif //#ifdef __SSL_H_
