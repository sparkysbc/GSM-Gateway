/*
 * liballogsmat: An implementation of ALLO GSM cards
 *
 * Parts taken from libpri
 * Written by mark.liu <mark.liu@openvox.cn>
 *
 * $Id: liballogsmat.h 294 2011-03-08 07:50:07Z liuyuan $
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 *
 */

 
#ifndef _LIBALLOGSMAT_H
#define _LIBALLOGSMAT_H




#include <sys/time.h>

#define NEED_CHECK_PHONE 1
#if (NEED_CHECK_PHONE>0)
#define CONFIG_CHECK_PHONE
#define DEFAULT_CHECK_TIMEOUT 20
#endif

#define ALLOGSM_MAX_PHONE_NUMBER 64
#define ALLOGSM_MAX_SMS_LENGTH 2048              //codec byte size, this is a assume value
#define ALLOGSM_MAX_PDU_LENGTH 2048

/* No more than 128 scheduled events */
#define ALLO_MAX_SCHED	10000//128

#define MAX_TIMERS	32

/* Node types */
#define ALLOGSM_NETWORK		1
#define ALLOGSM_CPE			2

/* Debugging */
#define ALLOGSM_DEBUG_Q921_RAW		(1 << 0)	/* 0x0000 Show raw HDLC frames */
#define ALLOGSM_DEBUG_Q921_DUMP		(1 << 1)	/* 0x0002 Show each interpreted Q.921 frame */
#define ALLOGSM_DEBUG_Q921_STATE	(1 << 2)	/* 0x0004 ebug state machine changes */
#define ALLOGSM_DEBUG_CONFIG		(1 << 3) 	/* 0x0008 Display error events on stdout */
#define ALLOGSM_DEBUG_AT_DUMP		(1 << 5)	/* 0x0020 Show AT Command */
#define ALLOGSM_DEBUG_AT_STATE		(1 << 6)	/* 0x0040 Debug AT Command state changes */
#define ALLOGSM_DEBUG_AT_ANOMALY	(1 << 7)	/* 0x0080 Show unexpected events */
#define ALLOGSM_DEBUG_APDU			(1 << 8)	/* 0x0100 Debug of APDU components such as ROSE */
#define ALLOGSM_DEBUG_AOC			(1 << 9)	/* 0x0200 Debug of Advice of Charge ROSE Messages */
#define ALLOGSM_DEBUG_AT_RECEIVED	(1 << 10)	/* 0x0400 Debug of received AT Command */
#define ALLOGSM_DEBUG_SMS			(1 << 11)	/* 0x0800 Debug of received AT Command */

#define ALLOGSM_DEBUG_ALL			(0xffff)	/* Everything */

/* Switch types */
#define ALLOGSM_SWITCH_UNKNOWN		0
#define ALLOGSM_SWITCH_E169			1
#define ALLOGSM_SWITCH_SIMCOM		2
#define ALLOGSM_SWITCH_SIM340DZ		ALLOGSM_SWITCH_SIMCOM
#define ALLOGSM_SWITCH_EM200		3
#define ALLOGSM_SWITCH_M20			4
#define ALLOGSM_SWITCH_SIM900		5
#define ALLOGSM_SWITCH_SIERRA_Q2687RD 	7

#define QUEUE_SMS 1
#define PDU_LONG
/* EXTEND D-Channel Events */
enum EVENT_DEFINE {
	ALLOGSM_EVENT_DCHAN_UP = 1,		/* D-channel is up */
	ALLOGSM_EVENT_DETECT_MODULE_OK,
	ALLOGSM_EVENT_DCHAN_DOWN, 		/* D-channel is down */
	ALLOGSM_EVENT_RESTART,			/* B-channel is restarted */
	ALLOGSM_EVENT_CONFIG_ERR,		/* Configuration Error Detected */
	ALLOGSM_EVENT_CALL_WAITING,		/* Waiting Call*/
	ALLOGSM_EVENT_RING,			/* Incoming call (SETUP) */
	ALLOGSM_EVENT_HANGUP,			/* Call got hung up (RELEASE/RELEASE_COMPLETE/other) */
	ALLOGSM_EVENT_RINGING,			/* Call is ringing (ALERTING) */
	ALLOGSM_EVENT_ANSWER,			/* Call has been answered (CONNECT) */
	ALLOGSM_EVENT_HANGUP_ACK,		/* Call hangup has been acknowledged */
	ALLOGSM_EVENT_RESTART_ACK,		/* Restart complete on a given channel (RESTART_ACKNOWLEDGE) */
	ALLOGSM_EVENT_FACNAME,			/* Caller*ID Name received on Facility */
	ALLOGSM_EVENT_INFO_RECEIVED,	/* Additional info (digits) received (INFORMATION) */
	ALLOGSM_EVENT_PROCEEDING,		/* When we get CALL_PROCEEDING */
	ALLOGSM_EVENT_SETUP_ACK,		/* When we get SETUP_ACKNOWLEDGE */
	ALLOGSM_EVENT_HANGUP_REQ,		/* Requesting the higher layer to hangup (DISCONNECT) */
	ALLOGSM_EVENT_NOTIFY,			/* Notification received (NOTIFY) */
	ALLOGSM_EVENT_PROGRESS,			/* When we get PROGRESS */
	ALLOGSM_EVENT_KEYPAD_DIGIT,		/* When we receive during ACTIVE state (INFORMATION) */
	ALLOGSM_EVENT_SMS_RECEIVED,		/* SMS event */
	ALLOGSM_EVENT_SIM_FAILED,		/* SIM  not inserted */
	ALLOGSM_EVENT_PIN_REQUIRED,		/* SIM Pin required */
	ALLOGSM_EVENT_PIN_ERROR,		/* SIM Pin error */
	ALLOGSM_EVENT_SMS_SEND_OK,	
	ALLOGSM_EVENT_SMS_SEND_FAILED,
	ALLOGSM_EVENT_USSD_RECEIVED,
	ALLOGSM_EVENT_USSD_SEND_FAILED,
	ALLOGSM_EVENT_OPERATOR_LIST_RECEIVED,
	ALLOGSM_EVENT_OPERATOR_LIST_FAILED,
	ALLOGSM_EVENT_SAFE_AT_RECEIVED,
	ALLOGSM_EVENT_SAFE_AT_FAILED,
	ALLOGSM_EVENT_NO_SIGNAL,
#ifdef CONFIG_CHECK_PHONE
	ALLOGSM_EVENT_CHECK_PHONE,		/*Check phone stat*/
#endif //CONFIG_CHECK_PHONE
#ifdef VIRTUAL_TTY
	ALLOGSM_EVENT_INIT_MUX,
#endif //VIRTUAL_TTY
};

