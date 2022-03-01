 /*
  * todo:
  * _print add static
  * static inline single_ts_start
  * static inline single_ts_end
  * */

 
 /**
 * @file ssl.c
 * @brief SSL module
 * @author 	Wan Wenkai (wanwenkai@ict.ac.cn)
 * 			Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2020.8.25
 * @version 0.1
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2020.8.25
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date: 2020.11.27
 *      Author: wanwenkai
 *      Modification: Stable handshake version
 */
/******************************************************************************/

/**/
/*
 * +---+                                +---+
 * |...|--->read() -->BIO_write(rbio)-->|...|-->SSL_read(ssl)--->in
 * |...|                                |...|
 * |...|                                |...|
 * |...|                                |...|
 * |...|<--write(fd)<--BIO_read(wbio)<--|...|<--SSL_write(ssl)<--out
 * +---+                                +...+
 *
 *     |                                |   |                   |
 *     |<------------------------------>|   |<----------------->|
 *     |         encrypted bytes        |   |  decrypted bytes  |
 */

/* make sure to define trace_level before include! */
#ifndef TRACE_LEVEL
//	#define TRACE_LEVEL	TRACELV_DEBUG
#endif

struct qapp_context *qapp;
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
#include "ssl.h"
#include "api.h"
#include "global_macro.h"
#include <pthread.h>
/******************************************************************************/
/* local macros */
#define READLINE 1024
#define QUEUE_SIZE 1024
#define FIRST_N_BYTE 2
#define BIO_BUFSIZ 1500
#define TESTING
#undef TESTING
#define HIGH //testing
#define TS_CYCLE 2000
#define DEBUG
/******************************************************************************/
/* local data structures */
typedef struct low_priority {
    bool encrypt;     /* encrypt or decrypt */
    char *data;       /* payload pointer */
    int len;          /* payload len */
    int (*encrypt_decrypt)(q_SSL *q_ssl, char *data, int len);
    q_SSL *q_ssl;     /* tls context pointer */
    qstack_t qstack;  /* qingyun golbal context pointer */
    mbuf_t mbuf;      /* rte_mbuf */
    struct timespec time;
    struct low_priority *next;
    struct low_priority *prev;
    int core_id;

} qlow_t;


/* Qingyun adopts a priority method processing data packets, 
 * and low-priority data packets will be processed by a 
 * single thread At the tls level, in order to cooperate 
 * with Qingyun's priority feature, tls uses tow lock-free 
 * queues. 
 * */
struct n21_queue non_EnDecrypt_list;
struct n21_queue EnDecrypt_list;
ssl_mgt_t q_ssl_mgt;
/******************************************************************************/
/* local static functions */
static inline ssl_mgt_t
get_ssl_mgt()
{
#if INSTACK_TLS
	return get_global_ctx()->g_ssl_mgt;
#else
	return NULL;
#endif
}

static inline q_SSL*
get_ssl_from_stream(tcp_stream_t cur_stream)
{
#if INSTACK_TLS
	return cur_stream->ssl;
#else
	return NULL;
#endif
}
/******************************************************************************/
/* functions */

char *cacert = "/home/wwk/qingyun/qstack/src/cacert.pem";
char *privk = "/home/wwk/qingyun/qstack/src/privkey.pem";


static void pkt_time_cal(struct timespec start)
{
    static int count = 0;
    static int sum_count = 0;
    struct timespec time;
    long single_time;
    static long avg_time;
    if (unlikely(count == TS_CYCLE)) {

        count = 0;
        sum_count++;
        clock_gettime(CLOCK_REALTIME, &time);

        single_time = (time.tv_sec - start.tv_sec)*1000*1000*1000 
                        + (time.tv_nsec - start.tv_nsec);
        avg_time += single_time;

        TRACE_TRACE("timeval test: %llu ns,  avg: %llu ns\n",
            single_time, avg_time/sum_count);

    } else {
        count++;
    }
}


static void begin_handshake_cal(q_SSL *q_ssl)
{
    struct timespec begin;

    clock_gettime(CLOCK_REALTIME, &begin);
    q_ssl->begin_handshake = begin;
}


