/*
 * Copyright (c) 2005-2010 Thierry FOURNIER
 * $Id: sens.c 87 2006-05-09 07:58:27Z thierry $
 *
 */

#include "config.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "arpalert.h"
#include "loadconfig.h"
#include "data.h"
#include "sens.h"
#include "log.h"

// hash table size ; this number must be primary number
#define HASH_SIZE 1999

/* debug: */
// #define DEBUG 1

/* HACHAGE */
#define sens_hash(a, b, c) ( ( ( (u_char)(a->octet[4]) << 8 ) + (u_char)(a->octet[5]) + \
                           (u_int32_t)(b.ip) + (u_int32_t)(c) ) % HASH_SIZE )

#define BUF_SIZE 1024
#define MAC_ADRESS_MAX_LEN 17
#define IP_ADRESS_MAX_LEN 15
#define MASK_MAX_LEN 2

// masks 
#define END_OF_MASKS 0x00000001

// conv binary mask to ip style mask
const u_int32_t dec_to_bin[33] = {
	0x00000000,
	0x80000000,
	0xc0000000,
	0xe0000000,
	0xf0000000,
	0xf8000000,
	0xfc000000,
	0xfe000000,
	0xff000000,
	0xff800000,
	0xffc00000,
	0xffe00000,
	0xfff00000,
	0xfff80000,
	0xfffc0000,
	0xfffe0000,
	0xffff0000,
	0xffff8000,
	0xffffc000,
	0xffffe000,
	0xfffff000,
	0xfffff800,
	0xfffffc00,
	0xfffffe00,
	0xffffff00,
	0xffffff80,
	0xffffffc0,
	0xffffffe0,
	0xfffffff0,
	0xfffffff8,
	0xfffffffc,
	0xfffffffe,
	0xffffffff
};

/* structures */
struct pqt {
	data_mac mac;
	data_ip ip_d;
	u_int32_t mask;
	struct pqt *next;
};

/* hash */
struct pqt *pqt_h[HASH_SIZE];

/* mask list */
u_int32_t used_masks[33];

