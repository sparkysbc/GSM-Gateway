/*
 * liballogsmat: An implementation of ALLO GSM cards
 *
 * Parts taken from libpri
 * Written by mark.liu <mark.liu@openvox.cn>
 *
 * $Id: gsm.c 356 2011-06-15 02:56:27Z wuyiping $
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <stdarg.h>
#include <time.h>

#include "gsm_timers.h"
#include "liballogsmat.h"
#include "gsm_internal.h"
#include "gsm_module.h"
#include "gsm_config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//#define NO_WIND_BLOWING 1
struct msgtype causes[] = {
	{ ALLOGSM_CAUSE_UNALLOCATED,				"Unallocated (unassigned) number" },
	{ ALLOGSM_CAUSE_NO_ROUTE_TRANSIT_NET,		"No route to specified transmit network" },
	{ ALLOGSM_CAUSE_NO_ROUTE_DESTINATION,		"No route to destination" },
	{ ALLOGSM_CAUSE_CHANNEL_UNACCEPTABLE,		"Channel unacceptable" },
	{ ALLOGSM_CAUSE_CALL_AWARDED_DELIVERED,		"Call awarded and being delivered in an established channel" },
	{ ALLOGSM_CAUSE_NORMAL_CLEARING,			"Normal Clearing" },
	{ ALLOGSM_CAUSE_USER_BUSY,					"User busy" },
	{ ALLOGSM_CAUSE_NO_USER_RESPONSE,			"No user responding" },
	{ ALLOGSM_CAUSE_NO_ANSWER,					"User alerting, no answer" },
	{ ALLOGSM_CAUSE_CALL_REJECTED,				"Call Rejected" },
	{ ALLOGSM_CAUSE_NUMBER_CHANGED,				"Number changed" },
	{ ALLOGSM_CAUSE_DESTINATION_OUT_OF_ORDER,	"Destination out of order" },
	{ ALLOGSM_CAUSE_INVALID_NUMBER_FORMAT,		"Invalid number format" },
	{ ALLOGSM_CAUSE_FACILITY_REJECTED,			"Facility rejected" },
	{ ALLOGSM_CAUSE_RESPONSE_TO_STATUS_ENQUIRY,	"Response to STATus ENQuiry" },
	{ ALLOGSM_CAUSE_NORMAL_UNSPECIFIED,			"Normal, unspecified" },
	{ ALLOGSM_CAUSE_NORMAL_CIRCUIT_CONGESTION,	"Circuit/channel congestion" },
	{ ALLOGSM_CAUSE_NETWORK_OUT_OF_ORDER,		"Network out of order" },
	{ ALLOGSM_CAUSE_NORMAL_TEMPORARY_FAILURE,	"Temporary failure" },
	{ ALLOGSM_CAUSE_SWITCH_CONGESTION,			"Switching equipment congestion" },
	{ ALLOGSM_CAUSE_ACCESS_INFO_DISCARDED,		"Access information discarded" },
	{ ALLOGSM_CAUSE_REQUESTED_CHAN_UNAVAIL,		"Requested channel not available" },
	{ ALLOGSM_CAUSE_PRE_EMPTED,					"Pre-empted" },
	{ ALLOGSM_CAUSE_FACILITY_NOT_SUBSCRIBED,	"Facility not subscribed" },
	{ ALLOGSM_CAUSE_OUTGOING_CALL_BARRED,		"Outgoing call barred" },
	{ ALLOGSM_CAUSE_INCOMING_CALL_BARRED,		"Incoming call barred" },
	{ ALLOGSM_CAUSE_BEARERCAPABILITY_NOTAUTH,	"Bearer capability not authorized" },
	{ ALLOGSM_CAUSE_BEARERCAPABILITY_NOTAVAIL,	"Bearer capability not available" },
	{ ALLOGSM_CAUSE_BEARERCAPABILITY_NOTIMPL,	"Bearer capability not implemented" },
	{ ALLOGSM_CAUSE_SERVICEOROPTION_NOTAVAIL,	"Service or option not available, unspecified" },
	{ ALLOGSM_CAUSE_CHAN_NOT_IMPLEMENTED,		"Channel not implemented" },
	{ ALLOGSM_CAUSE_FACILITY_NOT_IMPLEMENTED,	"Facility not implemented" },
	{ ALLOGSM_CAUSE_INVALID_CALL_REFERENCE,		"Invalid call reference value" },
	{ ALLOGSM_CAUSE_IDENTIFIED_CHANNEL_NOTEXIST,"Identified channel does not exist" },
	{ ALLOGSM_CAUSE_INCOMPATIBLE_DESTINATION,	"Incompatible destination" },
	{ ALLOGSM_CAUSE_INVALID_MSG_UNSPECIFIED,	"Invalid message unspecified" },
	{ ALLOGSM_CAUSE_MANDATORY_IE_MISSING,		"Mandatory information element is missing" },
	{ ALLOGSM_CAUSE_MESSAGE_TYPE_NONEXIST,		"Message type nonexist." },
	{ ALLOGSM_CAUSE_WRONG_MESSAGE,				"Wrong message" },
	{ ALLOGSM_CAUSE_IE_NONEXIST,				"Info. element nonexist or not implemented" },
	{ ALLOGSM_CAUSE_INVALID_IE_CONTENTS,		"Invalid information element contents" },
	{ ALLOGSM_CAUSE_WRONG_CALL_STATE,			"Message not compatible with call state" },
	{ ALLOGSM_CAUSE_RECOVERY_ON_TIMER_EXPIRE,	"Recover on timer expiry" },
	{ ALLOGSM_CAUSE_MANDATORY_IE_LENGTH_ERROR,	"Mandatory IE length error" },
	{ ALLOGSM_CAUSE_PROTOCOL_ERROR,				"Protocol error, unspecified" },
	{ ALLOGSM_CAUSE_INTERWORKING,				"Interworking, unspecified" },
};

static char *code2str(int code, struct msgtype *codes, int max)
{
	int x;
	
	for (x = 0; x < max; x++) {
		if (codes[x].msgnum == code) {
			return codes[x].name;
		}
	}

	return "Unknown";
}

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

static int convert_str(const char* ori, char *buf, int len)
{
        char tbuf[1024];

        if(len < sizeof(tbuf)) return 0;

        strncpy(tbuf,ori,1024);

        trim_CRLF(tbuf);

        if(tbuf[0] == 0x1A && tbuf[1] == '\0') {  //Send Message end flag
                strcpy(tbuf,"Ctrl+Z");
        }

        return snprintf(buf,len,"TX:[%s]\r\n",tbuf);
}

/* 
 Initialize setting debug at commands to file /var/log/asterisk/at/span_num 
*/
static int __gsm_init_set_debugat(struct allogsm_modul *gsm)
{
        if(gsm->debug_at_fd <= 0)
        {
                char debug_at_file[256];
                char *debug_at_dir = "/var/log/asterisk/at";
                if(access(debug_at_dir, R_OK)) {
                        mkdir(debug_at_dir,0774);
                }
                snprintf(debug_at_file,256,"%s/%d",debug_at_dir,gsm->span);
                if(access(debug_at_file, R_OK)) {
                        gsm->debug_at_fd = open(debug_at_file,O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
                } else {
                        gsm->debug_at_fd = open(debug_at_file,O_WRONLY);
                        if(gsm->debug_at_fd > 0) {
                                lseek(gsm->debug_at_fd, 0, SEEK_END);
                        }
                }
        }

        return gsm->debug_at_fd > 0 ? 1 : 0;
}

/* 
 Clear setting debug at commands to file
*/
static void __gsm_deinit_set_debugat(struct allogsm_modul *gsm)
{
        if(gsm->debug_at_fd > 0) {
                close(gsm->debug_at_fd);
                gsm->debug_at_fd = 0;
        }
}


/******************************************************************************
 * Read AT Command or AT Command feedback info
 * If buflen is zero, __gsm_read() returns zero and has no other results
 * param:
 *		gsm: struct allogsm_modul
 *		buf: save the received info
 *		buflen: receive up to buflen bytes from D-channel
 * return:
 *		The number of bytes read is returned; this may be less than buflen
 *		-1 : error, and errno is set appropriately
 ******************************************************************************/
static int __gsm_read(struct allogsm_modul *gsm, void *buf, int buflen)
{
	int res = read(gsm->fd, buf, buflen);
	if (res < 0) {
		if (errno != EAGAIN) {
			gsm_error(gsm, "Read on %d failed: %s\n", gsm->fd, strerror(errno));
		}
		return 0;
	}

        if((gsm->debug_at_fd > 0) && (gsm->debug_at_flag)){
		write(gsm->debug_at_fd,buf,res);
		char tbuf[5];
		int tres;
        	tres = snprintf(tbuf,5,"\r\n");
		write(gsm->debug_at_fd,buf,tres);
	}
	
	return res;
}


/******************************************************************************
 * Write AT Command
 * param:
 *		gsm: struct allogsm_modul
 *		buf: AT Command
 *		buflen: AT Command Length
 * return:
 *		The number of bytes sent; this may be less than buflen
 ******************************************************************************/
static int __gsm_write(struct allogsm_modul *gsm, const void *buf, int buflen)
{
	int res = write(gsm->fd, buf, buflen);
	if (res < 0) {
		if (errno != EAGAIN) {
			gsm_error(gsm, "Write to %d failed: %s\n", gsm->fd, strerror(errno));
		}
		return 0;
	}

        if((gsm->debug_at_fd > 0) && (gsm->debug_at_flag)) {
		char wbuf[1024];
		int wlen;
		wlen = convert_str((const char *)buf,wbuf,1024);
                if(wlen > 0) {
			write(gsm->debug_at_fd,wbuf,wlen);
		}
		//printf("[%s]%s:%d len: %d, wbuf:%s \n", __FILE__, __func__, __LINE__,wlen, wbuf );
	}
	
	return res;
}

static int gsm_call_proceeding(struct allogsm_modul *gsm, struct alloat_call *c, int channel, int info)
{
	if (channel) { 
		channel &= 0xff;
		c->channelno = channel;		
	}

	/* Set channel flag */
	c->chanflags &= ~FLAG_PREFERRED;
	c->chanflags |= FLAG_EXCLUSIVE;

	/* Set our call state */
	UPDATE_OURCALLSTATE(gsm, c, AT_CALL_STATE_INCOMING_CALL_PROCEEDING);

	/* Set peer call state */
	c->peercallstate = AT_CALL_STATE_OUTGOING_CALL_PROCEEDING;
	
	if (info) {
		c->progloc = LOC_PRIV_NET_LOCAL_USER;			/* Set Progress location */
		c->progcode = CODE_CCITT;						/* Set Progress coding */
		c->progressmask = ALLOGSM_PROG_INBAND_AVAILABLE;	/* Set progress indicator */
	} else {
		c->progressmask = 0; /* Set progress indicator */
	}

	/* We've sent a call proceeding / alerting */
	c->proc = 1;

	/* Call is alive */
	c->alive = 1;
	
	return 0;
}


static int gsm_call_disconnect(struct allogsm_modul *gsm, struct alloat_call *c, int cause)
{
	/* Set our call state */
	UPDATE_OURCALLSTATE(gsm, c, AT_CALL_STATE_DISCONNECT_REQUEST);

	/* Set peer call state */	
	c->peercallstate = AT_CALL_STATE_DISCONNECT_INDICATION;

	/* Set call dead */
	if (c->alive) {
		c->alive			= 0;	/* call is not alive */
		c->cause			= cause;	/* Set event cause */
		c->causecode		= CODE_CCITT;	/* Set cause coding */
		c->causeloc			= LOC_PRIV_NET_LOCAL_USER; /* Set cause progress location */
		c->sendhangupack	= 1; 	/* Sending a hangup ack */
		/* Delete schedule */
		if (gsm->retranstimer) {
			gsm_schedule_del(gsm, gsm->retranstimer);
		}
		//c->retranstimer = gsm_schedule_event(gsm, gsm->timers[ALLOGSM_TIMER_T305], gsm_disconnect_timeout, c);
		return 0;
	} else {
		return 0;
	}
}

static void gsm_call_destroy(struct allogsm_modul *gsm, int cr, struct alloat_call *call)
{
	struct alloat_call *cur, *prev;

	prev = NULL;
	cur = *gsm->callpool;
	while(cur) {
		if ((call && (cur == call)) || (!call && (cur->cr == cr))) {
			if (prev) {
				prev->next = cur->next;
			}
			else {
				*gsm->callpool = cur->next;
			}
			if (gsm->debug & ALLOGSM_DEBUG_AT_STATE) {
				gsm_message(gsm, "NEW_HANGUP DEBUG: Destroying the call, ourstate %s, peerstate %s\n",callstate2str(cur->ourcallstate),callstate2str(cur->peercallstate));
			}
			/* Delete schedule */
			if (gsm->retranstimer) {
				gsm_schedule_del(gsm, gsm->retranstimer);
			}
			/* free alloat_call */
			free(cur);
			
			return;
		}
		prev = cur;
		cur = cur->next;
	}

	//Freedom Modify 2011-12-07 18:03
	if( NULL != call && 0 == cr ) {
		gsm_message(gsm, "Can't destroy call %d!\n", cr);
	}
	//gsm_message(gsm, "Can't destroy call %d!\n", cr);
}

/*Freedom Modify 2011-10-10 10:11*/
/*
static void gsm_default_timers(struct allogsm_modul *gsm, int switchtype)
{
	static const int defaulttimers[20][GSM_MAX_TIMERS] = GSM_TIMERS_ALL;
	int x;

	if (!gsm) {
		return;
	}

	for (x = 0; x < GSM_MAX_TIMERS; x++) {
		gsm->timers[x] = defaulttimers[switchtype][x];
	}
}
*/
static void gsm_default_timers(struct allogsm_modul *gsm)
{
	static const int defaulttimers[GSM_MAX_TIMERS] = GSM_TIMERS_DEFAULT;
	int x;

	if (!gsm) {
		return;
	}

	for (x = 0; x < GSM_MAX_TIMERS; x++) {
		gsm->timers[x] = defaulttimers[x];
	}
}


static void gsm_reset_timeout(void *data)
{
	struct alloat_call *c = data;
	struct allogsm_modul *gsm = c->gsm;
	
	if (gsm->debug & ALLOGSM_DEBUG_AT_DUMP) {
		gsm_message(gsm, "Timed out resetting span. Starting Reset again\n");
	}
	
	gsm->retranstimer = gsm_schedule_event(gsm, gsm->timers[ALLOGSM_TIMER_T316], gsm_reset_timeout, c);
	module_restart(gsm);
}


/******************************************************************************
 * Initialize allogsm_sr
 * param:
 *		req: struct allogsm_sr
 * return:
 *		void
 ******************************************************************************/
static void gsm_sr_init(struct allogsm_sr *req)
{
	memset(req, 0, sizeof(struct allogsm_sr));
}


/******************************************************************************
 * Get Network status info
 * param:
 *		id: gsm->network
 *			GSM_NET_UNREGISTERED 	Unregistered
 *			GSM_NET_HOME 			home Registered
 *			GSM_NET_ROAMING 		roaming
 *			other					Unknown Network Status
 * return:
 *		network status string info
 ******************************************************************************/
static char *gsm_network2str(int id)
{
	switch(id) {
		case GSM_NET_UNREGISTERED:
			return "Not registered";
		case GSM_NET_HOME:
			return "Registered (Home network)";
		case GSM_NET_SEARCHING:
			return "Searching";
		case GSM_NET_DENIED:
			return "Registration denied";
		case GSM_NET_UNKNOWN:
			return "Unknown";
		case GSM_NET_ROAMING:
			return "Registered (Roaming)";
		case GSM_NET_REGISTERED:
			return "Registered";
		default:
			return "Unknown Network Status";
	}
}


/******************************************************************************
 * Get call state message
 * param:
 *		call state id
 * return:
 *		call state message
 ******************************************************************************/
char *callstate2str(int callstate)
{
	static struct msgtype callstates[] = {
		{  0, "Null" },
		{  1, "Call Initiated" },
		{  2, "Overlap sending" },
		{  3, "Outgoing call  Proceeding" },
		{  4, "Call Delivered" },
		{  6, "Call Present" },
		{  7, "Call Received" },
		{  8, "Connect Request" },
		{  9, "Incoming Call Proceeding" },
		{ 10, "Active" },
		{ 11, "Disconnect Request" },
		{ 12, "Disconnect Indication" },
		{ 15, "Suspend Request" },
		{ 17, "Resume Request" },
		{ 19, "Release Request" },
		{ 22, "Call Abort" },
		{ 25, "Overlap Receiving" },
		{ 61, "Restart Request" },
		{ 62, "Restart" },
	};
	return code2str(callstate, callstates, sizeof(callstates) / sizeof(callstates[0]));
}


/******************************************************************************
 * Create a new allogsm_modul
 * param:
 *		fd: FD's for D-channel
 *		nodetype:
 *		switchtype:
 * return:
 *		A new allogsm_modul structure
 * e.g.
 *		__gsm_new_tei(fd, nodetype, switchtype, __gsm_read, __gsm_write, NULL);
 ******************************************************************************/
/*Freedom Modify 2011-10-10 10:11*/
//struct allogsm_modul *__gsm_new_tei(int fd, int nodetype, int switchtype, int span, allogsm_rio_cb rd, allogsm_wio_cb wr, void *userdata)
struct allogsm_modul *__gsm_new_tei(int fd, int nodetype, int switchtype, int span, allogsm_rio_cb rd, allogsm_wio_cb wr, void *userdata, int at_debug, int call_waiting_enabled, int auto_modem_reset)
{
	struct allogsm_modul *gsm;
	/* malloc allogsm_modul */
	if (!(gsm = calloc(1, sizeof(*gsm)))) {
		return NULL;
	}

	gsm->fd			= fd;
	gsm->read_func	= rd;
	gsm->write_func	= wr;
	gsm->userdata	= userdata;
	gsm->localtype	= nodetype;
	gsm->switchtype	= switchtype;
	gsm->cref		= 1; /* Next call reference value */
	gsm->callpool	= &gsm->localpool;
	gsm->span		= span;
	gsm->sms_mod_flag = SMS_UNKNOWN;
        gsm->debug_at_fd = 0;
        gsm->debug_at_flag = at_debug;
        gsm->call_waiting_enabled = call_waiting_enabled;
        gsm->auto_modem_reset = auto_modem_reset;
	gsm->retries 	= 0;	
	gsm->retry_count=0;
	gsm->dial_initiated = 0;
        gsm->dial_initiated_hangup=0;
	gsm->autoReloadLoopActive=0;
	/*Freedom Modify 2011-10-10 10:11*/
	/* Set default timer by switchtype */
//	gsm_default_timers(gsm, switchtype);
	gsm_default_timers(gsm);
		
	/* set network status */
	gsm->network = GSM_NET_UNREGISTERED;

	/* Set network coverage */
	gsm->coverage = -1;
	gsm->coverage_level = -1;
	print_signal_level(gsm,-1);
	
	gsm->send_at = 0;

#ifdef CONFIG_CHECK_PHONE
	/*Makes modify 2012-04-10 17:03*/
	gsm->check_mode = 0;
	gsm->phone_stat = -1;
	gsm->auto_hangup_flag = 0;
#endif

#ifdef VIRTUAL_TTY
	gsm->already_set_mux_mode = 0;
#endif //VIRTUAL_TTY
#ifdef QUEUE_SMS
	gsm->sms_queue = QueueCreate();
#endif
       
	if(gsm->debug_at_flag) {
                __gsm_init_set_debugat(gsm);
        } else {
                gsm->debug_at_fd = -1;
        }

	/* set timer by switchtype and start gsm module */
	if (gsm) {
		allogsm_set_timer(gsm, ALLOGSM_TIMER_T316, 5000);
		allogsm_module_start(gsm);
	}

	return gsm;
}


/******************************************************************************
 * Free allogsm_modul
 * param:
 *		gsm: allogsm_modul
 * return:
 *		void
 ******************************************************************************/
void __gsm_free_tei(struct allogsm_modul *gsm)
{
	if (gsm) {
#ifdef QUEUE_SMS
		QueueDestroy(gsm->sms_queue);
#endif

                __gsm_deinit_set_debugat(gsm);
		gsm->debug_at_flag = 0;
		free (gsm);
	}
}

void allogsm_set_debugat(struct allogsm_modul *gsm,int mode)
{
        if(mode > 0) {
                gsm->debug_at_flag = 1;
                __gsm_init_set_debugat(gsm);
        } else {
                gsm->debug_at_flag = 0;
                __gsm_deinit_set_debugat(gsm);
        }
}

int allogsm_set_state_ready(struct allogsm_modul *gsm){
	if((gsm->state == ALLOGSM_STATE_SAFE_AT))
		gsm->state = ALLOGSM_STATE_READY;
	return 0;
}

/******************************************************************************
 * Make a config error event
 * param:
 *		gsm: allogsm_modul
 *		errstr: config error message
 * return:
 *		allogsm_event
 ******************************************************************************/
allogsm_event *gsm_mkerror(struct allogsm_modul *gsm, char *errstr)
{
	/* Return a configuration error */
	gsm->ev.err.e = ALLOGSM_EVENT_CONFIG_ERR;
	strncpy(gsm->ev.err.err, errstr, sizeof(gsm->ev.err.err));
	return &gsm->ev;
}


/******************************************************************************
 * Dump AT Command Message
 * param:
 *		gsm: gsm module
 *		h  : AT Command
 *		len: AT Command Length
 *      txrx:
 *			1: Show sended message
 *			0: Show received message
 * return:
 * 			void
 * e.g.
 *		gsm_dump(gsm, "AT+CREG?\r\n", 10, 1);
 ******************************************************************************/
void gsm_dump(struct allogsm_modul *gsm, const char *at, int len, int txrx)
{
	if ( NULL == gsm || NULL == at || len <=0 ) {
		return;
	}
	
    int i=0;
    int j=0;
	char *dbuf;
	
	dbuf = (char*)malloc(len*sizeof(char)*2);
	printf("Response %s\n",at);	
    for (i = 0; i < len; i++) {
        if (at[i] == '\r') {
            dbuf[j++] = '\\';
			dbuf[j++] = 'r';
        } else if (at[i] == '\n') {
        	dbuf[j++] = '\\';
			dbuf[j++] = 'n';
#if 0
        }else if ( (at[i] == '>') && (at[i+1] == '4'))  { 
		dbuf[j++]= '>' ;
		dbuf[j++]= '4' ;
		dbuf[j++]=  '\\' ;
		dbuf[j++] = 'r';
			
	} else	if( at[i] == '>') {
		dbuf[j++]='>' ;
		dbuf[j++]='\\' ;
		dbuf[j++]= 'r';
#endif		
	} else	{
        	dbuf[j++] = at[i];
        }
    }
    dbuf[j] = '\0';

//	printf("[%s]%s:%d{SPAN %d } response %s \n", __FILE__, __func__, __LINE__ , dbuf );
	gsm_message(gsm, "[%s:%s:%d] Span %d:%s %s\n", __FILE__, __func__, __LINE__ ,gsm->span,(txrx) ? "-->" : "<--", dbuf);
	
	free(dbuf);
}


/******************************************************************************
 * Transmit AT Command
 * param:
 *		gsm: gsm module
 *		at  : AT Command
 * return:
 *	   	0: trasmit ok
 *		-1: error
 * e.g.
 *		gsm_transmit(gsm, "AT+CREG?\r\n");
 ******************************************************************************/
int gsm_send_at(struct allogsm_modul *gsm, const char *at) 
{
	if ( NULL == gsm || NULL == at ) {
		return -1;
	}
/*Debug */
	
	int res, len;
	char *dbuf;

	/* get AT Command length */
	len = strlen(at);
		
	if ((gsm->debug & ALLOGSM_DEBUG_AT_RECEIVED) || (gsm->span==1))
	{
#define FORMAT  "     %-2d-->   %-40.40s  || sz: %-3d|| %-23s ||\n"
		int ii=0;
		gsm_message(gsm, FORMAT, gsm->span, at, len, allogsm_state2str(gsm->state)+14);
                for(ii=40; ii<len; ii=ii+40)
			gsm_message(gsm,"             %-40.40s\n", at+ii);
#undef FORMAT
	}

	dbuf = (char*)malloc(len*sizeof(char)+2+1);
	usleep (5000); //pawan
	
	/* Just send it raw */
	/* Dump AT Message*/
	if (gsm->debug & (ALLOGSM_DEBUG_AT_DUMP)) {
		gsm_dump(gsm, at, len, 1);
	}
	
	strcpy(dbuf, at);
	dbuf[len++]	= '\r';
	dbuf[len++]	= '\n';
	dbuf[len] = '\0';

		/* Write an extra two bytes for the FCS */
	res = gsm->write_func ? gsm->write_func(gsm, dbuf, len + 2) : 0;
	if (res != (len + 2)) {
		gsm_error(gsm, "Short write: %d/%d (%s)\n", res, len + 2, strerror(errno));
		
		free(dbuf);
		return -1;
	}

	/* Last sent command to dchan */
	strncpy(gsm->at_last_sent, dbuf, sizeof(gsm->at_last_sent));
	/* at_lastsent length */
	gsm->at_last_sent_idx = len;
	
	free(dbuf);
	return 0;
}

/******************************************************************************
 * Transmit SMS in Packet of 10 bytes
 * param:
 *		gsm	: gsm module
 *		msg  	: sms
 * return:
 *	   	0: trasmit ok
 *		-1: error
 * e.g.
 *		gsm_transmit_sms(gsm, "message");
 ******************************************************************************/
int gsm_transmit_sms(struct allogsm_modul *gsm, const char *msg) 
{
	if ( NULL == gsm || NULL == msg ) {
		return -1;
	}
	
	int res, len, count, rem;
	int j=0;

	/* get AT Command length */
	len = strlen(msg);
#if 0 
	printf("Transmitted msg is >>%s<< len: %d\n",msg, len);
#endif
	if (gsm->debug & ALLOGSM_DEBUG_AT_RECEIVED)
	{
#define FORMAT  "     %-2d-->   %-40.40s  || sz: %-3d|| %-23s ||\n"
		int ii=0;
		gsm_message(gsm, FORMAT, gsm->span, msg, len, allogsm_state2str(gsm->state)+14);
                for(ii=40; ii<len; ii=ii+40)
			gsm_message(gsm,"             %-40.40s\n", msg+ii);
#undef FORMAT
	}
	/* Just send it raw */
	/* Dump AT Message*/
	if (gsm->debug & (ALLOGSM_DEBUG_AT_DUMP)) {
		gsm_dump(gsm, msg, len, 1);
	}

#define SMS_SHORT_LEN	10
        count = len/SMS_SHORT_LEN;
        rem = len%SMS_SHORT_LEN;
        for (j=0; j<count; ++j){
		res = gsm->write_func ? gsm->write_func(gsm, &msg[j*SMS_SHORT_LEN], SMS_SHORT_LEN + 2) : 0;
		if (res != (SMS_SHORT_LEN + 2)) {
			gsm_error(gsm, "Short write: %d/%d (%s)\n", res,  SMS_SHORT_LEN + 2, strerror(errno));
			return -1;
		}
		usleep(3000);
	}
	if(rem){
		res = gsm->write_func ? gsm->write_func(gsm, &msg[j*SMS_SHORT_LEN], rem + 2) : 0;
		if (res != (rem + 2)) {
			gsm_error(gsm, "Short write: %d/%d (%s)\n", res,  rem + 2, strerror(errno));
			return -1;
		}
	}	

#if 0
        count = len/10;
        rem = len%10;
        for (j=0; j<count; ++j){
//		usleep(20000);
                char temp[11];
		int templen=0;
                snprintf(temp,11, "%s", msg+(10*j));
                temp[10]='\0';
 //               printf("%s\n",temp);
		templen = strlen(temp);
                printf("%d %s\n",templen,temp);
		/* Write an extra two bytes for the FCS */
		res = gsm->write_func ? gsm->write_func(gsm, temp, templen + 2) : 0;
		if (res != (templen + 2)) {
			gsm_error(gsm, "Short write: %d/%d (%s)\n", res, templen + 2, strerror(errno));
			return -1;
		}
        }
        if (rem){
		char temp[11];
		int templen=0;
                snprintf(temp,rem, "%s", msg+(10*j));
                temp[rem]='\0';
		templen = strlen(temp);
                printf("%d %s\n",templen,temp);
		/* Write an extra two bytes for the FCS */
		res = gsm->write_func ? gsm->write_func(gsm, temp, templen + 2) : 0;
		if (res != (templen + 2)) {
			gsm_error(gsm, "Short write: %d/%d (%s)\n", res, templen + 2, strerror(errno));
			return -1;
		}
	}
#endif
	return 0;
}

/******************************************************************************
 * Transmit AT Command
 * param:
 *		gsm: gsm module
 *		at  : AT Command
 * return:
 *	   	0: trasmit ok
 *		-1: error
 * e.g.
 *		gsm_transmit(gsm, "AT+CREG?\r\n");
 ******************************************************************************/
int gsm_transmit(struct allogsm_modul *gsm, const char *at) 
{
	if ( NULL == gsm || NULL == at ) {
		return -1;
	}
	
	int res, len;

	/* get AT Command length */
	len = strlen(at);
#if 0 
	printf("Transmitted at command is >>%s<< len: %d\n",at, len);
#endif
	/* Just send it raw */
	/* Dump AT Message*/
	if (gsm->debug & (ALLOGSM_DEBUG_AT_DUMP)) {
		gsm_dump(gsm, at, len, 1);
	}
	
	/* Write an extra two bytes for the FCS */
	res = gsm->write_func ? gsm->write_func(gsm, at, len + 2) : 0;
	if (res != (len + 2)) {
		gsm_error(gsm, "Short write: %d/%d (%s)\n", res, len + 2, strerror(errno));
		return -1;
	}

	/* Last sent command to dchan */
	strncpy(gsm->at_last_sent, at, sizeof(gsm->at_last_sent));
	
	/* at_lastsent length */
	gsm->at_last_sent_idx = len;
	
	return 0;
}


int allogsm_transmit(struct allogsm_modul *gsm, const char *at) {
	return gsm_transmit(gsm, at);
}


int gsm_transmit_data(struct allogsm_modul *gsm, const char *data, int len) 
{
	if ( NULL == gsm || NULL == data || len <= 0 ) {
		return -1;
	}
	
	int res;

	/* Just send it raw */
	/* Dump AT Message*/
	if (gsm->debug & (ALLOGSM_DEBUG_AT_DUMP)) {
		gsm_dump(gsm, data, len, 1);
	}

	/* Write an extra two bytes for the FCS */
	res = gsm->write_func ? gsm->write_func(gsm, data, len + 2) : 0;
	if (res != (len + 2)) {
		gsm_error(gsm, "Short write: %d/%d (%s)\n", res, len + 2, strerror(errno));
		return -1;
	}
	
	return 0;
}


static void gsm_resend_safe_at(void *info)
{
	safe_at_t *at_info = info;
	struct allogsm_modul *gsm	= at_info->gsm;
	char *command			= at_info->command;

	if (gsm->state != ALLOGSM_STATE_READY){ 
		if (at_info->safe_at_retries<5){	
			int resendsmsidx = gsm_schedule_event(gsm, 1000, gsm_resend_safe_at, info);
			gsm_error(gsm, "---------Resched for call fwd! retries %d, span %d, cmd %s\n", at_info->safe_at_retries, gsm->span, command );
			++(at_info->safe_at_retries);
			if (resendsmsidx < 0 ) {
				gsm_error(gsm, "Can't schedule sending sms!\n");
				free(at_info);
				at_info = NULL;
			}
		}
	} else {
		module_send_safe_at(gsm, command);
	}
}

int allogsm_test_atcommand_safe(struct allogsm_modul *gsm, char *at) {
	int res = -1;

	safe_at_t *at_info;
	if (!gsm)
	{
		return res;
	}	
	
	at_info = malloc(sizeof(safe_at_t));
	if (!at_info) {
		gsm_error(gsm, "unable to malloc!\n");
		return res;
	}
	at_info->gsm = gsm;
	strncpy(at_info->command, at, sizeof(at_info->command));
	
	if (ALLOGSM_STATE_READY != gsm->state) {
		if (gsm_schedule_check(gsm) < 0) {
			gsm_error(gsm, "No enough space for sending operator list!\n");
			return -1;
		}
		// schedule index
		at_info->safe_at_retries = 1;	
		int sched = gsm_schedule_event(gsm, 1000, gsm_resend_safe_at, (void *)at_info);
		if (sched < 0 ) {
			gsm_error(gsm, "Can't schedule sending operator list!\n");
			return -1;
		}
		res = 0;
	} else {
		module_send_safe_at(gsm, at);
		res = 0;
	}

	return res;
}
int allogsm_test_atcommand(struct allogsm_modul *gsm, char *at) 
{
	if ( NULL == gsm || NULL == at /*|| ALLOGSM_STATE_READY != gsm->state*/ ) {
		return -1;
	}
	
	int res, len;
	int i;
	char *dbuf;

	/* get AT Command length */
	len = strlen(at);
	
	dbuf = (char*)malloc(len*sizeof(char)+2+1);
	
	/* Just send it raw */
	/* Dump AT Message*/
	if (gsm->debug & (ALLOGSM_DEBUG_AT_DUMP)) {
		gsm_dump(gsm, at, len, 1);
	}

	strcpy(dbuf, at);
	
	for(i=0; i<len; i++) {
		if( '@' == dbuf[i] ) {
			dbuf[i] = '?';
		}
	}

	dbuf[len++]	= '\r';
	dbuf[len++]	= '\n';		
	dbuf[len] = '\0';
						
	/* Write an extra two bytes for the FCS */
	res = gsm->write_func ? gsm->write_func(gsm, dbuf, len + 2) : 0;
	if (res != (len + 2)) {
		gsm_error(gsm, "Short write: %d/%d (%s)\n", res, len + 2, strerror(errno));
		free(dbuf);
		return -2;
	}

	/* Last sent command to dchan */
	strncpy(gsm->at_last_sent, dbuf, sizeof(gsm->at_last_sent));

	/* at_lastsent length */
	gsm->at_last_sent_idx = len;
		
	gsm->send_at = 1;
	
	free(dbuf);
	
	return 0;
}

void allogsm_check_signal(struct allogsm_modul *gsm){
	if (ALLOGSM_STATE_READY == gsm->state){
		gsm_switch_state(gsm, ALLOGSM_STATE_READY, get_at(gsm->switchtype,AT_NET_NAME));
	}
}

/******************************************************************************
 * Get a call
 * param:
 *		gsm: struct allogsm_modul
 *		cr: Call Reference in alloat_call
 *		outboundnew: not used
 * return:
 *	   	alloat_call
 ******************************************************************************/
struct alloat_call *allogsm_getcall(struct allogsm_modul *gsm, int cr, int outboundnew)
{
	struct alloat_call *cur, *prev;
	struct allogsm_modul *master;

	master = gsm;

	/* Get alloat_call */
	cur = *master->callpool;
	prev = NULL;
	while(cur) {
		if (cur->cr == cr) {
			return cur;
		}
		prev = cur;
		cur = cur->next;
	}
	
//		gsm_message(gsm, "-- Making new call for cr %d\n", cr);
	/* No call exists, make a new one */
	if (gsm->debug & ALLOGSM_DEBUG_AT_STATE) {
		gsm_message(gsm, "-- Making new call for cr %d\n", cr);
	}

	/* calloc a new call */
	if (!(cur = calloc(1, sizeof(*cur)))) {
		return NULL;
	}

	/* Initialize the new call */
	cur->cr = cr;		/* Set Call reference */
	cur->gsm = gsm;
	cur->channelno		= -1;
	cur->newcall		= 1;
	cur->ring_count		= 0;
	cur->ourcallstate	= AT_CALL_STATE_NULL;
	cur->peercallstate	= AT_CALL_STATE_NULL;
	cur->next 			= NULL;
	cur->already_hangup     = 0;

	/* Append to end of list */
	if (prev) {
		prev->next = cur;
	} else {
		*master->callpool = cur;
	}

	/* return the new call f*/
	return cur;
}


/******************************************************************************
 * String Comparation
 * param:
 *		str_at: source string
 *		str_cmp: destination string
 * return:
 *		1: equal
 *		0: not equal
 * e.g.
 *		gsm_compare("abc", "abc")			=> 1
 *		gsm_compare("ab", "abcd")			=> 0 
 *		gsm_compare("abcd", "ab")			=> 1 
 *		gsm_compare("ab\r\ncd", "ab")		=> 1 
 *		gsm_compare("ab\r\ncd", "abcd")		=> 0
 *		gsm_compare("\r\nab\r\ncd", "ab")	=> 1
 ******************************************************************************/
int gsm_compare(const char *str_at, const char *str_cmp) 
{
#if 0
	int res;
	int i;
	int j =0;
	char buf[1024];
	int k = strlen(str_at);

	if ((NULL == str_at) || (NULL == str_cmp)) {
		return 0;
	}
	
	res = strncmp(str_at, str_cmp, strlen(str_cmp));
	if (!res) {
		return 1;
	}

	for (i=0; i < k ;i++) {
		/* skip \r or \n */
		if ((str_at[i] != '\r') && (str_at[i] != '\n') ) {
			buf[j++] = str_at[i];
		}
		/*  */
		if (('\n' == str_at[i])) {
			buf[j] = '\0';
			if (j > 0) {
				res = strncmp(buf, str_cmp, strlen(str_cmp));
				if (!res) {
					return 1;
				}
			}
			j=0;
		}
	}

	res = strncmp(buf, str_cmp, strlen(str_cmp));
	if (!res) {
		return 1;
	}

	return 0;
#else
	return NULL==strstr(str_at,str_cmp) ? 0 : 1;
#endif
}


/******************************************************************************
 * Strip \r\n
 * param:
 *		in: source string
 *		out: destination string
 * return:
 *		string without \r\n
 * e.g.
 *		gsm_trim() 		=>
 *		gsm_trim("abc") 	=>
 *		gsm_trim("abc") 	=> abc
 *		gsm_trim("\r\nabc") 	=> abc
 *		gsm_trim("abc\r\n") 	=> abc
 *		gsm_trim("\r\nabc\r\n") => abc
 ******************************************************************************/
int gsm_trim(const char *in, char *out, int len) 
{
    int i=0;
    int j=0;

    for (i = 0; i < len; i++) {
        if ((in[i] != '\r') && (in[i] != '\n') ) {
            out[j++] = in[i];
        }
    }
    out[j] = '\0';
	
    return j;
}

void gsm_get_manufacturer(struct allogsm_modul *gsm, char *h) 
{
	char buf[sizeof(gsm->manufacturer)];
	gsm_trim(h, buf, strlen(h));
	strncpy(gsm->manufacturer, buf, sizeof(gsm->manufacturer));
	
	return;
}

void gsm_get_smsc(struct allogsm_modul *gsm, char *h) 
{
	char buf[sizeof(gsm->sim_smsc)];
	gsm_trim(h, buf, strlen(h));

	char *ps, *pe;
	ps = strchr(buf,'"');
	pe =strrchr(buf,'"');

	if(pe == 0 || ps == 0) {
		strncpy(gsm->sim_smsc, "", sizeof(gsm->sim_smsc));
		return;
	}

	if( (pe-ps) < sizeof(gsm->sim_smsc)) {
		strncpy(gsm->sim_smsc, ps+1, pe-ps-1);
	} else {
		strncpy(gsm->sim_smsc, "", sizeof(gsm->sim_smsc));
	}
	
	return;
}


void gsm_get_model_name(struct allogsm_modul *gsm, char *h) 
{
	char buf[sizeof(gsm->model_name)];
	gsm_trim(h, buf, strlen(h));
	strncpy(gsm->model_name, buf, sizeof(gsm->model_name));
	
	return;
}

void gsm_get_model_version(struct allogsm_modul *gsm, char *h) 
{
	char buf[sizeof(gsm->revision)];
	gsm_trim(h, buf, strlen(h));
	strncpy(gsm->revision, buf, sizeof(gsm->revision));
	return;
}

void gsm_get_imsi(struct allogsm_modul *gsm, char *h) 
{
	char buf[sizeof(gsm->imsi)];
	gsm_trim(h, buf, strlen(h));
	strncpy(gsm->imsi, buf, sizeof(gsm->imsi));
	return;
}

void gsm_get_imei(struct allogsm_modul *gsm,char *h) 
{
	char buf[sizeof(gsm->imei)];
	gsm_trim(h, buf, strlen(h));
	strncpy(gsm->imei, buf, sizeof(gsm->imei));
}

void gsm_get_operator(struct allogsm_modul *gsm, char *buf) 
{
	char* key ="\"";
	char* start;
	char* end;

	if (!gsm) {
		return;
	}

	start = strstr(buf, key);
	if (0 == start) {
		return;
	}
	start += strlen(key);
	end = strstr(start, key);
	strncpy(gsm->net_name, start,(end - start));
}

int gsm_switch_state(struct allogsm_modul *gsm, int state, const char *next_command)
{
    gsm->state = state;
    if (next_command) {
		//Freedom Modify 2011-10-10 15:58
		//gsm_transmit(gsm, next_command);
		int i;
		gsm_send_at(gsm,next_command);
    }

/*This is a special case added here for CME ERROR 515 BUG
  If incoming calls and msg are tried together, Sometimes module becomes unresponsive and
  For any AT Command fired it replies with 515
  We have to restart in between.
  But one more Issue is there that we should hangup call for asterisk states or 
  finish sms sending with Error maybe.
  So in all those cases control is Returned to ALLOGSM_STATE_READY. That is the best time to reload module.
***/
    if (gsm->CME_515_count > 3 && state==ALLOGSM_STATE_READY){
	gsm->wind_state=0;
	gsm->creg_state=0;
	gsm->CME_515_count=0;
	gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_GENERAL_INDICATION));
    }
