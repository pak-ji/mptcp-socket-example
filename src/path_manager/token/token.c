#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <byteswap.h>

#include "token.h"

uint32_t get_token(int loc_rem)
{
	FILE *fp = NULL;
	char line[1024];
	int count = 0;

	char *loc_token;
	char *rem_token;

	uint32_t l_token;
	uint32_t r_token;
	uint32_t loc_tkn;
	uint32_t rem_tkn;

	fp = popen("cat /proc/net/mptcp_net/mptcp", "r");
	if(fp == NULL){
		perror("popen() : ");
		return -1;
	}

	while(fgets(line, 1024, fp) != NULL){
		if(count == 1) {
			strtok(line, " ");
			loc_token = strtok(NULL, " ");
			rem_token = strtok(NULL, " ");
			break;
		}
		count++;
	}
	pclose(fp);

	//printf("loc tokcn : %s\n", loc_token);
	//printf("rem tokcn : %s\n", rem_token);
	//printf("\n");

	l_token = strtoul(loc_token, NULL, 16);
	r_token = strtoul(rem_token, NULL, 16);

	//printf("l token : %X\n", l_token);
	//printf("r token : %X\n", r_token);
	//printf("\n");

	loc_tkn = bswap_32(l_token);
	rem_tkn = bswap_32(r_token);

	//printf("toc tkn : %X\n", loc_tkn);
	//printf("rem tkn : %X\n", rem_tkn);
	
	if(loc_rem == 1)
		return l_token;
	else
		return r_token;
}