#define WAVECOM 1
/* Simple states */
enum STATE_DEFINE {
	ALLOGSM_STATE_DOWN = 0,
	ALLOGSM_STATE_INIT,
	ALLOGSM_STATE_UP,

//Freedom Add 2011-10-14 09:20 Fix up sim900d bug,must send "ATH" after "ATZ"
//////////////////////////////////////////////////////////////////////////////////
	ALLOGSM_STATE_SEND_HANGUP,
//////////////////////////////////////////////////////////////////////////////////
#ifdef WAVECOM
	ALLOGSM_STATE_SET_GEN_INDICATION_OFF,
	ALLOGSM_STATE_SET_SIM_INDICATION_OFF,
	ALLOGSM_STATE_SET_CREG_INDICATION_OFF,
	ALLOGSM_STATE_UPDATE_1,
	ALLOGSM_STATE_UPDATE_2,
	ALLOGSM_STATE_UPDATE_SUCCESS,
#endif
	ALLOGSM_STATE_SET_ECHO,
	ALLOGSM_STATE_SET_REPORT_ERROR,
	ALLOGSM_STATE_MODEL_NAME_REQ,
	ALLOGSM_STATE_MANUFACTURER_REQ,
	ALLOGSM_STATE_VERSION_REQ,
/*** SIM Selection****************/
	ALLOGSM_STATE_SET_SIM_SELECT_1,
	ALLOGSM_STATE_SET_SIM_SELECT_2,
	ALLOGSM_STATE_SET_SIM_SELECT_3,
/********************************/
	ALLOGSM_STATE_GSN_REQ,
	ALLOGSM_STATE_IMEI_REQ,
	ALLOGSM_STATE_IMSI_REQ,
	ALLOGSM_STATE_INIT_0,
	ALLOGSM_STATE_INIT_1,
	ALLOGSM_STATE_INIT_2,
	ALLOGSM_STATE_INIT_3,
	ALLOGSM_STATE_INIT_4,
	ALLOGSM_STATE_INIT_5,
	ALLOGSM_STATE_SIM_READY_REQ,/* for sim card */
	ALLOGSM_STATE_SIM_PIN_REQ,/* for sim card */
	ALLOGSM_STATE_SIM_PUK_REQ,/* for sim card */
	ALLOGSM_STATE_SIM_READY,/* for sim card */
	ALLOGSM_STATE_UIM_READY_REQ,/* for uim card */
	ALLOGSM_STATE_UIM_PIN_REQ,/* for uim card */
	ALLOGSM_STATE_UIM_PUK_REQ,/* for uim card */
	ALLOGSM_STATE_UIM_READY,/* for uim card */

#ifdef VIRTUAL_TTY
	ALLOGSM_INIT_MUX,
#endif //VIRTUAL_TTY

	ALLOGSM_STATE_MOC_STATE_ENABLED,
	ALLOGSM_STATE_SET_SIDE_TONE,
	ALLOGSM_STATE_SET_NOISE_CANCEL,
	ALLOGSM_STATE_DEL_SIM_MSG,
	ALLOGSM_STATE_CLIP_ENABLED,
	ALLOGSM_STATE_RSSI_ENABLED,
	ALLOGSM_STATE_SMS_MODE,
	ALLOGSM_STATE_SET_NET_URC,
	ALLOGSM_STATE_NET_REQ,
	ALLOGSM_STATE_NET_OK,
	ALLOGSM_AT_MODE,
	ALLOGSM_STATE_NET_NAME_REQ,
	ALLOGSM_STATE_READY,
	ALLOGSM_STATE_CALL_INIT,
	ALLOGSM_STATE_CALL_MADE,
	ALLOGSM_STATE_CALL_PRESENT,
	ALLOGSM_STATE_CALL_PROCEEDING,
	ALLOGSM_STATE_CALL_PROGRESS,
	ALLOGSM_STATE_PRE_ANSWER,
	ALLOGSM_STATE_CALL_ACTIVE_REQ,
	ALLOGSM_STATE_CALL_ACTIVE,
	ALLOGSM_STATE_CLIP,
	ALLOGSM_STATE_RING,
	ALLOGSM_STATE_RINGING,
	ALLOGSM_STATE_CALL_WAITING,
	ALLOGSM_STATE_HANGUP_REQ,
	ALLOGSM_STATE_HANGUP_REQ_CALL_WAITING,
	ALLOGSM_STATE_HANGUP_ACQ,
	ALLOGSM_STATE_HANGUP,
	ALLOGSM_STATE_GET_SMSC_REQ,
	ALLOGSM_STATE_SMS_SET_CHARSET,
	ALLOGSM_STATE_SMS_SET_INDICATION,
	ALLOGSM_STATE_SMS_SET_SMSC,
	ALLOGSM_STATE_SET_SPEEK_VOL,
	ALLOGSM_STATE_SET_MIC_VOL,
	ALLOGSM_STATE_SET_SPEAKER,
	ALLOGSM_STATE_SET_GAIN_INDEX,
	ALLOGSM_STATE_SET_ECHOCANSUP,
	ALLOGSM_STATE_SET_DTMF_DETECTION,
	ALLOGSM_STATE_SET_CALL_NOTIFICATION,
	ALLOGSM_STATE_SMS_SET_UNICODE,
	ALLOGSM_STATE_SMS_SENDING,
	ALLOGSM_STATE_SMS_SENT,
	ALLOGSM_STATE_SMS_SENT_END,
	ALLOGSM_STATE_SMS_RECEIVED,
	ALLOGSM_STATE_USSD_SENDING,
	ALLOGSM_STATE_OPERATOR_QUERY,
	ALLOGSM_STATE_SAFE_AT,
	ALLOGSM_STATE_CHECK_BAUD_RATE,

#ifdef CONFIG_CHECK_PHONE
	ALLOGSM_STATE_PHONE_CHECK,
#endif
};