/************************************/
    return 0;
}

int gsm_switch_sim_state(struct allogsm_modul *gsm, int state, char *next_command)
{
    gsm->sim_state = state;
    if (next_command) {
        //Freedom Modify 2011-10-10 15:58
		//gsm_transmit(gsm, next_command);
		gsm_send_at(gsm,next_command);
    }

    return 0;
}

int gsm_write_file(char* file_name, char* data)
{
        char data1[100];
        FILE* fd;
        if((fd=fopen(file_name,"w+")) == NULL) {
                printf("Can't open %s\n",file_name);
        }
        fprintf(fd, "%s", data);
        fclose(fd);

        return 0;
}


static void (*__gsm_error)(struct allogsm_modul *gsm, char *stuff);
static void (*__gsm_message)(struct allogsm_modul *gsm, char *stuff);

/******************************************************************************
 * General message reporting function
 * param:
 *		gsm: gsm module
 *		fmt: format string
 * return:
 *		void
 * e.g.
 *		gsm_message(gsm, "Timed out resetting span. Starting Reset again\n");
 ******************************************************************************/
void gsm_message(struct allogsm_modul *gsm, char *fmt, ...)
{
	char tmp[1024];

	if (!gsm) {
		return;
	}

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	
	if (__gsm_message) {
		__gsm_message(gsm, tmp);
	} else {
		fputs(tmp, stdout);
	}
}


