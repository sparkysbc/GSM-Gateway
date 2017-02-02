/*
 * liballogsmat: An implementation of ALLO GSM cards
 *
 * Written by mark.liu <mark.liu@openvox.cn>
 * 
 * Modified by freedom.huang <freedom.huang@openvox.cn>
 * 
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#include "liballogsmat.h"
#include "gsm_internal.h"
#include "gsm_module.h"
#include "gsm_config.h"

//#define TESTING 1
#define SIERRA_HL6	1

static char* trim_CRLF( char *String )
{
#define ISCRLF(x) ((x)=='\n'||(x)=='\r'||(x)==' ')

	char *Tail, *Head;
	for ( Tail = String + strlen( String ) - 1; Tail >= String; Tail-- ) {
		if (!ISCRLF( *Tail ))
			break;
	}
	Tail[1] = 0;

	for ( Head = String; Head <= Tail; Head ++ ) {
		if ( !ISCRLF( *Head ) )
			break;
	}

	if ( Head != String )
		memcpy( String, Head, ( Tail - Head + 2 ) * sizeof(char) );

	return String;
}

int print_signal_level(struct allogsm_modul *gsm, int level){

	#define ESC 0x1b
	#define COLOR_BLACK 	30
	#define COLOR_GREEN	32
	#define COLOR_WHITE     37
	
	char tmp1[20]= "▁▃▄▆█";
	char tmp2[20]= "▁▃▄▆█";
	//char tmp1[20]= "#####";
	//char tmp2[20]= "#####";
	if (level<0){
		sprintf(gsm->coverage_level_string, "[%c[%dmX%s%c[%dm%c[0m][NOT DETECTABLE]\n", ESC, COLOR_WHITE,tmp2, ESC, COLOR_BLACK, ESC);
		return 0;
	}
	tmp1[(3*level)+1]='\0';
	sprintf(gsm->coverage_level_string, "[%c[%dm%s%c[%dm%s%c[%dm%c[0m][%d]\n", ESC, COLOR_GREEN, tmp1, ESC, COLOR_WHITE,tmp2+(3*level), ESC, COLOR_BLACK, ESC,level);
	return 0;
}
static void module_get_coverage(struct allogsm_modul *gsm, char *h)
{
	int coverage = -1;
	
	if (gsm_compare(h,get_at(gsm->switchtype,AT_CHECK_SIGNAL1))) {
		coverage = get_coverage1(gsm->switchtype,h);
	} else if (gsm_compare(h,get_at(gsm->switchtype,AT_CHECK_SIGNAL2))) {
		coverage = get_coverage2(gsm->switchtype,h);
	}
	if ((coverage) && (coverage != 99)) {
		gsm->coverage = coverage;
	} else {
		gsm->coverage = -1;
	}
	/*Signal Level calculation here*/


        // ASU ranges from 0 to 31 - TS 27.007 Sec 8.5
        // asu = 0 (-113dB or less) is very weak
        // signal, its better to show 0 bars to the user in such cases.
        // asu = 99 is a special case, where the signal strength is unknown.
	
	/*Custom Close to android but supporting 5 bar level*/	
	int  level = -1 ;
	if (coverage <= 0 || coverage == 99) level = -1;
	else if (coverage >= 16) level = 5;
	else if (coverage >= 12) level = 4;
	else if (coverage >= 8) level = 3;
	else if (coverage >= 4) level = 2;
        else if (coverage >= 2) level = 1;
        else level = 0;
#if 0
	/*Android*/
        if (coverage <= 0 || coverage == 99) level = 0;
        else if (coverage >= 16) level = 4;
        else if (coverage >= 8)  level = 3;
        else if (coverage >= 4)  level = 2;
        else level = 1;
	/*Older standards*/
	int  level = -1 ;
	if (coverage <= 2 || coverage == 99) level = -1;
	else if (coverage >= 23) level = 5;
	else if (coverage >= 20) level = 4;
	else if (coverage >= 18) level = 3;
	else if (coverage >= 15) level = 2;
        else if (coverage >= 13) level = 1;
        else level = 0;
#endif
	gsm->coverage_level = level;
	print_signal_level(gsm,level);
}
/*
int module_start(struct allogsm_modul *gsm) 
{
	gsm->resetting = 0;
	char tmp[5];
	gsm->wind_state=0;
        sprintf(tmp, "%c", 0x1A);
        gsm_transmit(gsm, tmp);
	gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_CHECK));

	return 0;
}
*/
int module_restart(struct allogsm_modul *gsm) 
{
	gsm->resetting = 1;
	gsm_switch_state(gsm, ALLOGSM_STATE_UP, get_at(gsm->switchtype,AT_ATZ));
	return 0;
}

int module_dial(struct allogsm_modul *gsm, struct alloat_call *call) 
{
	char buf[128];

	memset(buf, 0x0, sizeof(buf));
	
	get_dial_str(gsm->switchtype, call->callednum, buf, sizeof(buf));
#ifdef WAVECOM
#endif
	gsm_switch_state(gsm, ALLOGSM_STATE_CALL_INIT, buf);

	if (gsm->dial_initiated)
		gsm_message(gsm, "---------------here %d dial_initiated %d\n", __LINE__,gsm->dial_initiated);
        gsm->dial_retry = 0;
	gsm->dial_initiated = 0;
	return 0;
}

int module_answer(struct allogsm_modul *gsm) 
{
	gsm->answer_retry = 0;
	gsm_switch_state(gsm, ALLOGSM_STATE_PRE_ANSWER, get_at(gsm->switchtype,AT_ANSWER));
	return 0;
}

int module_senddtmf(struct allogsm_modul *gsm, int digit)
{
	char buf[512];
#ifdef SIERRA_HL6
	snprintf(buf,512,"AT+VTS=\"{%c,15}\"",digit);	/* duration is multiple of 10 ms */
#else
	snprintf(buf,512,"AT+VTS=%c,150",digit);
#endif
	
	return gsm_send_at(gsm, buf);
}

int module_hangup(struct allogsm_modul *gsm, struct alloat_call *c)
{
	if (gsm->state == ALLOGSM_STATE_CALL_WAITING){
		if(c->call_waiting_idx == 2)
			gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ_CALL_WAITING, "AT+CHLD=11");
		else if (c->call_waiting_idx == 1)
			gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ_CALL_WAITING, "AT+CHLD=12");
	}else{
		gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
		gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
	}
	if (gsm->dial_initiated)
		gsm_message(gsm, "---------------here %d dial_initiated %d\n", __LINE__,gsm->dial_initiated);
        gsm->dial_initiated_hangup = 0;
        gsm->dial_initiated = 0;
	
	return 0;
}

static char* get_cur_time(char* buf, int size)
{
	time_t  t;
	struct tm *ptm;
	int len = 0;

	time(&t);

	ptm = localtime(&t);
	//len =  strftime(buf,size, "%Y-%m-%d %H:%M:%S", ptm);
	len =  strftime(buf,size, "%H:%M:%S", ptm);
	buf[len] = '\0';

	return buf;
}
int module_send_safe_at(struct allogsm_modul *gsm, char *command) 
{	
	//gsm_message(gsm, "Sending AT command in safe mode\n");
	if (gsm->state == ALLOGSM_STATE_READY) {
		gsm_switch_state(gsm, ALLOGSM_STATE_SAFE_AT, command);
		return 0;
	}
	
	if (gsm->debug & ALLOGSM_DEBUG_AT_RECEIVED) {
		gsm_message(gsm, "Cannot send operator list query, waiting...\n");
	}
	
	return -1;
}

int module_send_operator_list(struct allogsm_modul *gsm) 
{	
	if (gsm->state == ALLOGSM_STATE_READY) {
		gsm_switch_state(gsm, ALLOGSM_STATE_OPERATOR_QUERY, "AT+COPS=?");
		return 0;
	}
	
	if (gsm->debug & ALLOGSM_DEBUG_AT_RECEIVED) {
		gsm_message(gsm, "Cannot send operator list query, waiting...\n");
	}
	
	return -1;
}
int module_send_ussd(struct allogsm_modul *gsm, char *message) 
{	
	if (gsm->state == ALLOGSM_STATE_READY) {
//		char time_buf[20];
		char buf[1024];
/*		get_cur_time(time_buf,20);
		gsm_message(gsm, "Send USSD on span %d at %s\n",gsm->span,time_buf);*/
		snprintf(gsm->ussd_sent_buffer, sizeof(gsm->ussd_sent_buffer) - 1, "%s", message);
		get_ussd_str(gsm->switchtype, message, buf, sizeof(buf));
		gsm_switch_state(gsm, ALLOGSM_STATE_USSD_SENDING, buf);
		return 0;
	}
	
	if (gsm->debug & ALLOGSM_DEBUG_AT_RECEIVED) {
		gsm_message(gsm, "Cannot send USSD when not ready, waiting...\n");
	}
	
	return -1;
}

int module_send_text(struct allogsm_modul *gsm, char *destination, char *message) 
{	
	if (gsm->state == ALLOGSM_STATE_READY) {
		char time_buf[20];
		get_cur_time(time_buf,20);
		gsm_message(gsm, "Send SMS  to %s on span %d at %s %d \n",gsm->sms_info->txt_info.destination,gsm->span,time_buf,__LINE__);
		snprintf(gsm->sms_sent_buffer, sizeof(gsm->sms_sent_buffer) - 1, "%s", message);
		gsm->sms_retry=0;
		gsm->sms_old_retry=-1;
//		gsm_send_at(gsm, "AT+CMMS=2"); /*Added for testing purpose, later commented*/

		gsm->smsTimeoutSched = gsm_schedule_event(gsm, 30000, gsm_sms_sending_timeout, gsm);

		gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SENDING, get_at(gsm->switchtype,AT_SEND_SMS_TEXT_MODE));
		return 0;
	}
	
	if (gsm->debug & ALLOGSM_DEBUG_AT_RECEIVED) {
		gsm_message(gsm, "Cannot send SMS when not ready, waiting...\n");
	}
	
	return -1;
}

int module_send_pdu( struct allogsm_modul *gsm, char *pdu)
{	
	if ((gsm->state == ALLOGSM_STATE_READY) && (gsm->dial_initiated != 1)) {
		char time_buf[20];
		get_cur_time(time_buf,20);
		gsm_message(gsm, "Send SMS to %s on span %d at %s %d\n",gsm->sms_info->txt_info.destination,gsm->span,time_buf,__LINE__);
		snprintf(gsm->sms_sent_buffer, sizeof(gsm->sms_sent_buffer) - 1, "%s", pdu);

		gsm->sms_retry=0;
		gsm->sms_old_retry=-1;

		gsm->smsTimeoutSched = gsm_schedule_event(gsm, 30000, gsm_sms_sending_timeout, gsm);
		gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SENDING, get_at(gsm->switchtype,AT_SEND_SMS_PDU_MODE));
		return 0;
	} 
	
	if (gsm->debug & ALLOGSM_DEBUG_AT_RECEIVED) {
		gsm_message(gsm, "Cannot send PDU when not ready, waiting...\n");
	}
	
	return -1;
}

int module_set_echocansup(struct allogsm_modul *gsm, int echocanval){
	char buf[256];
	memset(buf, 0x0, sizeof(buf));
	/* If the SIM PIN is blocked */
	if (gsm->state == ALLOGSM_STATE_SET_ECHOCANSUP) {
		get_echocansup_str(gsm->switchtype, get_at(gsm->switchtype,AT_ECHOCANSUP),  echocanval, buf, sizeof(buf));
	}
	gsm_send_at(gsm, buf);

	return 0;
}

int module_set_gain(struct allogsm_modul *gsm, int gain)
{
	char buf[256];
	memset(buf, 0x0, sizeof(buf));
	/* If the SIM PIN is blocked */
	if (gsm->state == ALLOGSM_STATE_SET_MIC_VOL) {
		get_gain_str(gsm->switchtype, get_at(gsm->switchtype,AT_CMIC),  gain, buf, sizeof(buf));
	}else if (gsm->state == ALLOGSM_STATE_SET_SPEEK_VOL) {
		get_gain_str(gsm->switchtype, get_at(gsm->switchtype,AT_CLVL),  gain, buf, sizeof(buf));
	}
	gsm_send_at(gsm, buf);

	return 0;
}


int module_send_pin(struct allogsm_modul *gsm, char *pin)
{
	char buf[256];
	/* If the SIM PIN is blocked */
	if (gsm->state == ALLOGSM_STATE_SIM_PIN_REQ) {
		
		memset(buf, 0x0, sizeof(buf));
		get_pin_str(gsm->switchtype, pin, buf, sizeof(buf));
		gsm_send_at(gsm, buf);
	}

	return 0;
}

char *remove_all_chars(char* str, char c) {
    if (str==NULL)
        return (str);

    char *pr = str, *pw = str;
    while (*pr) {
        *pw = *pr++;
        pw += (*pw != c);
    }
    *pw = '\0';
    return(str);
}

#if 1
static void allogsm_update_count(struct allogsm_modul *gsm, int status)
{
        char filename[128]="/mnt/smsout_count/";
	int count = 0;
        char name[64];
        sprintf(name,"%s/%s", status == SENT_SUCCESS ? "success": "fail", gsm->sms_info->pdu_info.id);
        strcat(filename, name);

        FILE *f=fopen(filename, "a+");

        if (!f) {
                printf("Error: Cannot open the file for count update (%s) error (%s)\n",filename, strerror(errno));
                return;
        }
        fscanf(f, "%d", &count); /* Read count value here */

        if (freopen(filename, "w+", f) == NULL)
        {
                printf("Error: Cannot open the file for count update (%s) error (%s)\n",filename, strerror(errno));
                return;
        }

        fprintf(f, "%d", count+1); /* Write New count here */

        fclose(f);
	// system("/bin/sync");
	sync();
}
static void allogsm_save_sms(struct allogsm_modul *gsm, int status)
{
        char filename[128]="/mnt/";
        struct timeval tv;
        gettimeofday(&tv,NULL);

        char name[64];
        sprintf(name,"%s/%s", status == SENT_SUCCESS ? "smsout_success": "smsout_fail", gsm->sms_info->pdu_info.id);
        strcat(filename, name);

        FILE *f=fopen(filename, "a+");

        if (!f) {
                printf("Error: Cannot save Failed sms at (%s) error (%s)\n",filename, strerror(errno));
                return;
        }
	if(gsm->sms_mod_flag = SMS_PDU){
		fprintf(f, "\"%s\",\"%s\",\"%d\"\r\n",
			gsm->sms_info->pdu_info.destination,
			gsm->sms_info->pdu_info.text,
			gsm->span);
	}else if (gsm->sms_mod_flag = SMS_TEXT){
		fprintf(f, "\"%s\",\"%s\",\"%d\"\r\n",
			gsm->sms_info->txt_info.destination,
			gsm->sms_info->txt_info.message,
			gsm->span);
	}else
		printf("Unknown msg type %s %d", __func__, __LINE__);
        fclose(f);
//	system("/bin/sync");
	sync();
}
#endif
static int parse_callforward(struct allogsm_modul *gsm,const char* response)
{
        char *temp_buffer=NULL;
        char *temp=NULL;
        char *end=NULL;

        if(!strncmp(response,"+CCFC",5)){

                memset(&gsm->ev.callforward_number.number,0,sizeof(gsm->ev.callforward_number.number));
                temp_buffer=strchr(response,'+');
                if(!strstr(temp_buffer,",\"")){
			gsm->ev.callforward_number.number[0] = '0';
                        return 0;
                }

                temp=strchr(temp_buffer,'"');
                if(!temp)
                        return -1;
                else
                        temp++;
                end=strrchr(temp,'"');
                if(!end)
                        return -1;
                strncpy(&gsm->ev.callforward_number.number,temp,end-temp);
                gsm->ev.callforward_number.number[end-temp]='\0';		
                return 0;
        }
        else
                return -1;
}