static void end_handshake_cal(q_SSL *q_ssl)
{    
    struct timespec end;

    clock_gettime(CLOCK_REALTIME, &end);
    q_ssl->end_handshake = end;
}

static void handshake_time_cal(q_SSL *ssl)
{
    static int hs_count = 0;
    static int hs_sum_count = 0;
    long single_time;
    static long hs_avg_time;
    if (unlikely(hs_count == TS_CYCLE)) {

        hs_count = 0;
        hs_sum_count++;

        single_time = (ssl->end_handshake.tv_sec 
            - ssl->begin_handshake.tv_sec)*1000*1000*1000 
            + (ssl->end_handshake.tv_nsec 
            - ssl->begin_handshake.tv_nsec);
        hs_avg_time += single_time;

        TRACE_TRACE("timeval test: %llu ns,  avg: %llu ns\n",
            single_time, hs_avg_time/hs_sum_count);

    } else {
        hs_count++;
    }
}


static int ssl_parse_conf_test()
{
#if INSTACK_TLS
    q_ssl_mgt->ssl_config[q_ssl_mgt->ctx_num].port 
			= 80;  //use for https 
//			= 8783; // use for http

	q_ssl_mgt->ssl_config[q_ssl_mgt->ctx_num].servcert
			= cacert; 

	q_ssl_mgt->ssl_config[q_ssl_mgt->ctx_num].serpkey 
			= privk; 

	q_ssl_mgt->ctx_num++;
#endif
	return SUCCESS;

}

static void _init_ssl_mgt(void)
{
#if INSTACK_TLS
    int i = 0;
    q_ssl_mgt->ctx_num = 0;
    for (; i < SSL_MGT_NUM; i++) {
        q_ssl_mgt->ssl_config[i].cacert = NULL;
        q_ssl_mgt->ssl_config[i].servcert = NULL;
        q_ssl_mgt->ssl_config[i].serpkey = NULL;
    }
#endif
}


static int err_check(SSL *ssl, int ret)
{
	int err = SSL_get_error(ssl, ret);
	while ((err = ERR_get_error())) {
		TRACE_FUNC("ERR: %s\n", ERR_error_string(err, NULL));
	}
#if 0
	if (ssl == NULL) {
		TRACE_EXCP("ssl is null\n");
	}
	if (err == SSL_ERROR_NONE) {
		TRACE_FUNC("SSL_ERROR_NONE\n");
	
	} else if (err == SSL_ERROR_WANT_READ) {
		TRACE_FUNC("SSL_ERROR_WANT_READ\n");

	} else if (err == SSL_ERROR_WANT_WRITE) {
	
		TRACE_FUNC("SSL_ERROR_WANT_WRITE\n");
	} else if (err == SSL_ERROR_SYSCALL) {
	
		TRACE_FUNC("SSL_ERROR_WANT_SYSCALL\n");
	} else if (err == SSL_ERROR_SSL){
		
		TRACE_FUNC("SSL_ERROR_SSL\n");
		
		while ((err = ERR_get_error())) {
			TRACE_FUNC("ERR: %s\n", ERR_error_string(err, NULL));
		}
	} else {
	
		TRACE_FUNC("Other error\n");
	}
#endif
	return -1;
}

/* Encrypt plaintext data into ciphertext */
static int encrypt_data(q_SSL *q_ssl, char *data, int len)
{
    int wlen = 0;
    int ret = -1;
    
	//ERR_clear_error();
//    TRACE_FUNC("before encrypt, len = %d\n", len);
    ret = SSL_write(q_ssl->ssl, data, len);
	if (ret <= 0)
		return err_check(q_ssl->ssl, ret);
    wlen = BIO_ctrl_pending(q_ssl->sink);
    if (unlikely(wlen <= 0)) {
        TRACE_EXCP("SSL_write failure!, len = %d, ret = %d\n", len, ret);
		return -1;
    }
    
    return BIO_read(q_ssl->sink, data, wlen);
}