/******************************************************************************
 * General error reporting function
 * param:
 *		gsm: gsm module
 *		fmt: format string
 * return:
 *		void
 * e.g.
 *		gsm_error(gsm, "Short write: %d/%d (%s)\n", res, len + 2, strerror(errno));
 ******************************************************************************/
void gsm_error(struct allogsm_modul *gsm, char *fmt, ...)
{
	char tmp[1024];

	if (!gsm) {
		return;
	}

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	if (__gsm_error) {
		__gsm_error(gsm, tmp);
	} else {
		fputs(tmp, stderr);
	}
}

/*============================================================================
 *
 * Called by chan_allogsm.so
 *
 * ===========================================================================*/

/******************************************************************************
 * Set message reporting function
 * param:
 *		a function pointer
 * return:
 *		void
 * e.g.
 *		used in chan_allogsm.so
 *		allogsm_set_message(allogsm_extend_message);
 ******************************************************************************/
void allogsm_set_message(void (*func)(struct allogsm_modul *gsm, char *stuff))
{
	__gsm_message = func;
}


/******************************************************************************
 * Set error reporting function
 * param:
 *		a function pointer
 * return:
 *		void
 * e.g.
 *		used in chan_allogsm.so
 *		allogsm_set_error(allogsm_extend_error);
 ******************************************************************************/
void allogsm_set_error(void (*func)(struct allogsm_modul *gsm, char *stuff))
{
	__gsm_error = func;
}

/******************************************************************************
 * Set debug level
 * param:
 *		gsm: struct allogsm_modul
 *		debug: debug level
 * return:
 *		void
 * e.g.
 *		used in chan_allogsm.so
 ******************************************************************************/
void allogsm_set_debug(struct allogsm_modul *gsm, int debug)
{
	if (!gsm) {
		return;
	}
	
	gsm->debug = debug;
}


/******************************************************************************
 * Get debug level
 * param:
 *		gsm: struct allogsm_modul
 * return:
 *		void
 * e.g.
 *		used in chan_allogsm.so
 ******************************************************************************/
int allogsm_get_debug(struct allogsm_modul *gsm)
{
	if (!gsm) {
		return -1;
	}
	return gsm->debug;
}

char *allogsm_cause2str(int cause)
{
	return code2str(cause, causes, sizeof(causes) / sizeof(causes[0]));
}

char *allogsm_event2str(int id)
{
	switch(id) {
	case ALLOGSM_EVENT_DCHAN_UP:
		return "D-Channel Up";
	case ALLOGSM_EVENT_DETECT_MODULE_OK:
		return "Detect module OK";
	case ALLOGSM_EVENT_DCHAN_DOWN:
		return "D-channel Down";
	case ALLOGSM_EVENT_RESTART:
		return "Restart channel";
	case ALLOGSM_EVENT_RING:
		return "Ring";
	case ALLOGSM_EVENT_HANGUP:
		return "Hangup";
	case ALLOGSM_EVENT_RINGING:
		return "Ringing";
	case ALLOGSM_EVENT_ANSWER:
		return "Answer";
	case ALLOGSM_EVENT_HANGUP_ACK:
		return "Hangup ACK";
	case ALLOGSM_EVENT_RESTART_ACK:
		return "Restart ACK";
	case ALLOGSM_EVENT_FACNAME:
		return "FacName";
	case ALLOGSM_EVENT_INFO_RECEIVED:
		return "Info Received";
	case ALLOGSM_EVENT_PROCEEDING:
		return "Proceeding";
	case ALLOGSM_EVENT_SETUP_ACK:
		return "Setup ACK";
	case ALLOGSM_EVENT_HANGUP_REQ:
		return "Hangup Req";
	case ALLOGSM_EVENT_NOTIFY:
		return "Notify";
	case ALLOGSM_EVENT_PROGRESS:
		return "Progress";
	case ALLOGSM_EVENT_CONFIG_ERR:
		return "Configuration Error";
	case ALLOGSM_EVENT_KEYPAD_DIGIT:
		return "Keypad digit";
	case ALLOGSM_EVENT_SMS_RECEIVED:
		return "SMS received";
	case ALLOGSM_EVENT_SIM_FAILED:
		return "SIM failed";
	case ALLOGSM_EVENT_PIN_REQUIRED:
		return "PIN required";
	case ALLOGSM_EVENT_PIN_ERROR:
		return "PIN error";
	case ALLOGSM_EVENT_SMS_SEND_OK:
		return "SMS send OK";	
	case ALLOGSM_EVENT_SMS_SEND_FAILED:	
		return "SMS send failed";
	case ALLOGSM_EVENT_USSD_RECEIVED:
		return "USSD received";
	case ALLOGSM_EVENT_USSD_SEND_FAILED:
		return "USSD send failed";
#ifdef CONFIG_CHECK_PHONE
	case ALLOGSM_EVENT_CHECK_PHONE:
		return "Check phone";
#endif //CONFIG_CHECK_PHONE

#ifdef VIRTUAL_TTY
	case ALLOGSM_EVENT_INIT_MUX:
		return "Init Multiplexer";
#endif //VIRTUAL_TTY
	case ALLOGSM_EVENT_NO_SIGNAL:
		return "No signal";
	default:
		return "Unknown Event";
	}
}

int allogsm_keypad_facility(struct allogsm_modul *gsm, struct alloat_call *call, char *digits)
{
	if (!gsm || !call || !digits || !digits[0]) {
		return -1;
	}

	strncpy(call->keypad_digits, digits, sizeof(call->keypad_digits));
	return 0;
}


/******************************************************************************
 * Get D-Channel Fileno
 * param:
 *		gsm: gsm module
 * return:
 *		-1: error
 *      other: gsm->fd
 ******************************************************************************/
int allogsm_fd(struct allogsm_modul *gsm)
{
	if (!gsm) {
		return -1;
	}

	return gsm->fd;
}

int allogsm_call(struct allogsm_modul *gsm, struct alloat_call *c, int transmode, int channel, int exclusive, 
					int nonisdn, char *caller, int callerplan, char *callername, int callerpres, char *called,
					int calledplan,int ulayer1)
{
	struct allogsm_sr req;

	if (!gsm || !c) {
		return -1;
	}

	gsm_sr_init(&req);
	req.transmode = transmode;
	req.channel = channel;
	req.exclusive = exclusive;
	req.nonisdn =  nonisdn;
	req.caller = caller;
	req.callername = callername;
	req.called = called;
	req.userl1 = ulayer1;
	return allogsm_setup(gsm, c, &req);
}	

char *allogsm_node2str(int node)
{
	switch(node) {
	case ALLOGSM_NETWORK:
		return "Network";
	case ALLOGSM_CPE:
		return "CPE";
	default:
		return "Invalid value";
	}
}

char *allogsm_switch2str(int sw)
{
	switch(sw) {
	case ALLOGSM_SWITCH_E169:
		return "Huawei E169/K3520";
	case ALLOGSM_SWITCH_SIMCOM:
		return "SimCom 100/300";
	case ALLOGSM_SWITCH_SIM900:
		return "SimCom 900";
	case ALLOGSM_SWITCH_M20:
		return "Quectel M20";
	case ALLOGSM_SWITCH_EM200:
		return "Huawei EM200 CDMA 1X 800M";
	case ALLOGSM_SWITCH_SIERRA_Q2687RD:
		return "Sierra WAVECOM Q2687RD";
	default:
		return "Unknown switchtype";
	}
}


/******************************************************************************
 * Set timer
 * param:
 *		gsm: struct allogsm_modul
 *		timer: timer type
 *		value: ms
 * return:
 *		-1: error
 *		 0: ok
 ******************************************************************************/
int allogsm_set_timer(struct allogsm_modul *gsm, int timer, int value)
{
	if (timer < 0 || timer > GSM_MAX_TIMERS || value < 0) {
		return -1;
	}
	
	gsm->timers[timer] = value;
	return 0;
}


/******************************************************************************
 * Get timer
 * param:
 *		gsm: struct allogsm_modul
 *		timer: timer type
 * return:
 *		 -1: error
 *		> 0: timer value
 ******************************************************************************/
int allogsm_get_timer(struct allogsm_modul *gsm, int timer)
{
	if (timer < 0 || timer > GSM_MAX_TIMERS) {
		return -1;
	}
	return gsm->timers[timer];
}


/******************************************************************************
 * Create a new allogsm_modul
 * param:
 *		fd: FD's for D-channel
 *		nodetype:
 *		switchtype:
 * return:
 *		A new allogsm_modul structure
 * e.g.
 *		used in chan_allogsm.so
 *		extend->dchan = allogsm_new(extend->fd, extend->nodetype, extend->switchtype);
 ******************************************************************************/
struct allogsm_modul *allogsm_new(int fd, int nodetype, int switchtype, int span, int at_debug, int call_waiting_enabled, int auto_modem_reset)
{
	return __gsm_new_tei(fd, nodetype, switchtype, span, __gsm_read, __gsm_write, NULL,at_debug, call_waiting_enabled, auto_modem_reset);
}


int allogsm_restart(struct allogsm_modul *gsm)
{	
	if (gsm) {
		gsm->network = GSM_NET_UNREGISTERED;
		gsm->imei[0] = 0x0;
		gsm->imsi[0] = 0x0;
		gsm->net_name[0] = 0x0;
		gsm->coverage = -1;
		
		allogsm_module_start(gsm);

		return 0;
	}
	
	return -1;
}

