/*
 * Tencent is pleased to support the open source community by making Libco available.  Libco is licensed under the Apache License, Version 2.0, and a copy of the license is included in this file.
 * 
 * Copyright (C) 2014 THL A29 Limited, a Tencent company.  All rights reserved.
 * 
 * 
 * Terms of the Apache License, Version 2.0:
 * ---------------------------------------------------
 * 
 * TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION
 * 
 * 1. Definitions.
 * 
 * “License” shall mean the terms and conditions for use, reproduction, and distribution as defined by Sections 1 through 9 of this document.
 * 
 * “Licensor” shall mean the copyright owner or entity authorized by the copyright owner that is granting the License.
 * 
 * “Legal Entity” shall mean the union of the acting entity and all other entities that control, are controlled by, or are under common control with that entity. For the purposes of this definition, “control” means (i) the power, direct or indirect, to cause the direction or management of such entity, whether by contract or otherwise, or (ii) ownership of fifty percent (50%) or more of the outstanding shares, or (iii) beneficial ownership of such entity.
 * 
 * “You” (or “Your”) shall mean an individual or Legal Entity exercising permissions granted by this License.
 * 
 * “Source” form shall mean the preferred form for making modifications, including but not limited to software source code, documentation source, and configuration files.
 * 
 * “Object” form shall mean any form resulting from mechanical transformation or translation of a Source form, including but not limited to compiled object code, generated documentation, and conversions to other media types.
 * 
 * “Work” shall mean the work of authorship, whether in Source or Object form, made available under the License, as indicated by a copyright notice that is included in or attached to the work (an example is provided in the Appendix below).
 * 
 * “Derivative Works” shall mean any work, whether in Source or Object form, that is based on (or derived from) the Work and for which the editorial revisions, annotations, elaborations, or other modifications represent, as a whole, an original work of authorship. For the purposes of this License, Derivative Works shall not include works that remain separable from, or merely link (or bind by name) to the interfaces of, the Work and Derivative Works thereof.
 * 
 * “Contribution” shall mean any work of authorship, including the original version of the Work and any modifications or additions to that Work or Derivative Works thereof, that is intentionally submitted to Licensor for inclusion in the Work by the copyright owner or by an individual or Legal Entity authorized to submit on behalf of the copyright owner. For the purposes of this definition, “submitted” means any form of electronic, verbal, or written communication sent to the Licensor or its representatives, including but not limited to communication on electronic mailing lists, source code control systems, and issue tracking systems that are managed by, or on behalf of, the Licensor for the purpose of discussing and improving the Work, but excluding communication that is conspicuously marked or otherwise designated in writing by the copyright owner as “Not a Contribution.”
 * 
 * “Contributor” shall mean Licensor and any individual or Legal Entity on behalf of whom a Contribution has been received by Licensor and subsequently incorporated within the Work.
 * 
 * 2. Grant of Copyright License. Subject to the terms and conditions of this License, each Contributor hereby grants to You a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable copyright license to reproduce, prepare Derivative Works of, publicly display, publicly perform, sublicense, and distribute the Work and such Derivative Works in Source or Object form.
 * 
 * 3. Grant of Patent License. Subject to the terms and conditions of this License, each Contributor hereby grants to You a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable (except as stated in this section) patent license to make, have made, use, offer to sell, sell, import, and otherwise transfer the Work, where such license applies only to those patent claims licensable by such Contributor that are necessarily infringed by their Contribution(s) alone or by combination of their Contribution(s) with the Work to which such Contribution(s) was submitted. If You institute patent litigation against any entity (including a cross-claim or counterclaim in a lawsuit) alleging that the Work or a Contribution incorporated within the Work constitutes direct or contributory patent infringement, then any patent licenses granted to You under this License for that Work shall terminate as of the date such litigation is filed.
 * 
 * 4. Redistribution. You may reproduce and distribute copies of the Work or Derivative Works thereof in any medium, with or without modifications, and in Source or Object form, provided that You meet the following conditions:
 * 
 * a) 	You must give any other recipients of the Work or Derivative Works a copy of this License; and
 * 
 * b) 	You must cause any modified files to carry prominent notices stating that You changed the files; and
 * 
 * c) 	You must retain, in the Source form of any Derivative Works that You distribute, all copyright, patent, trademark, and attribution notices from the Source form of the Work, excluding those notices that do not pertain to any part of the Derivative Works; and
 * 
 * d) 	If the Work includes a “NOTICE” text file as part of its distribution, then any Derivative Works that You distribute must include a readable copy of the attribution notices contained within such NOTICE file, excluding those notices that do not pertain to any part of the Derivative Works, in at least one of the following places: within a NOTICE text file distributed as part of the Derivative Works; within the Source form or documentation, if provided along with the Derivative Works; or, within a display generated by the Derivative Works, if and wherever such third-party notices normally appear. The contents of the NOTICE file are for informational purposes only and do not modify the License. You may add Your own attribution notices within Derivative Works that You distribute, alongside or as an addendum to the NOTICE text from the Work, provided that such additional attribution notices cannot be construed as modifying the License. 
 * 
 * You may add Your own copyright statement to Your modifications and may provide additional or different license terms and conditions for use, reproduction, or distribution of Your modifications, or for any such Derivative Works as a whole, provided Your use, reproduction, and distribution of the Work otherwise complies with the conditions stated in this License. 
 * 
 * 5. Submission of Contributions. Unless You explicitly state otherwise, any Contribution intentionally submitted for inclusion in the Work by You to the Licensor shall be under the terms and conditions of this License, without any additional terms or conditions. Notwithstanding the above, nothing herein shall supersede or modify the terms of any separate license agreement you may have executed with Licensor regarding such Contributions.
 * 
 * 6. Trademarks. This License does not grant permission to use the trade names, trademarks, service marks, or product names of the Licensor, except as required for reasonable and customary use in describing the origin of the Work and reproducing the content of the NOTICE file.
 * 
 * 7. Disclaimer of Warranty. Unless required by applicable law or agreed to in writing, Licensor provides the Work (and each Contributor provides its Contributions) on an “AS IS” BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied, including, without limitation, any warranties or conditions of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A PARTICULAR PURPOSE. You are solely responsible for determining the appropriateness of using or redistributing the Work and assume any risks associated with Your exercise of permissions under this License.
 * 
 * 8. Limitation of Liability. In no event and under no legal theory, whether in tort (including negligence), contract, or otherwise, unless required by applicable law (such as deliberate and grossly negligent acts) or agreed to in writing, shall any Contributor be liable to You for damages, including any direct, indirect, special, incidental, or consequential damages of any character arising as a result of this License or out of the use or inability to use the Work (including but not limited to damages for loss of goodwill, work stoppage, computer failure or malfunction, or any and all other commercial damages or losses), even if such Contributor has been advised of the possibility of such damages.
 * 
 * 9. Accepting Warranty or Additional Liability. While redistributing the Work or Derivative Works thereof, You may choose to offer, and charge a fee for, acceptance of support, warranty, indemnity, or other liability obligations and/or rights consistent with this License. However, in accepting such obligations, You may act only on Your own behalf and on Your sole responsibility, not on behalf of any other Contributor, and only if You agree to indemnify, defend, and hold each Contributor harmless for any liability incurred by, or claims asserted against, such Contributor by reason of your accepting any such warranty or additional liability.
 */
