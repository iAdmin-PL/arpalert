/*
 * Copyright (c) 2005-2010 Thierry FOURNIER
 * $Id: log.c 223 2006-10-05 19:44:46Z thierry $
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef USE_SYSLOG
#include <syslog.h>
#endif

#include "arpalert.h"
#include "log.h"
#include "loadconfig.h"

extern int errno;

FILE *lf;
const char *mois[12] = {
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};

void initlog(void){
	#ifdef USE_SYSLOG
	if(config[CF_USESYSLOG].valeur.integer == TRUE){
		openlog("arpalert", LOG_CONS, LOG_DAEMON);
	}
	#endif
	if(config[CF_LOGFILE].valeur.string != NULL){
		lf = fopen(config[CF_LOGFILE].valeur.string, "a");
		if(lf == NULL){
			fprintf(stderr, "[%s %d] fopen[%d]: %s\n",
			        __FILE__, __LINE__, errno, strerror(errno));
			exit(1);
		}
	}
}

void logmsg(int priority, const char *fmt, ...){
	va_list ap;
	char msg[4096];
	struct tm *tm;

	// return if do not log in file or on standard output
	if(
		// check if I do log this priority
		priority > config[CF_LOGLEVEL].valeur.integer ||

		(
			config[CF_LOGFILE].valeur.string == NULL &&
			config[CF_DAEMON].valeur.integer == TRUE
			#ifdef USE_SYSLOG
			&& config[CF_USESYSLOG].valeur.integer == FALSE
			#endif
		)
	){
		return;
	}

	//get current tim 
	tm = localtime(&current_time);

	va_start(ap, fmt);
	vsnprintf(msg, 4096, fmt, ap);
	va_end(ap);

	#ifdef USE_SYSLOG
	if(config[CF_USESYSLOG].valeur.integer == TRUE){
		syslog(priority, msg); 
	}
	#endif

	if(config[CF_LOGFILE].valeur.string != NULL){
		fprintf(lf, "%s % 2d %02d:%02d:%02d arpalert: %s\n",
		        mois[tm->tm_mon],
		        tm->tm_mday,
		        tm->tm_hour,
		        tm->tm_min,
		        tm->tm_sec, 
		        //for year: tm->tm_year+1900,
		        msg);
		fflush(lf);
	}

	if(config[CF_DAEMON].valeur.integer == FALSE){
		printf("%s % 2d %02d:%02d:%02d arpalert:  %s\n", 
		        mois[tm->tm_mon],
		        tm->tm_mday,
		        tm->tm_hour,
		        tm->tm_min,
		        tm->tm_sec, 
		        msg);
	}
}