static int parse_operator_list(struct allogsm_modul *gsm,char* lineptr)
{
        char *tmp;
	int i=0;
	memset(&gsm->ev.operator_list_received, 0, sizeof(gsm_event_operator_list_received));
        do{
                if (lineptr[0]==',' && lineptr[1]==',')
                        break;

                tmp=strsep(&lineptr, "(");
                tmp=NULL;
                tmp=strsep(&lineptr, ")");
                if (tmp==NULL)
                        break;
		
                gsm->ev.operator_list_received.stat[i] = atoi(remove_all_chars(strsep(&tmp,","), '\"'));
                strcpy(gsm->ev.operator_list_received.long_operator_name[i], remove_all_chars(strsep(&tmp,","), '\"'));
                strcpy(gsm->ev.operator_list_received.short_operator_name[i], remove_all_chars(strsep(&tmp,","), '\"'));
                gsm->ev.operator_list_received.num_operator[i] = atoi(remove_all_chars((tmp), '\"'));
		++i;
        }while(lineptr!=NULL);
	gsm->ev.operator_list_received.count=i;
	return 0;
}

static int parse_ussd_code(struct allogsm_modul *gsm,const char* ussd_code)
{
	int response_type;
	char *temp_buffer=NULL;
	temp_buffer=strchr(ussd_code,':');
	if(temp_buffer) {
		if(strchr(temp_buffer,' ')) {
			temp_buffer=strchr(temp_buffer,' ');
			temp_buffer=temp_buffer+1;
		} else {
			temp_buffer=temp_buffer+1;
		}
	} else {
		return -1;
	}

	if(!strstr(temp_buffer,",\""))
		return -1;

	response_type=atoi(temp_buffer);

	char buf[1024];
	char *temp=buf;
	char *end=NULL;
	strncpy(buf, temp_buffer, sizeof(buf));
	memset(&gsm->ev.ussd_received,0,sizeof(gsm_event_ussd_received));
	gsm->ev.ussd_received.ussd_stat=atoi(temp);
	temp=strchr(temp,'"');
	if(!temp)
		return -1;
	else
		temp++;
	end=strrchr(temp,'"');
	if(!end)
		return -1;
	strncpy(gsm->ev.ussd_received.text,temp,end-temp);
	gsm->ev.ussd_received.text[end-temp]='\0';
	//header_len = gsm_hex2int(gsm->ev.ussd_received.text, 2) + 1;
	gsm->ev.ussd_received.len=strlen(gsm->ev.ussd_received.text);
	temp=strchr(end,',');
	if(temp)
	{
		temp++;
		gsm->ev.ussd_received.ussd_coding=atoi(temp);
	}
	else {
		gsm->ev.ussd_received.ussd_coding=0;
	}

	return response_type;
}

static allogsm_event *module_check_safe_at(struct allogsm_modul *gsm, char *buf, int i){
	int response_type;
	if(gsm->state != ALLOGSM_STATE_SAFE_AT)
		return NULL;
	gsm_message(gsm, "\nSAFE AT STATE\n");
	if(gsm_compare(buf, "+CCFC:")) {

		char *error_msg = NULL;
		response_type = parse_callforward(gsm,buf);
		if(!response_type){ 
			gsm->ev.e  = ALLOGSM_EVENT_SAFE_AT_RECEIVED;
			gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
                	return &gsm->ev;
			}
		else{
			gsm->ev.e  = ALLOGSM_EVENT_SAFE_AT_FAILED;
			gsm_message(gsm, "Error: Callforward command failed");
			gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
			return &gsm->ev;
		}
	}
	else if(gsm_compare(buf, "+CME ERROR: 30")){
		gsm->ev.e  = ALLOGSM_EVENT_SAFE_AT_FAILED;
		gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
		return &gsm->ev;
	}
	else if(gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype, AT_DEL_MSG))) {
		if(gsm_compare(buf, get_at(gsm->switchtype, AT_OK))){
			gsm_message(gsm, "\nSAFE AT DEL MSG %s\n", get_at(gsm->switchtype, AT_DEL_MSG));
			gsm->ev.e  = ALLOGSM_EVENT_SAFE_AT_RECEIVED;
			gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
                	return &gsm->ev;
		}else{
			gsm->ev.e  = ALLOGSM_EVENT_SAFE_AT_FAILED;
			gsm_message(gsm, "Error: DEL MSG command failed coming out of SAFE AT State");
			gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
			return &gsm->ev;
		}
	}
	else
		return NULL;
}

static allogsm_event *module_check_operator_list_query(struct allogsm_modul *gsm, char *buf, int i)
{
	int response_type;
	if(gsm->state != ALLOGSM_STATE_OPERATOR_QUERY)
		return NULL;

	if(gsm_compare(buf, "+COPS: (")) {
		char *error_msg = NULL;
		response_type = parse_operator_list(gsm,buf);
                if( -1 == response_type ) {
                        error_msg = "Operator List parse failed\n";
                }
		if(error_msg) {
			gsm->at_last_recv[0] = '\0';
			gsm->at_pre_recv[0] = '\0';
			gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
			gsm->ev.e = ALLOGSM_EVENT_OPERATOR_LIST_FAILED;
			return &gsm->ev;
	        } else { //Successful
			gsm->ev.e = ALLOGSM_EVENT_OPERATOR_LIST_RECEIVED;
			gsm->at_last_recv[0] = '\0';
			gsm->at_pre_recv[0] = '\0';
			gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
			return &gsm->ev;
		}		
	} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_ERROR)) || 
			   gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER)) ||
				gsm_compare(buf, get_at(gsm->switchtype,AT_CME_ERROR))) {
		gsm_error(gsm, "Error RX Operator List Query (%s) on span %d.\n", buf, gsm->span);
		gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
		gsm->ev.e = ALLOGSM_EVENT_OPERATOR_LIST_FAILED;
		return &gsm->ev;
	}
	gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL); //pawan temp..fix it later

	return NULL;
}

static allogsm_event *module_check_ussd(struct allogsm_modul *gsm, char *buf, int i)
{
	int response_type;

	if(gsm->state != ALLOGSM_STATE_USSD_SENDING)
		return NULL;

	if(gsm_compare(buf, get_at(gsm->switchtype,AT_CHECK_USSD))) {
		char *error_msg = NULL;
		response_type=parse_ussd_code(gsm,buf);
#if 0
		switch(response_type) {
		case 1:		//Successful;
			break;
		case -1:
			error_msg = "USSD parse failed\n";
		case 0:
			error_msg = "USSD response type: No further action required 0\n";
			break;
		case 2:
			error_msg = "USSD response type: USSD terminated by network 2\n";
			break;
		case 3:
			error_msg = "USSD response type: Other local client has responded 3\n";
			break;
		case 4:
			error_msg = "USSD response type: Operation not supported 4\n";
			break;
		case 5:
			error_msg = "USSD response type: Network timeout 5\n";
			break;
		default:
			error_msg = "CUSD message has unknown response type \n";
			break;
		}
#else
                if( -1 == response_type ) {
                        error_msg = "USSD parse failed\n";
                }
#endif
		if(error_msg) {
			gsm->at_last_recv[0] = '\0';
			gsm->at_pre_recv[0] = '\0';
			if (gsm->ussd_info) {
				free(gsm->ussd_info);
				gsm->ussd_info = NULL;
			}
			gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
			gsm->ev.e = ALLOGSM_EVENT_USSD_SEND_FAILED;
			return &gsm->ev;
        } else { //Successful
			gsm->ev.e = ALLOGSM_EVENT_USSD_RECEIVED;
			gsm->at_last_recv[0] = '\0';
			gsm->at_pre_recv[0] = '\0';
			
			if (gsm->ussd_info) {
				free(gsm->ussd_info);
				gsm->ussd_info = NULL;
			}
			gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
			return &gsm->ev;
		}		
	} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_CMS_ERROR)) || 
			   gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER)) ||
				gsm_compare(buf, get_at(gsm->switchtype,AT_CME_ERROR))) {
		gsm_error(gsm, "Error sending USSD (%s) on span %d.\n", buf, gsm->span);
		if (gsm->ussd_info) {
			free(gsm->ussd_info);
			gsm->ussd_info = NULL;
		}
		gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
		gsm->ev.e = ALLOGSM_EVENT_USSD_SEND_FAILED;
		return &gsm->ev;
	}

	return NULL;
}

int updateSmsStatus( struct allogsm_modul *gsm, int status){
	char cmd[512];
	gsm_schedule_del(gsm, gsm->smsTimeoutSched);
	if(strlen(gsm->sms_info->pdu_info.id))
		gsm_message(gsm,"ALLO GSM: ID %s \n",gsm->sms_info->pdu_info.id);
	else {
		gsm_message(gsm,"ALLO GSM: no ID\n");
		return 1;
	}

#if 0
/*If writing count to DB use this code, or if writing to file use else part
  Issue: Sometimes count was not updated as DB was found locked and transation fails. 
  - We could of gone to same solution as it is present for CDR updation to sqlite3 with repeated trials after 200 ms delay, still we wont be sure.
  - We already tried locking DB but GUI was facing issues because of that.
  - Finally writing to file.
*/
	if (status == SENT_SUCCESS){
		sprintf(cmd, "/var/lib/asterisk/agi-bin/db_list \
				/var/GUI/DB/Gateway.db \
				\'UPDATE SMS_BATCH SET SENT = SENT + 1 WHERE REFNAME = \"%s\"\'",
				gsm->sms_info->pdu_info.id);
		system (cmd);
	}else if(status == SENT_FAILED) {
		sprintf(cmd, "/var/lib/asterisk/agi-bin/db_list \
				/var/GUI/DB/Gateway.db \
				\'UPDATE SMS_BATCH SET FAILED = FAILED + 1 WHERE REFNAME = \"%s\"\'",
				gsm->sms_info->pdu_info.id);
		system (cmd);
		allogsm_save_sms(gsm);
	}
#else
	allogsm_update_count(gsm, status);

	allogsm_save_sms(gsm, status);
#endif
	return 0;
}
static allogsm_event *module_check_sms(struct allogsm_modul *gsm, char *buf, int i)
{
	int res;
	int compare1;
	int compare2;
	char sms_buf[1024],fun[256];
	char cmt_buf[1024];
	int len;	
	char sms_end[5];
	sprintf(sms_end, "%c", 0x1A);

	compare1 = gsm_compare(buf , get_at(gsm->switchtype,AT_CHECK_SMS));

	if (compare1) {
		cmt_buf[0]='\0';
		strcpy(cmt_buf, buf);
		gsm_message(gsm,"cmt_buf %s\n",cmt_buf);
		strcat(cmt_buf, "\r\n");
		len = gsm_san(gsm, gsm->at_last_recv, buf, 0);
		strcat(cmt_buf, buf);
		strcat(cmt_buf, "\r\n");
		gsm_message(gsm,"cmt_buf %s\n",cmt_buf);
		gsm_message(gsm,"gsm->sanidx %d at_last_recv_idx:%d smslen:%d \n",gsm->sanidx,gsm->at_last_recv_idx, strlen(cmt_buf));
		enum sms_mode mode;
		mode = gsm_check_sms_mode(gsm, cmt_buf);
		
		if (SMS_TEXT == mode) {
		    memcpy(gsm->sms_recv_buffer, cmt_buf, gsm->at_last_recv_idx);
		
			gsm_text2sm_event2(gsm, cmt_buf, gsm->sms_recv_buffer);
			if (!res) {
				gsm->ev.e = ALLOGSM_EVENT_SMS_RECEIVED;
				cmt_buf[0] = '\0';
				return &gsm->ev;
			} else {
				return NULL;
			}
		} else if (SMS_PDU == mode) {
			char *temp_buffer = NULL;
			temp_buffer = strchr(cmt_buf,',');
			if(temp_buffer) {
				temp_buffer = strstr(temp_buffer,"\r\n");
				if(temp_buffer)
					temp_buffer += 2;
				else
 					temp_buffer = gsm->sms_recv_buffer;
			} else {
				temp_buffer=gsm->sms_recv_buffer;
			}

			int len = strlen(temp_buffer);
			if((temp_buffer[len-2]=='\r')||(temp_buffer[len-2]=='\n')) {
				temp_buffer[len-2]='\0';
			} else if((temp_buffer[len-1]=='\r')||(temp_buffer[len-1]=='\n')) {
				temp_buffer[len-1]='\0';
			}

			strncpy(gsm->sms_recv_buffer, temp_buffer, sizeof(gsm->sms_recv_buffer));
			if (!gsm_pdu2sm_event(gsm, gsm->sms_recv_buffer)) {
		gsm_message(gsm,"herer %d---------------------\n",__LINE__);
				gsm->ev.e = ALLOGSM_EVENT_SMS_RECEIVED;
				cmt_buf[0] = '\0';
				gsm->at_pre_recv[0] = '\0';
				gsm->sms_recv_buffer[0] = '\0';
				return &gsm->ev;
			} else {
				gsm->at_pre_recv[0] = '\0';
				gsm->sms_recv_buffer[0] = '\0';
				return NULL;
			}
		}

#if 0
	compare1 = gsm_compare(gsm->at_last_recv, get_at(gsm->switchtype,AT_CHECK_SMS));  // AT_CHECK_SMS: +CMT
	compare2 = gsm_compare(gsm->at_pre_recv, get_at(gsm->switchtype,AT_CHECK_SMS));
	if (((2 == i) && compare1) || ((1 == i) && compare2)) {
		enum sms_mode mode;
		if (2 == i) {
			mode = gsm_check_sms_mode(gsm, gsm->at_last_recv);
		} else if (1 == i) {
			mode = gsm_check_sms_mode(gsm, gsm->at_pre_recv);		
		}
		
		if (SMS_TEXT == mode) {
		    memcpy(gsm->sms_recv_buffer, gsm->at_last_recv, gsm->at_last_recv_idx);
		
			res = (2 == i) ? gsm_text2sm_event2(gsm, gsm->at_last_recv, gsm->sms_recv_buffer) : \
			                 gsm_text2sm_event2(gsm, gsm->at_pre_recv, gsm->sms_recv_buffer);
			if (!res) {
				gsm->ev.e = ALLOGSM_EVENT_SMS_RECEIVED;
				gsm->at_last_recv[0] = '\0';
				return &gsm->ev;
			} else {
				return NULL;
			}
		} else if (SMS_PDU == mode) {
			if (((2 == i) && compare1) || ((1 == i) && compare2)) {
				char *temp_buffer = NULL;
				temp_buffer = strchr(gsm->at_last_recv,',');
				if(temp_buffer) {
					temp_buffer = strstr(temp_buffer,"\r\n");
					if(temp_buffer)
						temp_buffer += 2;
					else
 						temp_buffer = gsm->sms_recv_buffer;
				} else {
					temp_buffer=gsm->sms_recv_buffer;
				}

				int len = strlen(temp_buffer);
				if((temp_buffer[len-2]=='\r')||(temp_buffer[len-2]=='\n')) {
					temp_buffer[len-2]='\0';
				} else if((temp_buffer[len-1]=='\r')||(temp_buffer[len-1]=='\n')) {
					temp_buffer[len-1]='\0';
				}

				strncpy(gsm->sms_recv_buffer, temp_buffer, sizeof(gsm->sms_recv_buffer));
				if (!gsm_pdu2sm_event(gsm, gsm->sms_recv_buffer)) {
					gsm->ev.e = ALLOGSM_EVENT_SMS_RECEIVED;
					gsm->at_last_recv[0] = '\0';
					gsm->at_pre_recv[0] = '\0';
					gsm->sms_recv_buffer[0] = '\0';
					return &gsm->ev;
				} else {
					gsm->at_pre_recv[0] = '\0';
					gsm->sms_recv_buffer[0] = '\0';
					return NULL;
				}
			}
		}
#endif
	//Freedom del 2011-12-13 17:52
	/////////////////////////////////////////////////////////////////////////////////////
#if 0
	} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_ERROR)) || 
			   gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER)) ||
			   gsm_compare(buf, get_at(gsm->switchtype,AT_NO_ANSWER))) {
		gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
#endif
	/////////////////////////////////////////////////////////////////////////////////////
	}