#define ALLOGSM_PROGRESS_MASK		0

/* Progress indicator values */
#define ALLOGSM_PROG_CALL_NOT_E2E_ISDN						(1 << 0)
#define ALLOGSM_PROG_CALLED_NOT_ISDN						(1 << 1)
#define ALLOGSM_PROG_CALLER_NOT_ISDN						(1 << 2)
#define ALLOGSM_PROG_INBAND_AVAILABLE						(1 << 3)
#define ALLOGSM_PROG_DELAY_AT_INTERF						(1 << 4)
#define ALLOGSM_PROG_INTERWORKING_WITH_PUBLIC				(1 << 5)
#define ALLOGSM_PROG_INTERWORKING_NO_RELEASE				(1 << 6)
#define ALLOGSM_PROG_INTERWORKING_NO_RELEASE_PRE_ANSWER		(1 << 7)
#define ALLOGSM_PROG_INTERWORKING_NO_RELEASE_POST_ANSWER	(1 << 8)
#define ALLOGSM_PROG_CALLER_RETURNED_TO_ISDN				(1 << 9)


/* Causes for disconnection */
#define ALLOGSM_CAUSE_UNALLOCATED					1
#define ALLOGSM_CAUSE_NO_ROUTE_TRANSIT_NET			2	/* !Q.SIG */
#define ALLOGSM_CAUSE_NO_ROUTE_DESTINATION			3
#define ALLOGSM_CAUSE_CHANNEL_UNACCEPTABLE			6
#define ALLOGSM_CAUSE_CALL_AWARDED_DELIVERED		7	/* !Q.SIG */
#define ALLOGSM_CAUSE_NORMAL_CLEARING				16
#define ALLOGSM_CAUSE_USER_BUSY						17
#define ALLOGSM_CAUSE_NO_USER_RESPONSE				18
#define ALLOGSM_CAUSE_NO_ANSWER						19
#define ALLOGSM_CAUSE_CALL_REJECTED					21
#define ALLOGSM_CAUSE_NUMBER_CHANGED				22
#define ALLOGSM_CAUSE_DESTINATION_OUT_OF_ORDER		27
#define ALLOGSM_CAUSE_INVALID_NUMBER_FORMAT			28
#define ALLOGSM_CAUSE_FACILITY_REJECTED				29	/* !Q.SIG */
#define ALLOGSM_CAUSE_RESPONSE_TO_STATUS_ENQUIRY	30
#define ALLOGSM_CAUSE_NORMAL_UNSPECIFIED			31
#define ALLOGSM_CAUSE_NORMAL_CIRCUIT_CONGESTION		34
#define ALLOGSM_CAUSE_NETWORK_OUT_OF_ORDER			38	/* !Q.SIG */
#define ALLOGSM_CAUSE_NORMAL_TEMPORARY_FAILURE		41
#define ALLOGSM_CAUSE_SWITCH_CONGESTION				42	/* !Q.SIG */
#define ALLOGSM_CAUSE_ACCESS_INFO_DISCARDED			43	/* !Q.SIG */
#define ALLOGSM_CAUSE_REQUESTED_CHAN_UNAVAIL		44
#define ALLOGSM_CAUSE_PRE_EMPTED					45	/* !Q.SIG */
#define ALLOGSM_CAUSE_FACILITY_NOT_SUBSCRIBED		50	/* !Q.SIG */
#define ALLOGSM_CAUSE_OUTGOING_CALL_BARRED			52	/* !Q.SIG */
#define ALLOGSM_CAUSE_INCOMING_CALL_BARRED			54	/* !Q.SIG */
#define ALLOGSM_CAUSE_BEARERCAPABILITY_NOTAUTH		57
#define ALLOGSM_CAUSE_BEARERCAPABILITY_NOTAVAIL		58
#define ALLOGSM_CAUSE_SERVICEOROPTION_NOTAVAIL		63	/* Q.SIG */
#define ALLOGSM_CAUSE_BEARERCAPABILITY_NOTIMPL		65
#define ALLOGSM_CAUSE_CHAN_NOT_IMPLEMENTED			66	/* !Q.SIG */
#define ALLOGSM_CAUSE_FACILITY_NOT_IMPLEMENTED		69	/* !Q.SIG */
#define ALLOGSM_CAUSE_INVALID_CALL_REFERENCE		81
#define ALLOGSM_CAUSE_IDENTIFIED_CHANNEL_NOTEXIST	82	/* Q.SIG */
#define ALLOGSM_CAUSE_INCOMPATIBLE_DESTINATION		88
#define ALLOGSM_CAUSE_INVALID_MSG_UNSPECIFIED		95	/* !Q.SIG */
#define ALLOGSM_CAUSE_MANDATORY_IE_MISSING			96
#define ALLOGSM_CAUSE_MESSAGE_TYPE_NONEXIST			97
#define ALLOGSM_CAUSE_WRONG_MESSAGE					98
#define ALLOGSM_CAUSE_IE_NONEXIST					99
#define ALLOGSM_CAUSE_INVALID_IE_CONTENTS			100
#define ALLOGSM_CAUSE_WRONG_CALL_STATE				101
#define ALLOGSM_CAUSE_RECOVERY_ON_TIMER_EXPIRE		102
#define ALLOGSM_CAUSE_MANDATORY_IE_LENGTH_ERROR		103	/* !Q.SIG */
#define ALLOGSM_CAUSE_PROTOCOL_ERROR				111
#define ALLOGSM_CAUSE_INTERWORKING					127	/* !Q.SIG */