int allogsm_reset(struct allogsm_modul *gsm, int channel)
{
	struct alloat_call *c;

	if (!gsm) {
		return -1;
	}

	/* Get alloat_call */
	c = allogsm_getcall(gsm, 0, 1);
	if (!c) {
		return -1;
	}

	/* check channel */
	if (!channel) {
		return -1;
	}
	channel &= 0xff;
	
	c->channelno = channel;		
	c->chanflags &= ~FLAG_PREFERRED;
	c->chanflags |= FLAG_EXCLUSIVE;

	/* Set our call state */
	UPDATE_OURCALLSTATE(gsm, c, AT_CALL_STATE_RESTART);
	
	/* Set peer call state */	
	c->peercallstate = AT_CALL_STATE_RESTART_REQUEST;

	/* restart gsm module */
	gsm->retranstimer = gsm_schedule_event(gsm, gsm->timers[ALLOGSM_TIMER_T316], gsm_reset_timeout, c);
	module_restart(gsm);

	return 0;
}
#ifdef WAVECOM
void gsm_start_timeout_junk(void *data)
{
/*
	struct alloat_call *call = data;
	struct allogsm_modul *gsm = call->gsm;
*/
	struct allogsm_modul *gsm = data;
	//gsm->debug=1;
	if (gsm->state<ALLOGSM_STATE_READY &&
	    (gsm->state!=ALLOGSM_STATE_UPDATE_1 && gsm->state!=ALLOGSM_STATE_UPDATE_2 && gsm->state!=ALLOGSM_STATE_UPDATE_SUCCESS)){
		//if (gsm->debug & ALLOGSM_DEBUG_AT_DUMP) { pawan
			gsm_message(gsm, "Timed out start span. Starting again\n");
		//}
                if (gsm->retries>=0){
			gsm->retry_count=0;
                        if ((gsm->coverage < 1) && (gsm->state==ALLOGSM_STATE_NET_NAME_REQ) && gsm->retries<4) {
                                ++(gsm->retries);
                                gsm_switch_state(gsm, ALLOGSM_STATE_NET_NAME_REQ, get_at(gsm->switchtype,AT_NET_NAME));
                                gsm_schedule_event(gsm, 10000, gsm_start_timeout_junk, gsm);
                        } else {
                                gsm_schedule_event(gsm, 60000, gsm_start_timeout_junk, gsm);
                                gsm->resetting = 0;
                                gsm->wind_state=0;
				gsm->creg_state=0;
                                gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_GENERAL_INDICATION));
                        }
                }else{
                        gsm_message(gsm, "NOT Retrying anymore to start GSM modem\n");
                }
	} else {
		if (!gsm->auto_modem_reset)
			return;
                gsm->retries=0;
                ++gsm->retry_count;
                if (gsm->retry_count >=gsm->auto_modem_reset && gsm->state==ALLOGSM_STATE_READY){
                        gsm->resetting = 0;
                        gsm->wind_state=0;
			gsm->creg_state=0;
                	gsm->retry_count=0;
                        gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_GENERAL_INDICATION));
		} else if (gsm->retry_count >= gsm->auto_modem_reset){
                	gsm->retry_count=0;
		}
                gsm_schedule_event(gsm, 60000, gsm_start_timeout_junk, gsm);
	}
}
void gsm_cmd_sched(void *data)
{
	struct alloat_call *call = data;
	struct allogsm_modul *gsm = call->gsm;
	
	gsm_switch_state(gsm, gsm->sched_state, gsm->sched_command);
}
void gsm_sim_waiting_sched(void *data)
{
	struct alloat_call *call = data;
	struct allogsm_modul *gsm = call->gsm;

	if (gsm->state == ALLOGSM_STATE_IMSI_REQ){
		gsm_switch_state(gsm, ALLOGSM_STATE_IMSI_REQ, get_at(gsm->switchtype,AT_IMSI));
	}
}
void gsm_sms_service_waiting(void *data)
{
	struct alloat_call *call = data;
	struct allogsm_modul *gsm = call->gsm;

	if (gsm->state == ALLOGSM_STATE_GET_SMSC_REQ){
		gsm_switch_state(gsm, ALLOGSM_STATE_GET_SMSC_REQ, get_at(gsm->switchtype,AT_GET_SMSC));
	}
}
void gsm_callwaiting_sched(void *data)
{
	struct alloat_call *call = data;
	struct allogsm_modul *gsm = call->gsm;

	if (gsm->state == ALLOGSM_STATE_CALL_WAITING){
		gsm_message(gsm, "CALL Waiting\n");
		gsm_switch_state(gsm, ALLOGSM_STATE_CALL_WAITING, "AT");
	}
}
void gsm_sms_sending_timeout(void *data)
{
/*
	struct alloat_call *call = data;
	struct allogsm_modul *gsm = call->gsm;
*/
	struct allogsm_modul *gsm = data;
	char sms_end[5];
	sprintf(sms_end, "%c", 0x1A);
	if (gsm->state > ALLOGSM_STATE_READY){
		gsm_message(gsm, "SMS Sending Stuck on span:%d state:%s gsm->sms_old_retry %d gsm->sms_retry %d\n", gsm->span, allogsm_state2str(gsm->state), gsm->sms_old_retry, gsm->sms_retry);
		if ((gsm->sms_old_retry == gsm->sms_retry) || (gsm->dial_initiated))
			gsm->sms_retry=4;
		if (gsm->sms_retry<3){
			gsm_message(gsm, "SMS Sending Stuck. Retrying\n");
			gsm->sms_old_retry=gsm->sms_retry;
			gsm_switch_state(gsm, ALLOGSM_STATE_SMS_SENT, sms_end);
			gsm->smsTimeoutSched = gsm_schedule_event(gsm, 30000, gsm_sms_sending_timeout, gsm);
		}else{
			gsm_message(gsm, "SMS Sending Stuck\n");
			updateSmsStatus(gsm, SENT_FAILED); 
			gsm_switch_state(gsm, ALLOGSM_STATE_READY, sms_end);
		}
	}
}
void gsm_hangup_timeout(void *data)
{
/*
	struct alloat_call *call = data;
	struct allogsm_modul *gsm = call->gsm;
*/
	struct allogsm_modul *gsm = data;
	if (gsm->state == ALLOGSM_STATE_HANGUP_REQ){
		gsm_message(gsm, "Hangup Sending Stuck on span %d\n", gsm->span);
		gsm->hangupTimeoutSched = gsm_schedule_event(gsm, 2000, gsm_hangup_timeout, gsm);
		gsm_switch_state(gsm, ALLOGSM_STATE_HANGUP_REQ, get_at(gsm->switchtype,AT_HANGUP));
	}
}
void gsm_dial_retry(void *data)
{
	struct alloat_call *call = data;
	struct allogsm_modul *gsm = call->gsm;

        if ((gsm->state == ALLOGSM_STATE_SMS_SENDING) ||
            (gsm->state == ALLOGSM_STATE_SMS_SENT) ||
            (gsm->state == ALLOGSM_STATE_SMS_SENT_END) ){

		(gsm->dial_retry)++;
		if (gsm->dial_retry < 85 ){  /*75 = 15 * 5 ; coz schedular repeats after 200ms i.e 75 times in 15 secs and 15 sec is sms fail timeout */
			gsm_schedule_event(gsm, 200, gsm_dial_retry, call);
		}else{
			/*fail call here*/
			gsm_schedule_event(gsm, 200, gsm_dial_retry, call);
			if (gsm->dial_initiated_hangup)
				allogsm_hangup(gsm, call, ALLOGSM_CAUSE_SWITCH_CONGESTION);
		}
	}else{
                if (gsm->dial_initiated_hangup){
                        module_hangup(gsm,call);
                        call->newcall=1;
                        call->ring_count=0;
                        allogsm_destroycall(gsm, call);
                }else{
                        module_dial(gsm, call);
                }
	}
}
#endif
static void gsm_start_timeout(void *data)
{
	struct alloat_call *call = data;
	struct allogsm_modul *gsm = call->gsm;
	if (gsm->debug & ALLOGSM_DEBUG_AT_DUMP) {
		gsm_message(gsm, "Timed out start span. Starting again gsm_start_timeout\n");
	}
	gsm->resetting = 0;
	gsm_switch_state(gsm, ALLOGSM_STATE_UP, get_at(gsm->switchtype,AT_ATZ));
}

#define POWEROFF_FILE "/var/poweroff_"
int is_poweroff(int span){
	struct stat st;
	char filename[25];
	sprintf(filename,"%s%d", POWEROFF_FILE, span);
	if(!stat (filename, &st)){	//stat RETURNs ZERO if file present
		return 1;		/*file exits*/	
	}
return 0;
}
allogsm_event * module_check_wind(struct allogsm_modul *gsm, struct alloat_call *call, char *buf, int i)
{
#if 1
	if (gsm_compare(buf, "+CREG: ")) {
		int creg_state;
		if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG10))){ creg_state = CREG_0_NOT_REG_NOT_SERARCHING;
		} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG11))) { creg_state =CREG_1_REGISTERED_HOME;
		} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG12))) { creg_state =CREG_2_NOT_REG_SERARCHING;
		} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG13))) { creg_state =CREG_3_REGISTRATION_DENIED;
		} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG14))) { creg_state =CREG_4_UNKNOWN;
		} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG15))) { creg_state =CREG_5_REGISTERED_ROAMING;
		}
		if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG0))){ creg_state = CREG_0_NOT_REG_NOT_SERARCHING;
		} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG1))) { creg_state =CREG_1_REGISTERED_HOME;
		} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG2))) { creg_state =CREG_2_NOT_REG_SERARCHING;
		} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG3))) { creg_state =CREG_3_REGISTRATION_DENIED;
		} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG4))) { creg_state =CREG_4_UNKNOWN;
		} else if (!strcmp(buf,get_at(gsm->switchtype,AT_CREG5))) { creg_state =CREG_5_REGISTERED_ROAMING;
		}
		gsm->creg_state=creg_state;
		return NULL;
	}
		
	if (gsm_compare(buf, "+WIND: ")) {

		if (!strcmp(buf,"+WIND: 0")) { gsm->wind_state |=WIND_0_SIM_REMOVED;
		} else if (!strcmp(buf,"+WIND: 1")) { gsm->wind_state |=WIND_1_SIM_INSERTED;
		} else if (!strcmp(buf,"+WIND: 3")) { gsm->wind_state |=WIND_3_READY_BASIC_AT_COMMANDS;
		} else if (!strcmp(buf,"+WIND: 4")) { gsm->wind_state |=WIND_4_READY_ALL_AT_COMMANDS;
		} else if (!strcmp(buf,"+WIND: 7")) { gsm->wind_state |=WIND_7_READY_EMERGENCY_CALL;
		}

			
		if ((!strcmp(buf,"+WIND: 0")) || (!strcmp(buf,"+WIND: 1"))) { 
			if (gsm->state !=  ALLOGSM_STATE_INIT && gsm->state != ALLOGSM_STATE_UPDATE_1){
				gsm_message(gsm, "Reseting modem on span %d in Check wind as %s was rcvd specifying sim insertion(1) or removal(0)\n", gsm->span, buf);
				gsm->wind_state=0;
				gsm->creg_state=0;
				gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_GENERAL_INDICATION));
			}
		}else if (!strcmp(buf,"+WIND: 3")) { 
			if (gsm->state !=  ALLOGSM_STATE_INIT && gsm->state != ALLOGSM_STATE_UPDATE_1){
				gsm_message(gsm, "INITIALING modem on span %d, coz WIND 3 received when not expected\n", gsm->span);
				gsm->wind_state=0;
				gsm->creg_state=0;
				gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_GENERAL_INDICATION));
			}
		}
//		gsm_message(gsm, "GOT WIND on span %d:%s: wind_state %d\n", gsm->span, buf, gsm->wind_state);
		return NULL;
	}
	if (gsm_compare(buf, "+KSUP: ")) {
		if (!strcmp(buf,"+KSUP: 0")) { 
			gsm->wind_state |=WIND_3_READY_BASIC_AT_COMMANDS;
			if (gsm->state !=  ALLOGSM_STATE_INIT && gsm->state != ALLOGSM_STATE_UPDATE_1){
				gsm_message(gsm, "INITIALING modem on span %d, coz WIND 3 received when not expected\n", gsm->span);
				gsm->wind_state=0;
				gsm->creg_state=0;
				if(gsm->state == ALLOGSM_STATE_CALL_ACTIVE){
					gsm_message(gsm, "RESETING WHILE CALL ACTIVE on span %d, coz KSUP\n", gsm->span);
					/* Destory call and hangup all channels */
					UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_NULL);
					call->peercallstate = AT_CALL_STATE_NULL;
				}
				gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_GENERAL_INDICATION));
			}
		}
		return NULL;
	}
	if (gsm_compare(buf, "+SIM: ")) {
		if (gsm_compare(buf,"+SIM: 0")) { gsm->wind_state |=WIND_0_SIM_REMOVED;
		} else if (gsm_compare(buf,"+SIM: 1")) { gsm->wind_state |=WIND_1_SIM_INSERTED;
		}
		if ((gsm_compare(buf,"+SIM: 0")) || (gsm_compare(buf,"+SIM: 1"))) { 
			if (gsm->state !=  ALLOGSM_STATE_INIT && gsm->state != ALLOGSM_STATE_UPDATE_1){
				gsm_message(gsm, "Reseting modem on span %d in Check wind as %s was rcvd specifying sim insertion(1) or removal(0)\n", gsm->span, buf);
				gsm->wind_state=0;
				gsm->creg_state=0;
				if(gsm_compare(buf,"+SIM: 1")){	/* Only reset module on detect*/
					gsm_switch_state(gsm, ALLOGSM_STATE_INIT, "AT+CFUN=1,1");
					sleep(5); /* To reset module*/
				}
				gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_GENERAL_INDICATION));
			}
		}
		return NULL;
	}
	if (gsm_compare(buf, "+CME ERROR: 13")) {
		/*SIM Failure. Reset here.*/
		gsm->wind_state=0;
		gsm->creg_state=0;
		gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_GENERAL_INDICATION));
	}
#else	
//	if (gsm->state < ALLOGSM_STATE_READY) 
	{
		char tmp1[250];
		int i=0;
		strcpy(tmp1, buf);
		trim_CRLF(tmp1);
		//if (gsm->state != ALLOGSM_STATE_NET_REQ){
		if (gsm->state != ALLOGSM_STATE_READY) {
			if (!strcmp(tmp1,get_at(gsm->switchtype,AT_CREG0))){ i=10;
			} else if (!strcmp(tmp1,get_at(gsm->switchtype,AT_CREG1))) { i=11;
			} else if (!strcmp(tmp1,get_at(gsm->switchtype,AT_CREG2))) { i=12;
			} else if (!strcmp(tmp1,get_at(gsm->switchtype,AT_CREG3))) { i=13;
			} else if (!strcmp(tmp1,get_at(gsm->switchtype,AT_CREG4))) { i=14;
			} else if (!strcmp(tmp1,get_at(gsm->switchtype,AT_CREG5))) { i=15;
			}
			if (!strcmp(tmp1,"+WIND: 0")) { i=20;
			} else if (!strcmp(tmp1,"+WIND: 1")) { i=21;
			} else if (!strcmp(tmp1,"+WIND: 2")) { i=22;
			} else if (!strcmp(tmp1,"+WIND: 3")) { i=23;
			} else if (!strcmp(tmp1,"+WIND: 4")) { i=24;
			} else if (!strcmp(tmp1,"+WIND: 5")) { i=25;
			} else if (!strcmp(tmp1,"+WIND: 6")) { i=26;
			} else if (!strcmp(tmp1,"+WIND: 7")) { i=27;
			}
		}
		i=i-20;
		switch (i){
			case 0: 
				gsm->ev.e = ALLOGSM_EVENT_SIM_FAILED; //modifiy it.. not propper
				return &gsm->ev;				
				break;
			case 1: /*
				gsm->ev.e = ALLOGSM_EVENT_SIM_INSERTED;
				return &gsm->ev;				
				break;*/
			case 2: 
			case 3: 
			case 4: 
			case 5: 
			case 6: 
			case 7: 
			case 8: 
				//printf("Span %d received(i) %s\n",gsm->span ,i ,tmp1);
				return NULL;				
				break;
			default:
				return NULL;
			
		}
/*
		if (i){	 
			char tmp[250];
			gsm_trim(gsm->at_last_sent, tmp, strlen(gsm->at_last_sent));
			printf("RETRYING Command >>%s<< because of %d:: state %s :: tmp1 %s\n", tmp, i,allogsm_state2str(gsm->state), tmp1 );
			gsm_send_at (gsm, tmp);
		}
*/
	}
#endif
return NULL;
}
int allogsm_check_emergency_available(struct allogsm_modul *gsm){
        if (gsm->wind_state&WIND_7_READY_EMERGENCY_CALL){
                printf("READY %x %x %x\n", gsm->wind_state, WIND_7_READY_EMERGENCY_CALL, gsm->wind_state&WIND_7_READY_EMERGENCY_CALL);
		return 1;
        }else{
                printf("NOT READY %x %x %x\n", gsm->wind_state, WIND_7_READY_EMERGENCY_CALL,gsm->wind_state&WIND_7_READY_EMERGENCY_CALL);
		return 0;
	}
}
void allogsm_module_start(struct allogsm_modul *gsm)
{
	char tmp[5];
	gsm->wind_state=0;
	gsm->creg_state=0;
	gsm->CME_515_count=0;
        sprintf(tmp, "%c", 0x1A);
        gsm_transmit(gsm, tmp);
	if(!is_poweroff(gsm->span)){
		if (!gsm->autoReloadLoopActive){
			gsm->autoReloadLoopActive=1;
			gsm_schedule_event(gsm, 50000, gsm_start_timeout_junk, gsm);
		}

		gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_GENERAL_INDICATION));
	}else{
		gsm_message(gsm, "Not initializing GSM, Span %d powered Off\n", gsm->span);
	}
	//gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_CHECK));
}


static allogsm_event *gsm_set_switchtype(struct allogsm_modul *gsm, char *data, int len)
{
	struct alloat_call *call;
	char buf[1024];
	char receivebuf[1024];
	char *p;
	int i;
	allogsm_event* res_event=NULL;
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
		len = gsm_san(gsm, p, buf, len);
		if (!len) {
			return NULL;
		}
		
		//Freedom Modify 2011-09-14 16:45
		//if (gsm->debug & ALLOGSM_DEBUG_AT_RECEIVED)
		if ((gsm->debug & ALLOGSM_DEBUG_AT_RECEIVED) || (gsm->span==1))
		{
			char tmp[1024];
			gsm_trim(gsm->at_last_sent, tmp, strlen(gsm->at_last_sent));
#define FORMAT  "     %-2d<-- %-1d %-40.40s  || sz: %-3d|| %-23s || last sent: %s\n"
			int ii=0;
                        gsm_message(gsm, FORMAT, gsm->span, i, buf, len, allogsm_state2str(gsm->state)+14,tmp);
                        for(ii=40; ii<len; ii=ii+40)
                                gsm_message(gsm,"             %-40.40s\n", buf+ii);
#undef FORMAT
		}
		strncpy(p, gsm->sanbuf, sizeof(receivebuf));
		/*{
			char tmp[1024];
			gsm_trim(gsm->at_last_sent, tmp, strlen(gsm->at_last_sent));
			gsm_message(gsm, "\t\t%d:<<< last_send:%s  -- recive:%s , %d\n", gsm->span,tmp, buf, len);
		}*/
		res_event = module_check_wind(gsm, call ,buf, i);
		if (res_event) {
			return res_event;
		}
		gsm_message(gsm, "span %d, state:%s\n", gsm->span, allogsm_state2str(gsm->state)+14);
		switch (gsm->state) {
			case ALLOGSM_STATE_INIT:
				if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_GENERAL_INDICATION))) {
						/*In this case we are considering wether AT_GENERAL_INDICATION was pass or failed.
						  So instead of comparing wether ok or error and then checking inside for AT_GENERAL_INDICATION
						  We are giving this sent a higher priority and reseting anyways
						*/
						gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_RESET));
						gsm->ev.gen.e = ALLOGSM_EVENT_DCHAN_DOWN;
						return &gsm->ev;
				}else if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_RESET))) {
						if (gsm_compare("UPDATE AVAILABLE" ,get_at(gsm->switchtype,AT_UPDATE_1))) {
							if (gsm->span==atoi(get_at(gsm->switchtype,AT_UPDATE_SPAN))) {
								gsm_message(gsm,"Updating Span %d with %s\n", gsm->span, get_at(gsm->switchtype,AT_UPDATE_CMD));
								//gsm_switch_state(gsm, ALLOGSM_STATE_UPDATE_1, NULL);
								gsm_switch_state(gsm, ALLOGSM_STATE_UPDATE_1, get_at(gsm->switchtype,AT_UPDATE_CMD));
							}else{
    								gsm->state = ALLOGSM_STATE_UPDATE_SUCCESS;
							}
						}
						if(gsm->state != ALLOGSM_STATE_UPDATE_1)
							gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_CHECK));