//	printf("Buf is %s message is %s \n",buf,gsm->sms_sent_buffer);	
        snprintf(fun, sizeof(fun)-1, "%s%c",gsm->sms_sent_buffer,0x1A);

	switch(gsm->state) {
		case ALLOGSM_STATE_READY:
			if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
				if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SEND_SMS_PDU_MODE))) {
			                gsm_send_at(gsm, get_at(gsm->switchtype,AT_UCS2)); /* text to pdu mode */
					gsm->sms_mod_flag = SMS_PDU;
				} else if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_UCS2))) {
					gsm->sms_mod_flag = SMS_PDU;
				}
			}
			break;
		case ALLOGSM_STATE_SMS_SENDING:
			if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
				if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SEND_SMS_PDU_MODE))) {
					gsm_send_at(gsm, get_at(gsm->switchtype,AT_UCS2));
				} else if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SEND_SMS_TEXT_MODE))) {
					gsm_send_at(gsm, get_at(gsm->switchtype,AT_GSM));
				} else if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_UCS2))) {
					gsm->sms_mod_flag = SMS_PDU;
					memset(sms_buf, 0x0, sizeof(sms_buf));
					if (gsm->sms_info) {
						get_sms_len(gsm->switchtype, gsm->sms_info->pdu_info.len, sms_buf, sizeof(sms_buf));
					}
					gsm_send_at(gsm, sms_buf);
				} else if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_GSM))) {
					gsm->sms_mod_flag = SMS_TEXT;
					memset(sms_buf, 0x0, sizeof(sms_buf));
					if (gsm->sms_info) {
						get_sms_des(gsm->switchtype, gsm->sms_info->txt_info.destination, sms_buf, sizeof(sms_buf));	
					}	
					gsm_send_at(gsm, sms_buf);
				} else { 
				}
			} else if (strstr(buf,">")){
/*
                                memset(gsm->sms_sent_buffer, 0x0, sizeof(gsm->sms_sent_buffer));
				snprintf(gsm->sms_sent_buffer, sizeof(gsm->sms_sent_buffer) - 1, "%c", 0x1A);
				gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SENT, gsm->sms_sent_buffer);
*/
                                gsm_transmit_sms(gsm, gsm->sms_sent_buffer);
                                gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SENT, sms_end);
			} else if (strstr(buf,"STIN:")||strstr(buf,"WIND:")||strstr(buf,"CREG:")){
				//gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SENDING, get_at(gsm->switchtype,AT_SEND_SMS_TEXT_MODE));
				//gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
				if ((gsm->creg_state!=CREG_1_REGISTERED_HOME)&&(gsm->creg_state!=CREG_5_REGISTERED_ROAMING)){
					gsm_message(gsm,"Span %d: NETWORK DOWN while sending sms, network state %d\n",gsm->span, gsm->creg_state);
					gsm->sms_stuck=1;
				}else{
					if (gsm->sms_stuck){
						gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SENDING, get_at(gsm->switchtype,AT_SEND_SMS_TEXT_MODE));
						gsm_message(gsm,"Span %d: NETWORK UP while sending sms, network state %d\n",gsm->span, gsm->creg_state);
					}
					gsm->sms_stuck=0;
				}
			} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_CMS_ERROR)) || 
					   gsm_compare(buf, get_at(gsm->switchtype,AT_ERROR))) {
				gsm_message(gsm, "Error sending SMS (%s) on span %d.\n", buf, gsm->span);
				updateSmsStatus(gsm, SENT_FAILED); 
/*
				if (gsm->sms_info) {
					free(gsm->sms_info);
					gsm->sms_info = NULL;
				}
*/
				if (gsm_compare(buf, "+CMS: 512")){
                	                gsm_send_at(gsm, sms_end);
					gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_GENERAL_INDICATION));
				}else{
					gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
				}

				gsm->ev.e = ALLOGSM_EVENT_SMS_SEND_FAILED;
				return &gsm->ev;
#if 1 
			} else if (expectlist_compare(buf)) {
				if (strcmp(buf, " ")){
                	                gsm_send_at(gsm, sms_end);
//					gsm_message(gsm,"hererere -------------------------- %d %d >%s< \n",__LINE__, gsm->span,  gsm->at_last_sent);
					gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SENDING, AT_SEND_SMS_PDU_MODE);
				}
				if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SEND_SMS_PDU_MODE))) {
					gsm_send_at(gsm, get_at(gsm->switchtype,AT_UCS2));
				} else if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SEND_SMS_TEXT_MODE))) {
					gsm_send_at(gsm, get_at(gsm->switchtype,AT_GSM));
				} else if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_UCS2))) {
					gsm->sms_mod_flag = SMS_PDU;
					memset(sms_buf, 0x0, sizeof(sms_buf));
					if (gsm->sms_info) {
						get_sms_len(gsm->switchtype, gsm->sms_info->pdu_info.len, sms_buf, sizeof(sms_buf));
					}
					gsm_send_at(gsm, sms_buf);
				} else if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_GSM))) {
					gsm->sms_mod_flag = SMS_TEXT;
					memset(sms_buf, 0x0, sizeof(sms_buf));
					if (gsm->sms_info) {
						get_sms_des(gsm->switchtype, gsm->sms_info->txt_info.destination, sms_buf, sizeof(sms_buf));	
					}	
					gsm_send_at(gsm, sms_buf);
				} else { 
					gsm_switch_state(gsm, ALLOGSM_STATE_READY, sms_end);

					gsm_message(gsm,"ALLO GSM: SMS Sent Failed Try again from span %d \n",gsm->span);

					updateSmsStatus(gsm, SENT_FAILED); 

					gsm->ev.e = ALLOGSM_EVENT_SMS_SEND_FAILED;
					return &gsm->ev;
				}
#endif
			} else {
				if (gsm_compare(buf, "RING") || gsm_compare(buf, "+CLIP")){
					gsm_message(gsm,"ALLO GSM: Ring Received on %d while sending sms\n",gsm->span);
					updateSmsStatus(gsm, SENT_FAILED); 
					gsm_switch_state(gsm, ALLOGSM_STATE_READY, sms_end);
				}else{
					gsm_message(gsm,"Ref code at line %d. Handle this case were span is %d, buf is >>%s<<, at_last_sent >>%s<<\n", \
					__LINE__, gsm->span, buf, gsm->at_last_sent);
				}
			}
			break;
		case ALLOGSM_STATE_SMS_SENT:
			if (gsm_compare(buf, get_at(gsm->switchtype,AT_SEND_SMS_SUCCESS))) {
				gsm_message(gsm,"ALLO GSM: SMS Sent Successfully from span %d but Wait for OK\n",gsm->span);
			} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
#if 0
				gsm_message(gsm,"ALLO GSM: SMS Sent Successfully from span %d \n",gsm->span);
				if(gsm->sms_mod_flag==SMS_PDU)
				{
					gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
					gsm->ev.e = ALLOGSM_EVENT_SMS_SEND_OK;
					return &gsm->ev;
				}
				else
				{
					gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SENT_END,get_at(gsm->switchtype,AT_SEND_SMS_PDU_MODE));
				}
#else
				gsm_message(gsm,"ALLO GSM: SMS Sent Successfully from span %d \n",gsm->span);

				updateSmsStatus(gsm, SENT_SUCCESS); 

				gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
				gsm->ev.e = ALLOGSM_EVENT_SMS_SEND_OK;
				return &gsm->ev;
#endif
			} else if((gsm_compare(buf, get_at(gsm->switchtype,AT_CMS_ERROR)))|| (gsm_compare(buf, get_at(gsm->switchtype,AT_ERROR)))) {
				gsm_message(gsm,"Error Received on span %d while sending sms.\n",gsm->span);
				if (gsm->dial_initiated==1)
					gsm->sms_retry=4;
				if ((gsm->sms_retry < 3) &&((gsm->sms_mod_flag == SMS_PDU) || (gsm->sms_mod_flag == SMS_TEXT))){
					++(gsm->sms_retry);
					gsm_message(gsm,"ALLO GSM: SMS Sent Failed. Trying again from span %d \n",gsm->span);
					if (gsm->sms_mod_flag == SMS_PDU) {
						memset(sms_buf, 0x0, sizeof(sms_buf));
						if (gsm->sms_info) {
							get_sms_len(gsm->switchtype, gsm->sms_info->pdu_info.len, sms_buf, sizeof(sms_buf));
						}
						gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SENDING,sms_buf);
					} else if (gsm->sms_mod_flag == SMS_TEXT) {
						memset(sms_buf, 0x0, sizeof(sms_buf));
						if (gsm->sms_info) {
							get_sms_des(gsm->switchtype, gsm->sms_info->txt_info.destination, sms_buf, sizeof(sms_buf));	
						}	
						gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SENDING,sms_buf);
					}	
				}else{
					gsm_message(gsm,"ALLO GSM: SMS Sent Failed Try again from span %d \n",gsm->span);

					updateSmsStatus(gsm, SENT_FAILED); 
					if (gsm_compare(buf, "+CMS ERROR: 512")){
						gsm_send_at(gsm, sms_end);
						gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_GENERAL_INDICATION));
					}else{
						gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
					}
					gsm->ev.e = ALLOGSM_EVENT_SMS_SEND_FAILED;
					return &gsm->ev;
				}
			}else if(strstr(buf,"STIN:")||strstr(buf,"WIND:")||strstr(buf,"CREG:"))   {
				if ((gsm->creg_state!=CREG_1_REGISTERED_HOME)&&(gsm->creg_state!=CREG_5_REGISTERED_ROAMING)){
					gsm_message(gsm,"Span %d: NETWORK DOWN while sending sms, network state %d,  \n",gsm->span, gsm->creg_state);
					gsm->sms_stuck=1;
				}else{
					if (gsm->sms_stuck){
						gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SENDING, get_at(gsm->switchtype,AT_SEND_SMS_TEXT_MODE));
						gsm_message(gsm,"Span %d: NETWORK UP while sending sms, network state %d\n",gsm->span, gsm->creg_state);
					}
					gsm->sms_stuck=0;
				}
			//	gsm_message(gsm,"SMS Sent failed on span %d\n",gsm->span);
			//	gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
                        //        gsm->ev.e = ALLOGSM_EVENT_SMS_SEND_FAILED;
                        //        return &gsm->ev;
			} else if (strstr(buf,">")){
                                gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SENT, sms_end);
#if WAVECOM
			} else if (expectlist_compare(buf)) {
				if (!strcmp(buf, " ")){
					/*
                	                gsm_send_at(gsm, sms_end);
					gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SENDING, get_at(gsm->switchtype,AT_SEND_SMS_TEXT_MODE));
					*/
				}else{
					gsm_switch_state(gsm, ALLOGSM_STATE_READY, sms_end);

					gsm_message(gsm,"ALLO GSM: SMS Sent Failed Try again from span %d \n",gsm->span);

					updateSmsStatus(gsm, SENT_FAILED); 

					gsm->ev.e = ALLOGSM_EVENT_SMS_SEND_FAILED;
					return &gsm->ev;
				}
#endif
			}
			else
			{
				if (gsm_compare(buf, "RING") || gsm_compare(buf, "+CLIP")){
					gsm_message(gsm,"ALLO GSM: Ring Received on %d in state SMS Sent\n",gsm->span);
					updateSmsStatus(gsm, SENT_FAILED); 
					gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
				}else{
					gsm_message(gsm,"Ref code at line %d. Handle this case were span is %d, buf is >>%s<<, at_last_sent >>%s<<\n", \
					__LINE__,gsm->span, buf, gsm->at_last_sent);
				}
			}
			break;
		case ALLOGSM_STATE_SMS_SENT_END:
			if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
				gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
				gsm->ev.e = ALLOGSM_EVENT_SMS_SEND_OK;
				return &gsm->ev;
			} else {
				gsm_switch_state(gsm, ALLOGSM_STATE_READY,NULL);
				gsm->ev.e = ALLOGSM_EVENT_SMS_SEND_FAILED;
				return &gsm->ev;
			}
			break;
		default:
			break;
	}
	return NULL;
}