/* Transmit capabilities */
#define ALLOGSM_TRANS_CAP_SPEECH					0x0

#define ALLOGSM_LAYER_1_ULAW			0x22
#define ALLOGSM_LAYER_1_ALAW			0x23

/* WIND Responses */
#define WIND_0_SIM_REMOVED						(1 << 0)
#define WIND_1_SIM_INSERTED						(1 << 1)
#define WIND_3_READY_BASIC_AT_COMMANDS					(1 << 3)
#define WIND_4_READY_ALL_AT_COMMANDS					(1 << 4)
#define WIND_7_READY_EMERGENCY_CALL 					(1 << 7)

enum sms_mode {
	SMS_UNKNOWN,
	SMS_PDU,
	SMS_TEXT
};

#ifdef CONFIG_CHECK_PHONE
enum phone_stat {
	SPAN_USING,
	PHONE_CONNECT,
	PHONE_RING,
	PHONE_BUSY,
	PHONE_POWEROFF,
	PHONE_NOT_CARRIER,
	PHONE_NOT_ANSWER,
	PHONE_NOT_DIALTONE,
	PHONE_TIMEOUT
};
#endif

/* alloat_call datastructure */

struct alloat_call {
	struct allogsm_modul *gsm;	/* GSM */
	int cr;				/* Call Reference */
	struct alloat_call *next;
	int channelno; /* An explicit channel */
	int chanflags; /* Channel flags (0 means none retrieved) */
	
	int alive;			/* Whether or not the call is alive */
	int acked;			/* Whether setup has been acked or not */
	int sendhangupack;	/* Whether or not to send a hangup ack */
	int proc;			/* Whether we've sent a call proceeding / alerting */
	
	/* Bearer Capability */
	int userl1;
	int userl2;
	int userl3;
	
	int sentchannel;

	int progcode;			/* Progress coding */
	int progloc;			/* Progress Location */	
	int progress;			/* Progress indicator */
	int progressmask;		/* Progress Indicator bitmask */
	
	int causecode;			/* Cause Coding */
	int causeloc;			/* Cause Location */
	int cause;			/* Cause of clearing */
	
	int peercallstate;		/* Call state of peer as reported */
	int ourcallstate;		/* Our call state */
	int sugcallstate;		/* Status call state */
	
	char callernum[256];
	char callername[256];

	char keypad_digits[64];		/* Buffer for digits that come in KEYPAD_FACILITY */

	char callednum[256];		/* Called Number */
	int complete;			/* no more digits coming */
	int newcall;			/* if the received message has a new call reference value */
	int ring_count;			/* Count the Number of RING received from GSM module unless event is not sent to channel driver */

	int t308_timedout;		/* Whether t308 timed out once */
	int call_waiting_idx;		/* Index of waiting call */
	
	long aoc_units;			/* Advice of Charge Units */
	int already_hangup;             /* If call is already hangedup, flag this flag */
};

typedef struct gsm_event_generic {
	/* Events with no additional information fall in this category */
	int e;
} gsm_event_generic;

typedef struct gsm_event_error {
	int e;
	char err[256];
} gsm_event_error;

typedef struct gsm_event_restart {
	int e;
	int channel;
} gsm_event_restart;

typedef struct gsm_event_ringing {
	int e;
	int channel;
	int cref;
	int progress;
	int progressmask;
	struct alloat_call *call;
} gsm_event_ringing;

typedef struct gsm_event_answer {
	int e;
	int channel;
	int cref;
	int progress;
	int progressmask;
	struct alloat_call *call;
} gsm_event_answer;

typedef struct gsm_event_facname {
	int e;
	char callingname[256];
	char callingnum[256];
	int channel;
	int cref;
	struct alloat_call *call;
} gsm_event_facname;

#define GSM_CALLINGPLANANI
#define GSM_CALLINGPLANRDNIS
typedef struct gsm_event_ring {
	int e;
	int channel;				/* Channel requested */
	char callingnum[256];		/* Calling number */
	char callingname[256];		/* Calling name (if provided) */
	char callednum[256];		/* Called number */
	int flexible;				/* Are we flexible with our channel selection? */
	int cref;					/* Call Reference Number */
	int layer1;					/* User layer 1 */
	int complete;				/* Have we seen "Complete" i.e. no more number? */
	struct alloat_call *call;				/* Opaque call pointer */
	int progress;
	int progressmask;
} gsm_event_ring;

typedef struct gsm_event_hangup {
	int e;
	int channel;				/* Channel requested */
	int cause;
	int cref;
	struct alloat_call *call;				/* Opaque call pointer */
	long aoc_units;				/* Advise of Charge number of charged units */
	int isCallWaitingHangup;
} gsm_event_hangup;	

typedef struct gsm_event_restart_ack {
	int e;
	int channel;
} gsm_event_restart_ack;

#define GSM_PROGRESS_CAUSE
typedef struct gsm_event_proceeding {
	int e;
	int channel;
	int cref;
	int progress;
	int progressmask;
	int cause;
	struct alloat_call *call;
} gsm_event_proceeding;
 
typedef struct gsm_event_setup_ack {
	int e;
	int channel;
	struct alloat_call *call;
} gsm_event_setup_ack;

typedef struct gsm_event_notify {
	int e;
	int channel;
	int info;
} gsm_event_notify;