/*
 * Qcoroutine is developed based on libco , the change log from libco:
 * 1) replace the api names adaption to qstack
 * 2) modification related to structure changes
 * 3) add q_coYield_to() interface
 *
 * Author:Hui Song
 */
/*----------------------------------------------------------------------------*/
/**
 * @file q_coroutine.c
 * @brief qstack coroutine framework 
 * @author Hui Song  
 * @date 2018.8.19 
 * Function list: \n
 *	1.q_coCreate(): initialize coroutine and allocate memory \n
 *	2.q_coResume(): resume a coroutine \n
 *	3.q_coYield():  coroutine yield out \n
 *	4.q_coYield_ct(): get an element from queue's tail without remove \n
 *  5.q_coRelease():free the coroutine memeory
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *	1. Date: 2018.8.19 
 *	   Author: Song Hui
 *	   Modification: create
 */
/******************************************************************************/
#include "routine.h"
#include "context.h"
#include "qepoll.h"
	 
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
	 
#include <sys/time.h>
#include <errno.h>
	 
#include <assert.h>
	 
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <limits.h>
/******************************************************************************/

extern void coctx_swap( qcoctx_t *,qcoctx_t* ) asm("coctx_swap");

static qCoEnv_t *q_CoEnvPerThread[ROSTACK_SIZE] = { 0 };

/**
 * get the current thread pid
 * @return current thread pid
 * @ref q_coroutine.h
 * @see
 * @note
 */ 
static pid_t GetPid()
{
    static __thread pid_t pid = 0;
    static __thread pid_t tid = 0;
    if( !pid || !tid || pid != getpid() )
    {
        pid = getpid();
#if defined( __APPLE__ )
		tid = syscall( SYS_gettid );
		if( -1 == (long)tid )
		{
			tid = pid;
		}
#elif defined( __FreeBSD__ )
		syscall(SYS_thr_self, &tid);
		if( tid < 0 )
		{
			tid = pid;
		}
#else 
        tid = syscall( __NR_gettid );
#endif

    }
    return (tid % ROSTACK_SIZE);

}

