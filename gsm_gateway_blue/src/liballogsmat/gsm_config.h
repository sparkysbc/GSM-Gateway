#ifndef __GSM_CONFIG_H__
#define __GSM_CONFIG_H__

enum AT_CMDS {
	AT_CHECK = 0,
	AT_SET_ECHO_MODE ,
	AT_SET_CMEE,
	AT_GET_CGMM,
	AT_GET_CGMI,
	AT_CHECK_SIGNAL1,
	AT_CHECK_SIGNAL2,
	AT_GET_SIGNAL1,
	AT_GET_SIGNAL2,
	AT_DIAL,
	AT_ATZ,
	AT_ANSWER,
	AT_HANGUP,
	AT_SEND_SMS_TEXT_MODE,
	AT_SEND_SMS_PDU_MODE,
	AT_SEND_PIN,
	AT_CHECK_SMS,
	AT_ERROR,
	AT_NO_CARRIER,
	AT_NO_ANSWER,
	AT_OK,
	AT_UCS2,
	AT_GSM,
	AT_SEND_SMS_LEN,
	AT_SEND_SMS_DES,
	AT_CMS_ERROR,
	AT_CME_ERROR,
	AT_SEND_SMS_SUCCESS,
	AT_CREG,
	AT_CREG0,
	AT_CREG1,
	AT_CREG2,
	AT_CREG3,
	AT_CREG4,
	AT_CREG5,
	AT_CREG10,
	AT_CREG11,
	AT_CREG12,
	AT_CREG13,
	AT_CREG14,
	AT_CREG15,
	AT_GET_MANUFACTURER,
	AT_GET_SMSC,
	AT_GET_VERSION,
	AT_GET_IMEI,
	AT_ASK_PIN,
	AT_PIN_READY,
	AT_PIN_SIM,
	AT_IMSI,
	AT_SIM_NO_INSERTED,
	AT_MOC_ENABLED,
	AT_SET_SIDE_TONE,
	AT_NOISE_CANCEL,
	AT_SPEAKER,
	AT_DTMF_DETECTION,
	AT_CLIP_ENABLED,
	AT_RSSI_ENABLED,
	AT_SET_NET_URC,
	AT_ASK_NET,
	AT_NET_OK,
	AT_SMS_SET_CHARSET,
	AT_ASK_NET_NAME,
	AT_CHECK_NET,
	AT_NET_NAME,
	AT_INCOMING_CALL,
	AT_GET_CID,
	AT_RING,
	AT_GET_WAITING,
	AT_CALL_INIT,
	AT_CALL_PROCEEDING,
	AT_MO_CONNECTED,
	AT_NO_DIALTONE,
	AT_BUSY,
	AT_CMIC,/*Makes Add 2012-4-5 17:45 */
	AT_CLVL,/*Makes Add 2012-4-5 17:45 */
	AT_ECHOCANSUP,/*Makes Add 2012-4-5 17:45 */
	AT_CALL_NOTIFICATION,
	AT_MODE,/*Makes Add 2012-4-10 15:47 */
	AT_CMUX,/*Makes Add 2012-4-25 11:14 */
	AT_SEND_USSD,
	AT_CHECK_USSD,
	AT_GENERAL_INDICATION,
	AT_RESET,
	AT_POWER_OFF,
	AT_UPDATE_1,
	AT_UPDATE_2,
	AT_UPDATE_CMD,
	AT_UPDATE_SPAN,
	AT_SIM_TOOLKIT,
	AT_CREG_DISABLE,
	AT_SET_GAIN_INDEX,
	AT_DEL_MSG,
/*** SIM Selection****************/
	AT_SIM_SELECT_1,
	AT_SIM_SELECT_2,
	AT_SIM_SELECT_3,
/********************************/
	AT_CMDS_SUM,
	AT_CHECK_BAUD,
	AT_SMS_FREAK,
	AT_FREAKISH_SMS
};

struct cfg_info {
	char file_path[256];
	char module_name[256];
	char* at_cmds[AT_CMDS_SUM]; 
};

struct expect_list{
        char value[25];
}list[100];
unsigned int MAX_EXPECTLIST_SIZE;

int alloinit_cfg_file(void);
int allodestroy_cfg_file(void);

int expectlist_compare (char *buf);
const char* get_at(int module_id, int cfg_id);
int get_at_cmds_id(char* name);
int get_coverage1(int module_id,char* h);
int get_coverage2(int module_id,char* h);
char* get_dial_str(int module_id, char* number, char* buf, int len);
char* get_ussd_str(int module_id, char* ussd_code, char* buf, int len);
char* get_pin_str(int module_id, char* pin, char* buf, int len);
char* get_sms_len(int module_id, int sms_len, char* buf, int len);
char* get_sms_des(int module_id, char* des, char* buf, int len);
char* get_cid(int module_id, char* h, char* buf, int len);
void allogsm_set_module_id(int *const module_id, const char *name);
const char* allogsm_get_module_name(int module_id);
char *str_replace(const char *src, char* buf, int len, const char *oldstr, const char *newstr);
int save_cfg(int module_id);
char* get_sms_smsc(int module_id);
int set_sms_smsc(int module_id,char* smsc);

#endif //__GSM_CONFIG_H__