typedef struct gsm_event_keypad_digit {
	int e;
	int channel;
	struct alloat_call *call;
	char digits[64];
	int duration;
} gsm_event_keypad_digit;

typedef struct gsm_event_sms_received {
    int e;
	enum sms_mode mode;		/* sms mode */
    char pdu[512];          /* short message in PDU format */
    char sender[255];
    char smsc[255];
	char time[32];			/* receive sms time */
	char tz[16];			/* receive sms time zone*/
    int len;				/* sms body length */
    char text[1024];			/* short message in text format */
} gsm_event_sms_received;  

typedef struct gsm_event_ussd_received {
    int e;
    unsigned char ussd_stat;
    unsigned char ussd_coding;
    int len;
    char text[1024];
} gsm_event_ussd_received; 


typedef struct gsm_event_operator_list_received {
    int e;
    int count;
    int stat[20];
    char long_operator_name[20][50];
    char short_operator_name[20][50];
    int num_operator[20];
} gsm_event_operator_list_received; 

//*********added for callforwarding***********

typedef struct gsm_event_callforward{
    int e;
    char number[64];
}gsm_event_callforward;



typedef struct gsm_sms_pdu{
	int total_parts;
	int part_num;	
	unsigned char message_split[16][256];
}gsm_sms_pdu;
typedef union {
	int e;
	gsm_event_generic 	gen;		/* Generic view */
	gsm_event_restart	restart;	/* Restart view */
	gsm_event_error		err;		/* Error view */
	gsm_event_facname	facname;	/* Caller*ID Name on Facility */
	gsm_event_ring		ring;		/* Ring */
	gsm_event_hangup	hangup;		/* Hang up */
	gsm_event_ringing	ringing;	/* Ringing */
	gsm_event_answer  	answer;		/* Answer */
	gsm_event_restart_ack	restartack;	/* Restart Acknowledge */
	gsm_event_proceeding	proceeding;	/* Call proceeding & Progress */
	gsm_event_setup_ack	setup_ack;	/* SETUP_ACKNOWLEDGE structure */
	gsm_event_notify 	notify;		/* Notification */
	gsm_event_keypad_digit 	digit;		/* Digits that come during a call */
  	gsm_event_sms_received 	sms_received;  	/* SM RX */
	gsm_event_ussd_received	ussd_received; 	/*USSD RX */
	gsm_event_operator_list_received operator_list_received; /*Operator name List RX */
	gsm_event_callforward callforward_number; /*callforward details*/
} allogsm_event;

//struct allogsm_modul;
struct allogsm_sr;

#define ALLOGSM_SMS_PDU_FO_TP_RP        0x80 // (1000 0000) Reply path. Parameter indicating that reply path exists.
#define ALLOGSM_SMS_PDU_FO_TP_UDHI      0x40 // (0100 0000) User data header indicator. This bit is set to 1 if the User Data field starts with a header.
#define ALLOGSM_SMS_PDU_FO_TP_SRI       0x20 // (0010 0000) Status report indication. This bit is set to 1 if a status report is going to be returned to the SME
#define ALLOGSM_SMS_PDU_FO_TP_MMS       0x04 // (0000 0100) More messages to send. This bit is set to 0 if there are more messages to send
#define ALLOGSM_SMS_PDU_FO_TP_MTI       0x03 // (0000 0011) Message type indicator. See GSM 03.40 subsection 9.2.3.1

typedef struct sms_pdu_tpoa{
    unsigned char len;
    unsigned char type;
    char number[32];
} sms_pdu_tpoa;

typedef struct gsm_sms_pdu_info {
    unsigned char smsc_addr_len;    /* SMC address length */
    unsigned char smsc_addr_type;   /* SMS address type */
    char smsc_addr_number[32];      /* SMC addresss */

    unsigned char first_octet;  /* First Octet */

    struct sms_pdu_tpoa tp_oa;  /* TP-OA */

    unsigned char tp_pid;       /* TP-PID */
    unsigned char tp_dcs;       /* TP-DCS */
    char tp_scts[16];           /* TP-SCTS */
    unsigned char tp_udl;       /* TP-UDL */
    char tp_ud[320];            /* TP_UD */
    unsigned int total_part;
    unsigned int part_seq;
        
    int index;  /* sms index */
} gsm_sms_pdu_info;


#define GSM_IO_FUNCS
/* Type declaration for callbacks to read a HDLC frame as below */
typedef int (*allogsm_rio_cb)(struct allogsm_modul *gsm, void *buf, int buflen);

/* Type declaration for callbacks to write a HDLC frame as below */
typedef int (*allogsm_wio_cb)(struct allogsm_modul *gsm, const void *buf, int buflen);


struct gsm_sched {
	struct timeval when;
	void (*callback)(void *data);
	void *data;
};

typedef struct sms_txt_info_s {
	char id[512];
	struct allogsm_modul *gsm;
	int resendsmsidx;
	char message[1024];
	char destination[512];
	char text[1024];		//no use
	int len;				//no use
} sms_txt_info_t;

typedef struct sms_pdu_info_s {
	char id[512];
	struct allogsm_modul *gsm;
	int resendsmsidx;
	char message[1024];
	char destination[512];	//Freedom Add 2012-01-29 14:30
	char text[1024];		//Freedom Add 2012-02-13 16:44
	int len;
} sms_pdu_info_t;


typedef union {
	sms_txt_info_t txt_info;
	sms_pdu_info_t pdu_info;
} sms_info_u;

typedef struct ussd_info_s {
	struct allogsm_modul *gsm;
	int resendsmsidx;
	char message[1024];		//no use
	int len;				//no use
} ussd_info_t;

typedef struct safe_at_s {
	struct allogsm_modul *gsm;
	char command[1024];
	int return_flag; 
	char number[64];
	int safe_at_retries;
} safe_at_t;