#if NO_WIND_BLOWING
//						gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_CHECK));
#endif
						/*Here OK says Reset was successful.. now we have to wait for wind indications*/	
						gsm->ev.gen.e = ALLOGSM_EVENT_DETECT_MODULE_OK;
						return &gsm->ev; 
					} else if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_CHECK))) {
#ifdef VIRTUAL_TTY
						gsm->already_set_mux_mode = 0;
#endif //VIRTUAL_TTY
#if 1 
						gsm_switch_state(gsm, ALLOGSM_STATE_UP, get_at(gsm->switchtype,AT_ATZ));
#endif
					}
				}else if (gsm_compare(buf, "+KSUP: 0")){ 
						gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_CHECK));
#ifdef WAVECOM
				}else if (gsm_compare(buf, "+WIND: 3")){ 
						/*Module is ready to accept at commands but we wont start the process if dont get wind 7
						  coz anyways no benefit of doing info fetching if we cant to emergency calls itself.. */
						if (gsm->wind_state & WIND_1_SIM_INSERTED)
							gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_CHECK));
				}else if (gsm_compare(buf, "+WIND: 1")){ 
						/*SIM present*/
						if (gsm->wind_state & WIND_3_READY_BASIC_AT_COMMANDS)
							gsm_switch_state(gsm, ALLOGSM_STATE_INIT, get_at(gsm->switchtype,AT_CHECK));
				}else if (gsm_compare(buf, "+WIND: 0")){ 
						/*SIM not present*/
					gsm->ev.e = ALLOGSM_EVENT_SIM_FAILED;
					return &gsm->ev;
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
//						gsm_send_at(gsm, get_at(gsm->switchtype,AT_CHECK));
#endif
				}
				else
				{
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "span:%d--!%s!\n", DEBUGARGS, gsm->span,buf);
				}
				break;
			case ALLOGSM_STATE_UP:
			{
				/* Drops the current call, and resets the values to default configuration. */
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_ATZ))) {
						//sleep(3); /* To show the network status */
						
						//Freedom Add 2011-10-14 09:20 Fix up sim900d bug,must send "ATH" after "ATZ"
						//gsm_switch_state(gsm, ALLOGSM_STATE_SET_ECHO, get_at(gsm->switchtype,AT_SET_ECHO_MODE));
#ifdef WAVECOM
/*ADDED to check if WAVECOM module is not reaching upto ALLOGSM_STATE_READY in 30 sec because of junk data, reset it*/
/*
						if (!(gsm->retranstimer)) {
							gsm->retranstimer = gsm_schedule_event(gsm, 30000, gsm_start_timeout_junk, call);
						}
*/
/*
						if (gsm->retries<3){	
							gsm_schedule_event(gsm, 30000, gsm_start_timeout_junk, call);
							++(gsm->retries);
						}else{
							gsm_message(gsm, "NOT Retrying anymore to start GSM modem\n");
						}
*/
#endif
						
						gsm_switch_state(gsm, ALLOGSM_STATE_SEND_HANGUP, get_at(gsm->switchtype,AT_HANGUP));
					}
	
					if (gsm->resetting) {
						/* Destory call and hangup all channels */
						UPDATE_OURCALLSTATE(gsm, call, AT_CALL_STATE_NULL);
						call->peercallstate = AT_CALL_STATE_NULL;
						gsm->ev.gen.e = ALLOGSM_EVENT_RESTART;
					} else {
						//Freedom Del 2011-12-13 18:47
						//gsm->retranstimer = gsm_schedule_event(gsm, gsm->timers[ALLOGSM_TIMER_T316], gsm_start_timeout, call);
					}
				} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_ATZ))) {
				} else if (gsm_compare(buf, "Call Ready")) {
				} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_ERROR)) ||
						   gsm_compare(buf, get_at(gsm->switchtype,AT_NO_ANSWER)) || 
						   gsm_compare(buf, get_at(gsm->switchtype,AT_NO_CARRIER))){
					//gsm->retranstimer = gsm_schedule_event(gsm, gsm->timers[ALLOGSM_TIMER_T316], gsm_start_timeout, call);
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!\n", DEBUGARGS, buf);
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_ATZ));
#endif
				} else {
					if (!(gsm->retranstimer)) {
						gsm->retranstimer = gsm_schedule_event(gsm, gsm->timers[ALLOGSM_TIMER_T316], gsm_start_timeout, call);
					}
				}
				break;
			}	
#ifdef WAVECOM
			//pawan : DISABLE STIN, WIND, CREG unsolicited Response; Until Modules are up
			/////////////////////////////////////////////////////////////////////////////////////////
			case ALLOGSM_STATE_SEND_HANGUP:
				/* Disable echo */
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_HANGUP))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_SIM_INDICATION_OFF, get_at(gsm->switchtype,AT_SIM_TOOLKIT));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_HANGUP));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "span:%d--!%s!\n", DEBUGARGS, gsm->span,buf);
				}
				break;
			case ALLOGSM_STATE_SET_SIM_INDICATION_OFF:
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SIM_TOOLKIT))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_GEN_INDICATION_OFF, get_at(gsm->switchtype,AT_GENERAL_INDICATION));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_SIM_TOOLKIT));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "span:%d--!%s!\n", DEBUGARGS, gsm->span,buf);
				}
				break;
			case ALLOGSM_STATE_SET_GEN_INDICATION_OFF:
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_GENERAL_INDICATION) )) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_CREG_INDICATION_OFF, get_at(gsm->switchtype,AT_CREG_DISABLE));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_GENERAL_INDICATION));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "span:%d--!%s!\n", DEBUGARGS, gsm->span,buf);
#ifdef NO_WIND_BLOWING
					gsm_switch_state(gsm, ALLOGSM_STATE_SET_CREG_INDICATION_OFF, get_at(gsm->switchtype,AT_CREG_DISABLE));
#endif
				}
				break;
			case ALLOGSM_STATE_SET_CREG_INDICATION_OFF:
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_CREG_DISABLE))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_ECHO, get_at(gsm->switchtype,AT_SET_ECHO_MODE));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_CREG_DISABLE));
#endif
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "span:%d--!%s!\n", DEBUGARGS, gsm->span,buf);
				}
				break;
#else
			//////////////////////////////////////////////////////////////////////////////////////////
			//Freedom Add 2011-10-14 09:20 Fix up sim900d bug,must send "ATH" after "ATZ"
			/////////////////////////////////////////////////////////////////////////////////////////
			case ALLOGSM_STATE_SEND_HANGUP:
				/* Disable echo */
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_HANGUP))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_ECHO, get_at(gsm->switchtype,AT_SET_ECHO_MODE));
					}
				} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_HANGUP))) {
					/* echo */
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "span:%d--!%s!\n", DEBUGARGS, gsm->span,buf);
				}
				break;
			//////////////////////////////////////////////////////////////////////////////////////////
#endif
			case ALLOGSM_STATE_SET_ECHO:
				/* Disable echo */
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SET_ECHO_MODE))) {
						gsm_switch_state(gsm, ALLOGSM_STATE_SET_REPORT_ERROR, get_at(gsm->switchtype,AT_SET_CMEE));
					}
#ifdef WAVECOM_JUNK
				} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ") || expectlist_compare(buf)) {
						gsm_send_at(gsm, get_at(gsm->switchtype,AT_SET_ECHO_MODE));
#endif
				} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_SET_ECHO_MODE))) {
					/* echo */
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!\n", DEBUGARGS, buf);
				}
				break;
			case ALLOGSM_STATE_SET_REPORT_ERROR:
				/* Report mobile equipment error
					Response
						TA disables or enables the use of result code +CME ERROR: <err> as an indication of an error relating to the functionality of the ME.
							OK
					Parameters	
						0 disable result code
						1 enable result code and use numeric values
						2 enable result code and use verbose values
				*/
			//	if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_SET_CMEE))) {
// Update require after WIND 3;
						memset(gsm->model_name, 0, sizeof(gsm->model_name));
						gsm_switch_state(gsm, ALLOGSM_STATE_MODEL_NAME_REQ, get_at(gsm->switchtype,AT_GET_CGMM));

//////////////////////////////
					}
			//	} 
				else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!\n", DEBUGARGS, buf);
				}
				break;
			case ALLOGSM_STATE_UPDATE_1:

				/*if UPDATE OK,  report success from here*/
				if (gsm_compare(buf, "+WIND: 3")){ 
						/*Module is ready to accept at commands but we wont start the process if dont get wind 7
						  coz anyways no benefit of doing info fetching if we cant to emergency calls itself.. */
					if ((gsm->wind_state & WIND_1_SIM_INSERTED) || (gsm->wind_state & WIND_0_SIM_REMOVED))
						gsm_switch_state(gsm, ALLOGSM_STATE_UPDATE_1, get_at(gsm->switchtype, AT_UPDATE_CMD));
				} else if (gsm_compare(buf, "+WIND: 1")){ 
						/*SIM present*/
					if (gsm->wind_state & WIND_3_READY_BASIC_AT_COMMANDS)
						gsm_switch_state(gsm, ALLOGSM_STATE_UPDATE_1, get_at(gsm->switchtype, AT_UPDATE_CMD));
				} else if (gsm_compare(buf, "+WIND: 0")){ 
						/*SIM not present*/
					if (gsm->wind_state & WIND_3_READY_BASIC_AT_COMMANDS)
						gsm_switch_state(gsm, ALLOGSM_STATE_UPDATE_1, get_at(gsm->switchtype, AT_UPDATE_CMD));
				} else if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_UPDATE_CMD))) {
						gsm_message(gsm, "Successfully Updated\n");
    						gsm->state = ALLOGSM_STATE_UPDATE_SUCCESS;
					}
				} else if (gsm_compare(buf, "ERROR")) {
					gsm_message(gsm, "Failed With error : %s \n", buf);
    						gsm->state = ALLOGSM_STATE_UPDATE_SUCCESS;
				} else {
					gsm_message(gsm, "Received While updating : %s \n", buf);
				}
				break;
			case ALLOGSM_STATE_UPDATE_2:
				/*if UPDATE OK,  report success from here*/
				gsm_message(gsm, "HERE: %d %s\n", __LINE__, __func__); //pawan print
				if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))) {
					if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_UPDATE_CMD))) {
						gsm_message(gsm, "Successfully Updated\n");
				gsm_message(gsm, "HERE: %d %s\n", __LINE__, __func__); //pawan print
						gsm_write_file("/tmp/update_status", "SUCCESS");
    						gsm->state = ALLOGSM_STATE_UPDATE_SUCCESS;
					}
				} else if (gsm_compare(buf, "+CME ERROR: 30")) {
					gsm_message(gsm, "HERE: %d %s\n", __LINE__, __func__); //pawan print
					gsm_message(gsm, "Failed With error : %s \n", buf);
					gsm_write_file("/tmp/update_status", "FAILED: No network service");
					sleep (1);
					gsm_switch_state(gsm, ALLOGSM_STATE_UPDATE_2, get_at(gsm->switchtype, AT_UPDATE_CMD));
//    					gsm->state = ALLOGSM_STATE_UPDATE_SUCCESS;
				} else if (gsm_compare(buf, "ERROR")) {
					gsm_message(gsm, "HERE: %d %s\n", __LINE__, __func__); //pawan print
					gsm_message(gsm, "Failed With error : %s \n", buf);
					gsm_write_file("/tmp/update_status", "FAILED");
    					//gsm->state = ALLOGSM_STATE_UPDATE_SUCCESS;
					sleep (1);
					gsm_switch_state(gsm, ALLOGSM_STATE_UPDATE_2, get_at(gsm->switchtype, AT_UPDATE_CMD));
				}else{
					gsm_message(gsm, "Received While updating : %s \n", buf);
				}
				break;
			case ALLOGSM_STATE_UPDATE_SUCCESS:
				gsm_message(gsm, "HERE: %d %s\n", __LINE__, __func__); //pawan print
				break;
			case ALLOGSM_STATE_MODEL_NAME_REQ:
				/* Request model identification */
				if (gsm_compare(gsm->at_last_sent, get_at(gsm->switchtype,AT_GET_CGMM))) {
#if 1
//---------It was propper.. Changed for allo gsm, since some time garbage is received
/////////////----------------------------------------------------------
					if (gsm_compare(buf, get_at(gsm->switchtype,AT_OK))&&strlen(gsm->model_name)) {
						//usleep(125000);
						memset(gsm->manufacturer, 0, sizeof(gsm->manufacturer));
						gsm_switch_state(gsm, ALLOGSM_STATE_MANUFACTURER_REQ, get_at(gsm->switchtype,AT_GET_CGMI));
					} else if (gsm_compare(buf, "ERROR")) {  /*for em200: AT+CVER */
						gsm_switch_state(gsm, ALLOGSM_STATE_MODEL_NAME_REQ, "AT+CGMM");
#ifdef WAVECOM_JUNK
					} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
						gsm_send_at(gsm, get_at(gsm->switchtype, AT_GET_CGMM));
#endif
					} else if (!gsm_compare(buf, "+CME ERROR:")) {
						gsm_get_model_name(gsm, buf);
					} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
/////////////----------------------------------------------------------
#else
					if (gsm_compare(buf, "ERROR")) {  /*for em200: AT+CVER */
						gsm_switch_state(gsm, ALLOGSM_STATE_MODEL_NAME_REQ, "AT+CGMM");
#ifdef WAVECOM_JUNK
					} else if (gsm_compare(buf, "+CREG: ") || gsm_compare(buf, "+WIND: ")) {
						gsm_send_at(gsm, get_at(gsm->switchtype, AT_GET_CGMM));
#endif
					} else if (!gsm_compare(buf, "+CME ERROR:")) {
						gsm_get_model_name(gsm, buf);
						gsm_switch_state(gsm, ALLOGSM_STATE_MANUFACTURER_REQ, get_at(gsm->switchtype,AT_GET_CGMI));
/////////////----------------------------------------------------------
#endif

					} else {
						//Freedom del 2012-06-05 15:50
						//gsm_error(gsm, DEBUGFMT "!%s!\n", DEBUGARGS, buf);
					}
				} else if (gsm_compare(gsm->at_last_sent, "AT$HCTCM=0")) {
					if (gsm_compare(buf, "OK")) {
						gsm_switch_state(gsm, ALLOGSM_STATE_MODEL_NAME_REQ, "AT+GMM");						
					} else if (gsm_compare(buf, "$HCTCM:")) {
					} else {
						//Freedom del 2012-06-05 15:50
						//gsm_error(gsm, DEBUGFMT "!%s!\n", DEBUGARGS, buf);
					}				
				} else if (gsm_compare(gsm->at_last_sent, "AT+GMM")) {
					if (gsm_compare(buf, "OK")) {
						gsm_switch_state(gsm, ALLOGSM_STATE_MANUFACTURER_REQ, "AT+CGMI");
					} else if (gsm_compare(buf, "+GMM:")) {
						gsm_get_model_name(gsm, buf+6);
					} else {
						//Freedom del 2012-06-05 15:50
						//gsm_error(gsm, DEBUGFMT "!%s!\n", DEBUGARGS, buf);
					}
				} else {
					//Freedom del 2012-06-05 15:50
					//gsm_error(gsm, DEBUGFMT "!%s!\n", DEBUGARGS, buf);
				}
				break;						
		}
		
		i ++;

		if (-1 == len) {
			return NULL;
		}
		
		len = gsm->sanidx;
#if 0
		gsm->sanidx = 0;
#else
		gsm->sanidx = 0;
#endif
		
		if (gsm->state == ALLOGSM_STATE_MANUFACTURER_REQ) {
			break;
		}
	}
	
	allogsm_set_module_id(&(gsm->switchtype),gsm->model_name);

	return NULL;
}


/******************************************************************************
 * Handle received AT feedback from GSM D-channel
 * param:
 *		gsm: struct allogsm_modul
 * return:
 *		allogsm_event
 ******************************************************************************/