/**
 * allocate stack memory
 * @param[in] stack_size  stack size of a coroutine
 * @return stack memory structure
 * @ref q_coroutine.h
 * @see
 * @note
 * 	the last byte is set to be 0x5a to detect stack overflow
 */ 
qStackMem_t* 
q_coStack_alloc(unsigned int stack_size)
{
	qStackMem_t* stack_mem = (qStackMem_t*)malloc(sizeof(qStackMem_t));
	stack_mem->occupy_co= NULL;
	stack_mem->stack_size = stack_size;
	stack_mem->stack_buffer = (char*)malloc(stack_size);
	stack_mem->stack_buffer[stack_size-1] = 0x5a;
	stack_mem->stack_buffer[0] = 0x5a;
	stack_mem->stack_bp = stack_mem->stack_buffer + stack_size;
	return stack_mem;
}

/**
 * get current running coroutine 
 * @return current running coroutine 
 * @ref q_coroutine.h
 * @see
 * @note
 */
qCoEnv_t*
q_getCoCurrThread()
{
	pid_t ret = GetPid();
	return q_CoEnvPerThread[ ret ];
}

qCoroutine_t*
q_getCurrCo( qCoEnv_t *env )
{
	return env->pCallStack[ env->qCallStackSize - 1 ];
}
qCoroutine_t*
q_getCurrThreadCo( )
{
	qCoEnv_t *env = q_getCoCurrThread();
	if( !env ) return 0;
	return q_getCurrCo(env);
}

qCoroutine_t*
q_coSelf()
{
	return q_getCurrThreadCo();
}

void save_stack_buffer(qCoroutine_t* occupy_co)
{
	///copy out
	qStackMem_t* stack_mem = occupy_co->stack_mem;
	int len = stack_mem->stack_bp - occupy_co->stack_sp;

	if (occupy_co->save_buffer)
	{
		free(occupy_co->save_buffer), occupy_co->save_buffer = NULL;
	}

	occupy_co->save_buffer = (char*)malloc(len); //malloc buf;
	occupy_co->save_size = len;

	memcpy(occupy_co->save_buffer, occupy_co->stack_sp, len);
}

void co_swap(qCoroutine_t* curr, qCoroutine_t* pending_co)
{
 	qCoEnv_t* env = q_getCoCurrThread();

	//get curr stack sp
	char c;
	curr->stack_sp= &c;

	if (!pending_co->cIsShareStack)
	{
		env->pending_co = NULL;
		env->occupy_co = NULL;
	}
	else 
	{
		env->pending_co = pending_co;
		//get last occupy co on the same stack mem
		qCoroutine_t* occupy_co = pending_co->stack_mem->occupy_co;
		//set pending co to occupy thest stack mem;
		pending_co->stack_mem->occupy_co = pending_co;

		env->occupy_co = occupy_co;
		if (occupy_co && occupy_co != pending_co)
		{
			save_stack_buffer(occupy_co);
		}
	}

	//swap context
	coctx_swap(&(curr->ctx),&(pending_co->ctx) );

	//stack buffer may be overwrite, so get again;
	qCoEnv_t* curr_env = q_getCoCurrThread();
	qCoroutine_t* update_occupy_co =  curr_env->occupy_co;
	qCoroutine_t* update_pending_co = curr_env->pending_co;
	
	if (update_occupy_co && update_pending_co && update_occupy_co != update_pending_co)
	{
		//resume stack buffer
		if (update_pending_co->save_buffer && update_pending_co->save_size > 0)
		{
			memcpy(update_pending_co->stack_sp, update_pending_co->save_buffer, update_pending_co->save_size);
		}
	}
}

void q_coYield_env( qCoEnv_t *env )
{
	
	qCoroutine_t *last = env->pCallStack[ env->qCallStackSize - 2 ];
	qCoroutine_t *curr = env->pCallStack[ env->qCallStackSize - 1 ];

	env->qCallStackSize--;
	co_swap( curr, last);
}


static int 
CoRoutineFunc( qCoroutine_t *co,void * s)
{
	if( co->pfn )
	{
		co->pfn( co->arg );
	}
	co->cEnd = 1;

	qCoEnv_t *env = co->env;

	q_coYield_env( env );

	return 0;
}


/**
 * initialize coroutine parametres and allocate stack memory
 * @param[out] ppco  coroutine 
 * @param[in] attr 	 coroutine attr including stack size
 * @param[in] pfn    coroutine callback function
 * @param[in] arg    args for callback function
 * @return a coroutine structure
 * @ref q_coroutine.h
 * @see
 * @note
 */ 