static allogsm_event * module_check_network(struct allogsm_modul *gsm, struct alloat_call *call, char *buf, int i)
{
        if (gsm->state != ALLOGSM_STATE_READY && gsm->state !=  ALLOGSM_STATE_NET_REQ && gsm->state != ALLOGSM_STATE_NET_NAME_REQ) {
		return NULL;
	}

	if (gsm_compare(buf, get_at(gsm->switchtype,AT_CHECK_SIGNAL1))){
		module_get_coverage(gsm, buf);
                if (gsm->coverage < 1) {
                        gsm->ev.gen.e = ALLOGSM_EVENT_NO_SIGNAL;
                        gsm_switch_state(gsm, ALLOGSM_STATE_NET_REQ, NULL);
                } else {
                        gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_UP;
                        gsm_switch_state(gsm, ALLOGSM_STATE_READY, NULL);
                }
                return &gsm->ev;
	} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_CREG))) {
		/*
			0 not registered, ME is not currently searching a new operator to register to
			1 registered, home network
			2 not registered, but ME is currently searching a new operator to register to
			3 registration denied
			4 unknown
			5 registered, roaming
		*/
		trim_CRLF(buf);
		if (!strcmp(buf, get_at(gsm->switchtype,AT_CREG0))) {
			gsm->network = GSM_NET_UNREGISTERED;
			gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_DOWN;
			gsm->coverage		= -1;
			gsm->net_name[0]	= 0x0;
	                gsm_switch_state(gsm, ALLOGSM_STATE_NET_REQ, get_at(gsm->switchtype,AT_ASK_NET));
			return &gsm->ev;
		} else if (!strcmp(buf, get_at(gsm->switchtype,AT_CREG1))) {
			if(gsm->state == ALLOGSM_STATE_NET_REQ) {
                               	gsm->network = GSM_NET_HOME;
				gsm_switch_state(gsm, ALLOGSM_STATE_NET_OK, get_at(gsm->switchtype,AT_NET_OK));
			} else {
				gsm->network = GSM_NET_HOME;
				gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_UP;
				return &gsm->ev;
			}
		} else if (!strcmp(buf, get_at(gsm->switchtype,AT_CREG2))) {
			gsm->network = GSM_NET_SEARCHING;
			gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_DOWN;
			gsm->coverage		= -1;
			gsm->net_name[0]	= 0x0;
                        gsm_switch_state(gsm, ALLOGSM_STATE_NET_REQ, get_at(gsm->switchtype,AT_ASK_NET));
			return &gsm->ev;
		} else if (!strcmp(buf, get_at(gsm->switchtype,AT_CREG3))) {
			gsm->network = GSM_NET_DENIED;
			gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_DOWN;
			gsm->coverage		= -1;
			gsm->net_name[0]	= 0x0;
                        gsm_switch_state(gsm, ALLOGSM_STATE_NET_REQ, get_at(gsm->switchtype,AT_ASK_NET));
			return &gsm->ev;
		} else if (!strcmp(buf, get_at(gsm->switchtype,AT_CREG4))) {
			gsm->network = GSM_NET_UNKNOWN;
			gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_DOWN;
			gsm->coverage		= -1;
			gsm->net_name[0]	= 0x0;
                        gsm_switch_state(gsm, ALLOGSM_STATE_NET_REQ, get_at(gsm->switchtype,AT_ASK_NET));
			return &gsm->ev;
		} else if (!strcmp(buf, get_at(gsm->switchtype,AT_CREG5))) {
			if(gsm->state == ALLOGSM_STATE_NET_REQ) {
				gsm_switch_state(gsm, ALLOGSM_STATE_NET_OK, get_at(gsm->switchtype,AT_NET_OK));
			} else {
				gsm->network = GSM_NET_ROAMING;
				gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_UP;
				return &gsm->ev;
			}
//Freedom del 2012-06-05 17:10
/*		} else {
			gsm->network		= GSM_NET_UNREGISTERED;
			gsm->coverage		= -1;
    		gsm->net_name[0]	= 0x0;
*/
		}
	} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
		if (((2 == i) && (gsm_compare(gsm->at_last_recv, get_at(gsm->switchtype,AT_CHECK_SIGNAL1))))
			  || ((1 == i) && (gsm_compare(gsm->at_pre_recv, get_at(gsm->switchtype,AT_CHECK_SIGNAL1))))) {
			/*	
				i == 2: [ \r\n+CSQ: 25,6\r\n\r\nOK\r\n ]
				i == 1: [ \r\n+CSQ: 25,6\r\n ]
						[ \r\nOK\r\n ]
			*/
//			gsm_transmit(gsm, "AT+CREG?\r\n"); /* Req Net Status */
			if (gsm->coverage < 1) {
				gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_DOWN;
			} else {
				gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_UP;			
			}
			return &gsm->ev;
		} else if (((2 == i) && gsm_compare(gsm->at_last_recv, get_at(gsm->switchtype,AT_CREG)))
		      || ((1 == i) && gsm_compare(gsm->at_pre_recv, get_at(gsm->switchtype,AT_CREG)))) {
			/*if (GSM_NET_UNREGISTERED == gsm->network) {
				gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_DOWN;
				gsm->coverage		= -1;
				gsm->net_name[0]	= 0x0;
				return &gsm->ev;
			} else if (GSM_NET_HOME == gsm->network || GSM_NET_ROAMING == gsm->network) {
				gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_UP;
				return &gsm->ev;	
			}*/
		}
	} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_CHECK_SIGNAL2))) {
		module_get_coverage(gsm, buf);
//		gsm_transmit(gsm, "AT+CREG?\r\n"); /* Req Net Status */
		if (gsm->coverage < 1) {
			gsm->ev.gen.e = ALLOGSM_EVENT_NO_SIGNAL;	
		} else {
			gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_UP;		
		}
		return &gsm->ev;
//Freedom Add 2012-06-05 15:21 adjust network state
//////////////////////////////////////////////////////////////////////////////////////////////////////
	} else if (((2 == i) && gsm_compare(gsm->at_last_recv, get_at(gsm->switchtype,AT_CREG)))
		      || ((1 == i) && gsm_compare(gsm->at_pre_recv, get_at(gsm->switchtype,AT_CREG)))) {
			/*if (GSM_NET_UNREGISTERED == gsm->network) {
				gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_DOWN;
				gsm->coverage		= -1;
				gsm->net_name[0]	= 0x0;
				return &gsm->ev;
			} else if (GSM_NET_HOME == gsm->network || GSM_NET_ROAMING == gsm->network) {
				
				gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_UP;
				return &gsm->ev;	
			}*/
	}
//////////////////////////////////////////////////////////////////////////////////////////////////////

return NULL;
}

#ifdef VIRTUAL_TTY
int module_mux_end(struct allogsm_modul *gsm, int restart_at_flow)
{
	if(restart_at_flow) {
		//Restart AT command flow again, Because after execute "AT+CMUX=0" or clear MUX mode, something need reinitialize.
		gsm->already_set_mux_mode = 1;
		return gsm_switch_state(gsm, ALLOGSM_STATE_UP, get_at(gsm->switchtype,AT_ATZ));
	} else {
		return gsm_switch_state(gsm, ALLOGSM_STATE_READY, get_at(gsm->switchtype,AT_NET_NAME));
	}
}
#endif //VIRTUAL_TTY

allogsm_event *module_receive(struct allogsm_modul *gsm, char *data, int len)
{
	struct alloat_call *call;
	char buf[1024];
	char receivebuf[1024];
	char *p;
	allogsm_event* res_event=NULL;
	int i;
	int j;
	//Freedom Add 2012-02-07 15:24
	/////////////////////////////////////////////////////////
#if SIMCOM900D_NO_ANSWER_BUG
	static int first = 1;
	static struct timeval start_time,end_time;
#endif //SIMCOM900D_NO_ANSWER_BUG
	//////////////////////////////////////////////////////////

	/* get ast_call */
	call = allogsm_getcall(gsm, gsm->cref, 0);
	if (!call) {
		gsm_error(gsm, "Unable to locate call %d\n", gsm->cref);
		return NULL;
	}

	strncpy(receivebuf, data, sizeof(receivebuf));
	p = receivebuf;
	i = 0;

	while (1) {
received_junk_parse_next:	
		len = gsm_san(gsm, p, buf, len);
		if (0 == len || -1 == len) {
			return NULL;
		}

		//if (gsm->debug & ALLOGSM_DEBUG_AT_RECEIVED)
		if ((gsm->debug & ALLOGSM_DEBUG_AT_RECEIVED) || (gsm->span==1))
		{
			char tmp[1024];
			gsm_trim(gsm->at_last_sent, tmp, strlen(gsm->at_last_sent));
			if (-3 == len) {
				gsm_message(gsm, "\t\t%d<-- %d %s -- %s , NULL (----len -3----)\n", gsm->span, i, tmp, buf);		
			}
#define FORMAT  "     %-2d<-- %-1d %-40.40s  || sz: %-3d|| %-23s || last sent: %s\n"
			int ii=0;
                        gsm_message(gsm, FORMAT, gsm->span, i, buf, len, allogsm_state2str(gsm->state)+14,tmp);
                        for(ii=40; ii<len; ii=ii+40)
                                gsm_message(gsm,"             %-40.40s\n", buf+ii);
#undef FORMAT

		}
	
		strncpy(p, gsm->sanbuf, sizeof(receivebuf));
		len = (-3 == len) ? 0: gsm->sanidx;
//		gsm->sanidx = 0;
		len=0;
/*********** Ignore few responces.. Mostly unsolicited 
		Make it proper.. right now ignoring in begning***************/
		if (gsm_compare(buf,"+WBCI")) {
			goto received_junk_parse_next;
		}
/*******************************************/
		if(gsm_compare(buf, "+CME ERROR: 515")) {
			gsm->CME_515_count++;
		}
#ifdef WAVECOM
		res_event = module_check_wind(gsm, call ,buf, i);
		if (res_event) {
			return res_event;
		}
/*
		if (gsm_compare(buf, "+CREG: ")) {
			int creg_state;
			if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG0))){ creg_state = CREG_0_NOT_REG_NOT_SERARCHING;
			} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG1))) { creg_state =CREG_1_REGISTERED_HOME;
			} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG2))) { creg_state =CREG_2_NOT_REG_SERARCHING;
			} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG3))) { creg_state =CREG_3_REGISTRATION_DENIED;
			} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG4))) { creg_state =CREG_4_UNKNOWN;
			} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG5))) { creg_state =CREG_5_REGISTERED_ROAMING;
			}
			if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG10))){ creg_state = CREG_0_NOT_REG_NOT_SERARCHING;
			} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG11))) { creg_state =CREG_1_REGISTERED_HOME;
			} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG12))) { creg_state =CREG_2_NOT_REG_SERARCHING;
			} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG13))) { creg_state =CREG_3_REGISTRATION_DENIED;
			} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG14))) { creg_state =CREG_4_UNKNOWN;
			} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG15))) { creg_state =CREG_5_REGISTERED_ROAMING;
			}
			gsm->creg_state=creg_state;
		}
*/
#endif	
		switch (gsm->state) {
			case ALLOGSM_STATE_MANUFACTURER_REQ:
				/* Request manufacturer identification */
				if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_GET_MANUFACTURER))) {
					if (gsm_compare(buf,get_at(gsm->switchtype,AT_OK))&&strlen(gsm->manufacturer)) {
						memset(gsm->revision, 0, sizeof(gsm->revision));
						gsm_switch_state(gsm, ALLOGSM_STATE_VERSION_REQ, get_at(gsm->switchtype,AT_GET_VERSION));
#ifdef WAVECOM_JUNK
					} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_GET_MANUFACTURER));
#endif
					} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
					} else if (!gsm_compare(buf, get_at(gsm->switchtype,AT_CME_ERROR))) {
						gsm_get_manufacturer(gsm, buf);
					} else {
						//Freedom del 2012-06-05 15:50
						//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
					}
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;
		
			case ALLOGSM_STATE_VERSION_REQ:
				if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_GET_VERSION))) {
					if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))&&strlen(gsm->revision)) {
						//gsm_switch_state(gsm, ALLOGSM_STATE_IMEI_REQ, get_at(gsm->switchtype,AT_GET_IMEI)); /*Now selecting sim 1 here*/
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_SIM_SELECT_1, get_at(gsm->switchtype,AT_SIM_SELECT_1)); /*Now selecting sim 1 here*/
#ifdef WAVECOM_JUNK
					} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_GET_VERSION));
#endif
					} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
					} else if (!gsm_compare(buf, get_at(gsm->switchtype,AT_CME_ERROR))) {
						gsm_get_model_version(gsm, buf+9);
					} else {
						//Freedom del 2012-06-05 15:50
						//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
					}
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;
			case ALLOGSM_STATE_SET_SIM_SELECT_1:
				if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SIM_SELECT_1))) {
					if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_SIM_SELECT_2, get_at(gsm->switchtype,AT_SIM_SELECT_2));
#ifdef WAVECOM_JUNK
					} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_SIM_SELECT_1));
#endif
					} else {
						//Freedom del 2012-06-05 15:50
						//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
					}
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;
			case ALLOGSM_STATE_SET_SIM_SELECT_2:
				if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SIM_SELECT_2))) {
					if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_SIM_SELECT_3, get_at(gsm->switchtype,AT_SIM_SELECT_3));
#ifdef WAVECOM_JUNK
					} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_SIM_SELECT_2));
#endif
					} else {
						//Freedom del 2012-06-05 15:50
						//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
					}
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;
			case ALLOGSM_STATE_SET_SIM_SELECT_3:
				if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SIM_SELECT_3))) {
					if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
						memset(gsm->imei, 0, sizeof(gsm->imei));
						gsm_switch_state(gsm, ALLOGSM_STATE_IMEI_REQ, get_at(gsm->switchtype,AT_GET_IMEI)); 
#ifdef WAVECOM_JUNK
					} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_SIM_SELECT_3));