static int decrypt_prio(q_SSL *q_ssl, char *data, int len)
{

    int rlen = 0;
    char buf[FIRST_N_BYTE];

    rlen = BIO_write(q_ssl->sink, data, len);
    if (rlen == -2)
        return rlen;

    if (unlikely(rlen <= 0) || unlikely(rlen < FIRST_N_BYTE)) {
        TRACE_EXCP("rlen = %d, len = %d\n", rlen, len);
        TRACE_EXCP("BIO write failure or len invalid.\n");
        exit(-1);
    }

    memset(buf, 0, FIRST_N_BYTE);
    SSL_peek(q_ssl->ssl, buf, FIRST_N_BYTE);
#ifdef HIGH
    return buf[0] == 'G'? 1 : 0;
#else
    return buf[0] == 'f'? 1 : 0;
#endif
}


/* Decrypt ciphertext data into plaintext */
static int decrypt_data(q_SSL *q_ssl, char *data, int len)
{
	int rlen;
	if (q_ssl == NULL)
		return -1;
    rlen = BIO_write(q_ssl->sink, data, len);
    memset(data, 0, len);

    return SSL_read(q_ssl->ssl, data, rlen);
}


/* Qingyun use a thread deal with low priority packets */
static void low_prio_func()
{
	int ret = 0;
    while (1) {
        /* Get node from non_EnDecrypt_list. */
        qlow_t *node =  (qlow_t *)n21q_dequeue(&non_EnDecrypt_list);
        if (unlikely(!node)) {
            continue;
        }
        if (node->mbuf->mbuf_state != MBUF_STATE_TALLOC && node->encrypt) {
            printf("low_prio_func!\n");
            exit(-1);
        }

        /* Deal with data include encrypt or decrypt */
		ret = node->encrypt_decrypt(node->q_ssl, node->data, node->len);
		if (ret == -1) {
		    TRACE_EXCP("encrypt_decrypt failure!, encrypt = %d\n", node->encrypt);
			mbuf_free(node->core_id, node->mbuf);
            continue;
		}
        node->mbuf->payload_len = ret;

        /*Add data that had been dealed with to EnDecrypt_list.*/
        n21q_enqueue(&EnDecrypt_list, node->qstack->stack_id, node);
    }
}


void 
init_ssl_mgt(void)
{
#if INSTACK_TLS
    /* init ssl mgt. */
    q_ssl_mgt = (ssl_mgt_t)malloc(sizeof(struct ssl_mgt));
    _init_ssl_mgt();
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
    ssl_parse_conf_test();
    /* Initialize a lock-free circular queue, use to store packet 
     * which waiting been encrypted or decrypted. */
    n21q_init(&non_EnDecrypt_list, MAX_CORE_NUM, QUEUE_SIZE);
    /* Initialize a lock-free circular queue, use to store packet 
     * which had been encrypted or decrypted, waiting been send or 
     * deal with by qingyun stack. */
    TRACE_FUNC("MAX_STACK_NUM = %d\n", MAX_STACK_NUM);
    n21q_init(&EnDecrypt_list, MAX_STACK_NUM, QUEUE_SIZE);

    /* Create encrypt and decrypt thread. */
	qapp = __qstack_create_worker(CRYP_THREAD_CORE, low_prio_func, NULL);
#endif
}

/* Delete tail space */
static char *rtrim(char *str)
{
	if (NULL == str || *str == '\0')
		return str;
	
	char *p = str + strlen(str) -1;

	while (p >= str && isspace(*p)) {
		*p = '\0';
		--p;
	}

	return str;
}

/* Delete head space */
static char *ltrim(char *str)
{
	if (NULL == str || *str == '\0')
		return str;

	int len = 0;
	char *p = str;
	while (*p != '\0' && isspace(*p)) {
		++p;
		++len;
	}

	memmove(str, p, strlen(str)-len+1);

	return str;
}

/* Delete space of head and tail. */
static char *s_trim(char *str)
{
	str = rtrim(str);
	str = ltrim(str);

	return str;
}


static char* split_str(char *str, const char *substr)
{
	char *result = (char *)malloc(strlen(str)-strlen(substr));

	if (0 == strncmp(str, substr, strlen(substr))) {
		strcpy(result, str+strlen(substr));
		return result;
	}

	return NULL;
}