typedef struct gsm_ussd_received {
	int return_flag;
    unsigned char ussd_stat;
    unsigned char ussd_coding;
    int len;
    char text[1024];
} alloussd_recv_t; 
typedef struct gsm_operator_list_recv {
	int return_flag;
    int count;
    int stat[20];
    char long_operator_name[20][50];
    char short_operator_name[20][50];
    int num_operator[20];
} alloOperator_list_recv_t; 
enum creg_state_t {
	CREG_0_NOT_REG_NOT_SERARCHING=0,
	CREG_1_REGISTERED_HOME,
	CREG_2_NOT_REG_SERARCHING,
	CREG_3_REGISTRATION_DENIED,
	CREG_4_UNKNOWN,
	CREG_5_REGISTERED_ROAMING
};

#ifdef QUEUE_SMS 
typedef struct queueNodeTag {
        //queueElementT element;
        sms_info_u sms_info;
        struct queueNodeTag *next;
} queueNodeT;

typedef struct queueCDT {
        queueNodeT *front, *rear;
} queueCDT;

//typedef char queueElementT;
typedef struct queueCDT *queueADT;
#endif

struct allogsm_modul {
	int fd;				/* File descriptor for D-Channel */
	allogsm_rio_cb read_func;		/* Read data callback */
	allogsm_wio_cb write_func;		/* Write data callback */
	void *userdata;
	struct gsm_sched gsm_sched[ALLO_MAX_SCHED];	/* Scheduled events */
	int span;			/* Span number */
	int debug;			/* Debug stuff */
	int state;			/* State of D-channel */
	int sim_state;		/* State of Sim Card / UIM Card*/
	int switchtype;		/* Switch type */
	int localtype;		/* Local network type (unknown, network, cpe) */
	int remotetype;		/* Remote network type (unknown, network, cpe) */

	/* AT Stuff */
	char at_last_recv[1024];		/* Last Received command from dchan */
	int at_last_recv_idx;	/* at_lastrecv lenght */
	char at_last_sent[1024];		/* Last sent command to dchan */
	int at_last_sent_idx;	/* at_lastsent lenght */
	char *at_lastsent;
	
	char at_pre_recv[1024];
	
	char pin[16];				/* sim pin */
	char manufacturer[256];			/* gsm modem manufacturer */
	char sim_smsc[128];					/* gsm get SMSC AT+CSCA? */
	char model_name[256];			/* gsm modem name */
	char revision[256];			/* gsm modem revision */
	char imei[64];				/* span imei */
	char imsi[64];				/* sim imsi */

	int network;			/* 0 unregistered - 1 home - 2 roaming */
	int coverage;			/* net coverage -1 not signal */
	int coverage_level;		/* coverage level, 0-5 scale */
	char coverage_level_string[50];	/* 5 bar graphical representation of coverage level */
	int resetting;
	int retranstimer;		/* Timer for retransmitting DISC */
	int callwaitingsched;		/* Timer for Callwaiting sched */
	int smsTimeoutSched;		/* Timer for Sms Sending timeout */
	int hangupTimeoutSched;		/* Timer for hangup timeout */
	int dialSched;			/* Timer for Dial retry waiting for ready state */
	enum creg_state_t creg_state;	/* CREG Unsolicited State Received */
	unsigned int wind_state;	/* WIND Unsolicited State Received */
	unsigned int CME_515_count;	/* CME Error 515 Count */
	char sms_recv_buffer[1024];	/* sms received buffer */	
	char sms_sent_buffer[1024];	/* sms send buffer */
	int sms_sent_len;
	int sms_stuck;
	int sms_retry;
	int sms_old_retry; // To avoid deadloak if sms is really stuck
	char ussd_sent_buffer[1024];	/* ussd send buffer */
	int ussd_sent_len;
	int use_cleartext;
	//char sms_smsc[32];
	char sms_text_coding[64];
	enum sms_mode sms_mod_flag;
	sms_info_u *sms_info;
	ussd_info_t *ussd_info;
#ifdef QUEUE_SMS 
	queueADT sms_queue;
	sms_info_u last_sms;
#endif
	
    char sanbuf[4096];
    int sanidx;
	int sanskip;

	char net_name[64];			/* Network friendly name */
	
	int cref;			/* Next call reference value */
	
	int busy;			/* Peer is busy */

	/* Various timers */
	int sabme_timer;	/* SABME retransmit */
	int sabme_count;	/* SABME retransmit counter for BRI */
	/* All ISDN Timer values */
	int timers[MAX_TIMERS];

	/* Used by scheduler */
	struct timeval tv;
	int schedev;
	allogsm_event ev;		/* Static event thingy */
	
	/* Q.931 calls */
	struct alloat_call **callpool;
	struct alloat_call *localpool;
	
	/*Freedom add 2011-10-14 10:23, "gsm send at" show message*/
	int send_at;
#ifdef CONFIG_CHECK_PHONE
	/*Makes add 2012-04-09 16:56,check phone start time*/
	time_t check_timeout;
	int check_mode;
	int phone_stat;
	int auto_hangup_flag;
#endif //CONFIG_CHECK_PHONE

        int vol;
        int mic;
        int echocanval;
	int debug_at_fd;
        int debug_at_flag;
        int call_waiting_enabled;
        char call_waiting_caller_id[256];
        int answer_retry;

	int dial_initiated;  /*	flag to know the time when dialing starts(1) and number is dialed(0)
				That time can be long coz we will be rescheduling dialing until state is Ready */
        int dial_retry;		/* count for dialing retries.. ultimately at some time we have to timeout */

	int dial_initiated_hangup;
        int autoReloadLoopActive;

#ifdef VIRTUAL_TTY
	int already_set_mux_mode;
#endif //VIRTUAL_TTY
	int retries; //ADDED by pawan for WAVECOM BUG
	int retry_count; 
	int auto_modem_reset; 
	unsigned char sched_command[128];	/*The command to be resceduled will he stored here.. 
						  so before scheduling a schedular, copy command here. MUST*/
	int sched_state;			/*Jump to following state for command given above*/
};