#endif
					} else {
						//Freedom del 2012-06-05 15:50
						//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
					}
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;
			case ALLOGSM_STATE_IMEI_REQ:
				/* IMEI (International Mobile Equipment Identification). */
				if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_GET_IMEI))) {
					if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))&&strlen(gsm->imei)) {
						memset(gsm->imsi, 0, sizeof(gsm->imsi));
						gsm_switch_state(gsm, ALLOGSM_STATE_SIM_READY_REQ, get_at(gsm->switchtype,AT_ASK_PIN));
#ifdef WAVECOM_JUNK
					} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_GET_IMEI));
#endif
					} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
					} else if (!gsm_compare(buf, get_at(gsm->switchtype,AT_CME_ERROR))) {
						gsm_get_imei(gsm, buf);
					} else {
						//Freedom del 2012-06-05 15:50
						//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
					}
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;
			case ALLOGSM_STATE_SIM_READY_REQ:
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_PIN_READY))) {
					gsm_switch_sim_state(gsm, ALLOGSM_STATE_SIM_READY, NULL);
					allogsm_test_atcommand(gsm,"AT");
				} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_PIN_SIM))) { /* waiting for SIM PIN */
					gsm_switch_sim_state(gsm, ALLOGSM_STATE_SIM_PIN_REQ, NULL);
					gsm_switch_state(gsm, ALLOGSM_STATE_SIM_PIN_REQ, NULL);
					gsm->ev.e = ALLOGSM_EVENT_PIN_REQUIRED;
					return &gsm->ev;
				} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					switch(gsm->sim_state) {
						case ALLOGSM_STATE_SIM_READY:
								gsm_switch_state(gsm, ALLOGSM_STATE_INIT_4, NULL);
							break;
						case ALLOGSM_STATE_SIM_PIN_REQ:
							gsm_switch_state(gsm, ALLOGSM_STATE_SIM_PIN_REQ, NULL);
							gsm->ev.e = ALLOGSM_EVENT_PIN_REQUIRED;
							return &gsm->ev;
							break;
					}/*+CME ERROR:incorrect passwo!*/
				}  if(gsm_compare(buf, "+CME ERROR: 515")) {
                			gsm->sched_state = ALLOGSM_STATE_SIM_READY_REQ;
					strcpy(gsm->sched_command, get_at(gsm->switchtype,AT_ASK_PIN));
					gsm_schedule_event(gsm, 2000, gsm_cmd_sched, call);
				}  else if(gsm_compare(buf, get_at(gsm->switchtype,AT_CME_ERROR))) {
					/* Todo: other code support */
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
					gsm->retries=-1;
					gsm->ev.e = ALLOGSM_EVENT_SIM_FAILED;
					return &gsm->ev;
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
					if (!strcmp(buf,"+WIND: 0")) { 
						/*SIM not present*/
						gsm->ev.e = ALLOGSM_EVENT_SIM_FAILED;
						return &gsm->ev;
					}
					gsm_send_at(gsm, get_at(gsm->switchtype,AT_ASK_PIN));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;
			case ALLOGSM_STATE_SIM_PIN_REQ:
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) { /* \r\n+CPIN: SIM PIN\r\n\r\nOK\r\n */
					/* gsm_message(gsm, "LAST>%s<\n",gsm->lastcmd); */
					gsm_switch_state(gsm, ALLOGSM_STATE_INIT_4, NULL);
				} else 
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_SIM_NO_INSERTED))) {

					//Freedom Del 2011-12-13 18:47
					//////////////////////////////////////////////////////////
#if 0
					/* re-enter SIM if neccessary */
					gsm->state = ALLOGSM_STATE_SIM_PIN_REQ;
					gsm->ev.e = ALLOGSM_EVENT_PIN_REQUIRED;
					return &gsm->ev;
#endif
					//////////////////////////////////////////////////////////
					gsm->ev.e = ALLOGSM_EVENT_SIM_FAILED;
					return &gsm->ev;
				} else if(gsm_compare(buf, get_at(gsm->switchtype,AT_CME_ERROR))){
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
					gsm->retries=-1;
					gsm->ev.e = ALLOGSM_EVENT_PIN_ERROR;
					return &gsm->ev;
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
					gsm_switch_state(gsm, ALLOGSM_STATE_SIM_READY_REQ, get_at(gsm->switchtype,AT_ASK_PIN));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;
			case ALLOGSM_STATE_INIT_4:
#define NO_WIND_BLOWING 1
#ifndef NO_WIND_BLOWING
				if (gsm->wind_state & WIND_4_READY_ALL_AT_COMMANDS)
#endif
					gsm_switch_state(gsm, ALLOGSM_STATE_IMSI_REQ, get_at(gsm->switchtype,AT_IMSI));
				break;
			case ALLOGSM_STATE_IMSI_REQ:
				/* IMSI (International Mobile Subscriber Identity number). */
				if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_IMSI))) {
					if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))&&strlen(gsm->imsi)) {
						if(get_at(gsm->switchtype,AT_MOC_ENABLED))
						{
							gsm_switch_state(gsm, ALLOGSM_STATE_MOC_STATE_ENABLED, get_at(gsm->switchtype,AT_MOC_ENABLED));
						}
						else if(get_at(gsm->switchtype,AT_SET_SIDE_TONE))
						{
							gsm_switch_state(gsm, ALLOGSM_STATE_SET_SPEAKER, get_at(gsm->switchtype,AT_SPEAKER));
						}
						else
						{
							gsm_switch_state(gsm, ALLOGSM_STATE_CLIP_ENABLED, get_at(gsm->switchtype,AT_CLIP_ENABLED));
						}
					} else if (gsm_compare(buf, "+CME ERROR: 14") ){
						gsm_schedule_event(gsm, 1000, gsm_sim_waiting_sched, call);
					} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
					} else if (!gsm_compare(buf, get_at(gsm->switchtype,AT_CME_ERROR))) {
						gsm_get_imsi(gsm, buf);
					} else {
						//Freedom del 2012-06-05 15:50
						//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
					}
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break; 
			case ALLOGSM_STATE_MOC_STATE_ENABLED:
#ifdef TESTING
				if(1){
#else
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
#endif
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_MOC_ENABLED))) {
						if(get_at(gsm->switchtype,AT_SET_SIDE_TONE))
						{
							gsm_switch_state(gsm, ALLOGSM_STATE_SET_SPEAKER, get_at(gsm->switchtype,AT_SPEAKER));
						}
						else
						{
							gsm_switch_state(gsm, ALLOGSM_STATE_CLIP_ENABLED, get_at(gsm->switchtype,AT_CLIP_ENABLED));
						}
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
					gsm_send_at(gsm, get_at(gsm->switchtype,AT_MOC_ENABLED));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;

			case ALLOGSM_STATE_SET_SPEAKER:

				if (gsm_compare(buf,get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SPEAKER))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_GAIN_INDEX,  get_at(gsm->switchtype,AT_SET_GAIN_INDEX));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
					gsm_send_at(gsm, get_at(gsm->switchtype, AT_SPEAKER));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;

			case ALLOGSM_STATE_SET_GAIN_INDEX:

				if (gsm_compare(buf,get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SET_GAIN_INDEX))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_SIDE_TONE, get_at(gsm->switchtype,AT_SET_SIDE_TONE));
					}
				} else {
				}
				break;

			case ALLOGSM_STATE_SET_SIDE_TONE:
#ifdef TESTING
				if(1){
#else
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
#endif
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SET_SIDE_TONE))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_NOISE_CANCEL, get_at(gsm->switchtype,AT_NOISE_CANCEL));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
					gsm_send_at(gsm, get_at(gsm->switchtype,AT_SET_SIDE_TONE));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;

			case ALLOGSM_STATE_SET_NOISE_CANCEL:
#ifdef TESTING
				if(1){
#else
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
#endif
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_NOISE_CANCEL))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_DEL_SIM_MSG, get_at(gsm->switchtype,AT_DEL_MSG));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
					gsm_send_at(gsm, get_at(gsm->switchtype,AT_NOISE_CANCEL));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;

			case ALLOGSM_STATE_DEL_SIM_MSG:
#ifdef TESTING
				if(1){
#else
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
#endif
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_DEL_MSG))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_SPEEK_VOL, NULL);
						module_set_gain(gsm,gsm->vol);
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
					gsm_send_at(gsm, get_at(gsm->switchtype,AT_DEL_MSG));
#endif
				} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_CMS_ERROR))){
                			gsm->sched_state = ALLOGSM_STATE_DEL_SIM_MSG;
					strcpy(gsm->sched_command, get_at(gsm->switchtype,AT_DEL_MSG));
					gsm_schedule_event(gsm, 2000, gsm_cmd_sched, call);
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;

			case ALLOGSM_STATE_SET_SPEEK_VOL:
#ifdef TESTING
				if(1){
#else
				if (gsm_compare(buf,get_at(gsm->switchtype,AT_OK))) {
#endif
					gsm_switch_state(gsm, ALLOGSM_STATE_SET_MIC_VOL, NULL);
					module_set_gain(gsm,gsm->mic);
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
					module_set_gain(gsm,gsm->vol);
#endif
				} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_CME_ERROR))) {
					gsm_send_at(gsm, get_at(gsm->switchtype, AT_CHECK));
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;
			case ALLOGSM_STATE_SET_MIC_VOL:
#ifdef TESTING
				if(1){
#else
				if (gsm_compare(buf,get_at(gsm->switchtype,AT_OK))) {
#endif
					gsm_switch_state(gsm, ALLOGSM_STATE_SET_ECHOCANSUP, NULL);
					module_set_echocansup(gsm,gsm->echocanval);
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
					module_set_gain(gsm,gsm->mic);
#endif
				} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_CME_ERROR))) {
					gsm_send_at(gsm, get_at(gsm->switchtype, AT_CHECK));
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;

			case ALLOGSM_STATE_SET_ECHOCANSUP:

				if (gsm_compare(buf,get_at(gsm->switchtype,AT_OK))) {
					gsm_switch_state(gsm, ALLOGSM_STATE_SET_CALL_NOTIFICATION, get_at(gsm->switchtype,AT_CALL_NOTIFICATION));
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
					module_set_echocansup(gsm,gsm->echocanval);
#endif
				} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_CME_ERROR))) {
					gsm_send_at(gsm, get_at(gsm->switchtype, AT_CHECK));
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;

			case ALLOGSM_STATE_SET_CALL_NOTIFICATION:

				if (gsm_compare(buf,get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_CALL_NOTIFICATION))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_DTMF_DETECTION, get_at(gsm->switchtype,AT_DTMF_DETECTION));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
					gsm_send_at(gsm, get_at(gsm->switchtype, AT_CALL_NOTIFICATION));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;

			case ALLOGSM_STATE_SET_DTMF_DETECTION:

				if (gsm_compare(buf,get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_DTMF_DETECTION))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_CLIP_ENABLED, get_at(gsm->switchtype,AT_CLIP_ENABLED));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
					gsm_send_at(gsm, get_at(gsm->switchtype, AT_DTMF_DETECTION));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;

			case ALLOGSM_STATE_CLIP_ENABLED:
#ifdef TESTING
				if(1){
#else
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
#endif
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_CLIP_ENABLED))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_RSSI_ENABLED, get_at(gsm->switchtype,AT_RSSI_ENABLED));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
					gsm_send_at(gsm, get_at(gsm->switchtype, AT_CLIP_ENABLED));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;
			case ALLOGSM_STATE_RSSI_ENABLED:
#ifdef TESTING
				if(1){
#else
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
#endif
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_RSSI_ENABLED))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SMS_MODE, get_at(gsm->switchtype,AT_SEND_SMS_PDU_MODE));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_RSSI_ENABLED));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;	

			case ALLOGSM_STATE_SMS_MODE:

				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SEND_SMS_PDU_MODE))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_NET_URC, get_at(gsm->switchtype,AT_SET_NET_URC));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_SEND_SMS_PDU_MODE));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;	
			case ALLOGSM_STATE_SET_NET_URC:
#ifdef TESTING
				if(1){
#else
				if (gsm_compare(buf,  get_at(gsm->switchtype,AT_OK))) {
#endif
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SET_NET_URC))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_NET_REQ, get_at(gsm->switchtype,AT_ASK_NET));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_SET_NET_URC));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, "!%s!\n", buf);
				} 
				break;			
			case ALLOGSM_STATE_NET_REQ:
				if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_ASK_NET))) {
					trim_CRLF(buf);
					if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {						
						if ((GSM_NET_HOME == gsm->network) || (GSM_NET_ROAMING == gsm->network)) {
							gsm_switch_state(gsm, ALLOGSM_STATE_NET_OK, get_at(gsm->switchtype,AT_NET_OK));
						} else {
							gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_DOWN;
							//sleep (5);
							//gsm_switch_state(gsm, ALLOGSM_STATE_NET_REQ, get_at(gsm->switchtype,AT_ASK_NET)); /* Req Net Status */
							return &gsm->ev;
						}	/* FIXME ---- Do something better here... use a timer... */
					} else if (!strcmp(buf, get_at(gsm->switchtype,AT_CREG10))) {
						gsm_message(gsm,"ALLO GSM: Span: %d UNREGISTERED\n ", gsm->span);
						gsm->network = GSM_NET_UNREGISTERED;
					} else if (!strcmp(buf, get_at(gsm->switchtype,AT_CREG11))) {
						gsm_message(gsm,"ALLO GSM: Span: %d  REGISTERED TO HOME NETWORK\n", gsm->span);
						gsm->network = GSM_NET_HOME;
					} else if (!strcmp(buf, get_at(gsm->switchtype,AT_CREG12))) {
						gsm_message(gsm,"ALLO GSM: Span: %d  SEARCHING FOR NETWORK\n", gsm->span);
						gsm->network = GSM_NET_SEARCHING;
					} else if (!strcmp(buf, get_at(gsm->switchtype,AT_CREG13))) {
						gsm_message(gsm,"ALLO GSM: Span: %d  NETWORK DENIED\n", gsm->span);
						gsm->network = GSM_NET_DENIED;
					} else if (!strcmp(buf, get_at(gsm->switchtype,AT_CREG14))) {
						gsm_message(gsm,"ALLO GSM: Span: %d  UNKNOWN NETWORK\n", gsm->span);
						gsm->network = GSM_NET_UNKNOWN;
					} else if (!strcmp(buf, get_at(gsm->switchtype,AT_CREG15))) {
						gsm_message(gsm,"ALLO GSM: Span: %d  REGISTERED TO ROAMING NETWORK\n", gsm->span);
						gsm->network = GSM_NET_ROAMING;
#ifdef WAVECOM_JUNK
					} else if (gsm_compare(buf, "+CREG: ")) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_ASK_NET));
					} else if (gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_ASK_NET));