static int get_port(char *str)
{
	return atoi(split_str(str, "PORT = "));
}

static char *get_certi_file(char *str, const char *substr)
{
    return split_str(str, substr);
}


static void readline(char *str, FILE *fp_conf)
{
	memset(str, 0, sizeof(str));
	fgets(str, READLINE, fp_conf);
	s_trim(str);
}


int ssl_parse_conf(FILE *fp_conf)
{
#if INSTACK_TLS
    assert(fp_conf);
	if (unlikely(!fp_conf)) {
        TRACE_EXCP("fp_conf is null!\n");
		return ERROR;
    }
	
	char buf[READLINE];
	
	readline(buf, fp_conf);
    q_ssl_mgt->ssl_config[q_ssl_mgt->ctx_num].port 
			= get_port(buf);

    readline(buf, fp_conf);
    q_ssl_mgt->ssl_config[q_ssl_mgt->ctx_num].cacert
            = get_certi_file(buf, "CACERT_FILE = ");
	readline(buf, fp_conf);
	q_ssl_mgt->ssl_config[q_ssl_mgt->ctx_num].servcert
            = get_certi_file(buf, "SRVCERT_FILE = ");
	readline(buf,fp_conf);
	q_ssl_mgt->ssl_config[q_ssl_mgt->ctx_num].serpkey 
            = get_certi_file(buf, "SRVPKEY = ");
	q_ssl_mgt->ctx_num++;
#endif
	return SUCCESS;
}


static void
qssl_ctx_init(struct tcp_listener *listener, struct ssl_config_t *ssl_config)
{
#if INSTACK_TLS
    if (unlikely(!listener) || unlikely(!ssl_config)) {
        TRACE_EXCP("listener or ssl_config is null!\n");
        return;
    }

	SSL_CTX *ctx;

	ctx = SSL_CTX_new(SSLv23_server_method());

	if (ctx == NULL) {
		ERR_print_errors_fp(stdout);
		goto err;
	}
#if 0	
	if (!SSL_CTX_load_verify_locations(ctx, ssl_config->cacert, NULL) 
		|| (!SSL_CTX_set_default_verify_paths(ctx))) {
		printf("cacertf err\n");
		goto err;
	}
#endif
	if (SSL_CTX_use_certificate_file(ctx, ssl_config->servcert, 
		SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stdout);
		goto err;
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, ssl_config->serpkey, 
		SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stdout);
		goto err;
	}

	if (!SSL_CTX_check_private_key(ctx)) {
		ERR_print_errors_fp(stdout);
	    goto err;
	}
	
	q_SSL_CTX *q_ssl_ctx = (q_SSL_CTX *)malloc(sizeof(q_SSL_CTX));
	q_ssl_ctx->ssl_ctx = ctx;
	listener->ssl_ctx = q_ssl_ctx;
    
    return;

err:
    TRACE_EXCP("init err.\n");
    exit(-1);
    SSL_CTX_free(ctx);
#endif
}

static int https_inquire(uint16_t port)
{
#if INSTACK_TLS
	int i;
	int num = q_ssl_mgt->ctx_num;

	for (i = 0; i < num; i++) {
		if (q_ssl_mgt->ssl_config[i].port == port)
			break;
	}

	return (i == num) ? -1 : i;
#else 
	return 0;
#endif
}

int 
set_ssl_listener(struct tcp_listener *listener)
{
#if INSTACK_TLS
    if (unlikely(!listener)) {
        TRACE_EXCP("listener is null!\n");
        return FALSE;
    }
     
	int num = https_inquire(listener->port);
	if (num != -1) {
		qssl_ctx_init(listener, &q_ssl_mgt->ssl_config[num]);
		listener->is_ssl = true;
		return SUCCESS;
	}
#endif
	return FALSE;
}