allogsm_event *allogsm_check_event(struct allogsm_modul *gsm)
{
	char buf[1024];
	int res=0;
	int i=0;
	allogsm_event *e;
	char *p=NULL;
	e = NULL;
	int j;
        if (gsm->debug & ALLOGSM_DEBUG_AT_DUMP){
//	if (gsm->span==2)	{
		gsm_message(gsm, "--------after(%d)-->\n", gsm->sanidx);
		for (j = 0; j < gsm->sanidx ; j++) {
		if ( (gsm->sanbuf[j]<20)||(gsm->sanbuf[j]>127) )
			gsm_message(gsm, "0x%x", gsm->sanbuf[j]);
		else
			gsm_message(gsm, "%c", gsm->sanbuf[j]);
		}
		gsm_message(gsm, "\n<----\n");
	}

	if (gsm->sanidx>0){
#if 1
//		gsm->at_last_recv_idx	= 0;
		memset(gsm->at_last_recv,0,sizeof(gsm->at_last_recv));
		if (gsm->state < ALLOGSM_STATE_MANUFACTURER_REQ) {
			e = gsm_set_switchtype(gsm, gsm->at_last_recv, 0);
		} else {
			e = module_receive(gsm, gsm->at_last_recv, 0);
		}
#else
 		memset(gsm->at_last_recv,0,sizeof(gsm->at_last_recv));
		memcpy(gsm->at_last_recv, gsm->sanbuf, gsm->sanidx);
		gsm->at_last_recv_idx	= gsm->sanidx;
		gsm->sanidx=0;
		memset(gsm->sanbuf,0,sizeof(gsm->sanbuf));
		
 		if (gsm->state < ALLOGSM_STATE_MANUFACTURER_REQ) {
			e = gsm_set_switchtype(gsm, gsm->at_last_recv, gsm->at_last_recv_idx);
 		} else {
			e = module_receive(gsm, gsm->at_last_recv, gsm->at_last_recv_idx);
 		}
#endif
	} else 
	{
		/* Read from GSM D-channel */
		res = gsm->read_func ? gsm->read_func(gsm, buf, sizeof(buf)) : 0;
		if (!res) {
			return NULL;
		}

		if (gsm->debug & ALLOGSM_DEBUG_AT_DUMP){
//		if (gsm->span==4){
		gsm_message(gsm, "RAW received(%d)-->\n", res);
			for (i = 0; i < res; i++) {
			if ( (buf[i]<0x20)||(buf[i]>0x7E) )
				gsm_message(gsm, "0x%x", buf[i]);
			else
				gsm_message(gsm, "%c", buf[i]);
			}
			gsm_message(gsm, "\n<----\n");
		}

		if((gsm->at_last_recv_idx + res) > 1024){ /* Clear buff and dont allow to exceed the buff limit*/
			gsm->at_last_recv_idx	= 0;
			memset(gsm->at_last_recv,0,sizeof(gsm->at_last_recv));
			return NULL;
		}

		for (i = 0; i < res; i++) {
			if(buf[i]!=0x0){ //pawan added temporarily for allo2aCG s500 driver bug where 1st byte is received 0x0 ruining our state machine.
				gsm->at_last_recv[gsm->at_last_recv_idx] = buf[i];
				gsm->at_last_recv_idx++;
			}
		}
		if(gsm->at_last_recv_idx > 500){
			gsm_message(gsm, "Exceeded DATA limit!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! %d + %d = %d\n", res, gsm->at_last_recv_idx, (gsm->at_last_recv_idx+res));
		}
		

		memset(gsm->at_pre_recv,0,sizeof(gsm->at_pre_recv)); //pawan added; watchpoint for sms or AT command related bugs
		strncpy(gsm->at_pre_recv, gsm->at_last_recv, sizeof(gsm->at_pre_recv));
		if(gsm->at_last_recv_idx>2)
			p=&(gsm->at_last_recv[gsm->at_last_recv_idx-2]);
		else
			p=gsm->at_last_recv;


		if(gsm->state==ALLOGSM_STATE_SMS_SENDING){
			if((gsm->at_last_recv_idx>2) && (*p=='>')){
				gsm_message(gsm, "SMS SENDING GOT PROMPT  %X %X %X\n", gsm->at_last_recv[gsm->at_last_recv_idx-2],gsm->at_last_recv[gsm->at_last_recv_idx-1], gsm->at_last_recv[gsm->at_last_recv_idx]);
			}else{
				gsm_message(gsm, "Data on span:%d(len:%d)-->\n", gsm->span, gsm->at_last_recv_idx);
				for (i = 0; i < gsm->at_last_recv_idx; i++) {
					if ( (gsm->at_last_recv[i]<0x20)||(gsm->at_last_recv[i]>0x7E) )
						gsm_message(gsm, "0x%x", gsm->at_last_recv[i]);
					else
						gsm_message(gsm, " %c:", gsm->at_last_recv[i]);
				}
				gsm_message(gsm, "\n<----\n");
			}
		}
	  
		if (((gsm->at_last_recv[gsm->at_last_recv_idx-1]=='\r')||(gsm->at_last_recv[gsm->at_last_recv_idx-1]=='\n')) ||
			    ((gsm->state==ALLOGSM_STATE_SMS_SENDING)&&(*p=='>'))){ //sandeep changed
	/*		    ((gsm->state==ALLOGSM_STATE_SMS_SENDING)&& strstr(gsm->at_last_recv[gsm->at_last_recv_idx-1],'>'))){*/
			if (gsm->debug & ALLOGSM_DEBUG_AT_DUMP) {
				gsm_dump(gsm, gsm->at_last_recv, gsm->at_last_recv_idx, 0);
			}

			if (gsm->state==ALLOGSM_STATE_SMS_SENDING){
				gsm_message(gsm, "SMS SENDING STATE !!!!\n");
			}

			if (gsm->state < ALLOGSM_STATE_MANUFACTURER_REQ) {
				e = gsm_set_switchtype(gsm, gsm->at_last_recv, gsm->at_last_recv_idx);
			} else {
				if((p!=">")||(p!="4"))
					e = module_receive(gsm, gsm->at_last_recv, gsm->at_last_recv_idx);
				else
					e = module_receive(gsm, p, gsm->at_last_recv_idx);
			}
			gsm->at_last_recv_idx	= 0;
			memset(gsm->at_last_recv,0,sizeof(gsm->at_last_recv));
		}
	}
	return e;
}


allogsm_event *allogsm_check_event_buffered(struct allogsm_modul *gsm)
{
	char buf[1024];
	int res=0;
	int i=0;
	allogsm_event *e;
	char *p=NULL;
	e = NULL;
	int j;
	if (gsm->debug & ALLOGSM_DEBUG_AT_DUMP){
	gsm_message(gsm, "--------after(%d)-->\n", gsm->sanidx);
		for (j = 0; j < gsm->sanidx ; j++) {
		if ( (gsm->sanbuf[j]<20)||(gsm->sanbuf[j]>127) )
			gsm_message(gsm, "0x%x", gsm->sanbuf[j]);
		else
			gsm_message(gsm, "%c", gsm->sanbuf[j]);
		}
		gsm_message(gsm, "\n<----\n");
	}
	/* Read from GSM D-channel */
	if (gsm->sanidx>0){
//		gsm->at_last_recv_idx	= 0;
//		memset(gsm->at_last_recv,0,sizeof(gsm->at_last_recv));
		if (gsm->state < ALLOGSM_STATE_MANUFACTURER_REQ) {
			e = gsm_set_switchtype(gsm, NULL, 0);
		} else {
			e = module_receive(gsm, NULL, 0);
		}
	} else {
		return NULL;
	}
	return e;
}
int allogsm_acknowledge(struct allogsm_modul *gsm, struct alloat_call *c, int channel, int info)
{
	if (!gsm || !c) {
		return -1;
	}
	
	if (!c->proc) {
		gsm_call_proceeding(gsm, c, channel, 0);
	}
	if (info) {
		c->progloc = LOC_PRIV_NET_LOCAL_USER;
		c->progcode = CODE_CCITT;
		c->progressmask = ALLOGSM_PROG_INBAND_AVAILABLE;
	} else {
		c->progressmask = 0;
	}
	UPDATE_OURCALLSTATE(gsm, c, AT_CALL_STATE_CALL_RECEIVED);
	c->peercallstate = AT_CALL_STATE_CALL_DELIVERED;
	c->alive = 1;
	
	return 0;
}

int allogsm_proceeding(struct allogsm_modul *gsm, struct alloat_call *call, int channel, int info)
{
	if (!gsm || !call) {
		return -1;
	}
	
	return gsm_call_proceeding(gsm, call, channel, info);
}

int allogsm_progress(struct allogsm_modul *gsm, struct alloat_call *c, int channel, int info)
{
	if (!gsm || !c) {
		return -1;
	}

	if (channel) { 
		channel &= 0xff;
		c->channelno = channel;		
	}

	if (info) {
		c->progloc = LOC_PRIV_NET_LOCAL_USER;
		c->progcode = CODE_CCITT;
		c->progressmask = ALLOGSM_PROG_INBAND_AVAILABLE;
	} else {
		/* PI is mandatory IE for PROGRESS message - Q.931 3.1.8 */
		gsm_error(gsm, "XXX Progress message requested but no information is provided\n");
		c->progressmask = 0;
	}

	c->alive = 1;
	return 0;
}


int allogsm_information(struct allogsm_modul *gsm, struct alloat_call *call, char digit)
{
	if (!gsm || !call) {
		return -1;
	}
	call->callednum[0] = digit;
	call->callednum[1] = '\0';
	return 0;
}


int allogsm_need_more_info(struct allogsm_modul *gsm, struct alloat_call *c, int channel)
{
	if (!gsm || !c) {
		return -1;
	}
	if (channel) { 
		channel &= 0xff;
		c->channelno = channel;		
	}
	c->chanflags &= ~FLAG_PREFERRED;
	c->chanflags |= FLAG_EXCLUSIVE;
	c->progressmask = 0;
	UPDATE_OURCALLSTATE(gsm, c, AT_CALL_STATE_OVERLAP_RECEIVING);
	c->peercallstate = AT_CALL_STATE_OVERLAP_SENDING;
	c->alive = 1;
	return 0;
}

int allogsm_senddtmf(struct allogsm_modul *gsm, int digit)
{
	return module_senddtmf(gsm,digit);
}

int allogsm_answer(struct allogsm_modul *gsm, struct alloat_call *c, int channel)
{
	if (!gsm || !c) {
		return -1;
	}
	if (channel) { 
		channel &= 0xff;
		c->channelno = channel;
	}
	c->chanflags &= ~FLAG_PREFERRED;
	c->chanflags |= FLAG_EXCLUSIVE;
	c->progressmask = 0;
	UPDATE_OURCALLSTATE(gsm, c, AT_CALL_STATE_CONNECT_REQUEST);
	c->peercallstate = AT_CALL_STATE_ACTIVE;
	c->alive = 1;
	/* Connect request timer */
	if (gsm->retranstimer) {
		gsm_schedule_del(gsm, gsm->retranstimer);
	}
	gsm->retranstimer = 0;

	module_answer(gsm);

	return 0;
}


int allogsm_setup(struct allogsm_modul *gsm, struct alloat_call *c, struct allogsm_sr *req)
{
	if (!gsm || !c) {
		return -1;
	}

	/* get law */
	if (!req->userl1) {
		req->userl1 = ALLOGSM_LAYER_1_ULAW;
	}
	c->userl1 = req->userl1;
	c->userl2 = -1;
	c->userl3 = -1;

	/* get D-channel number*/
	req->channel &= 0xff;
	c->channelno = req->channel;
	
	c->newcall = 0;
	c->complete = req->numcomplete; 

	/* get channel flag */
	if (req->exclusive) {
		c->chanflags = FLAG_EXCLUSIVE;
	} else if (c->channelno) {
		c->chanflags = FLAG_PREFERRED;
	}

	/* get caller and callername */
	if (req->caller) {
		strncpy(c->callernum, req->caller, sizeof(c->callernum));
		if (req->callername) {
			strncpy(c->callername, req->callername, sizeof(c->callername));
		} else {
			c->callername[0] = '\0';
		}
	} else {
		c->callernum[0] = '\0';
		c->callername[0] = '\0';
	}

	/* get callednum */
	if (req->called) {
		strncpy(c->callednum, req->called, sizeof(c->callednum));
	} else {
		return -1;
	}

	c->progressmask = 0;

	gsm->dial_initiated = 1;
	gsm_message(gsm, "---------------here %d dial_initiated %d\n", __LINE__,gsm->dial_initiated);

	if ((gsm->state == ALLOGSM_STATE_SMS_SENDING) || (gsm->state == ALLOGSM_STATE_SMS_SENT)){
		gsm->dial_retry=0;
		gsm_schedule_event(gsm, 200, gsm_dial_retry, c);
	}else{
		module_dial(gsm, c);
	}
	c->alive = 1;
	/* make sure we call ALLOGSM_EVENT_HANGUP_ACK once we send/receive RELEASE_COMPLETE */
	c->sendhangupack = 1;
	UPDATE_OURCALLSTATE(gsm, c, AT_CALL_STATE_CALL_INITIATED);
	c->peercallstate = AT_CALL_STATE_OVERLAP_SENDING;	
	
	return 0;
}


void allogsm_destroycall(struct allogsm_modul *gsm, struct alloat_call *call)
{
	if (gsm && call) {
		gsm_call_destroy(gsm, 0, call);
	}
	return;
}


int allogsm_hangup(struct allogsm_modul *gsm, struct alloat_call *c, int cause)
{
/*	int disconnect = 1;
	int release_compl = 0;
*/
	if (!gsm || !c) {
		return -1;
	}

        if (c->already_hangup)
                return -1;
        else
                c->already_hangup = 1; // raise the flag

	if (cause == -1) {
		/* normal clear cause */
		cause = 16;
	}
	if (gsm->debug & ALLOGSM_DEBUG_AT_STATE) {
		gsm_message(gsm, "NEW_HANGUP DEBUG: Calling at_hangup, ourstate %s, peerstate %s\n",callstate2str(c->ourcallstate),callstate2str(c->peercallstate));
	}

	/* If mandatory IE was missing, insist upon that cause code */
	if (c->cause == ALLOGSM_CAUSE_MANDATORY_IE_MISSING) {
		cause = c->cause;
	}
#if 0
	if (cause == 34 || cause == 44 || cause == 82 || cause == 1 || cause == 81) {
		/* We'll send RELEASE_COMPLETE with these causes */
		disconnect = 0;
		release_compl = 1;
	}
	if (cause == 6 || cause == 7 || cause == 26) {
		/* We'll send RELEASE with these causes */
		disconnect = 0;
	}
#endif
	
	/* All other causes we send with DISCONNECT */
	switch(c->ourcallstate) {
		case AT_CALL_STATE_NULL:
			if (c->peercallstate == AT_CALL_STATE_NULL) {
				/* free the resources if we receive or send REL_COMPL */
				gsm_call_destroy(gsm, c->cr, NULL);
			} else if (c->peercallstate == AT_CALL_STATE_RELEASE_REQUEST) {
				gsm_call_destroy(gsm, c->cr, NULL);
			}
			break;
		case AT_CALL_STATE_CALL_INITIATED:
			/* we sent SETUP */
		case AT_CALL_STATE_OVERLAP_SENDING:
			/* received SETUP_ACKNOWLEDGE */
		case AT_CALL_STATE_OUTGOING_CALL_PROCEEDING:
			/* received CALL_PROCEEDING */
		case AT_CALL_STATE_CALL_DELIVERED:
			/* received ALERTING */
		case AT_CALL_STATE_CALL_PRESENT:
			/* received SETUP */
		case AT_CALL_STATE_CALL_RECEIVED:
			/* sent ALERTING */
		case AT_CALL_STATE_CONNECT_REQUEST:
			/* sent CONNECT */
		case AT_CALL_STATE_INCOMING_CALL_PROCEEDING:
			/* we sent CALL_PROCEEDING */
		case AT_CALL_STATE_OVERLAP_RECEIVING:
			/* received SETUP_ACKNOWLEDGE */
			/* send DISCONNECT in general */
			gsm_call_disconnect(gsm,c,cause);	
			//gsm_call_destroy(gsm, c->cr, NULL);
			break;
		case AT_CALL_STATE_ACTIVE:
			/* received CONNECT */
			
			gsm_call_disconnect(gsm,c,cause);
				//gsm_call_destroy(gsm, c->cr, NULL);

			break;
		case AT_CALL_STATE_DISCONNECT_REQUEST:
			/* sent DISCONNECT */
			gsm_call_destroy(gsm, c->cr, NULL);

			break;
		case AT_CALL_STATE_DISCONNECT_INDICATION:
			/* received DISCONNECT */
			gsm_call_destroy(gsm, c->cr, NULL);
			break;
		case AT_CALL_STATE_RELEASE_REQUEST:
			/* sent RELEASE */
			/* don't do anything, waiting for RELEASE_COMPLETE */
			gsm_call_destroy(gsm, c->cr, NULL);

			break;
		case AT_CALL_STATE_RESTART:
		case AT_CALL_STATE_RESTART_REQUEST:
			/* sent RESTART */
			gsm_error(gsm, "at_hangup shouldn't be called in this state, ourstate %s, peerstate %s\n",callstate2str(c->ourcallstate),callstate2str(c->peercallstate));
			break;
		default:
			gsm_message(gsm, "We're not yet handling hanging up when our state is %d, ourstate %s, peerstate %s\n",
				  c->ourcallstate,
				  callstate2str(c->ourcallstate),
				  callstate2str(c->peercallstate));
			return -1;
	}
	
	if (c->ourcallstate != AT_CALL_STATE_NULL) {
                if ( gsm->dial_initiated == 1){
	gsm_message(gsm, "---------------here %d dial_initiated %d\n", __LINE__,gsm->dial_initiated);
                        gsm->dial_initiated_hangup=1;
                        return 0;
                }else{
                        module_hangup(gsm, c);
                }
	}
	c->newcall=1;
	c->ring_count=0;
	allogsm_destroycall(gsm, c);
	
	/* we did handle hangup properly at this point */
	return 0;
}


/******************************************************************************
 * Create a call
 * param:
 *		gsm: struct allogsm_modul
 * return:
 *	   	struct alloat_call
 ******************************************************************************/
struct alloat_call *allogsm_new_call(struct allogsm_modul *gsm)
{
	struct alloat_call *cur;
	
	if (!gsm) {
		return NULL;
	}

	gsm->cref++;
	if (gsm->cref > 32767) {
		gsm->cref = 1;
	}


	cur = allogsm_getcall(gsm, gsm->cref, 1);
	return cur;
}


/******************************************************************************
 * Dump gsm event message
 * param:
 *		gsm: struct allogsm_modul
 *		e: allogsm_event
 * return:
 *	   	void
 ******************************************************************************/
void allogsm_dump_event(struct allogsm_modul *gsm, allogsm_event *e)
{
	if (!gsm || !e) {
		return;
	}
	
	gsm_message(gsm, "Event type: %s (%d)\n", allogsm_event2str(e->gen.e), e->gen.e);
	switch(e->gen.e) {
		case ALLOGSM_EVENT_DCHAN_UP:
		case ALLOGSM_EVENT_DCHAN_DOWN:
			break;
		case ALLOGSM_EVENT_CONFIG_ERR:
			gsm_message(gsm, "Error: %s", e->err.err);
			break;
		case ALLOGSM_EVENT_RESTART:
			gsm_message(gsm, "Restart on channel %d\n", e->restart.channel);
		case ALLOGSM_EVENT_RING:
			gsm_message(gsm, "Calling number: %s \n", e->ring.callingnum);
			gsm_message(gsm, "Called number: %s \n", e->ring.callednum);
			gsm_message(gsm, "Channel: %d (%s) Reference number: %d\n", e->ring.channel, e->ring.flexible ? "Flexible" : "Not Flexible", e->ring.cref);
			break;
		case ALLOGSM_EVENT_HANGUP:
			gsm_message(gsm, "Hangup, reference number: %d, reason: %s\n", e->hangup.cref, allogsm_cause2str(e->hangup.cause));
			break;
		default:
			gsm_message(gsm, "Don't know how to dump events of type %d\n", e->gen.e);
	}
}

/******************************************************************************
 * Dump GSM sys info
 *			Show Switchtype
 *			Show Type
 *			Show Network Status
 *			Show Net Coverage
 *			Show SIM IMSI
 *			Show Card IMEI
 * param:
 *		gsm: struct allogsm_modul
 * return:
 *		gsm module sys info
 * e.g.
 *		used in chan_allogsm.so
 *		gsm show span 1
			D-channel: 2
			Status: Provisioned, Down, Active
			Switchtype: SimCom 100/300
			Type: CPE
			Network Status: Unregistered
			Network Name:
			Signal Quality (0,31): -1
			SIM IMSI:
			Card IMEI:
 ******************************************************************************/
