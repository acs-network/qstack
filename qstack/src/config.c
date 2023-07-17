#include <stdlib.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

#include "config.h"
#include "qstack.h"

#define MAX_OPTLINE_LEN 1024
/* total cpus detected in the qstack*/
int num_cpus;

struct qstack_config CONFIG = {0};

int 
GetNumCPUs() 
{
	return sysconf(_SC_NPROCESSORS_ONLN);
}
/*----------------------------------------------------------------------------*/
static inline int
mystrtol(const char *nptr, int base)
{
	int rval;
	char *endptr;

	errno = 0;
	rval = strtol(nptr, &endptr, 10);
	/* check for strtol errors */
	if ((errno == ERANGE && (rval == LONG_MAX ||
				 rval == LONG_MIN))
	    || (errno != 0 && rval == 0)) {
		perror("strtol");
		exit(EXIT_FAILURE);
	}
	if (endptr == nptr) {
		fprintf(stderr,"Parsing strtol error!\n");
		exit(EXIT_FAILURE);
	}

	return rval;
}
/*----------------------------------------------------------------------------*/
uint32_t 
MaskFromPrefix(int prefix)
{
	uint32_t mask = 0;
	uint8_t *mask_t = (uint8_t *)&mask;
	int i, j;

	for (i = 0; i <= prefix / 8 && i < 4; i++) {
		for (j = 0; j < (prefix - i * 8) && j < 8; j++) {
			mask_t[i] |= (1 << (7 - j));
		}
	}

	return mask;
}
/*----------------------------------------------------------------------------*/
void
set_host_ip(char *optstr)
{
	char *daddr_s;
	char *prefix;
	char * saveptr;
	int net_prefix;

	saveptr = NULL;
	daddr_s = strtok_r(optstr, "/", &saveptr);
	prefix = strtok_r(NULL, " ", &saveptr);

	assert(daddr_s != NULL);
	assert(prefix != NULL);
	
	net_prefix = mystrtol(prefix, 10);

	CONFIG.eths[0].ip_addr = inet_addr(daddr_s);
	CONFIG.eths[0].netmask = MaskFromPrefix(net_prefix);
}
/*----------------------------------------------------------------------------*/
static int 
ParseConfiguration(char *line)
{
	char optstr[MAX_OPTLINE_LEN];
	char *p, *q;

	char *saveptr;

	strncpy(optstr, line, MAX_OPTLINE_LEN - 1);
	saveptr = NULL;

	p = strtok_r(optstr, " \t=", &saveptr);
	if (p == NULL) {
		fprintf(stderr,"No option name found for the line: %s\n", line);
		return -1;
	}

	q = strtok_r(NULL, " \t=", &saveptr);
	if (q == NULL) {
		fprintf(stderr,"No option value found for the line: %s\n", line);
		return -1;
	}

	if (strcmp(p, "num_cores") == 0) {
		CONFIG.num_cores = mystrtol(q, 10);
		if (CONFIG.num_cores <= 0) {
			fprintf(stderr,"Number of cores should be larger than 0.\n");
			return -1;
		}
		if (CONFIG.num_cores > num_cpus) {
			fprintf(stderr,"Number of cores should be smaller than "
					"# physical CPU cores.\n");
			return -1;
		}
		num_cpus = CONFIG.num_cores;
	} else if (strcmp(p, "max_concurrency") == 0) {
		CONFIG.max_concurrency = mystrtol(q, 10);
		if (CONFIG.max_concurrency < 0) {
			fprintf(stderr,"The maximum concurrency should be larger than 0.\n");
			return -1;
		}
	} else if (strcmp(p, "max_num_buffers") == 0) {
		CONFIG.max_num_buffers = mystrtol(q, 10);
		if (CONFIG.max_num_buffers < 0) {
			fprintf(stderr,"The maximum # buffers should be larger than 0.\n");
			return -1;
		}
	} else if (strcmp(p, "rcvbuf") == 0) {
		CONFIG.rcvbuf_size = mystrtol(q, 10);
		if (CONFIG.rcvbuf_size < 64) {
			fprintf(stderr,"Receive buffer size should be larger than 64.\n");
			return -1;
		}
	} else if (strcmp(p, "sndbuf") == 0) {
		CONFIG.sndbuf_size = mystrtol(q, 10);
		if (CONFIG.sndbuf_size < 64) {
			fprintf(stderr,"Send buffer size should be larger than 64.\n");
			return -1;
		}
	} else if (strcmp(p, "stack_thread") == 0) {
		CONFIG.stack_thread = mystrtol(q, 10);
		if (CONFIG.stack_thread == 0) {
			CONFIG.stack_thread = 1;
		}
		if (CONFIG.stack_thread > CONFIG.num_cores) {
        	fprintf(stderr,"Wrong stack num: %d, MAX_STACK_NUM: %d\n",
                CONFIG.stack_thread, CONFIG.num_cores);
    	}
	} else if (strcmp(p, "app_thread") == 0) {
		CONFIG.app_thread = mystrtol(q, 10);
		if (CONFIG.app_thread == 0) {
			CONFIG.app_thread = 1;
		}
#ifdef SHARED_NOTHING_MODE
		if (CONFIG.stack_thread > CONFIG.num_cores) {
        	fprintf(stderr,"Wrong app num: %d, MAX_STACK_NUM: %d\n",
                CONFIG.stack_thread, CONFIG.num_cores);
    	}
#else
		if ((CONFIG.stack_thread + CONFIG.app_thread) > CONFIG.num_cores) {
        	fprintf(stderr,"Wrong stack num: %d + app num:%d, MAX_STACK_NUM: %d\n",
                CONFIG.stack_thread, CONFIG.app_thread, CONFIG.num_cores);
    	}
#endif
	} else if (strcmp(p, "stat_print") == 0) {
		CONFIG.eths[0].stat_print = mystrtol(q, 10);
		if(CONFIG.eths[0].stat_print == 0)
			CONFIG.eths[0].stat_print = num_cpus - 1;
	} else if (strcmp(p, "host_ip") == 0) {
		set_host_ip(q);
	} else if (strcmp(p, "pri_enable") == 0) {
		CONFIG.pri = mystrtol(q, 10);
	} else {
		fprintf(stderr,"Unknown option type: %s\n", line);
		return -1;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
int 
load_configuration(const char *fname)
{
	FILE *fp;
	char optstr[MAX_OPTLINE_LEN];

	TRACE_LOG("----------------------------------------------------------"
			"-----------------------\n");
	TRACE_LOG("Loading qstack configuration from : %s\n", fname);

	fp = fopen(fname, "r");
	if (fp == NULL) {
		perror("fopen");
		fprintf(stderr,"Failed to load configuration file: %s\n", fname);
		return -1;
	}

	/* set default configuration */
	num_cpus = GetNumCPUs();
	CONFIG.num_cores = 24;
	CONFIG.max_concurrency = MAX_FLOW_NUM;
	CONFIG.max_num_buffers = 100000;
	CONFIG.rcvbuf_size = STATIC_BUFF_SIZE;
	CONFIG.sndbuf_size = STATIC_BUFF_SIZE;
	
    CONFIG.eths_num = 1;
	CONFIG.eths = (struct eth_table*)calloc(1, sizeof(struct eth_table));
    CONFIG.eths[0].ifindex = 0;


	while (1) {
		char *p;
		char *temp;

		if (fgets(optstr, MAX_OPTLINE_LEN, fp) == NULL)
			break;

		p = optstr;

		// skip comment
		if ((temp = strchr(p, '#')) != NULL)
			*temp = 0;
		// remove front and tailing spaces
		while (*p && isspace((int)*p))
			p++;
		temp = p + strlen(p) - 1;
		while (temp >= p && isspace((int)*temp))
			   *temp = 0;
		if (*p == 0) /* nothing more to process? */
			continue;

		if (ParseConfiguration(p) < 0) {
			fclose(fp);
			return -1;
		}
	}

	fclose(fp);

	return 0;
}
/*----------------------------------------------------------------------------*/