int 
set_ssl_stream(tcp_stream_t cur_stream, q_SSL_CTX *q_ssl_ctx)
{
#if INSTACK_TLS
    if (unlikely(!cur_stream) || unlikely(!q_ssl_ctx)) {
        TRACE_EXCP("cur_stream or q_ssl_ctx is null!\n");
        return -1;
    }

	q_SSL *q_ssl = (q_SSL*)malloc(sizeof(q_SSL));
	q_ssl->ssl = SSL_new(q_ssl_ctx->ssl_ctx);
    q_ssl->stream_state = HANDSHAKE_UNINITED;
	q_ssl->low_count = 0;
	q_ssl->high_count = 0;
	q_ssl->priority = 0;
	/* bind ssl to tcp stream */
	cur_stream->ssl = q_ssl;
	q_ssl->cur_stream = cur_stream;
	cur_stream->is_ssl = true;
    /* Not properly initialized yet */

    /* Create BIO pair */
    if (!BIO_new_bio_pair(&q_ssl->source, BIO_BUFSIZ, &q_ssl->sink, BIO_BUFSIZ)) {
        ERR_print_errors_fp(stdout);
        TRACE_EXCP("BIO_new_bio_pair error!\n");
        return -1;
    }

    assert(q_ssl->ssl);
    /* Bind ssl to one of BIO pair */
    SSL_set_bio(q_ssl->ssl, q_ssl->source, q_ssl->source);
#endif
	return 0;
}


static int q_SSL_connect()
{
    return 1;
}

static int
q_SSL_accept(q_SSL *q_ssl, int sockid, uint8_t core_id, char *ssl_ptr, int r_len)
{
    if (unlikely(!q_ssl)) {
        TRACE_EXCP("q_ssl is null!\n");
        return 0;
    }
#ifdef DEBUG
	if (core_id < 0 || core_id > 2)
		TRACE_ERROR("core id invalid\n");
	assert(ssl_ptr);
#endif
    /* Not properly initialized yet */
    struct rte_mbuf *s_mbuf;
    int w_len = 0;
    char *payload = NULL;
    
    BIO *server = q_ssl->source;
    BIO *server_io = q_ssl->sink;

    BIO_write(server_io, ssl_ptr, r_len);
    /* Deal with the client hello from client, and response 
     * server hello. */
    SSL_accept(q_ssl->ssl);
    if ((w_len = BIO_ctrl_pending(server_io)) <= 0) {
        TRACE_EXCP("BIO_ctrl_pending error, len = %d, ssl_ptr = %s\n", r_len, ssl_ptr);
        return 0;
    }

    /* Apply mbuf, and write data to mbuf */
    s_mbuf = io_get_wmbuf(core_id, &payload, &w_len, QFLAG_SEND_HIGHPRI);
    assert(payload); //debug
    w_len = BIO_read(server_io, payload, 1500);
    _q_tcp_send(core_id, sockid, s_mbuf, w_len, 1);

	return 1;
}


/* free tls/ssl connection */
static int qssl_close(int core_id, q_SSL *q_ssl)
{
    if (!q_ssl)
        return SUCCESS;

	if (q_ssl->stream_state == CLOSE_TLS_WAITING) {
		q_ssl->stream_state = CLOSE_TLS;
		q_ssl->cur_stream = NULL;
    	SSL_free(q_ssl->ssl);
    	free(q_ssl);

		return SUCCESS;
	}

    return FALSE;
}


int qssl_close_wait(int core_id, q_SSL *q_ssl)
{
    if (!q_ssl) {
		TRACE_EXCP("q_ssl is null.\n");
        return TRUE;
	}

    int w_len = 0;
    char *payload = NULL;
    struct rte_mbuf *s_mbuf;

	/* waiting response packet be sent */
	if (q_ssl->low_count > 0 || q_ssl->high_count > 0) {
		q_ssl->stream_state = CLOSE_TLS_WAITING;
		return FALSE;
	}

	SSL_shutdown(q_ssl->ssl);
	w_len = BIO_ctrl_pending(q_ssl->sink);
	if (unlikely(w_len <= 0)) {

		TRACE_EXCP("SSL_shutdown failure!\n");
		err_check(q_ssl->ssl, w_len);
		return FALSE;
	}

	/* Apply mbuf, and write data to mbuf */
	s_mbuf = io_get_wmbuf(core_id, &payload, &w_len, 1);
	assert(payload); //debug
	//ERR_clear_error();
	w_len = BIO_read(q_ssl->sink, payload, 1500);
	if (unlikely(w_len <= 0)) {
		TRACE_EXCP("BIO_read failure!\n");
		err_check(q_ssl->ssl, w_len);
		mbuf_free(core_id, s_mbuf);
		return FALSE;
	}
	s_mbuf->payload_len = w_len;
	_q_tcp_send(core_id, q_ssl->cur_stream->socket->id, s_mbuf, w_len, QFLAG_SEND_HIGHPRI);
	q_ssl->stream_state = CLOSE_TLS_WAITING;
	qssl_close(core_id, q_ssl);

	return TRUE;
}


