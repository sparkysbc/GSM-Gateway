/*
 * liballogsmat: An implementation of ALLO GSM cards
 *
 * Parts taken from libpri
 * Written by mark.liu <mark.liu@openvox.cn>
 *
 * $Id: module.h 60 2010-09-09 07:59:03Z liuyuan $
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 *
 */
 
#ifndef __GSM_MODULE_H__
#define __GSM_MODULE_H__

#define SENT_SUCCESS	1
#define SENT_FAILED	2

//extern int module_start(struct allogsm_modul *gsm);

extern int module_restart(struct allogsm_modul *gsm);

extern int module_dial(struct allogsm_modul *gsm, struct alloat_call *call);

extern int module_answer(struct allogsm_modul *gsm);

extern int module_senddtmf(struct allogsm_modul *gsm, int digit);

extern int module_hangup(struct allogsm_modul *gsm, struct alloat_call *c);

extern int module_send_ussd(struct allogsm_modul *gsm, char *message);

extern int module_send_text(struct allogsm_modul *gsm, char *destination, char *msg);

extern int module_send_pdu(struct allogsm_modul *gsm, char *pdu);

extern int module_send_pin(struct allogsm_modul *gsm, char *pin);

extern allogsm_event *module_receive(struct allogsm_modul *gsm, char *data, int len);

extern int print_signal_level(struct allogsm_modul *gsm, int level);

#ifdef CONFIG_CHECK_PHONE
/*Makes Add 2012-4-9 14:08*/
void module_hangup_phone(struct allogsm_modul *gsm);

int module_check_phone_stat(struct allogsm_modul *gsm, char *phone_number,int hangup_flag,unsigned int timeout);
#endif //CONFIG_CHECK_PHONE

#ifdef VIRTUAL_TTY
int module_mux_end(struct allogsm_modul *gsm, int restart_at_flow);
#endif //VIRTUAL_TTY

#endif

extern void gsm_callwaiting_sched(void *data);
extern void gsm_sms_sending_timeout(void *data);
extern void gsm_hangup_timeout(void *data);
extern void gsm_cmd_sched(void *data);
extern void gsm_sim_waiting_sched(void *data);
extern void gsm_sms_service_waiting(void *data);