struct alloat_call *allogsm_getcall(struct allogsm_modul *gsm, int cr, int outboundnew);

/* Create a D-channel on a given file descriptor.  The file descriptor must be a
   channel operating in HDLC mode with FCS computed by the fd's driver.  Also it
   must be NON-BLOCKING! Frames received on the fd should include FCS.  Nodetype 
   must be one of GSM_NETWORK or GSM_CPE.  switchtype should be ALLOGSM_SWITCH_* */

struct allogsm_modul *allogsm_new(int fd, int nodetype, int switchtype, int span, int at_debug, int call_waiting_enabled, int auto_modem_reset);

/* Set debug parameters on EXTEND -- see above debug definitions */
void allogsm_set_debug(struct allogsm_modul *gsm, int debug);

/* Get debug parameters on EXTEND -- see above debug definitions */
int allogsm_get_debug(struct allogsm_modul *gsm);

/* Check for an outstanding event on the EXTEND */
allogsm_event *allogsm_check_event(struct allogsm_modul *gsm);

/* Give a name to a given event ID */
char *allogsm_event2str(int id);

/* Give a name to a node type */
char *allogsm_node2str(int id);

/* Give a name to a switch type */
char *allogsm_switch2str(int id);

/* Print an event */
void allogsm_dump_event(struct allogsm_modul *gsm, allogsm_event *e);

/* Turn presentation into a string */
char *allogsm_pres2str(int pres);

/* Turn numbering plan into a string */
char *allogsm_plan2str(int plan);

/* Turn cause into a string */
char *allogsm_cause2str(int cause);

/* Acknowledge a call and place it on the given channel.  Set info to non-zero if there
   is in-band data available on the channel */
int allogsm_acknowledge(struct allogsm_modul *gsm, struct alloat_call *call, int channel, int info);

/* Send a digit in overlap mode */
int allogsm_information(struct allogsm_modul *gsm, struct alloat_call *call, char digit);

#define GSM_KEYPAD_FACILITY_TX
/* Send a keypad facility string of digits */
int allogsm_keypad_facility(struct allogsm_modul *gsm, struct alloat_call *call, char *digits);

/* Answer the incomplete(call without called number) call on the given channel.
   Set non-isdn to non-zero if you are not connecting to ISDN equipment */
int allogsm_need_more_info(struct allogsm_modul *gsm, struct alloat_call *call, int channel);

/* Answer the call on the given channel (ignored if you called acknowledge already).
   Set non-isdn to non-zero if you are not connecting to ISDN equipment */
int allogsm_answer(struct allogsm_modul *gsm, struct alloat_call *call, int channel);

int allogsm_senddtmf(struct allogsm_modul *gsm, int digit);

#undef allogsm_release
#undef allogsm_disconnect

/* backwards compatibility for those who don't use asterisk with liballogsmat */
#define allogsm_release(a,b,c) \
	allogsm_hangup(a,b,c)

#define allogsm_disconnect(a,b,c) \
	allogsm_hangup(a,b,c)

/* Hangup a call */
#define GSM_HANGUP
int allogsm_hangup(struct allogsm_modul *gsm, struct alloat_call *call, int cause);

#define GSM_DESTROYCALL
void allogsm_destroycall(struct allogsm_modul *gsm, struct alloat_call *call);

#define GSM_RESTART
int allogsm_restart(struct allogsm_modul *gsm);

int allogsm_reset(struct allogsm_modul *gsm, int channel);

/* Create a new call */
struct alloat_call *allogsm_new_call(struct allogsm_modul *gsm);

/* How long until you need to poll for a new event */
struct timeval *allogsm_schedule_next(struct allogsm_modul *v);

/* Run any pending schedule events */
extern allogsm_event *allogsm_schedule_run(struct allogsm_modul *gsm);
//extern allogsm_event *allogsm_schedule_run_tv(struct allogsm_modul *gsm, const struct timeval *now);

int allogsm_call(struct allogsm_modul *gsm, struct alloat_call *c, int transmode, int channel,
   int exclusive, int nonisdn, char *caller, int callerplan, char *callername, int callerpres,
	 char *called,int calledplan, int ulayer1);

struct allogsm_sr *allogsm_sr_new(void);
void allogsm_sr_free(struct allogsm_sr *sr);

int allogsm_sr_set_channel(struct allogsm_sr *sr, int channel, int exclusive, int nonisdn);
int allogsm_sr_set_called(struct allogsm_sr *sr, char *called, int complete);
int allogsm_sr_set_caller(struct allogsm_sr *sr, char *caller, char *callername, int callerpres);

int allogsm_setup(struct allogsm_modul *gsm, struct alloat_call *call, struct allogsm_sr *req);

/* Override message and error stuff */
#define GSM_NEW_SET_API
void allogsm_set_message(void (*__gsm_error)(struct allogsm_modul *gsm, char *));
void allogsm_set_error(void (*__gsm_error)(struct allogsm_modul *gsm, char *));

#define GSM_DUMP_INFO_STR
char *allogsm_dump_info_str(struct allogsm_modul *gsm);
char *allogsm_dump_info_str_GUI(struct allogsm_modul *gsm, int stat);

/* Get file descriptor */
int allogsm_fd(struct allogsm_modul *gsm);

#define GSM_PROGRESS
/* Send progress */
int allogsm_progress(struct allogsm_modul *gsm, struct alloat_call *c, int channel, int info);


#define GSM_PROCEEDING_FULL
/* Send call proceeding */
int allogsm_proceeding(struct allogsm_modul *gsm, struct alloat_call *c, int channel, int info);

