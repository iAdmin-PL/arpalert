/*
 * Copyright (c) 2005-2010 Thierry FOURNIER
 * $Id: alerte.h 313 2006-10-16 12:54:40Z thierry $
 *
 */

#ifndef __ALERTE_H__
#define __ALERTE_H__

// send new alert
void alerte(char *, char *, char *, int);

// init memory structurs
void alerte_init(void);

// return the next timeout and the functionn to call
void *alerte_next(struct timeval *tv);

// check validity of all current alert scripts
void alerte_check(void);

//void alerte_kill_pid(int signal);

#endif