char *allogsm_dump_info_str(struct allogsm_modul *gsm)
{
	char buf[4096];
	int len = 0;
	
	if (!gsm) {
		return NULL;
	}

        //return x > 0 ? 1 : 0;

	/* Might be nice to format these a little better */
	len += sprintf(buf + len, "Type: %s\n", allogsm_node2str(gsm->localtype));
	//len += sprintf(buf + len, "Switchtype: %s\n", allogsm_switch2str(gsm->switchtype));
	len += sprintf(buf + len, "Switchtype: %s\n", "Sierra WAVECOM Q2687RD");
	len += sprintf(buf + len, "Manufacturer: %s\n", strlen(gsm->manufacturer) < 1 ? "UNKNOWN": gsm->manufacturer);
	len += sprintf(buf + len, "Model Name: %s\n", strlen(gsm->model_name)  < 1 ? "UNKNOWN": gsm->model_name);
	len += sprintf(buf + len, "Model IMEI: %s\n", strlen(gsm->imei)  < 1 ? "UNKNOWN": gsm->imei);
	len += sprintf(buf + len, "Revision: %s\n", strlen(gsm->revision)  < 1 ? "UNKNOWN": gsm->revision);
	len += sprintf(buf + len, "Network Name: %s\n", strlen(gsm->net_name)  < 1 ? "UNKNOWN": gsm->net_name);
	len += sprintf(buf + len, "Network Status: %s\n", gsm_network2str(gsm->network));
	len += sprintf(buf + len, "Signal Quality (0,31): %d\n", gsm->coverage);
	len += sprintf(buf + len, "Signal Level: %d\n", gsm->coverage_level);
//Freedom del 2011-10-10 10:11
//	len += sprintf(buf + len, (gsm->switchtype == ALLOGSM_SWITCH_EM200) ? "Card GSN: %s\n" : "Card IMEI: %s\n", gsm->imei);
	len += sprintf(buf + len, "SIM IMSI: %s\n", gsm->imsi == NULL ? "UNKNOWN": gsm->imsi);
	len += sprintf(buf + len, "SIM SMS Center Number: %s\n",gsm->sim_smsc == NULL ? "UNKNOWN": gsm->sim_smsc);

	return strdup(buf);
}

/**
Dump String for GUI
*///////////

char *allogsm_dump_info_str_GUI(struct allogsm_modul *gsm, int stat)
{
	char buf[4096];
	int len = 0;
	
	if (!gsm) {
		return NULL;
	}

	//len += sprintf(buf + len, " %d ", gsm->network);

	len += sprintf(buf + len, "%d ", stat);
	len += sprintf(buf + len, " %d ", gsm->creg_state);

	if ((gsm->creg_state != CREG_1_REGISTERED_HOME && gsm->creg_state != CREG_5_REGISTERED_ROAMING) || (stat != 1 && stat != 2)){
		len += sprintf(buf + len, " %d ", -1);
		len += sprintf(buf + len, " %s ", "UNKNOWN");
	} else {
		len += sprintf(buf + len, " %d ", gsm->coverage_level);
		len += sprintf(buf + len, " %s ", strlen(gsm->net_name)  < 1 ? "UNKNOWN": gsm->net_name);
	}
	return strdup(buf);
}
struct allogsm_sr *allogsm_sr_new(void)
{
	struct allogsm_sr *req;
	req = malloc(sizeof(*req));
	if (req) {
		gsm_sr_init(req);
	}
	
	return req;
}

void allogsm_sr_free(struct allogsm_sr *sr)
{
	if (sr) {
		free(sr);
	}
}

int allogsm_sr_set_channel(struct allogsm_sr *sr, int channel, int exclusive, int nonisdn)
{
	sr->channel = channel;
	sr->exclusive = exclusive;
	sr->nonisdn = nonisdn;
	return 0;
}

int allogsm_sr_set_called(struct allogsm_sr *sr, char *called, int numcomplete)
{
	sr->called = called;
	sr->numcomplete = numcomplete;
	return 0;
}

int allogsm_sr_set_caller(struct allogsm_sr *sr, char *caller, char *callername, int callerpres)
{
	sr->caller = caller;
	sr->callername = callername;
	return 0;
}

static int __gsm_send_text(struct allogsm_modul *gsm, char *destination, char *message)
{
	return module_send_text(gsm, destination, message);
}
#ifdef QUEUE_SMS 

static void gsm_dequeue_sms_txt(void *gsm_ptr)
{
	struct allogsm_modul *gsm	= gsm_ptr;
	char *destination;
	char *message;

	if ((ALLOGSM_STATE_READY != gsm->state) ||
	    (gsm->dial_initiated == 1) ||
	    ((gsm->creg_state!=CREG_1_REGISTERED_HOME) &&
	    (gsm->creg_state!=CREG_5_REGISTERED_ROAMING))) {
		gsm_message(gsm, "here after dequeue %s %d\n",__func__, __LINE__ ); //pawan print
		int resendsmsidx = gsm_schedule_event(gsm, 1000, gsm_dequeue_sms_txt, gsm_ptr);
		gsm->sms_queue->front->sms_info.txt_info.resendsmsidx = resendsmsidx;
		//Freedom Add 2011-10-27 16:41 Release memory
		if (resendsmsidx < 0 && &(gsm->sms_queue->front->sms_info)) {
			gsm_error(gsm, "Can't schedule sending sms!\n");
			free (&(gsm->sms_queue->front->sms_info));
//			sms_info = NULL;
		}
	} else {
		gsm->sms_info=&(gsm->last_sms);
		if (QueueDelete(gsm) == 1){
			gsm_message(gsm, "here after dequeue %s %d\n",__func__, __LINE__ ); //pawan print
			int resendsmsidx = gsm_schedule_event(gsm, 1000, gsm_dequeue_sms_txt, (void *)gsm);
			gsm->sms_queue->front->sms_info.txt_info.resendsmsidx = resendsmsidx;
			//Freedom Add 2011-10-27 16:41 Release memory
			if (resendsmsidx < 0 && &(gsm->sms_queue->front->sms_info)) {
				gsm_error(gsm, "Can't schedule sending sms!\n");
				free (&(gsm->sms_queue->front->sms_info));
			//			sms_info = NULL;
			}
		}

		destination = gsm->sms_info->txt_info.destination;
		message	= gsm->sms_info->txt_info.message;
//		gsm->sms_info = sms_info;
		gsm_message(gsm, "here after dequeue %s %d\n",__func__, __LINE__ ); //pawan print
		__gsm_send_text(gsm, destination, message);
	}
}

#else
static void gsm_resend_sms_txt(void *info)
{
	sms_info_u *sms_info = info;
	struct allogsm_modul *gsm	= sms_info->txt_info.gsm;
	char *destination		= sms_info->txt_info.destination;
	char *message			= sms_info->txt_info.message;

	if ((ALLOGSM_STATE_READY != gsm->state) ||
	    (gsm->dial_initiated == 1) ||
	    ((gsm->creg_state!=CREG_1_REGISTERED_HOME) &&
	    (gsm->creg_state!=CREG_5_REGISTERED_ROAMING))) {
		int resendsmsidx = gsm_schedule_event(gsm, 2000, gsm_resend_sms_txt, info);
		sms_info->txt_info.resendsmsidx = resendsmsidx;
		//Freedom Add 2011-10-27 16:41 Release memory
		if (resendsmsidx < 0 && sms_info) {
			gsm_error(gsm, "Can't schedule sending sms!\n");
			free(sms_info);
			sms_info = NULL;
		}
	} else {
		gsm->sms_info = sms_info;
		__gsm_send_text(gsm, destination, message);
	}
}
#endif

static void gsm_resend_operator_list(void *info)
{
	struct allogsm_modul *gsm	= info;

	if (gsm->state != ALLOGSM_STATE_READY) {
		int resendsmsidx = gsm_schedule_event(gsm, 2000, gsm_resend_operator_list, info);
		if (resendsmsidx < 0 ) {
			gsm_error(gsm, "Can't schedule sending sms!\n");
		}
	} else {
		module_send_operator_list(gsm);
	}
}

int allogsm_send_operator_list(struct allogsm_modul *gsm) {
	int res = -1;
	if (!gsm)
	{
		return res;
	}	
	
	if (ALLOGSM_STATE_READY != gsm->state) {
		if (gsm_schedule_check(gsm) < 0) {
			gsm_error(gsm, "No enough space for sending operator list!\n");
			return -1;
		}
		// schedule index
		int resendsmsidx = gsm_schedule_event(gsm, 2000, gsm_resend_operator_list, gsm);
		if (resendsmsidx < 0 ) {
			gsm_error(gsm, "Can't schedule sending operator list!\n");
			return -1;
		}
		res = 0;
	} else {
		module_send_operator_list(gsm);
		res = 0;
	}

	return res;
}
static void gsm_resend_ussd(void *info)
{
	ussd_info_t *ussd_info = info;
	struct allogsm_modul *gsm	= ussd_info->gsm;
	char *message			= ussd_info->message;

	if (gsm->state != ALLOGSM_STATE_READY) {
		int resendsmsidx = gsm_schedule_event(gsm, 2000, gsm_resend_ussd, info);
		ussd_info->resendsmsidx = resendsmsidx;
		if (resendsmsidx < 0 && ussd_info) {
			gsm_error(gsm, "Can't schedule sending sms!\n");
			free(ussd_info);
			ussd_info = NULL;
		}
	} else {
		gsm->ussd_info = ussd_info;
		module_send_ussd(gsm, message);
	}
}
int allogsm_send_ussd(struct allogsm_modul *gsm, char *message) 
{
	int res = -1;
	ussd_info_t *ussd_info;
	if (!gsm)
	{
		return res;
	}	
	
	ussd_info = malloc(sizeof(ussd_info_t));
	if (!ussd_info) {
		gsm_error(gsm, "unable to malloc!\n");
		return res;
	}
	ussd_info->gsm = gsm;
	strncpy(ussd_info->message, message, sizeof(ussd_info->message));
	
	if (ALLOGSM_STATE_READY != gsm->state) {
		if (gsm_schedule_check(gsm) < 0) {
			gsm_error(gsm, "No enough space for sending sms!\n");
			return -1;
		}
		// schedule index
		int resendsmsidx = gsm_schedule_event(gsm, 2000, gsm_resend_ussd, (void *)ussd_info);
		ussd_info->resendsmsidx=resendsmsidx;
		if (resendsmsidx < 0 && ussd_info) {
			gsm_error(gsm, "Can't schedule sending ussd!\n");
			free(ussd_info);
			ussd_info = NULL;
			return -1;
		}
		res = 0;
	} else {
		gsm->ussd_info = ussd_info;
		module_send_ussd(gsm, ussd_info->message);
		res = 0;
	}

	return res;
}
#ifdef CSV_SMS
int allogsm_send_text_csv(struct allogsm_modul *gsm, char *destination, char *message, char *id) 
{
}
#endif
/******************************************************************************
 * send sms
 * param:
 *		gsm: gsm module
 *		destination: called number
 *		message: sms body
 * return:
 *		0: send sms ok
 *		-1: can not send sms
 * e.g.
 *		allogsm_send_text(gsm, "1000", "Hello World")
 ******************************************************************************/
#ifdef QUEUE_SMS
int allogsm_send_text(struct allogsm_modul *gsm, char *destination, unsigned char *message, char *id)
{
	int res = -1;
	sms_info_u *sms_info = NULL;	
	
	if (!gsm) {
		return res;
	}
	
	sms_info = malloc(sizeof(sms_txt_info_t));
	if (!sms_info) {
		gsm_error(gsm, "unable to malloc!\n");
		return res;
	}

	//id
	if(id) {
		strncpy(sms_info->pdu_info.id,id,sizeof(sms_info->pdu_info.id));
	} else {
		sms_info->pdu_info.id[0] = '\0';
	}
	
	sms_info->txt_info.gsm = gsm;
	strncpy(sms_info->txt_info.destination, destination, sizeof(sms_info->txt_info.destination));
	strncpy(sms_info->txt_info.message, message, sizeof(sms_info->txt_info.message));		
	
	if ((ALLOGSM_STATE_READY != gsm->state) ||
	    (gsm->dial_initiated == 1) ||
	    ((gsm->creg_state!=CREG_1_REGISTERED_HOME) &&
	    (gsm->creg_state!=CREG_5_REGISTERED_ROAMING))) {

		if (QueueIsEmpty(gsm->sms_queue)) {	
			/*sched here*/
		gsm_message(gsm, "here at enqueue %d..\n",__LINE__); //pawan print
			QueueEnter(gsm, *sms_info);
			if (gsm_schedule_check(gsm) < 0) {
				gsm_error(gsm, "No enough space for sending sms!\n");
				return -1;
			}
			int resendsmsidx = gsm_schedule_event(gsm, 1000, gsm_dequeue_sms_txt, (void *)gsm);
			sms_info->txt_info.resendsmsidx = resendsmsidx;
			if (resendsmsidx < 0 && sms_info) {
				gsm_error(gsm, "Can't schedule sending sms!\n");
				free(sms_info);
				sms_info = NULL;
				return -1;
			}
			res = 0;
		}else{
		gsm_message(gsm, "here at enqueue %d..\n",__LINE__); //pawan print
			QueueEnter(gsm, *sms_info);
		}
	}  else if (QueueIsEmpty(gsm->sms_queue)) {	
		gsm_message(gsm, "here at enqueue %d..\n",__LINE__); //pawan print
		gsm->sms_info = sms_info;
		__gsm_send_text(gsm, sms_info->txt_info.destination, sms_info->txt_info.message);
		res = 0;
	} else { //put in queue
		gsm_message(gsm, "here at enqueue %d..\n",__LINE__); //pawan print
		QueueEnter(gsm, *sms_info);
	}

	return res;
}
#else
int allogsm_send_text(struct allogsm_modul *gsm, char *destination, char *message, char *id) 
{
	int res = -1;
	sms_info_u *sms_info = NULL;	
	
	if (!gsm) {
		return res;
	}
	
	sms_info = malloc(sizeof(sms_txt_info_t));
	if (!sms_info) {
		gsm_error(gsm, "unable to malloc!\n");
		return res;
	}

	//id
	if(id) {
		strncpy(sms_info->pdu_info.id,id,sizeof(sms_info->pdu_info.id));
	} else {
		sms_info->pdu_info.id[0] = '\0';
	}
	
	sms_info->txt_info.gsm = gsm;
	strncpy(sms_info->txt_info.destination, destination, sizeof(sms_info->txt_info.destination));
	strncpy(sms_info->txt_info.message, message, sizeof(sms_info->txt_info.message));		
	
	if ((ALLOGSM_STATE_READY != gsm->state) ||
	    (gsm->dial_initiated == 1) ||
	    ((gsm->creg_state!=CREG_1_REGISTERED_HOME) &&
	    (gsm->creg_state!=CREG_5_REGISTERED_ROAMING))) {

		if (gsm_schedule_check(gsm) < 0) {
			gsm_error(gsm, "No enough space for sending sms!\n");
			return -1;
		}
		int resendsmsidx = gsm_schedule_event(gsm, 2000, gsm_resend_sms_txt, (void *)sms_info);
		sms_info->txt_info.resendsmsidx = resendsmsidx;
		if (resendsmsidx < 0 && sms_info) {
			gsm_error(gsm, "Can't schedule sending sms!\n");
			free(sms_info);
			sms_info = NULL;
			return -1;
		}
		res = 0;
	}  else {	
		gsm->sms_info = sms_info;
		__gsm_send_text(gsm, sms_info->txt_info.destination, sms_info->txt_info.message);
		res = 0;
	}

	return res;
}
#endif

static int __gsm_send_pdu(struct allogsm_modul *gsm, char *message)
{
	return module_send_pdu(gsm, message);
}

static void gsm_resend_sms_pdu(void *info)
{
	sms_info_u *sms_info = info;
	struct allogsm_modul *gsm	= sms_info->pdu_info.gsm;

	if ((ALLOGSM_STATE_READY != gsm->state) ||
	    (gsm->dial_initiated == 1) ||
	    ((gsm->creg_state!=CREG_1_REGISTERED_HOME) &&
	    (gsm->creg_state!=CREG_5_REGISTERED_ROAMING))) {
		int resendsmsidx = gsm_schedule_event(gsm, 2000, gsm_resend_sms_pdu, info);
		sms_info->pdu_info.resendsmsidx = resendsmsidx;
		//Freedom Add 2011-10-27 16:41 Release memory
		if (resendsmsidx < 0 && sms_info) {
			gsm_error(gsm, "Can't schedule sending sms!\n");
			free(sms_info);
			sms_info = NULL;
		}
	} else {
		gsm->sms_info = sms_info;
		__gsm_send_pdu(gsm, sms_info->pdu_info.message);
	}
}

/******************************************************************************
 * send pdu
 * param:
 *		gsm: gsm module
 *  	message: pdu body
 * return:
 *		0: send pdu ok
 *		-1: can not send pdu
 * e.g.
 *		allogsm_send_pdu(gsm, "0891683110808805F0040BA13140432789F300F1010112316435230BE8F71D14969741F9771D")
 ******************************************************************************/
int allogsm_send_pdu(struct allogsm_modul *gsm,  char *message, unsigned char *text, char *id) 
{
	int res = -1;
	sms_info_u *sms_info = NULL;	
	char smsc[3];
	int smsc_len;
	int len = 0;
	if (!gsm) {
		return res;
	}	
	
	sms_info = malloc(sizeof(sms_pdu_info_t));
	if (!sms_info) {
		gsm_error(gsm, "unable to malloc!\n");
		return res;
	}

	//id
	if(id) {
		strncpy(sms_info->pdu_info.id,id,sizeof(sms_info->pdu_info.id));
	} else {
		sms_info->pdu_info.id[0] = '\0';
	}

	//text
	if(text) {
		strncpy(sms_info->pdu_info.text,text,sizeof(sms_info->pdu_info.text));
	} else {
		sms_info->pdu_info.text[0] = '\0';
	}
	
	// gsm
	sms_info->pdu_info.gsm = gsm;

	// pdu body
	strncpy(sms_info->pdu_info.message, message, 1024);	

	// Destination
	pdu_get_send_number(message, sms_info->pdu_info.destination, sizeof(sms_info->pdu_info.destination));

	// SMSC information length
	len = strlen(message);
	strncpy(smsc, message, sizeof(smsc) - 1);
	smsc[2] = '\0';
	smsc_len = gsm_hex2int(smsc, 2);
	len = (len / 2) - 1 - smsc_len;
	sms_info->pdu_info.len = len;
	if (gsm->dial_initiated)
		gsm_message(gsm, "---------------here %d dial_initiated %d\n", __LINE__,gsm->dial_initiated);
	if ((ALLOGSM_STATE_READY != gsm->state) ||
	    (gsm->dial_initiated == 1) ||
	    ((gsm->creg_state!=CREG_1_REGISTERED_HOME) &&
	    (gsm->creg_state!=CREG_5_REGISTERED_ROAMING))) {
		if (gsm_schedule_check(gsm) < 0) {
			gsm_error(gsm, "No enough space for sending sms!\n");
			return -1;
		}
		// schedule index
		int resendsmsidx = gsm_schedule_event(gsm, 2000, gsm_resend_sms_pdu, (void *)sms_info);
		sms_info->pdu_info.resendsmsidx = resendsmsidx;
		if (resendsmsidx < 0 && sms_info) {
			gsm_error(gsm, "Can't schedule sending sms!\n");
			free(sms_info);
			sms_info = NULL;
			return -1;
		}
		res = 0;
	} else {
		gsm->sms_info = sms_info;
		__gsm_send_pdu(gsm, sms_info->pdu_info.message);
		res = 0;
	}

	return res;
}