static void put_low_priority_packet_list(q_SSL *q_ssl, qstack_t qstack, mbuf_t mbuf, char *data, int len, bool state, int (*encrypt_decrypt)(), int core_id, struct timespec time)
{
    qlow_t *node = (qlow_t *)malloc(sizeof(qlow_t));

    /* Initialize node. */
    node->q_ssl = q_ssl;
    node->qstack = qstack;
    node->mbuf = mbuf;
    node->data = data;
    node->len = len;
    node->encrypt = state;
    node->core_id = core_id;
    node->encrypt_decrypt = encrypt_decrypt;
    node->time = time;
	node->next = node->prev = NULL;

    if (mbuf->mbuf_state != MBUF_STATE_TALLOC && node->encrypt) {
        TRACE_FUNC("put low priority packet list!\n");
        exit(-1);
    }
    /* put a node to lock-free circular queue. */
    n21q_enqueue(&non_EnDecrypt_list, core_id, node);
}


/* Decrypt the specified first n bytes of the packet payload 
 * and determine the priority. */
static int packet_decrypto(qstack_t qstack, tcp_stream_t cur_stream, mbuf_t mbuf, char *ssl_ptr, int len)
{
#if INSTACK_TLS
    int ret;
    struct timespec time1 = {0, 0}; 

    clock_gettime(CLOCK_REALTIME, &time1);
    mbuf->mbuf_state = MBUF_STATE_RBUFFED;
	if (!mbuf->priority) {
		cur_stream->ssl->priority = 0;
        put_low_priority_packet_list(cur_stream->ssl, qstack, mbuf, ssl_ptr, len, 0, decrypt_data, qstack->stack_id, time1);
        /* wake up thread that deal with low priority packet.*/
        wakeup_app_thread(qapp);

        return 1;
    }
	cur_stream->ssl->priority = 1;
	mbuf->payload_len = decrypt_data(cur_stream->ssl, ssl_ptr, len);
    /* */
    rb_put(qstack, cur_stream, mbuf);
	cur_stream->ssl->high_count++;
    raise_read_event(qstack, cur_stream, get_sys_ts(), 1);
#endif
    return 1;
}


int process_ssl_packet(qstack_t qstack, tcp_stream_t cur_stream,
		mbuf_t mbuf, char *ssl_ptr, int len)
{
#if INSTACK_TLS
    if (unlikely(!qstack) || unlikely(!cur_stream->ssl) ||unlikely(!mbuf) || unlikely(!ssl_ptr)) {
        return FALSE;
    }
	if (len == 24) {
        TRACE_FUNC("tls/ssl close handshake!\n");
		mbuf_free(qstack->stack_id, mbuf);
		return FALSE;
	}

    if (cur_stream->ssl->stream_state == HANDSHAKE_UNINITED) {
        /* complete ssl handshake */
        if (unlikely(!q_SSL_accept(cur_stream->ssl, cur_stream->socket->id, qstack->stack_id, ssl_ptr, len))) {
            TRACE_EXCP("tls/ssl accept failure!\n");
            return FALSE;
        }

        cur_stream->ssl->stream_state = HANDSHAKE_WAITING;
    } else if (cur_stream->ssl->stream_state == HANDSHAKE_WAITING) {

        //begin_handshake_cal(cur_stream->ssl);
        /* receive client cipher data. */
        if (unlikely(!q_SSL_accept(cur_stream->ssl, cur_stream->socket->id, qstack->stack_id, ssl_ptr, len))) {
            TRACE_EXCP("tls/ssl accept failure, len = %d\n", len);
            return FALSE;
        }
        cur_stream->ssl->stream_state = HANDSHAKE_COMPLETE;
        /* Tell qingyun stack that tls hanshake has established.*/
        //end_handshake_cal(cur_stream->ssl);
        //handshake_time_cal(cur_stream->ssl);
      
	    raise_accept_event(qstack, cur_stream);
    } else {
        if (unlikely(!packet_decrypto(qstack, cur_stream, mbuf, ssl_ptr, len))) {
            TRACE_EXCP("tls/ssl decrypt failure!\n");
            return FALSE;
        }
		return SUCCESS;
    }
#endif
    return FALSE;
}