void sens_init(void) {
	int fd;
	char buf[BUF_SIZE];
	int read_size;
	char *parse;
	char *find;
	char current[IP_ADRESS_MAX_LEN + MASK_MAX_LEN + 2];
	int  current_count=0;
	char cur_dec = 0; // current type read: 0: null; 1: ip; 2: mac; 3: comment
	data_mac last_mac;
	u_int32_t ip, mask;
	u_int line = 1;
	int i, j;
	int flag_mask = FALSE;
	char sort_tmp;
	char list_mask[33];

	memset(&pqt_h, 0, HASH_SIZE * sizeof(struct pqt *));
	memset(&list_mask, -1, 33);

	if(config[CF_AUTHFILE].valeur.string[0]==0)return;

	// open config file
	fd = open(config[CF_AUTHFILE].valeur.string, O_RDONLY);
	if(fd == -1){
		logmsg(LOG_ERR, "[%s %i] didn't find authorization file %s",
		       __FILE__, __LINE__, config[CF_AUTHFILE].valeur.string);
		exit(1);
	}

	// parsing acces file
	current[0] = 0;
	do {
		read_size = read(fd, buf, BUF_SIZE);
		if(read_size < BUF_SIZE){
			buf[read_size] = '\n';
			read_size++;
		}

		parse = buf;
		while(parse < &buf[read_size]){
			if(*parse == '\r'){
				parse++;
				continue;
			}

			if(*parse == ' '  ||
			   *parse == '\t' ||
			   *parse == ']' ||
			   *parse == '\n' ){
				if(cur_dec == 1){
					current[current_count] = 0;
					find = &current[0];
					mask = 32;
					while(*find != 0){
						if(*find=='/'){
							*find = 0;
							find++;
							mask = atoi(find);
							break;
						}
						find++;
					}
					ip = str_to_ip(current);

					// network address validation
					if( (ip & dec_to_bin[mask]) != ip){
						logmsg(LOG_ERR, "[%s %i] error in config file \"%s\" "
						       "at line %d: the value %s/%u are incorrect",
						       __FILE__, __LINE__, config[CF_AUTHFILE].valeur.string,
						       line, current, mask);
						exit(1);
					}

					// add this network value in hash
					sens_add(&last_mac, (data_ip)ip, dec_to_bin[mask]);
					
					// find next free position in mask_list or mask itself
					i=0;
					while(list_mask[i] != mask && list_mask[i] != -1){
						i++;
					}
					if(list_mask[i] == -1){
						list_mask[i] = mask;
					}
					
					current[0] = 0;
					cur_dec = 0;
				}
				if(cur_dec == 2){
					current[current_count] = 0;
					str_to_mac(current, &last_mac);
					current[0] = 0;
					cur_dec = 0;
				}
				if(cur_dec == 3 && *parse == '\n'){
					cur_dec = 0;
				}
				if(*parse == '\n'){
					line++;
				}
				current_count = 0;
				parse++;
				continue;
			}
			
			if(*parse == '#'){
				cur_dec = 3;
				parse++;
				continue;
			}
			
			if(*parse == '[' && cur_dec != 3){
				cur_dec = 2;
				parse++;
				continue;
			}

			if(cur_dec != 2 && cur_dec != 3) cur_dec = 1;
			
			if(cur_dec == 1){
				if(current_count == IP_ADRESS_MAX_LEN &&
				   flag_mask == FALSE && *parse != '/'){
					// syntax error
					logmsg(LOG_ERR, "[%s %d] syntax error decoding IP at line %d",
					       __FILE__, __LINE__, line);
					exit(1);
				}
				if(current_count == IP_ADRESS_MAX_LEN + MASK_MAX_LEN + 1 &&
				   flag_mask == TRUE){
					logmsg(LOG_ERR, "[%s %d] syntax error decoding IP at line %d",
					       __FILE__, __LINE__, line);
					exit(1);
				}
				current[current_count] = *parse;
				if(*parse == '/'){
					flag_mask = TRUE;
				}
				current_count ++;
				parse ++;
				continue;
			}
			
			if(cur_dec == 2){
				if(current_count == MAC_ADRESS_MAX_LEN){
					//syntax error
					logmsg(LOG_ERR, "[%s %d] syntax error decoding IP at line %d",
					       __FILE__, __LINE__, line);
					exit(1);
				}
				current[current_count] = *parse;
				current_count ++;
				parse++;
				continue;
			}
			
			parse++;
		}
	} while(read_size == BUF_SIZE);

	close(fd);
	
	// sort list_mask
	for(i=0; i<32; i++){
		for(j=32; j>i; j--){
			if(list_mask[j] > list_mask[j-1]){
				sort_tmp = list_mask[j-1];
				list_mask[j-1] = list_mask[j];
				list_mask[j] = sort_tmp;
			}
		}
		// convert decimal mask to binary mask
		if(list_mask[i] != -1){
			used_masks[i] = dec_to_bin[(u_char)list_mask[i]];
		} else {
			used_masks[i] = END_OF_MASKS;
		}
	}
}

// add data to hash
void sens_add(data_mac *mac, data_ip ipb, u_int32_t mask){
	u_int h;
	struct pqt *mpqt;

	mpqt = (struct pqt *)malloc(sizeof(struct pqt));
	if(mpqt == NULL){
		logmsg(LOG_ERR, "[%s %d] allocation memory error",
		       __FILE__, __LINE__);
		exit(1);
	}
	data_cpy(&mpqt->mac, mac);
	mpqt->ip_d = ipb;
	mpqt->mask = mask;

	// calculate hash
	h = sens_hash(mac, ipb, mask);
	// find a free space
	mpqt->next = pqt_h[h];
	pqt_h[h] = mpqt;
}

void sens_free(void){
	int i;
	struct pqt *free_pqt;
	struct pqt *current_pqt;

	for(i=0; i<HASH_SIZE; i++){
		current_pqt = pqt_h[i];
		while(current_pqt != NULL){
			free_pqt = current_pqt;
			current_pqt = current_pqt->next;
			free(free_pqt);
		}
		pqt_h[i] = NULL;
	}
}

void sens_reload(void){
	sens_free();
	sens_init();
}

int sens_exist(data_mac *mac, data_ip ipb){
	u_int h;
	struct pqt *spqt;
	u_int32_t *masks = &used_masks[0];
	data_ip ip;

	// test all masks
	while(*masks != END_OF_MASKS){
		
		// apply mask
		ip.ip = ipb.ip & *masks;

		// get data in hash
		h = sens_hash(mac, ip, (*masks));
		spqt = pqt_h[h];

		// find data
		while(spqt != NULL){
			if(spqt->ip_d.ip == ip.ip &&
			   spqt->mask == *masks &&
			   data_cmp(&spqt->mac, mac) == 0 ){
				return(TRUE);
			}
			spqt = spqt->next;
		}
		masks++;
	}
	return(FALSE);
}