/******************************************************************************
 * send pin
 * param:
 *		gsm: gsm module
 *
 * return:
 *
 * e.g.
 *		allogsm_send_pin(gsm, "1234")
 ******************************************************************************/
int allogsm_send_pin(struct allogsm_modul *gsm, char *pin)
{	
	if (!gsm) {
		return -1;
	}

	return module_send_pin(gsm, pin);
}

int gsm_san(struct allogsm_modul *gsm, char *in, char *out, int len) 
{
	int i=0;
	int skip=0;
	char *tmp = NULL;
	int skipEnd=0;

	if ((len <= 0) && (gsm->sanidx <= 0)){
		return 0;
	}

	int j;
	if (gsm->debug & ALLOGSM_DEBUG_AT_DUMP){
//	if (gsm->span==2){
	gsm_message(gsm, "--------SAN BUF b4 (gsm->sanidx: %d) (len: %d)-->\n", gsm->sanidx, len); //pawan san
		for (j = 0; j < gsm->sanidx ; j++) {
		if ( (gsm->sanbuf[j]<20)||(gsm->sanbuf[j]>127) )
			gsm_message(gsm, "0x%x", gsm->sanbuf[j]);
		else
			gsm_message(gsm, "%c", gsm->sanbuf[j]);
		}
		gsm_message(gsm, "\n<----\n");
	}

	if ((len > 0) && ((gsm->sanidx + len < sizeof(gsm->sanbuf)))) {
		memcpy(gsm->sanbuf + gsm->sanidx, in, len);
		gsm->sanidx += len;
		gsm->sanbuf[gsm->sanidx] = '\0';

	if (gsm->debug & ALLOGSM_DEBUG_AT_DUMP)
	//if (gsm->span==2)
		gsm_message(gsm, "--------herer %d (%d) (%d)-->\n", __LINE__ , gsm->sanidx, len); //pawan san 
	}

	while ((gsm->sanbuf[i] == '\r') || (gsm->sanbuf[i] == '\n')) {
		i++;
	}
	skip = i;
	gsm->sanskip = skip;
	tmp = (char *)memchr(gsm->sanbuf + skip, '\r', gsm->sanidx - skip);

	if (gsm->debug & ALLOGSM_DEBUG_AT_DUMP){
	//if (gsm->span==2){
	gsm_message(gsm, "--------SAN BUF(%d)-->\n", gsm->sanidx);
		for (j = 0; j < gsm->sanidx ; j++) {
		if ( (gsm->sanbuf[j]<20)||(gsm->sanbuf[j]>127) )
			gsm_message(gsm, "0x%x", gsm->sanbuf[j]);
		else
			gsm_message(gsm, "%c", gsm->sanbuf[j]);
		}
		gsm_message(gsm, "\n<----\n");
	}
	if (tmp){
		i = tmp - (gsm->sanbuf + skip);
		memcpy(out, (gsm->sanbuf + skip), i);
		out[i] = 0x0;

		if ((gsm->sanidx >= skip + i + 2 ) && (gsm->sanbuf[skip+i] == '\r') && (gsm->sanbuf[skip+i+1] == '\n'))
			skipEnd = 2;

		memmove(&gsm->sanbuf[0], &gsm->sanbuf[skip+i+skipEnd], sizeof(gsm->sanbuf) - i - skip - skipEnd);
		gsm->sanidx = gsm->sanidx - (i + skip + skipEnd);
/*
		if (gsm->span==2){
		gsm_message(gsm, "--------SAN BUF left(%d)-->\n", gsm->sanidx);
			for (j = 0; j < gsm->sanidx ; j++) {
			if ( (gsm->sanbuf[j]<20)||(gsm->sanbuf[j]>127) )
				gsm_message(gsm, "0x%x", gsm->sanbuf[j]);
			else
				gsm_message(gsm, "%c", gsm->sanbuf[j]);
			}
			gsm_message(gsm, "\n<----\n");
		}
*/
		return i;
	} else {
		if ((gsm->sanidx - skip == 2) && (gsm->sanbuf[gsm->sanidx - 2] == '>') && (gsm->sanbuf[gsm->sanidx - 1] == ' ')) {
			/* for sim340dz and m20 */
			out[0] = '>';
			out[1] = ' ';
			out[2] = 0x0;
			gsm->sanidx = 0;
			return 2;
		} else if ((gsm->sanidx - skip == 3) && (gsm->sanbuf[gsm->sanidx - 3] == '>') && (gsm->sanbuf[gsm->sanidx - 2] == ' ') && (gsm->sanbuf[gsm->sanidx - 1] == ' ')) {
			/* for Sierra wireless HL modem on S500 board */
			out[0] = '>';
			out[1] = ' ';
			out[2] = ' ';
			out[3] = 0x0;
			gsm->sanidx = 0;
			return 3;
		} else if ((gsm->sanidx - skip == 1) && (gsm->sanbuf[gsm->sanidx - 1] == '>')) {
			/* for em200 */
			out[0] = '>';
			out[1] = 0x0;
			gsm->sanidx = 0;
			return 1;
		} else if (0x0 == gsm->sanbuf[skip]) {
			/* gsm->sanbuf = "\r\n" */
			if (gsm->sanskip == 4 && gsm_compare(gsm->at_last_recv, "+CMT: \""))
			{
				out[0] = '\0';
				return -3;
			}
			if (gsm->sanskip == gsm->sanidx)
				gsm->sanidx=0;
			gsm->sanskip = 0;
			return 0;
		} else if (gsm->sanskip == gsm->sanidx) {
			gsm->sanidx = 0;
			gsm->sanskip = 0;
			return 0;
		} else {
			i = gsm->sanidx - skip;
			memcpy(out, (gsm->sanbuf + skip), i);
			out[i] = 0x0;
			return -1;
		}
	}

	return 0;
}

/*Makes Add 2012-4-9 14:01*/

#ifdef CONFIG_CHECK_PHONE

void allogsm_hangup_phone(struct allogsm_modul *gsm)
{
	module_hangup_phone(gsm);
}
void allogsm_set_check_phone_mode(struct allogsm_modul *gsm,int mode)
{
	gsm->check_mode=mode;
}

int allogsm_check_phone_stat(struct allogsm_modul *gsm, char *phone_number,int hangup_flag,unsigned int timeout)
{
	return module_check_phone_stat(gsm, phone_number,hangup_flag,timeout);
}
#endif //CONFIG_CHECK_PHONE

#ifdef VIRTUAL_TTY
int allogsm_get_mux_command(struct allogsm_modul *gsm,char *command)
{
	int ret=0;
	const char *string=get_at(gsm->switchtype,AT_CMUX);
	if(string)
	{
		strcpy(command,string);
	}
	else
		ret=-1;
	return ret;
}

int allogsm_mux_end(struct allogsm_modul *gsm, int restart_at_flow)
{
	return module_mux_end(gsm,restart_at_flow);
}
#endif //VIRTUAL_TTY

char *allogsm_state2str(int state)
{
	switch(state) {
		case ALLOGSM_STATE_DOWN:
			return "ALLOGSM STATE DOWN";
			break;
		case ALLOGSM_STATE_INIT:
			return "ALLOGSM STATE INIT";
			break;
		case ALLOGSM_STATE_UP:
			return "ALLOGSM STATE UP";
			break;
		case ALLOGSM_STATE_SEND_HANGUP:
			return "ALLOGSM STATE SEND HANGUP";
			break;
#ifdef WAVECOM
		case ALLOGSM_STATE_SET_GEN_INDICATION_OFF:
			return "ALLOGSM STATE SET GEN INDICATION OFF";
			break;
		case ALLOGSM_STATE_SET_SIM_INDICATION_OFF:
			return "ALLOGSM STATE SET SIM INDICATION OFF";
			break;
		case ALLOGSM_STATE_SET_CREG_INDICATION_OFF:
			return "ALLOGSM STATE SET CREG INDICATION OFF";
			break;
#endif
		case ALLOGSM_STATE_SET_ECHO:
			return "ALLOGSM STATE SET ECHO";
			break;
		case ALLOGSM_STATE_SET_REPORT_ERROR:
			return "ALLOGSM STATE SET REPORT ERROR";
			break;
		case ALLOGSM_STATE_MODEL_NAME_REQ:
			return "ALLOGSM STATE MODEL NAME REQ";
			break;
		case ALLOGSM_STATE_MANUFACTURER_REQ:
			return "ALLOGSM STATE MANUFACTURER REQ";
			break;
		case ALLOGSM_STATE_GET_SMSC_REQ:
			return "ALLOGSM STATE GET SMSC REQ";
			break;
		case ALLOGSM_STATE_VERSION_REQ:
			return "ALLOGSM STATE VERSION REQ";
			break;
		case ALLOGSM_STATE_GSN_REQ:
			return "ALLOGSM STATE GSN REQ";
			break;
		case ALLOGSM_STATE_IMEI_REQ:
			return "ALLOGSM STATE IMEI REQ";
			break;
		case ALLOGSM_STATE_IMSI_REQ:
			return "ALLOGSM STATE IMSI REQ";
			break;
		case ALLOGSM_STATE_INIT_0:
			return "ALLOGSM STATE INIT 0";
			break;
		case ALLOGSM_STATE_INIT_1:
			return "ALLOGSM STATE INIT 1";
			break;
		case ALLOGSM_STATE_INIT_2:
			return "ALLOGSM STATE INIT 2";
			break;
		case ALLOGSM_STATE_INIT_3:
			return "ALLOGSM STATE INIT 3";
			break;
		case ALLOGSM_STATE_INIT_4:
			return "ALLOGSM STATE INIT 4";
			break;
		case ALLOGSM_STATE_INIT_5:
			return "ALLOGSM STATE INIT 5";
			break;
		case ALLOGSM_STATE_SIM_READY_REQ:
			return "ALLOGSM STATE SIM READY REQ";
			break;
		case ALLOGSM_STATE_SIM_PIN_REQ:
			return "ALLOGSM STATE SIM PIN REQ";
			break;
		case ALLOGSM_STATE_SIM_PUK_REQ:
			return "ALLOGSM STATE SIM PUK REQ";
			break;
		case ALLOGSM_STATE_SIM_READY:
			return "ALLOGSM STATE SIM READY";
			break;
		case ALLOGSM_STATE_UIM_READY_REQ:
			return "ALLOGSM STATE UIM READY REQ";
			break;
		case ALLOGSM_STATE_UIM_PIN_REQ:
			return "ALLOGSM STATE UIM PIN REQ";
			break;
		case ALLOGSM_STATE_UIM_PUK_REQ:
			return "ALLOGSM STATE UIM PUK REQ";
			break;
		case ALLOGSM_STATE_UIM_READY:
			return "ALLOGSM STATE UIM READY";
			break;
#ifdef VIRTUAL_TTY
		case ALLOGSM_INIT_MUX:
			return "ALLOGSM INIT MUX";
			break;
#endif //VIRTUAL_TTY
		case ALLOGSM_STATE_MOC_STATE_ENABLED:
			return "ALLOGSM STATE MOC STATE ENABLED";
			break;
		case ALLOGSM_STATE_SET_SIDE_TONE:
			return "ALLOGSM STATE SET SIDE TONE";
			break;
		case ALLOGSM_STATE_CLIP_ENABLED:
			return "ALLOGSM STATE CLIP ENABLED";
			break;
		case ALLOGSM_STATE_RSSI_ENABLED:
			return "ALLOGSM STATE RSSI ENABLED";
			break;
		case ALLOGSM_STATE_SMS_MODE:
			return "ALLOGSM STATE SMS MODE";
			break;
		case ALLOGSM_STATE_SET_NET_URC:
			return "ALLOGSM STATE SET NET URC";
			break;
		case ALLOGSM_STATE_NET_REQ:
			return "ALLOGSM STATE NET REQ";
			break;
		case ALLOGSM_STATE_NET_OK:
			return "ALLOGSM STATE NET OK";
			break;
		case ALLOGSM_AT_MODE:
			return "ALLOGSM STATE AT MODE";
			break;
		case ALLOGSM_STATE_NET_NAME_REQ:
			return "ALLOGSM STATE NET NAME REQ";
			break;
		case ALLOGSM_STATE_READY:
			return "ALLOGSM STATE READY";
			break;
		case ALLOGSM_STATE_CALL_INIT:
			return "ALLOGSM STATE CALL INIT";
			break;
		case ALLOGSM_STATE_CALL_MADE:
			return "ALLOGSM STATE CALL MADE";
			break;
		case ALLOGSM_STATE_CALL_PRESENT:
			return "ALLOGSM STATE CALL PRESENT";
			break;
		case ALLOGSM_STATE_CALL_PROCEEDING:
			return "ALLOGSM STATE CALL PROCEEDING";
			break;
		case ALLOGSM_STATE_CALL_PROGRESS:
			return "ALLOGSM STATE CALL PROGRESS";
			break;
		case ALLOGSM_STATE_PRE_ANSWER:
			return "ALLOGSM STATE PRE ANSWER";
			break;
		case ALLOGSM_STATE_CALL_ACTIVE_REQ:
			return "ALLOGSM STATE CALL ACTIVE REQ";
			break;
		case ALLOGSM_STATE_CALL_ACTIVE:
			return "ALLOGSM STATE CALL ACTIVE";
			break;
		case ALLOGSM_STATE_CLIP:
			return "ALLOGSM STATE CLIP";
			break;
		case ALLOGSM_STATE_RING:
			return "ALLOGSM STATE RING";
			break;
		case ALLOGSM_STATE_RINGING:
			return "ALLOGSM STATE RINGING";
			break;
		case ALLOGSM_STATE_HANGUP_REQ:
			return "ALLOGSM STATE HANGUP REQ";
			break;
		case ALLOGSM_STATE_HANGUP_ACQ:
			return "ALLOGSM STATE HANGUP ACQ";
			break;
		case ALLOGSM_STATE_HANGUP:
			return "ALLOGSM STATE HANGUP";
			break;
		case ALLOGSM_STATE_SMS_SET_CHARSET:
			return "ALLOGSM STATE SMS SET CHARSET";
			break;
		case ALLOGSM_STATE_SMS_SET_INDICATION:
			return "ALLOGSM STATE SMS SET INDICATION";
			break;
		case ALLOGSM_STATE_SET_SPEEK_VOL:
			return "ALLOGSM STATE SET SPEEK VOL";
			break;
		case ALLOGSM_STATE_SET_MIC_VOL:
			return "ALLOGSM STATE SET MIC VOL";
			break;
		case ALLOGSM_STATE_SET_CALL_NOTIFICATION:
			return "ALLOGSM STATE SET CALL NOTIFICATION";
			break;
		case ALLOGSM_STATE_SMS_SET_UNICODE:
			return "ALLOGSM STATE SMS SET UNICODE";
			break;
		case ALLOGSM_STATE_SMS_SENDING:
			return "ALLOGSM STATE SMS SENDING";
			break;
		case ALLOGSM_STATE_SMS_SENT:
			return "ALLOGSM STATE SMS SENT";
			break;
		case ALLOGSM_STATE_SMS_SENT_END:
			return "ALLOGSM STATE SMS SENT END";
			break;
		case ALLOGSM_STATE_SMS_RECEIVED:
			return "ALLOGSM STATE SMS RECEIVED";
			break;
		case ALLOGSM_STATE_USSD_SENDING:
			return "ALLOGSM STATE USSD SENDING";
			break;
		case ALLOGSM_STATE_OPERATOR_QUERY:
			return "ALLOGSM STATE OPERATOR QUERY";
			break;
		case ALLOGSM_STATE_SAFE_AT:
			return "ALLOGSM STATE SAFE AT";
			break;
		case ALLOGSM_STATE_UPDATE_1:
			return "ALLOGSM STATE UPDATE 1";
			break;
		case ALLOGSM_STATE_UPDATE_2:
			return "ALLOGSM STATE UPDATE 2";
			break;
		case ALLOGSM_STATE_UPDATE_SUCCESS:
			return "ALLOGSM STATE UPDATE SUCCESS";
			break;
		case ALLOGSM_STATE_SET_SIM_SELECT_1:
			return "ALLOGSM STATE SET SIM SELECT 1";
			break;
		case ALLOGSM_STATE_SET_SIM_SELECT_2:
			return "ALLOGSM STATE SET SIM SELECT 2";
			break;
		case ALLOGSM_STATE_SET_SIM_SELECT_3:
			return "ALLOGSM STATE SET SIM SELECT 3";
			break;
		case ALLOGSM_STATE_CALL_WAITING:
			return "ALLOGSM STATE CALL WAITING";
			break;
		case ALLOGSM_STATE_HANGUP_REQ_CALL_WAITING:
			return "ALLOGSM STATE HANGUP REQ CALL WAITING";
			break;
		case ALLOGSM_STATE_DEL_SIM_MSG:
			return "ALLOGSM STATE DEL SIM MSG";
			break;
		case ALLOGSM_STATE_SET_NOISE_CANCEL:
			return "ALLOGSM STATE SET NOISE CANCEL";
			break;
#ifdef CONFIG_CHECK_PHONE
		case ALLOGSM_STATE_PHONE_CHECK:
			return "ALLOGSM STATE PHONE CHECK";
			break;
#endif
	}
	return "ALLOGSM STATE UNKNOW";
}