/* Get/Set EXTEND Timers  */
#define GSM_GETSET_TIMERS
int allogsm_set_timer(struct allogsm_modul *gsm, int timer, int value);
int allogsm_get_timer(struct allogsm_modul *gsm, int timer);

extern int allogsm_send_ussd(struct allogsm_modul *gsm, char *message);
extern int allogsm_send_operator_list(struct allogsm_modul *gsm);
extern int allogsm_send_text(struct allogsm_modul *gsm, char *destination, unsigned char *message, char *id);
extern int allogsm_send_pdu(struct allogsm_modul *gsm,  char *message, unsigned char *text, char *id); 
extern int allogsm_decode_pdu(struct allogsm_modul *gsm, char *pdu, struct gsm_sms_pdu_info *pdu_info);
extern int allogsm_send_pin(struct allogsm_modul *gsm, char *pin);
extern int allogsm_transmit(struct allogsm_modul *gsm, const char *at);

#define GSM_MAX_TIMERS 32

#define ALLOGSM_TIMER_N200	0	/* Maximum numer of q921 retransmissions */
#define ALLOGSM_TIMER_N201	1	/* Maximum numer of octets in an information field */
#define ALLOGSM_TIMER_N202	2	/* Maximum numer of transmissions of the TEI identity request message */
#define ALLOGSM_TIMER_K		3	/* Maximum number of outstanding I-frames */

#define ALLOGSM_TIMER_T200	4	/* time between SABME's */
#define ALLOGSM_TIMER_T201	5	/* minimum time between retransmissions of the TEI Identity check messages */
#define ALLOGSM_TIMER_T202	6	/* minimum time between transmission of TEI Identity request messages */
#define ALLOGSM_TIMER_T203	7	/* maxiumum time without exchanging packets */

#define ALLOGSM_TIMER_T300	8	
#define ALLOGSM_TIMER_T301	9	/* maximum time to respond to an ALERT */
#define ALLOGSM_TIMER_T302	10
#define ALLOGSM_TIMER_T303	11	/* maximum time to wait after sending a SETUP without a response */
#define ALLOGSM_TIMER_T304	12
#define ALLOGSM_TIMER_T305	13
#define ALLOGSM_TIMER_T306	14
#define ALLOGSM_TIMER_T307	15
#define ALLOGSM_TIMER_T308	16
#define ALLOGSM_TIMER_T309	17
#define ALLOGSM_TIMER_T310	18	/* maximum time between receiving a CALLPROCEEDING and receiving a ALERT/CONNECT/DISCONNECT/PROGRESS */
#define ALLOGSM_TIMER_T313	19
#define ALLOGSM_TIMER_T314	20
#define ALLOGSM_TIMER_T316	21	/* maximum time between transmitting a RESTART and receiving a RESTART ACK */
#define ALLOGSM_TIMER_T317	22
#define ALLOGSM_TIMER_T318	23
#define ALLOGSM_TIMER_T319	24
#define ALLOGSM_TIMER_T320	25
#define ALLOGSM_TIMER_T321	26
#define ALLOGSM_TIMER_T322	27

#define ALLOGSM_TIMER_TM20	28	/* maximum time avaiting XID response */
#define ALLOGSM_TIMER_NM20	29	/* number of XID retransmits */

/* Get EXTEND version */
extern const char *allogsm_get_version(void);
extern int allogsm_test_atcommand(struct allogsm_modul *gsm, char *at);
extern int allogsm_test_atcommand_safe(struct allogsm_modul *gsm, char *at);

/*Freedom Add 2011-10-10 11:33*/
int alloinit_cfg_file(void);
int allodestroy_cfg_file(void);
//===const char* get_at(int module_id, int cmds_id);
//===int get_at_cmds_id(char* name);
//===int get_coverage1(int module_id,char* h);
//===int get_coverage2(int module_id,char* h);
//===char* get_dial_str(int module_id, char* number, char* buf, int len);
//===char* get_pin_str(int module_id, char* pin, char* buf, int len);
//===char* get_sms_len(int module_id, int sms_len, char* buf, int len);
//===char* get_sms_des(int module_id, char* des, char* buf, int len);
//===char* get_cid(int module_id, char* h, char* buf, int len);
void allogsm_set_module_id(int *const module_id, const char *name);
const char* allogsm_get_module_name(int module_id);

#ifdef PDU_LONG
int allogsm_encode_pdu_ucs2(const char* SCA, const char* TPA, unsigned char* TP_UD, const char* code, gsm_sms_pdu* long_pdu, unsigned char* pDst);
#else
int allogsm_encode_pdu_ucs2(const char* SCA, const char* TPA, char* TP_UD, const char* code, char* pDst);
#endif
int allogsm_forward_pdu(const char* src_pdu,const char* TPA,const char* SCA, char* pDst);

#ifdef CONFIG_CHECK_PHONE
void allogsm_set_check_phone_mode(struct allogsm_modul *gsm,int mode);
void allogsm_hangup_phone(struct allogsm_modul *gsm);
int allogsm_check_phone_stat(struct allogsm_modul *gsm, char *phone_number,int hangup_flag,unsigned int timeout);
#endif //CONFIG_CHECK_PHONE

#ifdef VIRTUAL_TTY
int allogsm_get_mux_command(struct allogsm_modul *gsm,char *command);
int allogsm_mux_end(struct allogsm_modul *gsm, int restart_at_flow);
#endif  //VIRTUAL_TTY

void allogsm_module_start(struct allogsm_modul *gsm);
char *allogsm_state2str(int state);
void allogsm_set_debugat(struct allogsm_modul *gsm,int mode);
int allogsm_set_state_ready(struct allogsm_modul *gsm);
int allogsm_check_emergency_available(struct allogsm_modul *gsm);
void allogsm_check_signal(struct allogsm_modul *gsm);

#endif //_LIBALLOGSMAT_H