#endif
					} else {
						gsm->network		= GSM_NET_UNREGISTERED;
						gsm->coverage		= -1;
						gsm->net_name[0]	= 0x0;
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_ASK_NET));
#endif
				} else {
					if(gsm->ev.gen.e == ALLOGSM_EVENT_DCHAN_DOWN) {
						if(gsm->send_at) {
							gsm_message(gsm, "%s\n", data);
							gsm->send_at = 0;
						}
					} else {
						//Freedom del 2012-06-05 15:50
						//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
					}
				}
				break;
			case ALLOGSM_STATE_NET_OK:
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_NET_OK))) { /* set only <format> (for read command +COPS?) ¨C not shown in Read command response */
						memset(gsm->sim_smsc, 0, sizeof(gsm->sim_smsc));
						gsm_switch_state(gsm, ALLOGSM_STATE_GET_SMSC_REQ, get_at(gsm->switchtype,AT_GET_SMSC));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_NET_OK));
#endif
				}
				break;

			case ALLOGSM_STATE_GET_SMSC_REQ:
				/* Request smsc */
				if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_GET_SMSC))) {
						
					if (gsm_compare(buf,get_at(gsm->switchtype,AT_OK))&&strlen(gsm->sim_smsc)) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SET_CHARSET, get_at(gsm->switchtype,AT_SMS_SET_CHARSET));
					} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
					} else if (gsm_compare(buf, "+CMS ERROR: 301") ){
						gsm_schedule_event(gsm, 1000, gsm_sms_service_waiting, call);
					} else if (!gsm_compare(buf, get_at(gsm->switchtype,AT_CME_ERROR))) {
						if (gsm_compare(buf, "+CSCA: "))
							gsm_get_smsc(gsm, buf);
					} else {
						//Freedom del 2012-06-05 15:50
						//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_GET_SMSC));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;

			case ALLOGSM_STATE_SMS_SET_CHARSET:
#ifdef TESTING
				if(1){
#else
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
#endif
					if(get_at(gsm->switchtype,AT_MODE)){
						gsm_switch_state(gsm, ALLOGSM_AT_MODE, get_at(gsm->switchtype,AT_MODE));
					} else if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SMS_SET_CHARSET))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SET_INDICATION, get_at(gsm->switchtype,AT_GSM));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_SMS_SET_CHARSET));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;

			case ALLOGSM_AT_MODE:
#ifdef TESTING
				if(1){
#else
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
#endif
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_MODE))) { /*add by makes 2012-4-10 15:54*/
						gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SET_INDICATION, get_at(gsm->switchtype,AT_GSM));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_MODE));
#endif
				}
				break;

			case ALLOGSM_STATE_SMS_SET_INDICATION:
#ifdef TESTING
				if(1){
#else
				if (gsm_compare(buf,get_at(gsm->switchtype,AT_OK))) {
#endif
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_GSM))) {
						gsm->sms_mod_flag = SMS_TEXT;
						sleep(1);
						memset(gsm->net_name, 0, sizeof(gsm->net_name));
						gsm_switch_state(gsm, ALLOGSM_STATE_NET_NAME_REQ, get_at(gsm->switchtype,AT_ASK_NET_NAME));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_GSM));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;
			case ALLOGSM_STATE_NET_NAME_REQ:
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_CHECK_NET))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_ASK_NET_NAME))) {
						gsm_get_operator(gsm, buf);
						UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_NULL);
						call->peercallstate = AT_CALL_STATE_NULL;
#ifdef VIRTUAL_TTY
						if(gsm->already_set_mux_mode) {
							gsm_switch_state(gsm, ALLOGSM_STATE_NET_NAME_REQ, get_at(gsm->switchtype,AT_NET_NAME));
						} else {
							gsm_switch_state(gsm, ALLOGSM_INIT_MUX, get_at(gsm->switchtype,AT_CHECK));
						}
#else

						if (gsm_compare("UPDATE AVAILABLE" ,get_at(gsm->switchtype,AT_UPDATE_2))) {
							if (gsm->span==atoi(get_at(gsm->switchtype,AT_UPDATE_SPAN))) {
								gsm_message(gsm,"Updating Span %d with %s\n", gsm->span, get_at(gsm->switchtype,AT_UPDATE_CMD));
								gsm_switch_state(gsm, ALLOGSM_STATE_UPDATE_2, get_at(gsm->switchtype, AT_UPDATE_CMD));
							}else{
								gsm->state = ALLOGSM_STATE_UPDATE_SUCCESS;
							}

						}else{
//							gsm_schedule_event_lTime(gsm, 10*60*1000, gsm_start_timeout_junk, gsm);
							gsm_switch_state(gsm, ALLOGSM_STATE_NET_NAME_REQ, get_at(gsm->switchtype,AT_NET_NAME));
						}

#endif
						if (gsm->retranstimer) {
							gsm_schedule_del(gsm, gsm->retranstimer);
							gsm->retranstimer = 0;
						}
						if (gsm->resetting) {
							gsm->resetting = 0;
							gsm->ev.gen.e = ALLOGSM_EVENT_RESTART_ACK;
						} else {
							gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_UP;
						}
						return &gsm->ev;
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_ASK_NET_NAME));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!,last at tx:[%s]\n", DEBUGARGS, buf,gsm->at_last_sent);
				}
				break;
#ifdef VIRTUAL_TTY
			case ALLOGSM_INIT_MUX:
				gsm->ev.gen.e = ALLOGSM_EVENT_INIT_MUX;
				return &gsm->ev;
				break;
#endif //VIRTUAL_TTY
			case ALLOGSM_STATE_READY:
				/* Request operators */
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_CHECK_NET))) { /* AT+COPS? */
					//printf("cops buf: >>%s<<\n", buf);
						gsm_get_operator(gsm, buf);
						//strncpy(gsm->net_name, buf, sizeof(gsm->net_name));
			//	} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					//Freedom notice must to rewrite this code 2011-10-12 15:13
			//		gsm_message(gsm, "%s\n", data);
				} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_RING))) { /* RING  */
					/*This condition checks RINGS should not exeed a count of 2.
					  Coz after a RING, CLIP event comes.
					  If CLIP doesnt comes, assume no caller id gonna come and 
					  we gonna make a fake CLIP EVENT and goto next condition

					If anytime we are getting a call with UNKNOWN number 
					1st check CLIP is coming or not, If coming debug here otherwise everything is cool 
					*/
                                        if (call->newcall){
                                                call->ring_count=call->ring_count+1;
                                                if (call->ring_count>1){
							/* lets build CLIP RESPONSE */
							memset(buf, 0x0, sizeof(buf));
							sprintf(buf, "+CLIP: \"UNKNOWN\",129,1,,\"UNKNOWN\"");
                                                        goto call_without_callerid;
						}
                                        }
				} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_INCOMING_CALL))) { /* Incoming call */
call_without_callerid:
					/*If getting a CLIP event for first time, then only go inside this condition otherwise break*/
					if (!call->newcall) {
						break;
					}
					call->newcall = 0;

					char caller_id[64];
					/* An unsolicited result code is returned after every RING at a mobile terminating call  */
					/* +CLIP: <number>, <type>,¡±¡±,,<alphaId>,<CLI validity> */
					get_cid(gsm->switchtype,buf,caller_id,sizeof(caller_id));

					/* Set caller number and caller name (if provided) */
					if (!strlen(caller_id)) {
						printf("kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk\n");
						strncpy(gsm->ev.ring.callingnum, "", sizeof(gsm->ev.ring.callingnum));
					} else {
						strncpy(gsm->ev.ring.callingnum, caller_id, sizeof(gsm->ev.ring.callingnum));
						strncpy(gsm->ev.ring.callingname, caller_id, sizeof(gsm->ev.ring.callingname));
					}

					/* Return ring event */
					UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_CALL_PRESENT);
					call->peercallstate = AT_CALL_STATE_CALL_INITIATED;
					call->alive = 1;
					gsm->state = ALLOGSM_STATE_RING;
					gsm->ev.e					= ALLOGSM_EVENT_RING;
					gsm->ev.ring.channel		= call->channelno; /* -1 : default */
					gsm->ev.ring.cref		= call->cr;
					gsm->ev.ring.call		= call;
					gsm->ev.ring.layer1		= ALLOGSM_LAYER_1_ALAW; /* a law */
					gsm->ev.ring.complete		= call->complete; 
					gsm->ev.ring.progress		= call->progress;
					gsm->ev.ring.progressmask	= call->progressmask;
#if 1
					gsm->ev.ring.callednum[0]	= '\0';				/* called number should not be existed */ 
#else
                                        char tmp[4] ; 
                                        sprintf (tmp, "%d", gsm->span) ;
					strncpy(gsm->ev.ring.callednum, tmp, sizeof(gsm->ev.ring.callednum));
#endif
				//	gsm_switch_state(gsm, ALLOGSM_STATE_RING, get_at(gsm->switchtype,AT_RING));//sujay
					return &gsm->ev;
#ifdef WAVECOM
/*  Should not call AT+CSQ(AT_NET_NAME) here..
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: 4") || gsm_compare(buf, "+WIND: 7")) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_NET_NAME));
*/
#endif
				} else {
					//gsm_message(gsm, "send_at:%d %s\n", gsm->send_at,data);
					if(gsm->send_at) {
						gsm_message(gsm, "%s\n", data);
						gsm->send_at = 0;
					}
				}
				break;
			case ALLOGSM_STATE_RING:
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_RING))) {
					call->alive = 1;
					gsm->state = ALLOGSM_STATE_RINGING;
					gsm->ev.e			= ALLOGSM_EVENT_RINGING;
					gsm->ev.ring.channel		= call->channelno;
					gsm->ev.ring.cref		= call->cr;
					gsm->ev.ring.progress		= call->progress;
					gsm->ev.ring.progressmask	= call->progressmask;
					return &gsm->ev;
				}
				break;
			//Freedom Add 2011-12-08 15:32 Check reject call
			////////////////////////////////////////////////////
			case ALLOGSM_STATE_RINGING:
#ifdef WAVECOM
				if( gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER)) ||
					gsm_compare(buf, get_at(gsm->switchtype,AT_NO_ANSWER)) || gsm_compare(buf, "+WIND: 6,") ) {
#else				
				if( gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER)) ||
					gsm_compare(buf, get_at(gsm->switchtype,AT_NO_ANSWER)) ) {
#endif					
					gsm->state = ALLOGSM_STATE_READY;
					UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_NULL);
					call->peercallstate = AT_CALL_STATE_NULL;
					gsm->ev.e = ALLOGSM_EVENT_HANGUP;
					gsm->ev.hangup.channel = call->channelno;
					gsm->ev.hangup.cause = ALLOGSM_CAUSE_NO_ANSWER;
					gsm->ev.hangup.cref = call->cr;
					gsm->ev.hangup.call = call;
					call->alive = 0;
					call->sendhangupack = 0;
					allogsm_destroycall(gsm, call);
					return &gsm->ev;
				}
				break;
			////////////////////////////////////////////////////
			case ALLOGSM_STATE_PRE_ANSWER:
				/* Answer the remote calling */
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_ANSWER))) {
						UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_ACTIVE);
						call->peercallstate = AT_CALL_STATE_ACTIVE;
						call->alive = 1;
						gsm->state = ALLOGSM_STATE_CALL_ACTIVE;
						gsm->ev.e = ALLOGSM_EVENT_ANSWER;
						gsm->ev.answer.progress	= 0;
						gsm->ev.answer.channel	= call->channelno;
						gsm->ev.answer.cref		= call->cr;
						gsm->ev.answer.call		= call;
						return &gsm->ev;
					}
				} else if (gsm_compare(buf, "+CME ERROR: 515")){
					gsm->answer_retry++;
					if (gsm->answer_retry < 3){
						gsm_switch_state(gsm, ALLOGSM_STATE_PRE_ANSWER, get_at(gsm->switchtype,AT_ANSWER));
					}else{
						allogsm_hangup(gsm, call, ALLOGSM_CAUSE_NORMAL_CLEARING);
					}
				}
				break;
			case ALLOGSM_STATE_CALL_ACTIVE:
				/* Remote end of active all. Waiting ...*/
#ifdef WAVECOM
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER)) || 
					gsm_compare(buf, get_at(gsm->switchtype,AT_NO_ANSWER)) ||  gsm_compare(buf,"+WIND: 6,") 
					||  gsm_compare(buf,"BUSY") || gsm_compare(buf,"+CREG: ")) {
#else
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER)) || 
					gsm_compare(buf, get_at(gsm->switchtype,AT_NO_ANSWER))) {
#endif
					gsm_switch_state(gsm, ALLOGSM_STATE_READY, get_at(gsm->switchtype,AT_NET_NAME));
					UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_NULL);
					call->peercallstate = AT_CALL_STATE_NULL;
					call->alive = 0;
					call->sendhangupack = 0;
					gsm->ev.e				= ALLOGSM_EVENT_HANGUP;
					gsm->ev.hangup.channel	= 1;
					gsm->ev.hangup.cause	= ALLOGSM_CAUSE_NORMAL_CLEARING;
					gsm->ev.hangup.cref		= call->cr;
					gsm->ev.hangup.call		= call;
					allogsm_hangup(gsm, call, ALLOGSM_CAUSE_NORMAL_CLEARING);
					return &gsm->ev;
				}
#if 1
				if (gsm_compare(buf, "+WDDI")){

					char *dtmf_digit;
					int duration;
					char *pbuf;
					pbuf= buf;
					strsep(&pbuf, "\"");
					dtmf_digit=(char *)strsep(&pbuf, "\"");
					strsep(&pbuf, ",");
					duration=atoi(pbuf);
					gsm_message(gsm,"DTMF%s Duration:%d\n", dtmf_digit, duration);

                                        gsm->ev.e              = ALLOGSM_EVENT_KEYPAD_DIGIT;
                                        gsm->ev.digit.channel  = 1;
                                        gsm->ev.digit.call     = call;
                                        gsm->ev.digit.duration = duration;
                                        sprintf(gsm->ev.digit.digits, "%s",dtmf_digit);
//#define IGNORE_LESS_DURATION_DTMF 1
#ifdef IGNORE_LESS_DURATION_DTMF
					if (duration<80)
						gsm_message(gsm,"Ignoring DTMF coz of duration %d\n", duration);
					else
                                        	return &gsm->ev;
#else
					//gsm_message(gsm,"Ignoring ALL DTMF CORRECT IT\n");
                                        return &gsm->ev;
#endif
				}
#endif