qCoroutine_t* 
q_coCreate_env( qCoEnv_t *env, const qCoAttr_t* attr,
		pfn_coroutine_t pfn,void *arg )
{

	qCoAttr_t at;
	if( attr )
	{
		memcpy( &at,attr,sizeof(at) );
	}
	if( at.stack_size <= 0 )
	{
		at.stack_size = 128 * 1024;
	}
	else if( at.stack_size > 1024 * 1024 * 8 )
	{
		at.stack_size = 1024 * 1024 * 8;
	}

	if( at.stack_size & 0xFFF ) 
	{
		at.stack_size &= ~0xFFF;
		at.stack_size += 0x1000;
	}

	qCoroutine_t *lp = (qCoroutine_t*)malloc( sizeof(qCoroutine_t) );
	
	memset( lp,0,(long)(sizeof(qCoroutine_t))); 


	lp->env = env;
	lp->pfn = pfn;
	lp->arg = arg;

	qStackMem_t* stack_mem = NULL;
	
	stack_mem = q_coStack_alloc(at.stack_size);
	
	lp->stack_mem = stack_mem;

	lp->ctx.ss_sp = stack_mem->stack_buffer;
	lp->ctx.ss_size = at.stack_size;

	lp->cStart = 0;
	lp->cEnd = 0;
	lp->cIsMain = 0;
	lp->cIsShareStack = 0;//at.share_stack != NULL;

	lp->save_size = 0;
	lp->save_buffer = NULL;

	return lp;
}


/**
 * initialize a coroutine and allocate memory
 * @param[out] ppco  coroutine 
 * @param[in] attr 	 coroutine attr including stack size
 * @param[in] pfn    coroutine callback function
 * @param[in] arg    args for callback function
 * @return 0
 * @ref q_coroutine.h
 * @see
 * @note
 */ 
int	 
q_coCreate( qCoroutine_t **ppco,const qCoAttr_t *attr,pfn_coroutine_t pfn,void *arg )
{
	if( !q_getCoCurrThread() ) 
	{
		pid_t pid = GetPid();	
		q_CoEnvPerThread[ pid ] = (qCoEnv_t*)calloc( 1,sizeof(qCoEnv_t) );
		qCoEnv_t *env = q_CoEnvPerThread[ pid ];

		env->qCallStackSize = 0;
		qCoroutine_t *self = q_coCreate_env( env, NULL, NULL,NULL );
		self->cIsMain = 1;

		env->pending_co = NULL;
		env->occupy_co = NULL;

		coctx_init( &self->ctx );

		env->pCallStack[ env->qCallStackSize++ ] = self;
	}
	qCoroutine_t *co = q_coCreate_env( q_getCoCurrThread(), attr, pfn,arg );
	*ppco = co;
	return 0;
}
void	 
q_coResume( qCoroutine_t *co )
{
	qCoEnv_t *env = co->env;
	qCoroutine_t *lpCurrRoutine = env->pCallStack[ env->qCallStackSize - 1 ];
	if( !co->cStart )
	{
		coctx_make( &co->ctx,(coctx_pfn_t)CoRoutineFunc,co,0 );
		co->cStart = 1;
	}
	env->pCallStack[ env->qCallStackSize++ ] = co;
	co_swap( lpCurrRoutine, co );
}

void
q_coYield_to(qCoroutine_t *curr_co, qCoroutine_t *pend_co)
{	
	if( !pend_co->cStart )
	{
		coctx_make( &pend_co->ctx, (coctx_pfn_t)CoRoutineFunc, pend_co, 0 );
		pend_co->cStart = 1;
	}
	co_swap(curr_co, pend_co);
}

void	 
q_coYield( qCoroutine_t *co )
{
	q_coYield_env( co->env );
}
void	 
q_coYield_ct() //ct = current thread
{
	q_coYield_env( q_getCoCurrThread() );
}
void q_coFree( qCoroutine_t *co )
{
    if (!co->cIsShareStack) 
    {    
        free(co->stack_mem->stack_buffer);
        free(co->stack_mem);
    }   
    free( co );
}

void	 
q_coRelease( qCoroutine_t *co )
{	
	q_coFree( co );
}

void 
q_coInit_env()
{
	pid_t pid = GetPid();	
	q_CoEnvPerThread[ pid ] = (qCoEnv_t*)calloc( 1,sizeof(qCoEnv_t) );
	qCoEnv_t *env = q_CoEnvPerThread[ pid ];

	env->qCallStackSize = 0;
	qCoroutine_t *self = q_coCreate_env( env, NULL, NULL,NULL );
	self->cIsMain = 1;

	env->pending_co = NULL;
	env->occupy_co = NULL;

	coctx_init( &self->ctx );

	env->pCallStack[ env->qCallStackSize++ ] = self;
}