/* Encrypt data which from qingyun stack */
int ssl_write(q_SSL *q_ssl, int core_id, mbuf_t mbuf, char *data, int len)
{
    if (q_ssl == NULL || mbuf == NULL || data == NULL) {
        TRACE_FUNC("q_ssl or mbuf or data is empty!\n");
        return ERROR;    
    }
    
    int socket_id = q_ssl->cur_stream->socket->id;
	int ret = 0;
	qstack_t qstack = q_ssl->cur_stream->qstack;
    struct timespec time1 = {0, 0}; 
	tcp_stream_t cur_stream;

    clock_gettime(CLOCK_REALTIME, &time1);
	if (q_ssl->priority) {
        /* encrypt high priority packet. */
		ret = encrypt_data(q_ssl, data, len);
		if (ret == -1)
			return FAILED;
		mbuf->payload_len = ret;
        //pkt_time_cal(time1);
	    _q_tcp_send(core_id, socket_id, mbuf, mbuf->payload_len, QFLAG_SEND_HIGHPRI);
		q_ssl->high_count--;
		if (q_ssl->stream_state == CLOSE_TLS_WAITING) {
			TRACE_FUNC("high count = %d\n", q_ssl->high_count);
			qssl_close_wait(core_id, q_ssl);
			cur_stream = q_ssl->cur_stream;
			tcp_stream_close(core_id, cur_stream);
			return ret; 
		}
    } else {
        if (mbuf->mbuf_state != MBUF_STATE_TALLOC) {
            TRACE_FUNC("ssl_write!\n");
            exit(-1);
        }
        /* deal with low priority packet. */
        put_low_priority_packet_list(q_ssl, qstack, mbuf, data, len, 1, encrypt_data, core_id, time1);
        /* wake up thread that deal with low priority packet.*/
        wakeup_app_thread(qapp);

        return FAILED; 
    }

    return ret;
}


void handle_cryption_rsp(qstack_t qstack)
{
    qlow_t *node = (qlow_t *)n21q_dequeue(&EnDecrypt_list);
	tcp_stream_t cur_stream;
    
    while (node != NULL) {
        if (node->encrypt) {
            assert(node->mbuf->mbuf_state == MBUF_STATE_TALLOC);
		    //pkt_time_cal(node->time);
            _q_tcp_send(node->qstack->stack_id, node->q_ssl->cur_stream->socket->id, node->mbuf, node->mbuf->payload_len, 0);
			node->q_ssl->low_count--;
			if (node->q_ssl->stream_state == CLOSE_TLS_WAITING) {
				qssl_close_wait(node->qstack->stack_id, node->q_ssl);
				cur_stream = node->q_ssl->cur_stream;
				tcp_stream_close(node->qstack->stack_id, cur_stream);
			}

        } else {
			rb_put(node->qstack, node->q_ssl->cur_stream, node->mbuf);
			node->q_ssl->low_count++;
//          pkt_time_cal(node->time);
			raise_read_event(node->qstack, node->q_ssl->cur_stream, get_sys_ts(), 0);
        }

        node = (qlow_t *)n21q_dequeue(&EnDecrypt_list);
    };

    return SUCCESS;
}

/******************************************************************************/
/*----------------------------------------------------------------------------*/