//#define CALL_WAITING 0
#ifdef CALL_WAITING
				if (gsm_compare(buf, "+CCWA:")){
/*
					char *number;
					char *pbuf;
					pbuf= buf;
					strsep(&pbuf, "\"");
					number=(char *)strsep(&pbuf, "\"");
					gsm_message(gsm,"Call Waiting from %s Duration:%d\n", num);
                                        gsm->ev.e              = ALLOGSM_EVENT_CALL_WAITING;
                                        gsm->ev.ring.channel   = 1;
                                        gsm->ev.waiting.call   = call;
                                        sprintf(gsm->ev.waiting.number, "%s",number);
                                        return &gsm->ev;
*/


					char caller_id[64];
					get_waiting(gsm->switchtype,buf,caller_id,sizeof(caller_id));
					gsm_message(gsm,"Call Waiting from %s \n", caller_id);

					if (!strlen(caller_id)) {
						strncpy(gsm->ev.ring.callingnum, "UNKNOWN", sizeof(gsm->ev.ring.callingnum));
					} else {
						strncpy(gsm->ev.ring.callingnum, caller_id, sizeof(gsm->ev.ring.callingnum));
						strncpy(gsm->ev.ring.callingname, caller_id, sizeof(gsm->ev.ring.callingname));
					}

					/* Return ring event */
					UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_CALL_WAITING);
					call->peercallstate = AT_CALL_STATE_CALL_INITIATED;
					call->alive = 1;
					//gsm->state = ALLOGSM_STATE_RING;
					gsm->ev.e			= ALLOGSM_EVENT_CALL_WAITING;
					gsm->ev.ring.channel		= call->channelno; /* -1 : default */
					gsm->ev.ring.cref		= call->cr;
					gsm->ev.ring.call		= call;
					gsm->ev.ring.layer1		= ALLOGSM_LAYER_1_ALAW; /* a law */
					gsm->ev.ring.complete		= call->complete; 
					gsm->ev.ring.progress		= call->progress;
					gsm->ev.ring.progressmask	= call->progressmask;
#if 1
					gsm->ev.ring.callednum[0]	= '\0';		/* called number should not be existed */ 
#else
                                        char tmp[4] ; 
                                        sprintf (tmp, "%d", gsm->span) ;
					strncpy(gsm->ev.ring.callednum, tmp, sizeof(gsm->ev.ring.callednum));
#endif
					return &gsm->ev;



				}
#endif
#if 0
				if (gsm_compare(buf, "+CCWA:")){
					char caller_id[64];
					get_waiting(gsm->switchtype,buf,caller_id,sizeof(caller_id));
					gsm_message(gsm,"Call Waiting from %s \n", caller_id);

					if (!strlen(caller_id)) {
						strncpy(gsm->call_waiting_caller_id, "UNKNOWN", sizeof(gsm->call_waiting_caller_id));
					} else {
						strncpy(gsm->call_waiting_caller_id, caller_id, sizeof(gsm->call_waiting_caller_id));
					}
                                        gsm->state = ALLOGSM_STATE_CALL_WAITING;
				}
#endif
				if (gsm_compare(buf, "+WIND: 5,")){

        				char *pbuf = buf;
				        strsep(&pbuf, ",");
				        int idx = atoi(pbuf);
					
					if (gsm->call_waiting_enabled){
						call->call_waiting_idx		= idx;

						gsm->ev.e			= ALLOGSM_EVENT_CALL_WAITING;
						gsm->ev.ring.channel		= call->channelno; /* -1 : default */
						gsm->ev.ring.cref		= call->cr;
						gsm->ev.ring.call		= call;
						gsm->ev.ring.layer1		= ALLOGSM_LAYER_1_ALAW; /* a law */
						gsm->ev.ring.complete		= call->complete; 
						gsm->ev.ring.progress		= call->progress;
						gsm->ev.ring.progressmask	= call->progressmask;
						gsm->ev.ring.callednum[0]	= '\0';		/* called number should not be existed */ 
						
						gsm->callwaitingsched = gsm_schedule_event(gsm, 3000, gsm_callwaiting_sched, call);
						gsm->state = ALLOGSM_STATE_CALL_WAITING;
						return &gsm->ev;
					} else {
						if(idx == 2)
							gsm_switch_state(gsm, ALLOGSM_STATE_CALL_ACTIVE, "AT+CHLD=12");
						else if (idx == 1)
							gsm_switch_state(gsm, ALLOGSM_STATE_CALL_ACTIVE, "AT+CHLD=11");
					}
				}
				break;

			case ALLOGSM_STATE_CALL_WAITING:
				{
				unsigned char waiting_wind[20], held_wind[20];
				if (call->call_waiting_idx == 1){
					sprintf(held_wind, "+WIND: 6,2");
					sprintf(waiting_wind, "+WIND: 6,1");
				}else{
					sprintf(held_wind, "+WIND: 6,1");
					sprintf(waiting_wind, "+WIND: 6,2");
				}

				/* Remote end of active call. Waiting ...*/
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER)) || 
				    gsm_compare(buf, get_at(gsm->switchtype,AT_NO_ANSWER)) ||  
				    gsm_compare(buf, held_wind) ||
				    gsm_compare(buf,"BUSY") || 
				    gsm_compare(buf,"+CREG: ")) {
		
					gsm_switch_state(gsm, ALLOGSM_STATE_READY, get_at(gsm->switchtype,AT_NET_NAME));
					UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_NULL);
					call->peercallstate = AT_CALL_STATE_NULL;
					call->alive = 0;
					call->sendhangupack = 0;
					gsm->ev.e				= ALLOGSM_EVENT_HANGUP;
					gsm->ev.hangup.channel	= 1;
					gsm->ev.hangup.cause	= ALLOGSM_CAUSE_NORMAL_CLEARING;
					gsm->ev.hangup.cref		= call->cr;
					gsm->ev.hangup.call		= call;
					allogsm_hangup(gsm, call, ALLOGSM_CAUSE_NORMAL_CLEARING);

					gsm_schedule_del(gsm, gsm->callwaitingsched);

					return &gsm->ev;
						
				}else if (gsm_compare(buf,waiting_wind)){

					gsm_schedule_del(gsm, gsm->callwaitingsched);

                                        gsm->state = ALLOGSM_STATE_CALL_ACTIVE;
					gsm_message(gsm,"Liballogsmat: Call Waiting disconnected..\n");
					return NULL;
				}
				if (gsm_compare(buf, "OK")){
					if (gsm_compare(gsm->at_last_sent, "AT")) {
						gsm->ev.e			= ALLOGSM_EVENT_CALL_WAITING;
						gsm->ev.ring.channel		= call->channelno; /* -1 : default */
						gsm->ev.ring.cref		= call->cr;
						gsm->ev.ring.call		= call;
						gsm->ev.ring.layer1		= ALLOGSM_LAYER_1_ALAW; /* a law */
						gsm->ev.ring.complete		= call->complete; 
						gsm->ev.ring.progress		= call->progress;
						gsm->ev.ring.progressmask	= call->progressmask;
						gsm->ev.ring.callednum[0]	= '\0';		/* called number should not be existed */ 
						gsm->callwaitingsched = gsm_schedule_event(gsm, 3000, gsm_callwaiting_sched, call);
						gsm->state = ALLOGSM_STATE_CALL_WAITING;
						return &gsm->ev;
					}
				}
				}
				break;
			case ALLOGSM_STATE_HANGUP_REQ:
				/* Hangup the active call */
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
junk_received_for_ATH:
					if ((gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_HANGUP))) 
					   ||(gsm_compare(gsm->at_last_sent, "AT+CHLD=1"))) {
						gsm_schedule_del(gsm, gsm->hangupTimeoutSched);
						UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_NULL);
						call->peercallstate		= AT_CALL_STATE_NULL;
						call->alive = 0;
						call->sendhangupack = 0;
						gsm->state = ALLOGSM_STATE_READY;
						gsm->ev.e				= ALLOGSM_EVENT_HANGUP;
						gsm->ev.hangup.channel	= 1;
						gsm->ev.hangup.cause	= ALLOGSM_CAUSE_NORMAL_CLEARING; // trabajar el c->cause con el ^CEND
						gsm->ev.hangup.cref		= call->cr;
						gsm->ev.hangup.call		= call;
						gsm->ev.hangup.isCallWaitingHangup	= 0;
#ifdef WAVECOM
						//gsm_send_at(gsm, "AT+WIND=0");
#endif
						return &gsm->ev;
					}
                                } else if (gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER)) ||
                                        gsm_compare(buf, get_at(gsm->switchtype,AT_NO_ANSWER)) ||  gsm_compare(buf,"+WIND: 6,1")
                                        ||  gsm_compare(buf,"BUSY") || gsm_compare(buf,"+CREG: ")) {
                                        return NULL;
#ifdef WAVECOM
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_HANGUP));
#endif
				} else if (expectlist_compare(buf)) {
					goto junk_received_for_ATH;
#endif
				}
				break;
			case ALLOGSM_STATE_HANGUP_REQ_CALL_WAITING:
				/* Hangup the active call */
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if ((gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_HANGUP))) 
					   ||(gsm_compare(gsm->at_last_sent, "AT+CHLD=1"))) {

						gsm_schedule_del(gsm, gsm->callwaitingsched);

						UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_NULL);
						call->peercallstate		= AT_CALL_STATE_NULL;
						call->alive = 0;
						call->sendhangupack = 0;
						gsm->state = ALLOGSM_STATE_READY;
						gsm->ev.e				= ALLOGSM_EVENT_HANGUP;
						gsm->ev.hangup.channel	= 1;
						gsm->ev.hangup.cause	= ALLOGSM_CAUSE_NORMAL_CLEARING; // trabajar el c->cause con el ^CEND
						gsm->ev.hangup.cref		= call->cr;
						gsm->ev.hangup.call		= call;
						gsm->ev.hangup.isCallWaitingHangup	= 1;
#ifdef WAVECOM
						//gsm_send_at(gsm, "AT+WIND=0");
#endif
						return &gsm->ev;
					}
#ifdef WAVECOM
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_HANGUP));
#endif
				} else if (expectlist_compare(buf)) {
					goto junk_received_for_ATH;
#endif
				}
				break;
			case ALLOGSM_STATE_CALL_INIT:
                                /* After dial 
				   *PSCSC: <Call Id>, <State>, <Status>, [<Number>]  , [<type>], [<Line Id>], [<CauseSelect>], [<Cause>], [<Bearer>] 
			 	   *PSCSC:          , 0      ,         , "8888888888", 129     , 0          ,                ,          ,
					<State> State of the call
					0 MO call SETUP (if no control by SIM)
					1 MO call SETUP WITH CONTROL BY SIM (accepted)
					2 MO call SETUP ERROR (control by SIM rejected or other problem)
					3 MO call PROCEED
					4 MO call ALERT (at distant)
					5 MO call CONNECT (with distant)
					6-9 RFU
					10 MT call SETUP
					11 MT call SETUP ACCEPTED (Bearer capabilities accepted by the ME)
					12 MT call SETUP REJECTED (Bearer capabilities rejected by the ME)
					13 MT call ALERT
					14 MT call CONNECT (ME has successfully accepted the call)
					15 MT call CONNECT ERROR (ME was not able to accept the call)
					16-19 RFU
					20 Call DISCONNECT BY NETWORK
					21 Call DISCONNECT BY USER
					22 Call REJECT BY USER

					*PSCSC: 1, 0,, "050657038", 129, 0,,,	Proceeding
					*PSCSC: 1, 3,,,,,,,			Progressing
					*PSCSC: 1, 4,,,,,,,			Ringing
					*PSCSC: 1, 5, 0,,,,,,			Answered
					*PSCSC: 1, 20,,,,, 67, 16,		Hangup
				*/
#ifdef WAVECOM
                                if (gsm_compare(buf, "*PSCSC: 1, 0,")) {
					/* 0 MO call SETUP (if no control by SIM) */
					
                                } else if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					/* OK for ATD */

                                } else if (gsm_compare(buf, "*PSCSC: 1, 3,")) {
//                                      gsm->state = ALLOGSM_STATE_CALL_MADE;
                                        call->channelno = 1;
                                        //gsm_switch_state(gsm, ALLOGSM_STATE_CALL_PROCEEDING, get_at(gsm->switchtype,AT_CALL_PROCEEDING));
                                        gsm_switch_state(gsm, ALLOGSM_STATE_CALL_PROCEEDING, NULL);
                                        gsm->ev.e                       = ALLOGSM_EVENT_PROCEEDING;
                                        gsm->ev.proceeding.progress     = 8;
                                        gsm->ev.proceeding.channel      = call->channelno;
                                        gsm->ev.proceeding.cause        = 0;
                                        gsm->ev.proceeding.cref         = call->cr;
                                        gsm->ev.proceeding.call         = call;
                                        return &gsm->ev;
                                }else{
                                        if (gsm_compare(gsm->at_last_sent, "ATD")) {
						gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
                                                gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
                                        }
                                }
#else
//                              if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
                                                gsm_send_at(gsm, get_at(gsm->switchtype,AT_CALL_INIT));
                                                gsm->state = ALLOGSM_STATE_CALL_MADE;
//                              }
#endif
				break;
			case ALLOGSM_STATE_CALL_MADE:
#ifdef WAVECOM
#else
			//	if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_CALL_INIT))) {
						call->channelno = 1;
						//gsm_switch_state(gsm, ALLOGSM_STATE_CALL_PROCEEDING, get_at(gsm->switchtype,AT_CALL_PROCEEDING));
						gsm_switch_state(gsm, ALLOGSM_STATE_CALL_PROCEEDING, NULL);
						gsm->ev.e 					= ALLOGSM_EVENT_PROCEEDING;
						gsm->ev.proceeding.progress	= 8;
						gsm->ev.proceeding.channel	= call->channelno;
						gsm->ev.proceeding.cause	= 0;
						gsm->ev.proceeding.cref		= call->cr;
						gsm->ev.proceeding.call		= call;
						return &gsm->ev;
					}
				//}
#endif
				break;
			case ALLOGSM_STATE_CALL_PROCEEDING:
