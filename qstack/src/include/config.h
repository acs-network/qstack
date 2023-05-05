/*
* mTCP source code is distributed under the Modified BSD Licence.
* 
* Copyright (C) 2015 EunYoung Jeong, Shinae Woo, Muhammad Jamshed, Haewon Jeong, 
* Sunghwan Ihm, Dongsu Han, KyoungSoo Park
* 
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the <organization> nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**
 * @file config.h
 * @brief
 * @author Guo ziting (guoziting@ict.ac.cn)
 * @date 2018.8.31
 * @version 0.1
 * @detail Function list: \n
 *   1. \n
 *   2. \n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.8.31
 *   	Author: Guo ziting
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __CONFIG_H_
#define __CONFIG_H_
/******************************************************************************/
#include "universal.h"
/******************************************************************************/
/* global macros */
#define MAX_DEVICES		20
extern int num_cpus;
extern int num_queues;
extern int num_devices;

extern int num_devices_attached;
extern int devices_attached[MAX_DEVICES];
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* data structures */
/******************************************************************************/
/* function declarations */
/**
 * load configuration from config file
 *
 * @param fname file name
 *
 * @return
 */
int 
load_configuration(const char *fname);

/**
 * set configurations from the setted interface information
 *
 * @param none
 *
 * @return
 */
int 
set_interface_info();

/**
 * set configurations from files
 *
 * @param none
 *
 * @return
 */
int 
set_routing_table();

/**
 * load ARP table
 *
 * @param 
 *
 * @return
 */
int 
load_arp_table();

/**
 * print setted configuration
 *
 * @param
 *
 * @return
 */
void 
print_configuration();
/*----------------------------------------------------------------------------*/
void 
print_interface_info();

void 
print_routing_table();

int 
set_socket_mode(int8_t socket_mode);

uint32_t 
mask_from_prefix(int prefix);

void 
parse_mac_address(unsigned char *haddr, char *haddr_str);

void
set_host_ip(char *optstr);

int 
parse_ip_address(uint32_t *ip_addr, char *ip_str);
/******************************************************************************/
/* inline functions */
/******************************************************************************/
/*----------------------------------------------------------------------------*/
/**
 *
 * @param
 * @param[out]
 * @return
 *
 * @ref
 * @see
 * @note
 */
#endif //ifdef __CONFIG_H_