#ifdef WAVECOM
                                if (gsm_compare(buf, "*PSCSC: 1, 4,")) {
                                        //Freedom Add 2012-02-07 15:24
                                        //////////////////////////////////////////////////////////////////////////
#if SIMCOM900D_NO_ANSWER_BUG
                                        first = 1;
#endif //SIMCOM900D_NO_ANSWER_BUG
                                        //////////////////////////////////////////////////////////////////////////
                                        gsm->state = ALLOGSM_STATE_CALL_PROGRESS;
                                        gsm->ev.proceeding.e            = ALLOGSM_EVENT_PROGRESS;
                                        gsm->ev.proceeding.progress     = 8;
                                        gsm->ev.proceeding.channel      = call->channelno;
                                        gsm->ev.proceeding.cause        = 0;
                                        gsm->ev.proceeding.cref         = call->cr;
                                        gsm->ev.proceeding.call         = call;
                                        return &gsm->ev;
                                } else if (gsm_compare(buf, "OK")) {
					goto auto_answer_without_progress;
#if 0
				} else if (gsm_compare(buf, "+CREG: 1")){
					/*Added for reset bug*/
                                        if (gsm_compare(gsm->at_last_sent, "ATD")) {
						gsm_send_at(gsm, gsm->at_last_sent);
					}
#endif
                                } else {
                                        if (gsm_compare(gsm->at_last_sent, "ATD")) {
						gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
                                                gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
                                        }
                                }
#else
				//if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
				//	if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_CALL_PROCEEDING))) {
						
						//Freedom Add 2012-02-07 15:24
						//////////////////////////////////////////////////////////////////////////
#if SIMCOM900D_NO_ANSWER_BUG
						first = 1;
#endif //SIMCOM900D_NO_ANSWER_BUG
						//////////////////////////////////////////////////////////////////////////
						gsm->state = ALLOGSM_STATE_CALL_PROGRESS;
						gsm->ev.proceeding.e		= ALLOGSM_EVENT_PROGRESS;
						gsm->ev.proceeding.progress	= 8;
						gsm->ev.proceeding.channel	= call->channelno;
						gsm->ev.proceeding.cause	= 0;
						gsm->ev.proceeding.cref		= call->cr;
						gsm->ev.proceeding.call		= call;
						return &gsm->ev;
				//	}
				//}
#endif
				break;
			case ALLOGSM_STATE_CALL_PROGRESS:
				//Freedom Add 2012-02-07 15:24
				//////////////////////////////////////////////////////////////////////////
#if SIMCOM900D_NO_ANSWER_BUG
				if(first) {
					first = 0;
					gettimeofday(&start_time,NULL);
				} else {
					gettimeofday(&end_time,NULL);
					if((end_time.tv_sec-start_time.tv_sec) >= 30 ) {
						first = 1;
						gsm_message(gsm,"Dial Timeout\n");
						gsm->state = ALLOGSM_STATE_READY;
						UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_NULL);
						call->peercallstate = AT_CALL_STATE_NULL;
						gsm->ev.e = ALLOGSM_EVENT_HANGUP;
						gsm->ev.hangup.channel = call->channelno;
						gsm->ev.hangup.cause = ALLOGSM_CAUSE_NO_ANSWER;
						gsm->ev.hangup.cref = call->cr;
						gsm->ev.hangup.call = call;
						call->alive = 0;
						call->sendhangupack = 0;
						module_hangup(gsm, call);
						allogsm_destroycall(gsm, call);
						return &gsm->ev;
					}
				}
#endif //SIMCOM900D_NO_ANSWER_BUG
				//////////////////////////////////////////////////////////////////////////
#ifdef WAVECOM
auto_answer_without_progress:
                                	if (gsm_compare(buf, "*PSCSC: 1, 5,")) {
                                                call->alive = 1;
                                                gsm->state = ALLOGSM_STATE_CALL_ACTIVE;
//                                              gsm->state = ALLOGSM_STATE_PRE_ANSWER; // If AT_MO_CONNECTED is not proper, comment if case and use this state
                                                gsm->ev.gen.e = ALLOGSM_EVENT_ANSWER;
                                                gsm->ev.answer.progress = 0;
                                                gsm->ev.answer.channel = call->channelno;
                                                gsm->ev.answer.cref = call->cr;
                                                gsm->ev.answer.call = call;
                                                return &gsm->ev;
                                        } else if( gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER)) ||
                                        gsm_compare(buf, get_at(gsm->switchtype,AT_NO_ANSWER)) || gsm_compare(buf, "*PSCSC: 1, 20") ) {
                                                gsm->state = ALLOGSM_STATE_READY; 
                                                UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_NULL);
                                                call->peercallstate = AT_CALL_STATE_NULL;
                                                gsm->ev.e = ALLOGSM_EVENT_HANGUP;
                                                gsm->ev.hangup.channel = call->channelno;
                                                gsm->ev.hangup.cause = ALLOGSM_CAUSE_NO_ANSWER;
                                                gsm->ev.hangup.cref = call->cr;
                                                gsm->ev.hangup.call = call;
                                                call->alive = 0;
                                                call->sendhangupack = 0;
                                                allogsm_destroycall(gsm, call);
                                                return &gsm->ev;
                                        }
#else
//	     				if (gsm_compare(buf, get_at(gsm->switchtype,AT_MO_CONNECTED))) {
						call->alive = 1;
				//		gsm->state = ALLOGSM_STATE_CALL_ACTIVE;
						gsm->state = ALLOGSM_STATE_PRE_ANSWER; // If AT_MO_CONNECTED is not proper, comment if case and use this state
						gsm->ev.gen.e = ALLOGSM_EVENT_ANSWER;
						gsm->ev.answer.progress = 0;
						gsm->ev.answer.channel = call->channelno;
						gsm->ev.answer.cref = call->cr;
						gsm->ev.answer.call = call;
						return &gsm->ev;
//				} 
#endif
				break;
#ifdef CONFIG_CHECK_PHONE
			case ALLOGSM_STATE_PHONE_CHECK: /*add by makes 2012-04-10 11:03 */
			{
				if(time(NULL)<gsm->check_timeout){
					if (gsm_compare(buf, get_at(gsm->switchtype,AT_RING))){
						if(gsm->auto_hangup_flag)
						gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
							gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
						gsm->ev.gen.e = ALLOGSM_EVENT_CHECK_PHONE;
						gsm->ev.notify.info = PHONE_RING;
						gsm->phone_stat=PHONE_RING;
						return &gsm->ev;
					}
					else if(gsm_compare(buf, get_at(gsm->switchtype,AT_BUSY))){
						gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
						gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
						gsm->ev.gen.e = ALLOGSM_EVENT_CHECK_PHONE;
						gsm->ev.notify.info = PHONE_BUSY;
						gsm->phone_stat=PHONE_BUSY;
						return &gsm->ev;
					}
					else if(gsm_compare(buf, get_at(gsm->switchtype,AT_MO_CONNECTED))){
						gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
						gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
						gsm->ev.gen.e = ALLOGSM_EVENT_CHECK_PHONE;
						gsm->ev.notify.info = PHONE_CONNECT;
						gsm->phone_stat=PHONE_CONNECT;
						return &gsm->ev;
					}
					else if(gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER))){
						gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
						gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
						gsm->ev.gen.e = ALLOGSM_EVENT_CHECK_PHONE;
						gsm->ev.notify.info = PHONE_NOT_CARRIER;
						gsm->phone_stat=PHONE_NOT_CARRIER;
						return &gsm->ev;
					}
					else if(gsm_compare(buf, get_at(gsm->switchtype,AT_NO_ANSWER))){
						gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
						gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
						gsm->ev.gen.e = ALLOGSM_EVENT_CHECK_PHONE;
						gsm->ev.notify.info = PHONE_NOT_ANSWER;
						gsm->phone_stat=PHONE_NOT_ANSWER;
						return &gsm->ev;
					}
					else if(gsm_compare(buf, get_at(gsm->switchtype,AT_NO_DIALTONE))){
						gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
						gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
						gsm->ev.gen.e = ALLOGSM_EVENT_CHECK_PHONE;
						gsm->ev.notify.info = PHONE_NOT_DIALTONE;
						gsm->phone_stat=PHONE_NOT_DIALTONE;
						return &gsm->ev;
					}
#ifdef WAVECOM
					else if(gsm_compare(buf, "*PSCSC: 1, 20")){
						gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
						gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
						gsm->ev.gen.e = ALLOGSM_EVENT_CHECK_PHONE;
						gsm->ev.notify.info = PHONE_NOT_CARRIER;
						gsm->phone_stat=PHONE_NOT_CARRIER;
						return &gsm->ev;
					}
#endif 
				}
				else{
					if(gsm->auto_hangup_flag)
					{
						gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
						gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
						gsm->ev.gen.e = ALLOGSM_EVENT_CHECK_PHONE;
						gsm->ev.notify.info = PHONE_TIMEOUT;
						gsm->phone_stat=PHONE_TIMEOUT;
						return &gsm->ev;
					}
					else
					{
						if(gsm_compare(buf, get_at(gsm->switchtype,AT_BUSY))){
							gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
							gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
							gsm->ev.gen.e = ALLOGSM_EVENT_CHECK_PHONE;
							gsm->ev.notify.info = PHONE_BUSY;
							gsm->phone_stat=PHONE_BUSY;
							return &gsm->ev;
						}
						else if(gsm_compare(buf, get_at(gsm->switchtype,AT_MO_CONNECTED))){
							gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
							gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
							gsm->ev.gen.e = ALLOGSM_EVENT_CHECK_PHONE;
							gsm->ev.notify.info = PHONE_CONNECT;
							gsm->phone_stat=PHONE_CONNECT;
							return &gsm->ev;
						}
						else if(gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER))){
							gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
							gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
							gsm->ev.gen.e = ALLOGSM_EVENT_CHECK_PHONE;
							gsm->ev.notify.info = PHONE_NOT_CARRIER;
							gsm->phone_stat=PHONE_NOT_CARRIER;
							return &gsm->ev;
						}
						else if(gsm_compare(buf, get_at(gsm->switchtype,AT_NO_ANSWER))){
							gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
							gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
							gsm->ev.gen.e = ALLOGSM_EVENT_CHECK_PHONE;
							gsm->ev.notify.info = PHONE_NOT_ANSWER;
							gsm->phone_stat=PHONE_NOT_ANSWER;
							return &gsm->ev;
						}
						else if(gsm_compare(buf, get_at(gsm->switchtype,AT_NO_DIALTONE))){
							gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
							gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
							gsm->ev.gen.e = ALLOGSM_EVENT_CHECK_PHONE;
							gsm->ev.notify.info = PHONE_NOT_DIALTONE;
							gsm->phone_stat=PHONE_NOT_DIALTONE;
							return &gsm->ev;
						}
#ifdef WAVECOM
						else if(gsm_compare(buf, "*PSCSC: 1, 20")){
							gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
							gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
							gsm->ev.gen.e = ALLOGSM_EVENT_CHECK_PHONE;
							gsm->ev.notify.info = PHONE_NOT_CARRIER;
							gsm->phone_stat=PHONE_NOT_CARRIER;
							return &gsm->ev;
						}
#endif 
					}
				}
				break;
			}
#endif
			default:
				break;
		}

		i ++;
				
		if(gsm->state >= ALLOGSM_STATE_READY) {
#ifdef WAVECOM
			if(gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER)) ||
				gsm_compare(buf, get_at(gsm->switchtype,AT_NO_ANSWER)) || gsm_compare(buf, "*PSCSC: 1, 20")) {
#else
			if(gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER)) ||
				gsm_compare(buf, get_at(gsm->switchtype,AT_NO_ANSWER))) {
#endif
				gsm->state = ALLOGSM_STATE_READY;
				UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_NULL);
				call->peercallstate = AT_CALL_STATE_NULL;
				gsm->ev.e = ALLOGSM_EVENT_HANGUP;
				gsm->ev.hangup.channel = call->channelno;
				gsm->ev.hangup.cause = ALLOGSM_CAUSE_NO_ANSWER;
				gsm->ev.hangup.cref = call->cr;
				gsm->ev.hangup.call = call;
				call->alive = 0;
				call->sendhangupack = 0;
				allogsm_destroycall(gsm, call);
				return &gsm->ev;
			} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_NO_DIALTONE))) {
				gsm->state = ALLOGSM_STATE_READY;
				UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_NULL);
				call->peercallstate = AT_CALL_STATE_NULL;
				gsm->ev.e = ALLOGSM_EVENT_HANGUP;
				gsm->ev.hangup.cause = ALLOGSM_CAUSE_NETWORK_OUT_OF_ORDER;
				gsm->ev.hangup.cref = call->cr;
				gsm->ev.hangup.call = call;
				call->alive = 0;
				call->sendhangupack = 0;
				allogsm_destroycall(gsm, call);
				return &gsm->ev;
			} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_BUSY))) {
				gsm->state = ALLOGSM_STATE_READY;
				UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_NULL);
				call->peercallstate = AT_CALL_STATE_NULL;
				gsm->ev.e = ALLOGSM_EVENT_HANGUP;
				gsm->ev.hangup.channel = call->channelno;
				gsm->ev.hangup.cause = ALLOGSM_CAUSE_USER_BUSY;
				gsm->ev.hangup.cref = call->cr;
				gsm->ev.hangup.call = call;
				call->alive = 0;
				call->sendhangupack = 0;
				allogsm_destroycall(gsm, call);
				return &gsm->ev;
			}
		}
#if 0	
		res_event = module_check_wind(gsm, call ,buf, i);
		if (res_event) {
			return res_event;
		}
#endif		
		res_event = module_check_sms(gsm, buf, i);
		if (res_event) {
			return res_event;
		}

		res_event = module_check_network(gsm, call ,buf, i);
		if (res_event) {
			return res_event;
		}

		res_event = module_check_ussd(gsm, buf, i);
		if (res_event) {
			return res_event;
		}

		res_event = module_check_operator_list_query(gsm, buf, i);
                if (res_event) {
                        return res_event;
                }
		res_event = module_check_safe_at(gsm, buf, i);
                if (res_event) {
                        return res_event;
                }

	}

	return res_event;
}

#ifdef CONFIG_CHECK_PHONE
void module_hangup_phone(struct allogsm_modul *gsm)
{
	gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
	gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
}

int module_check_phone_stat(struct allogsm_modul *gsm, char *phone_number,int hangup_flag,unsigned int timeout)
{
	char buf[128];
	int time_out=0;
	memset(buf, 0x0, sizeof(buf));
	gsm->phone_stat=-1;
	gsm->auto_hangup_flag=hangup_flag;
	if(timeout<=0)
		time_out=DEFAULT_CHECK_TIMEOUT;
	else
		time_out=timeout;
	if(gsm->state!=ALLOGSM_STATE_READY){
		return -1;
	}
	else
	{
		gsm->check_timeout=time(NULL)+time_out;
		get_dial_str(gsm->switchtype, phone_number, buf, sizeof(buf));
		gsm_switch_state(gsm, ALLOGSM_STATE_PHONE_CHECK, buf);
	}
	return 0;
}
#endif

