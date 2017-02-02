/*
 * liballogsmat: An implementation of ALLO GSM cards
 *
 * Written by mark.liu <mark.liu@openvox.cn>
 *
 * $Id: gsm_sms.c 288 2011-03-01 07:55:22Z liuyuan $
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
#include <ctype.h>
//#include <iconv.h> FIXME: Any how add this library for uClib
//#include "/opt/libiconv/include/iconv.h"
#include <iconv.h>
#include "liballogsmat.h"
#include "gsm_internal.h"


static int code_convert(const char *from_charset,const char *to_charset,char *inbuf,size_t inlen,char *outbuf,size_t outlen);

unsigned long gsm_hex2int(const char *hex, int len)
{
	int i,j;
	unsigned long res = 0;

	for (i=0; i<len; i++) {
		j = 0;
		if ((hex[i] >= 48) && (hex[i] <=57)) { /* '0' - '9' */
			j = hex[i] - 48;
		} else if ((hex[i] >= 65) && (hex[i] <=70)) { /* 'A' - 'F' */
			j = hex[i] - 55;
		} else if ((hex[i] >= 97) && (hex[i] <=102)) { /* 'a' - 'f' */
			j = hex[i] - 87;
		}
		/* j = [0-9,A-F,a-f] */
		res += j * (unsigned long)pow(16, len - i - 1);
	}
	return res;
}

static void gsm_dso2string(char *res, char *dso)
{
	int i;
	int len = strlen(dso);

	for (i=0; i < len; i+=2) {
		res[i] = dso[i+1];
		if ((dso[i] != 'F') && (dso[i] != 'f')) {
				res[i+1] = dso[i];
		} else {
				res[i+1] = '\0';
				return;
		} 
	}

	if (i >= len) {
		res[i] = '\0';
	}
}

static void gsm_string2byte(unsigned char *res, char *in, int len)
{
	int i;

	for (i = 0; i < len ; i++) {
		res[i] = (unsigned char)gsm_hex2int(&in[i*2], 2);
	}
	res[len * 2] = '\0';
}

static void gsm_to8Bit(unsigned char in[], unsigned char out[], int len, int flag);
static void gsm_convertNumber(struct allogsm_modul *gsm,char *res, char *dso, int toa, char len)
{
	if (toa == 0x91) {
		res[0] = '+';
		gsm_dso2string(res+1, dso);
	} else if ((toa & 0xd0) == 0xd0){

		unsigned char user_data_7bit[64] = {0};
		int len_8bit_str=32;
		gsm_string2byte(user_data_7bit, dso, len/2);
		len_8bit_str = (len/2) + (len/2)/7;
		gsm_to8Bit(user_data_7bit, res, len_8bit_str,0);
	} else {
		gsm_dso2string(res, dso);
	}
}

static char *gsm_get_dot_string(char *in, char *out, char flag)
{
	char *ret;
	int len;
	int i,j;
	
	if (NULL == in && NULL == out) {
		return 0;
	}

	ret = strchr(in, flag);
	if (ret != NULL) {
		len = ret - in;
		if (len >= 255) {
			return NULL;
		}
		for(i=0,j=0;i<len;i++)
		{
			if(in[i]!='"')
				out[j++]=in[i];
		}
		//strncpy(out, in, len);
		out[j] = '\0';
		return ret + 1;
	}

	return NULL;
}

#if 0
static void gsm_to8Bit(unsigned char in[], unsigned char out[], int len, int flag)
{
	int i = 0;
	int inputOffset = 0;
	int outputOffset = 0;

	while (inputOffset < len ) {
	out[outputOffset] = (in[inputOffset] & (unsigned char)(pow(2, 7-i)-1));
	if (i == 8) {
		out[outputOffset] = in[inputOffset-1] & 127;
		i = 1;
	} else {
		out[outputOffset] = out[outputOffset] << i;
		out[outputOffset] |= in[inputOffset-1] >> (8-i);
		inputOffset++;
		i++;
	}
		outputOffset++;
	}

	out[len] = '\0';
}
#endif

static void gsm_to8Bit(unsigned char in[], unsigned char out[], int len, int flag)
{
        int i = 0;
        int inputOffset = 0;
        int outputOffset = 0;
        unsigned char* tmp;
        tmp = (unsigned char*)malloc(320);
        memset(tmp,0,320);
	
	if(flag == 1){
        tmp[outputOffset] = in[inputOffset] >>1;
        outputOffset++;
	inputOffset++;
	}

        while (inputOffset < len ) {
                tmp[outputOffset] = (in[inputOffset] & (unsigned char)(pow(2, 7-i)-1));

                if (i == 8) {
                        tmp[outputOffset] = in[inputOffset-1] & 127;
                        i = 1;
                } else {
                        tmp[outputOffset] = tmp[outputOffset] << i;
                        tmp[outputOffset] |= in[inputOffset-1] >> (8-i);
                        inputOffset++;
                        i++;
                }
                outputOffset++;

                if(outputOffset>=320) {
                        printf("Warnning: Function %s overflow in %s,%d\n",__func__,__FILE__,__LINE__);
                }
        }

        // This is the transition table from 7-bit to ASCII
        const int UK = -1;
        const unsigned char EQ7BIT2ASCII[128] =
                {  64, 163,  36, 165, 232, 223, 249, 236, 242, 199,
                   10, 216, 248,  13, 197, 229,  UK,  95,  UK,  UK,
                   UK,  UK,  UK,  UK,  UK,  UK,  UK,  UK, 198, 230,
                  223, 201,  32,  33,  34,  35, 164,  37,  38,  39,
                   40,  41,  42,  43,  44,  45,  46,  47,  48,  49,
                   50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
                   60,  61,  62,  63, 161,  65,  66,  67,  68,  69,
                   70,  71,  72,  73,  74,  75,  76,  77,  78,  79,
                   80,  81,  82,  83,  84,  85,  86,  87,  88,  89,
                   90, 196, 204, 209, 220, 167, 191,  97,  98,  99,
                  100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
                  110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
                  120, 121, 122, 228, 246, 241, 252, 224
                };

        int j,k;

        for(j=0,k=0; j < len; j++,k++) {
                if(tmp[j] < 128) {
                        if(tmp[j] == 0x1b && j+1 < len) {  //Special character, two bytes mean one character.
                                switch(tmp[j+1]) {
                                case 0x0A: out[k] = 0x000C; j++; break;  //     FORM FEED
                                case 0x14: out[k] = 0x005E; j++; break;   //CIRCUMFLEX ACCENT
                                case 0x28: out[k] = 0x007B; j++; break;   //LEFT CURLY BRACKET
                                case 0x29: out[k] = 0x007D; j++; break;   //RIGHT CURLY BRACKET
                                case 0x2F: out[k] = 0x005C; j++; break;   //REVERSE SOLIDUS
                                case 0x3C: out[k] = 0x005B; j++; break;   //LEFT SQUARE BRACKET
                                case 0x3D: out[k] = 0x007E; j++; break;   //TILDE
                                case 0x3E: out[k] = 0x005D; j++; break;   //RIGHT SQUARE BRACKET
                                case 0x40: out[k] = 0x007C; j++; break;   //VERTICAL LINE
                                // case 0x65: out[k] = 0x20AC; j++; break;   //EURO SIGN
                                default: out[k] = EQ7BIT2ASCII[tmp[j]]; break;
                                }
                        } else {
                                out[k] = EQ7BIT2ASCII[tmp[j]];
                        }
                } else {
                        out[k] = tmp[j];
                }
        }

        free(tmp);
}

static inline char *pdu_get_smsc_len(char *body, char *smsc_len)
{
	char *pBegin, *pEnd;

	pBegin = body;
	pEnd = pBegin + 2;

	*smsc_len = (char)gsm_hex2int(pBegin, 2);

	return pEnd;
}

static inline char *pdu_get_smsc_toa(char *body, char *smsc_toa)
{
	char *pBegin, *pEnd;

	pBegin = body;
	pEnd = pBegin + 2;
		
	*smsc_toa = (char)gsm_hex2int(pBegin, 2);

	return pEnd;
}

static inline char *pdu_get_smsc_number(char *body, int smsc_len, char *smsc_number_dso)
{
	char *pBegin, *pEnd;
	
	int smsc_number_len = (smsc_len - 1) * 2;		
	pBegin = body;
	pEnd = pBegin + smsc_number_len;
	
	strncpy(smsc_number_dso, pBegin, smsc_number_len);
	smsc_number_dso[smsc_number_len] = '\0';

	return pEnd;
}

static inline char *pdu_get_first_octet(char *body, char *first_octet)
{
	char *pBegin, *pEnd;
	
	pBegin = body;
	pEnd = pBegin + 2;
		
	*first_octet = (char)gsm_hex2int(pBegin, 2);

	return pEnd;	
}

static inline char *pdu_get_tpoa_length(char *body, char *oa_length)
{
	char *pBegin, *pEnd;
	
	pBegin = body;
	pEnd = pBegin + 2;
		
	*oa_length = (char)gsm_hex2int(pBegin, 2);

	return pEnd;	
}

static inline char *pdu_get_tpoa_type(char *body, char *oa_type)
{
	char *pBegin, *pEnd;
	
	pBegin = body;
	pEnd = pBegin + 2;
		
	*oa_type = (char)gsm_hex2int(pBegin, 2);

	return pEnd;	
}

static inline char *pdu_get_tpoa_number(char *body, int oa_len, char *oa_number)
{
	char *pBegin, *pEnd;

	pBegin = body;

	if (oa_len % 2) {
		/* odd */
		oa_len++;
	}
	pEnd = pBegin + oa_len;

	strncpy(oa_number, pBegin, oa_len);
	oa_number[oa_len] = '\0';

	return pEnd;
}

static inline char *pdu_get_tppid(char *body, char *tppid)
{
	char *pBegin, *pEnd;
	
	pBegin = body;
	pEnd = pBegin + 2;
		
	*tppid = (char)gsm_hex2int(pBegin, 2);

	return pEnd;	
}

static inline char *pdu_get_tpdcs(char *body, char *tpdcs)
{
	char *pBegin, *pEnd;
	
	pBegin = body;
	pEnd = pBegin + 2;
		
	*tpdcs = (char)gsm_hex2int(pBegin, 2);

	return pEnd;	
}

static inline char *pdu_get_tpscts(char *body, char *tpscts)
{
	char *pBegin, *pEnd;
	int len = 14;
	
	pBegin = body;
	pEnd = pBegin + len;
	
	strncpy(tpscts, pBegin, len);
	tpscts[len] = '\0';

	return pEnd;
}

static inline char *pdu_get_tpudl(char *body, unsigned char *tpudl)
{
	char *pBegin, *pEnd;
	
	pBegin = body;
	pEnd = pBegin + 2;
		
	*tpudl = (unsigned char)gsm_hex2int(pBegin, 2);

	return pEnd;
}

int allogsm_decode_pdu(struct allogsm_modul *gsm, char *pdu, struct gsm_sms_pdu_info *pdu_info)
{
	int len;

	char smsc_number_dso[32];
	char sender_number_dso[32];
	char timestamp_dso[16];
	char *p;
	
	len = strlen(pdu);
	if (len < 14) {
		gsm_error(gsm, "PDU too short:%s\n",pdu);
		return -1;
	}

	p = pdu;
	
	memset(pdu_info, 0, sizeof(struct gsm_sms_pdu_info));

	/* Length of the SMSC information */
	p = pdu_get_smsc_len(p, (char *)&pdu_info->smsc_addr_len);

	if (pdu_info->smsc_addr_len > 0) {
		/* Type-of-address of the SMSC */
		p = pdu_get_smsc_toa(p, (char *)&pdu_info->smsc_addr_type);

		/* SMSC Number (in decimal semi-octets) */
		if ((pdu_info->smsc_addr_len - 1) * 2 >= sizeof(smsc_number_dso)) {
			gsm_error(gsm, "smsc number is too long\n");
			return -1;
		}
		p = pdu_get_smsc_number(p, pdu_info->smsc_addr_len, smsc_number_dso);
		gsm_convertNumber(gsm, (char *)pdu_info->smsc_addr_number, smsc_number_dso, pdu_info->smsc_addr_type, pdu_info->smsc_addr_len);
	} 

	/* PDU Type */
	p = pdu_get_first_octet(p, (char *)&pdu_info->first_octet);

	/* TP-OA */
	
	/* Address-Length, Length of the sender number */
	p = pdu_get_tpoa_length(p, (char *)&pdu_info->tp_oa.len);
	if (pdu_info->tp_oa.len > 0) {
		/* Type of address */
		p = pdu_get_tpoa_type(p, (char *)&pdu_info->tp_oa.type);
		/* Sender number (decimal semi-octets), with a trailing F */
		if (pdu_info->tp_oa.len + 1 >= sizeof(sender_number_dso)) {
			gsm_error(gsm, "Sender number is too long\n");
			return -1;
		}
		p = pdu_get_tpoa_number(p, pdu_info->tp_oa.len, sender_number_dso);
		gsm_convertNumber(gsm, pdu_info->tp_oa.number, sender_number_dso, pdu_info->tp_oa.type, pdu_info->tp_oa.len);
		//gsm_message(gsm, "-------- %d tp_oa.type %x, tp_oa.number %s, sender_number_dso %s,\n",__LINE__, pdu_info->tp_oa.type,pdu_info->tp_oa.number, sender_number_dso);
	}

	/* TP-PID: Protocol identifier */
	p = pdu_get_tppid(p, (char *)&pdu_info->tp_pid);    


	/* TP-DCS: Data coding scheme */
	p = pdu_get_tpdcs(p, (char *)&pdu_info->tp_dcs);    


	/* TP-SCTS: Time stamp (semi-octets */
	p = pdu_get_tpscts(p, timestamp_dso);
	gsm_dso2string(pdu_info->tp_scts, timestamp_dso);
	
	/* 
	 * TP-UDL: User data length, length of message. 
	 * The TP-DCS field indicated 7-bit data, so the length here is the number of septets (10). 
	 * If the TP-DCS field were set to indicate 8-bit data or Unicode, the length would be the number of octets (9).
	 */
	p = pdu_get_tpudl(p, &pdu_info->tp_udl);

	/* TP-UD */

	strncpy(pdu_info->tp_ud, p, sizeof(pdu_info->tp_ud));


	return 0;
}

static void format_time_for_pdu(char *org, char *format_time, int len, char* format_tz, int len2)
{
	//org_format
	//12052817005932
	//year:12
	//month:05
	//day:28
	//hour:17
	//minute:00
	//second:59
	//timezone:32

	char tmp[7][3];
	int i;
	for(i=0; i<7; i++) {
		tmp[i][2] = '\0';
	}

	tmp[0][0] = org[0];
	tmp[0][1] = org[1];

	tmp[1][0] = org[2];
	tmp[1][1] = org[3];

	tmp[2][0] = org[4];
	tmp[2][1] = org[5];

	tmp[3][0] = org[6];
	tmp[3][1] = org[7];

	tmp[4][0] = org[8];
	tmp[4][1] = org[9];

	tmp[5][0] = org[10];
	tmp[5][1] = org[11];

	tmp[6][0] = org[12];
	tmp[6][1] = org[13];

	snprintf(format_time,len,"20%s/%s/%s %s:%s:%s",tmp[0],tmp[1],tmp[2],tmp[3],tmp[4],tmp[5]);

	int tz;
	tz = atoi(tmp[6]);
	if(tz&0x80) {  //negative
		snprintf(format_tz,len2,"GMT-%d",(tz&0x7F)/4);	
	} else {
		snprintf(format_tz,len2,"GMT+%d",tz/4);	
	}
}

int gsm_pdu2sm_event(struct allogsm_modul *gsm, char *pdu) 
{
	struct gsm_sms_pdu_info pdu_info;
	int ret = 0;
        unsigned char user_data_7bit[320] = {0};
        unsigned char user_data_8bit[320] = {0};
	unsigned char udhi = 0;
	char buf[1024];
	unsigned long header_len = 0;

	pdu_info.total_part=0;
	pdu_info.part_seq=0;

	strncpy(gsm->ev.sms_received.pdu, pdu, sizeof(gsm->ev.sms_received.pdu));
	strncpy(buf, pdu, sizeof(buf));

	//gsm->debug=1;
	if (gsm->debug) {
		gsm_message(gsm, "SMS PDU = %s\n", pdu);
	}
	
	ret = allogsm_decode_pdu(gsm, buf, &pdu_info);
	if (ret < 0) {
		return -1;
	}

	if (pdu_info.first_octet & ALLOGSM_SMS_PDU_FO_TP_UDHI) {
		udhi = 1;
		if (gsm->debug) {
			gsm_message(gsm, "User Data Header Indicator = %d\n", udhi);
		}
	}

	if (gsm->debug) {
		gsm_message(gsm, "SMSC Length = %d\n", pdu_info.smsc_addr_len);
	}
	
	if (pdu_info.smsc_addr_len > 0) {
		if (gsm->debug) {
			gsm_message(gsm, "SMSC Type-of-address = %#x\n", pdu_info.smsc_addr_type);
			gsm_message(gsm, "SMSC Number = %s\n", pdu_info.smsc_addr_number);
		}
		strncpy(gsm->ev.sms_received.smsc, pdu_info.smsc_addr_number, sizeof(gsm->ev.sms_received.smsc));
	} else {
		gsm->ev.sms_received.smsc[0] = '\0';
	}

	if (gsm->debug) {
		gsm_message(gsm, "PDU First Octet = %#x\n", pdu_info.first_octet);
		gsm_message(gsm, "Sender number length = %d\n", pdu_info.tp_oa.len);
	}

	if (pdu_info.smsc_addr_len > 0) {
		if (gsm->debug) {
			gsm_message(gsm, "Sender Type-of-address = %#x\n", pdu_info.tp_oa.type);
			gsm_message(gsm, "Sender Number = %s\n", pdu_info.tp_oa.number);
		}
		strncpy(gsm->ev.sms_received.sender, pdu_info.tp_oa.number, sizeof(gsm->ev.sms_received.sender));
	} else {
		gsm->ev.sms_received.sender[0] = '\0';
	}

	if (gsm->debug) {
		gsm_message(gsm, "TP-PID = %#x\n", pdu_info.tp_pid);
		gsm_message(gsm, "TP-DCS = %#x\n", pdu_info.tp_dcs);
		gsm_message(gsm, "Timestamp = %s\n", pdu_info.tp_scts);
		gsm_message(gsm, "User data length = %d\n", pdu_info.tp_udl);
		gsm_message(gsm, "User data = %s\n", pdu_info.tp_ud);
	}
	gsm->ev.sms_received.len = pdu_info.tp_udl;

	/* TP-UD */
	if (udhi) {
		header_len = gsm_hex2int((char *)pdu_info.tp_ud, 2) + 1;
		gsm->ev.sms_received.len -= header_len;
		/** Number of UDH here (total and part number) ***/
		pdu_info.total_part = gsm_hex2int ((char *)(pdu_info.tp_ud+8), 2);
		pdu_info.part_seq = gsm_hex2int ((char *)(pdu_info.tp_ud+10), 2);
		if (gsm->debug){
			gsm_message(gsm, "Total Number of Parts = %d\n", pdu_info.total_part);
			gsm_message(gsm, "Part in Sequence = %d\n", pdu_info.part_seq);
		}
	}

	gsm_string2byte(user_data_7bit, pdu_info.tp_ud + header_len * 2, pdu_info.tp_udl - header_len);

	if (pdu_info.tp_dcs == 0x08) {
		user_data_8bit[0] = '\0';
	} else {
		if( udhi && pdu_info.tp_ud[1] == '5') //for UDH with lenth 5
			gsm_to8Bit(user_data_7bit, user_data_8bit, pdu_info.tp_udl - (header_len+1),1);
		else
			gsm_to8Bit(user_data_7bit, user_data_8bit, pdu_info.tp_udl - header_len,0);
	}

	if (gsm->debug) {
		gsm_message(gsm, "User data 8bit = %s\n", user_data_8bit);
	}
	
	memset(gsm->ev.sms_received.text, 0, sizeof(gsm->ev.sms_received.text));
	if (pdu_info.tp_udl) {
		if (pdu_info.tp_dcs == 0x08) {
                        int length=0;
                        length=sms_get_str(gsm,(char *)user_data_7bit, gsm->ev.sms_received.len, gsm->ev.sms_received.text, sizeof(gsm->ev.sms_received.text));
                        if(length>0)
                                gsm->ev.sms_received.len=length;
		} else {
			strncpy(gsm->ev.sms_received.text, (char *)user_data_8bit, pdu_info.tp_udl - header_len);
		}
		/***** for Seq Count */
		if (pdu_info.total_part > 1){
			unsigned char tmp [20];
			sprintf( tmp," (%d/%d)", pdu_info.part_seq, pdu_info.total_part);
			strcat(gsm->ev.sms_received.text, tmp);
		}
	}

	format_time_for_pdu(pdu_info.tp_scts, gsm->ev.sms_received.time, sizeof(gsm->ev.sms_received.time),gsm->ev.sms_received.tz,sizeof(gsm->ev.sms_received.tz));

	gsm->ev.sms_received.mode = SMS_PDU;
	
	return 0;
}

#if 0
//FIXME: Library libiconv is not present here... Either add functions (iconv_open, iconv, iconv_close) here OR compile library for uClib: pawan

typedef void *iconv_t;

iconv_t iconv_open (const char *tocode, const char *fromcode)
{
	printf("Function not implemented: gms_sms.c");
	return (iconv_t) -1;
}
size_t iconv (iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
	printf("Function not implemented: gms_sms.c");
	return -1;
}
int
iconv_close (iconv_t cd)
{
	printf("Function not implemented: gms_sms.c");
	return -1;
}
#endif
#if 0
/* Identifier for conversion method from one codeset to another.  */
typedef void *iconv_t;


/* Allocate descriptor for code conversion from codeset FROMCODE to
   codeset TOCODE.  */

iconv_t iconv_open (const char *tocode, const char *fromcode)
{
	/* Normalize the name. We remove all characters beside alpha-numeric,
	'_', '-', '/', '.', and ':'. */
	size_t tocode_len = strlen (tocode) + 3;
	char *tocode_conv;
	bool tocode_usealloca = __libc_use_alloca (tocode_len);
	if (tocode_usealloca)
		tocode_conv = (char *) alloca (tocode_len);
	else
	{
		tocode_conv = (char *) malloc (tocode_len);
		if (tocode_conv == NULL)
			return (iconv_t) -1;
	}
	strip (tocode_conv, tocode);
	tocode = (tocode_conv[2] == '\0' && tocode[0] != '\0' ? upstr (tocode_conv, tocode) : tocode_conv);
	
	size_t fromcode_len = strlen (fromcode) + 3;
	char *fromcode_conv;
	bool fromcode_usealloca = __libc_use_alloca (fromcode_len);
	if (fromcode_usealloca)
		fromcode_conv = (char *) alloca (fromcode_len);
	else
	{
		fromcode_conv = (char *) malloc (fromcode_len);
		if (fromcode_conv == NULL)
		{
			if (! tocode_usealloca)
				free (tocode_conv);
			return (iconv_t) -1;
		}
	}
	strip (fromcode_conv, fromcode);
	fromcode = (fromcode_conv[2] == '\0' && fromcode[0] != '\0' ? upstr (fromcode_conv, fromcode) : fromcode_conv);

	__gconv_t cd;
	int res = __gconv_open (tocode, fromcode, &cd, 0);
	
	if (! fromcode_usealloca)
		free (fromcode_conv);
	if (! tocode_usealloca)
		free (tocode_conv);

	if (__builtin_expect (res, __GCONV_OK) != __GCONV_OK)
	{
		/* We must set the error number according to the specs. */
		if (res == __GCONV_NOCONV || res == __GCONV_NODB)
			__set_errno (EINVAL);
		
		cd = (iconv_t) -1;
	}
	
	return (iconv_t) cd;
}



/* Convert at most *INBYTESLEFT bytes from *INBUF according to the
   code conversion algorithm specified by CD and place up to
   *OUTBYTESLEFT bytes in buffer at *OUTBUF.  */

size_t iconv (iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
	__gconv_t gcd = (__gconv_t) cd;
	char *outstart = outbuf ? *outbuf : NULL;
	size_t irreversible;
	int result;

	if (__builtin_expect (inbuf == NULL || *inbuf == NULL, 0))
	{
		if (outbuf == NULL || *outbuf == NULL)
			result = __gconv (gcd, NULL, NULL, NULL, NULL, &irreversible);
		else
			result = __gconv (gcd, NULL, NULL, (unsigned char **) outbuf,(unsigned char *) (outstart + *outbytesleft), &irreversible);
	} else {
		const char *instart = *inbuf;

		result = __gconv (gcd, (const unsigned char **) inbuf, (const unsigned char *) (*inbuf + *inbytesleft), (unsigned char **) outbuf, (unsigned char *) (*outbuf + *outbytesleft), &irreversible);

		*inbytesleft -= *inbuf - instart;
	}
	if (outstart != NULL)
		*outbytesleft -= *outbuf - outstart;

	switch (__builtin_expect (result, __GCONV_OK))
	{
		case __GCONV_ILLEGAL_DESCRIPTOR:
				__set_errno (EBADF);
				irreversible = (size_t) -1L;
				break;

		case __GCONV_ILLEGAL_INPUT:
				__set_errno (EILSEQ);
				irreversible = (size_t) -1L;
				break;

		case __GCONV_FULL_OUTPUT:
				__set_errno (E2BIG);
				irreversible = (size_t) -1L;
				break;

		case __GCONV_INCOMPLETE_INPUT:
				__set_errno (EINVAL);
				irreversible = (size_t) -1L;
				break;

		case __GCONV_EMPTY_INPUT:
		case __GCONV_OK:
				/* Nothing. */
				break;

		default:
				assert (!"Nothing like this should happen");
	}

	return irreversible;
}


/* Free resources allocated for descriptor CD for code conversion.  */
extern int iconv_close (iconv_t __cd) __THROW;

int
iconv_close (iconv_t cd)
{
	if (__builtin_expect (cd == (iconv_t *) -1L, 0))
	{
		__set_errno (EBADF);
		return -1;
	}

	return __gconv_close ((__gconv_t) cd) ? -1 : 0;
}

#endif //iconv library implementation
static int code_convert(const char *from_charset,const char *to_charset,char *inbuf,size_t inlen,char *outbuf,size_t outlen)
{
	iconv_t cd;
	char **pin = &inbuf;
	char **pout = &outbuf;

printf("to_charset >>%s<< from_charset >>%s<< inlen %d outlen %d\n", to_charset, from_charset, inlen, outlen);

	cd = iconv_open(to_charset,from_charset);
	if ( (iconv_t)-1 == cd ) {
		printf("file:%s,line:%d,iconv_open error!\n",__FILE__,__LINE__);
		return -1;
	}

printf ("==================================\n");
	memset(outbuf,0,outlen);

	if (iconv(cd,pin,&inlen,pout,&outlen) == -1) {
		printf("file:%s,line:%d,iconv error!\n",__FILE__,__LINE__);
		return -1;
	}
        printf ("output: >>%s<<\n", outbuf);

	iconv_close(cd);

	return outlen;
}
#if 0
static int unicode_2_gb18030(char *inbuf,size_t inlen,char *outbuf,size_t outlen)
{
	return code_convert("UCS-2","GB18030",inbuf,inlen,outbuf,outlen);
}

static int gb18030_2_unicode(char *inbuf,size_t inlen,char *outbuf,size_t outlen)
{
	return code_convert("GB18030","UCS-2",inbuf,inlen,outbuf,outlen);
}

static int sms_decode_gb18030(char *in, size_t inlen, char *out)
{
	char tmp[512];
	char swap_char;
	int outleft;
	int i;
	int j;
	int k;
	int out_len;

	outleft = gb18030_2_unicode(in,inlen,tmp,512);
	if (outleft <= 0) {
		return -1;
	}

	out_len = 512 - outleft;
	for (i = 2; i < out_len; i=i+2) {
		swap_char = tmp[i];
		tmp[i - 2] = tmp[i + 1];
		tmp[i - 1] = swap_char;
	}

	out_len -= 2;

	for (k = 0, i = 0, j = 0; i < out_len; i ++) {
		if (0x1A == tmp[i]) {
			out[k++] = 0xFF;
			out[k++] = 0xFF;
			out[k++] = 0xFA;
			j += 2;
		} else if (0x1B == tmp[i]) {
			out[k++] = 0xFF;
			out[k++] = 0xFF;
			out[k++] = 0xFB;
			j += 2;
		} else {
			out[k++] = tmp[i];
		}
	}

	out_len += j;

	for (i = 0; i< out_len ; i ++) {
		out[i] = (char)(out[i]);
	}

	return out_len;
}

int sms_set_str(char *in, char *out)
{
	char tmp[512];
	int i,j,k;
	int outlen;
	int outhexlen = 0;

	int inlen = strlen(in);

	for (i = 0, k = 0; i < inlen;) {
		outlen = sms_decode_gb18030(in + i, 2, tmp);
		i += 2;

		outhexlen += outlen;
		for (j = 0; j < outlen; j ++) {
			out[k++] = tmp[j];
		}
	}

	out[outhexlen] = '\0';

	return outhexlen;
}

static int sms_encode_gb18030(char *in, size_t inlen, char *out, size_t outlen)
{
	char tmp[512];
	int outleft;
	int i;
	int j;

//Freedom Modify 2012-06-06 13:22
//#define NEED_BOM
/////////////////////////////////////////////////////////////////////
#ifdef NEED_BOM
	tmp[0] = 0xFF;
	tmp[1] = 0xFE;
	j = 2 + inlen;

	for (i = 0; i < inlen; i += 2) {
		tmp[i + 2] = in[i + 1];
		tmp[i + 3] = in[i];
	}
#else  //NEED_BOM
	j = inlen;
	for (i = 0; i < inlen; i += 2) {
		tmp[i] = in[i + 1];
		tmp[i + 1] = in[i];
	}
#endif //NEED_BOM
/////////////////////////////////////////////////////////////////////

	outleft = unicode_2_gb18030(tmp, j, out, outlen);
	if (outleft <= 0) {
		return -1;
	}

	out[outlen-outleft] = '\0';

	return outlen - outleft;
}
#endif
static void to_upper_string(char *string)
{
        int i=0;
        int length=strlen(string);
        while (i<length)
        {
                if (islower(string[i]))
                        string[i]=toupper(string[i]);
                i++;
        }
}
static int sms_encode(char *sms_coding,char *in, size_t inlen, char *out, size_t outlen)
{
        int outleft;
        char coding[256];
        char tmp[512];
        int j;

        //Add BOM header
        tmp[0] = 0xFE;
        tmp[1] = 0xFF;
        memcpy(&tmp[2],in,inlen);
        j = 2 + inlen;

        memset(coding,0,sizeof(coding));
        memcpy(coding,sms_coding,strlen(sms_coding));
        to_upper_string(coding);
        //outleft = code_convert("UCS-2BE",coding,tmp,j,out,outlen);
        outleft = code_convert("UTF-16",coding,tmp,j,out,outlen);
        if (outleft <= 0) {
                return -1;
        }
        out[outlen-outleft] = '\0';

        return outlen - outleft;
}

int sms_get_str(struct allogsm_modul *gsm,char *in, size_t inlen, char *out, size_t outlen)
{
	//return sms_encode_gb18030(in, inlen, out, outlen);
        if(gsm->sms_text_coding == NULL || strlen(gsm->sms_text_coding) == 0)
                return sms_encode("UTF-8",in,inlen,out,outlen);
        return sms_encode(gsm->sms_text_coding,in,inlen,out,outlen);

}

int gsm_text2sm_event(struct allogsm_modul *gsm, char *sms_info) 
{
	char tmp[256];

	char *p;
	char *ret;
	int language;
	char *sms_body;

	if (NULL == sms_info) {
		return -1;
	}

//	gsm->debug = 1;
	
	p = sms_info;
	
	/* get %CMT */
	ret = gsm_get_dot_string(p, tmp, ':'); // +CMT:
	if (!ret) {
		return -1;
	}
	if (gsm->debug) {
		gsm_message(gsm, "+CMT = %s\n", tmp);
	}

	/* get callerID */
	ret = gsm_get_dot_string(ret, gsm->ev.sms_received.sender, ','); // callerID
	if (!ret) {
		return -1;
	}
	if (gsm->debug) {
		gsm_message(gsm, "CallerID = %s\n", gsm->ev.sms_received.sender);
	}

	/* get year */
	ret = gsm_get_dot_string(ret, tmp, ','); // year
	if (!ret) {
		return -1;
	}
	if (gsm->debug) {
		gsm_message(gsm, "Year = %s\n", tmp);
	}

	/* get month */
	ret = gsm_get_dot_string(ret, tmp, ','); // month
	if (!ret) {
		return -1;
	}
	if (gsm->debug) {
		gsm_message(gsm, "Month = %s\n", tmp);
	}
	
	/* get day */
	ret = gsm_get_dot_string(ret, tmp, ','); // day
	if (!ret) {
		return -1;
	}
	if (gsm->debug) {
		gsm_message(gsm, "Day = %s\n", tmp);
	}

	/* get hour */
	ret = gsm_get_dot_string(ret, tmp, ','); // hour
	if (!ret) {
		return -1;
	}
	if (gsm->debug) {
		gsm_message(gsm, "Hour = %s\n", tmp);
	}

	/* get minute */
	ret = gsm_get_dot_string(ret, tmp, ','); // minute
	if (!ret) {
		return -1;
	}
	if (gsm->debug) {
		gsm_message(gsm, "Minute = %s\n", tmp);
	}

	/* get language format */
	ret = gsm_get_dot_string(ret, tmp, ','); // format
	if (!ret) {
		return -1;
	}
	language = atoi(tmp);
	if (gsm->debug) {
		gsm_message(gsm, "Format = %d\n", language);
	}

	/* get length */
	ret = gsm_get_dot_string(ret, tmp, ','); // length
	if (!ret) {
		return -1;
	}
	gsm->ev.sms_received.len = atoi(tmp);
	if (gsm->debug) {
		gsm_message(gsm, "Length = %d\n", gsm->ev.sms_received.len);
	}

	/* get prt */
	ret = gsm_get_dot_string(ret, tmp, ','); // prt
	if (!ret) {
		return -1;
	}
	if (gsm->debug) {
		gsm_message(gsm, "Prt = %s\n", tmp);
	}
	
	/* get prv */
	ret = gsm_get_dot_string(ret, tmp, ','); // prv
	if (!ret) {
		return -1;
	}
	if (gsm->debug) {
		gsm_message(gsm, "Prv = %s\n", tmp);
	}
	
	/* get type */
	ret = gsm_get_dot_string(ret, tmp, '\r'); // type
	if (!ret) {
		return -1;
	}
	if (gsm->debug) {
		gsm_message(gsm, "Type = %s\n", tmp);
	}

	sms_body = ret + 1;
	if (gsm->debug) {
		gsm_message(gsm, "SMS Body = %s\n", sms_body);
	}

	/* get SMS body */
	gsm->ev.sms_received.pdu[0] = '\0';
	if (6 == language) {
		sms_get_str(gsm,sms_body, gsm->ev.sms_received.len, gsm->ev.sms_received.text, sizeof(gsm->ev.sms_received.text));
	} else {
		strncpy(gsm->ev.sms_received.text, sms_body, gsm->ev.sms_received.len);
		gsm->ev.sms_received.text[gsm->ev.sms_received.len] = '\0';
	}
	gsm->ev.sms_received.mode = SMS_TEXT;

	return 0;
}

int gsm_text2sm_event2(struct allogsm_modul *gsm, char *sms_info, char *sms_body) 
{
	char tmp[256];
	
	char *p;
	char *ret;

	if (NULL == sms_info) {
		return -1;
	}
		printf("[%s]%s:%d sms_info:>>%s<<\n", __FILE__, __func__, __LINE__, sms_info);// pawan print
	
        memset(tmp,0,256);
	memset(gsm->ev.sms_received.text, 0, 1024);

	p = sms_info;
	ret = gsm_get_dot_string(p, tmp, ':'); // +CMT:
	if (!ret) {
		return -1;
	}

	if (gsm->debug) {
		gsm_message(gsm, "+CMT = %s\n", tmp);
	}

	/* get callerID */
	ret = gsm_get_dot_string(ret, gsm->ev.sms_received.sender, ','); // callerID
	if (!ret) {
		return -1;
	}
	if (gsm->debug) {
		gsm_message(gsm, "CallerID = %s\n", gsm->ev.sms_received.sender);
	}

	ret = gsm_get_dot_string(ret, tmp, ','); // year
	if (!ret) {
		return -1;
	}

	/* get date */
	ret = gsm_get_dot_string(ret, tmp, '\r'); // year
	if (!ret) {
		return -1;
	}
	if (gsm->debug) {
		gsm_message(gsm, "Date = %s\n", tmp);
	}

	char* find;
	if((find=strstr(tmp,"+")) != NULL) {
		*find = '\0';
		strncpy(gsm->ev.sms_received.time, tmp,sizeof(gsm->ev.sms_received.time));
		int tz;
		tz=atoi(find+1);
		if(tz&0x80) {  //negative
			snprintf(gsm->ev.sms_received.tz,sizeof(gsm->ev.sms_received.tz),"GMT-%d",(tz&0x7F)/4);
		} else {
			snprintf(gsm->ev.sms_received.tz,sizeof(gsm->ev.sms_received.tz),"GMT+%d",tz/4);
		}
	} else {
		strncpy(gsm->ev.sms_received.time, tmp,sizeof(gsm->ev.sms_received.time));
		strcpy(gsm->ev.sms_received.tz,"unknow");
	}

	
	ret = gsm_get_dot_string(ret, tmp, '\n');
	if (!ret) {
		return -1;
	}
	strncpy(gsm->ev.sms_received.text, ret, strlen(ret));
	if(gsm->ev.sms_received.text[strlen(ret)-2]=='\r')
		gsm->ev.sms_received.text[strlen(ret)-2]='\0';
	if(gsm->ev.sms_received.text[strlen(ret)-1]=='\n')
		gsm->ev.sms_received.text[strlen(ret)-1]='\0';
	/* get sms body */
	if (gsm->debug) {
		gsm_message(gsm, "SMS Body = %s\n", gsm->ev.sms_received.text);
	}
	
	/* get SMS body */
	gsm->ev.sms_received.pdu[0] = '\0';
	//strncpy(gsm->ev.sms_received.text, sms_body, sizeof(gsm->ev.sms_received.text)); // commented by openvox
	gsm->ev.sms_received.len = strlen(gsm->ev.sms_received.text);
	gsm->ev.sms_received.mode = SMS_TEXT;

	return 0;
}

enum sms_mode gsm_check_sms_mode(struct allogsm_modul *gsm, char *sms_info) 
{
	char tmp[256];
	
	char *p;
	char *ret;

	enum sms_mode mode = SMS_PDU;
	
	if (NULL == sms_info) {
		return SMS_UNKNOWN;
	}
	
	p = sms_info;
	
	ret = gsm_get_dot_string(p, tmp, ':'); // +CMT:
	if (!ret) {
		return SMS_UNKNOWN;
	}

	if (!gsm_compare(tmp, "+CMT")) {
		return SMS_UNKNOWN;
	}

	ret = gsm_get_dot_string(ret, tmp, ',');
	if (!ret) {
		return SMS_UNKNOWN;
	}
	//gsm_message(gsm, "SMS INFO = %s\n", sms_info);
	if (ret[1] == '\"') {
		return SMS_TEXT;
	}
	
	return mode;
}

//Encode pdu
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/*The user's encoding*/
#define GSM_7BIT     0
#define GSM_8BIT     4
#define GSM_UCS2     8

/*the struct is shot message paraments .notice:the string end '\0'*/
struct sm_param{
	 char SCA[16];
	 char TPA[16];
	 char TP_PID;
	 char TP_DCS;
	 char TP_UD[161];
 };

/**
 * \fn        int gsmInvertNumbers(const char* pSrc, char* pDst,int nSrcLength)
 * \brief     for example  "8613678038107" -->"683176088301F7"
 * \param  const char* pSrc
 *             char* pDst
 *             int nSrcLength
 * \return  the length of the pDst.
 */
static int gsmInvertNumbers(const char* pSrc, char* pDst,int nSrcLength)
{
	int nDstLength; 
	int i;
	char ch;
	nDstLength = nSrcLength; 
	for(i=0; i<nSrcLength;i+=2) {
		ch = *pSrc++; 
		*pDst++ = *pSrc++;
		*pDst++ = ch; 
	} 

	if(nSrcLength & 1) {
		*(pDst-2) = 'F'; 
		nDstLength++;
	}
	
	*pDst = '\0'; 
	return nDstLength;
}

/**
 * \fn        int gsmBytes2String(unsigned char *pRc,char *pDst,unsigned char*nLength)
 * \brief   {0xC8, 0x32, 0x9B, 0xFD, 0x0E, 0x01} --> "C8329BFD0E01" 
 * \param unsigned char *pRcv
 *            char *pDst
 *            int nSrcLength
 * \return  the length of the pDst.
 */
static int gsmBytes2String(const unsigned char* pSrc, char* pDst, int nSrcLength)
{
	const char tab[]="0123456789ABCDEF";
	int i;
	for(i=0; i<nSrcLength; i++) {
		*pDst++ = tab[*pSrc >> 4];
		*pDst++ = tab[*pSrc & 0x0f];
		pSrc++;
	}

	*pDst = '\0';
	return nSrcLength * 2;
}

/**
 * \fn     int gsmBytes2String_pdu(const unsigned char* pSrc, char* pDst, int nSrcLength)
 * \brief  
 * \param  none
 * \return none
 */
static int gsmBytes2String_pdu(const unsigned char* pSrc, unsigned char* pDst, int nSrcLength)
{
	memcpy(pDst, pSrc,nSrcLength);
	pDst[nSrcLength] = '\0';
	return nSrcLength;
}

/**
 * \fn     int myUTF8_to_UNICODE(Uint16* unicode, unsigned char* utf8, int len)
 * \brief  
 * \param  none
 * \return none
 */
static int myUTF8_to_UNICODE(unsigned char* unicode, unsigned char* utf8, int len)
{
	int length=0;
	unsigned int code_int;
	int i;
	 
	for(i = 0; i<len;i++) {
		if(utf8[i]<=0x7f) {
			unicode[length]=0x00;
			length++;
			unicode[length]=utf8[i];
			length++;	 
		} else if(utf8[i]<=0xdf) {
			code_int = (((unsigned char) utf8[i] & 0x1f)  << 6) + ((unsigned char) utf8[i+1] & 0x3f) ;
			unicode[length] = (unsigned char) (code_int >> 8);
			i += 1;
			unicode[length+1] = (unsigned char) code_int;
			unicode[length] = (unsigned char) (code_int >> 8);				
			length += 2;
		} else {
			code_int = ((int) ((unsigned char) utf8[i] & 0x0f) << 12)+(((unsigned char) utf8[i+1] & 0x3f) << 6)+((unsigned char) utf8[i+2] & 0x3f);
			i += 2;
			unicode[length+1] = (unsigned char) code_int;
			unicode[length] = (unsigned char) (code_int >> 8);				
			length += 2;
		}	 	
	}

	return (length);
}
#if 0
/**
 * \fn        int gsmEncode7bit(const char* pSrc, unsigned char* pDst, int nSrcLength)
 * \brief  
 * \param  const char* pSrc
 *             unsigned char* pDst
 *             int nSrcLength
 * \return  the length of the pDst.
 */
static int gsmEncode7bit(const char* pSrc, unsigned char* pDst, int nSrcLength)
{
	int nSrc;
	int nDst; 
	int nChar;
	unsigned char nLeft; 
	 
	nSrc = 0;
	nDst = 0; 
 
	while (nSrc < nSrcLength) {
		nChar = nSrc & 7; 
		if(nChar == 0) {
			nLeft = *pSrc;
		} else {
			*pDst = (*pSrc << (8-nChar)) | nLeft;  
			nLeft = *pSrc >> nChar;	
			pDst++;
			nDst++;
		}
		pSrc++;
		nSrc++;
	} 
	return nDst;
}
#else

/**
 * \fn        int gsmEncode7bit(const char* pSrc, unsigned char* pDst, int nSrcLength)
 * \brief  
 * \param  const char* pSrc
 *             unsigned char* pDst
 *                 int nSrcLength
 *                 int *pUDLen   Set User Data len,7bit User Data
 *                 length not equal to pDst length
 * \return  the length of the pDst.
 */
static int gsmEncode7bit(const char* pSrc, unsigned char* pDst, int nSrcLength, unsigned char* pUDLen)
{
        int nSrc;
        int nDst;
        int nChar;
        unsigned char nLeft;
        int len;

        unsigned char* tmp;
        tmp = (unsigned char*)malloc(320);
        memset(tmp,0,320);

        // This is the transition table from 7-bit to ASCII
   int UK = -1;
   const unsigned char EQ7BIT2ASCII[128] =
                {  64, 163,  36, 165, 232, 223, 249, 236, 242, 199,
                   10, 216, 248,  13, 197, 229,  UK,  95,  UK,  UK,
                   UK,  UK,  UK,  UK,  UK,  UK,  UK,  UK, 198, 230,
                  223, 201,  32,  33,  34,  35, 164,  37,  38,  39,
                   40,  41,  42,  43,  44,  45,  46,  47,  48,  49,
                   50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
                   60,  61,  62,  63, 161,  65,  66,  67,  68,  69,
                   70,  71,  72,  73,  74,  75,  76,  77,  78,  79,
                   80,  81,  82,  83,  84,  85,  86,  87,  88,  89,
                   90, 196, 204, 209, 220, 167, 191,  97,  98,  99,
                  100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
                  110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
                  120, 121, 122, 228, 246, 241, 252, 224
                };

        int i,j,k;

        len = nSrcLength;
        for(i=0,j=0; i<nSrcLength; i++,j++) {
                switch(pSrc[i]) {
                case 0x000C: tmp[j] = 0x1B; tmp[++j] = 0x0A; len++; break;  //  FORM FEED
                case 0x005E: tmp[j] = 0x1B; tmp[++j] = 0x14; len++; break;   //CIRCUMFLEX ACCENT
                case 0x007B: tmp[j] = 0x1B; tmp[++j] = 0x28; len++; break;   //LEFT CURLY BRACKET
                case 0x007D: tmp[j] = 0x1B; tmp[++j] = 0x29; len++; break;   //RIGHT CURLY BRACKET
                case 0x005C: tmp[j] = 0x1B; tmp[++j] = 0x2F; len++; break;   //REVERSE SOLIDUS
                case 0x005B: tmp[j] = 0x1B; tmp[++j] = 0x3C; len++; break;   //LEFT SQUARE BRACKET
                case 0x007E: tmp[j] = 0x1B; tmp[++j] = 0x3D; len++; break;   //TILDE
                case 0x005D: tmp[j] = 0x1B; tmp[++j] = 0x3E; len++; break;   //RIGHT SQUARE BRACKET
                case 0x007C: tmp[j] = 0x1B; tmp[++j] = 0x40; len++; break;   //VERTICAL LINE
                default: tmp[j] = pSrc[i];
                //case 0x20AC; tmp[j] = 0x1B; tmp[++j] = 0x65; break; //EURO SIGN
                }

                for(k=0; k<128; k++) {
                        if(pSrc[i] == EQ7BIT2ASCII[k] ) {
                                tmp[j] = k;
                                break;
                        }
                }
		if(j<5 || j>150)
		printf(" ++ j %d tmp[j] %X\n", j, tmp[j]);
        }

		printf(" ++ len %d \n", len);
        nSrc = 0;
        nDst = 0;

        unsigned char* p;
        p = tmp;
	*pDst = (*p << 1);
	pDst++;
	p++;
	nDst++;

        while (nSrc < len) {
                nChar = nSrc & 7;
                if(nChar == 0) {
                        nLeft = *p;
                } else {
                        *pDst = (*p << (8-nChar)) | nLeft;
                        nLeft = *p >> nChar;
                        pDst++;
                        nDst++;
                }
                p++;
                nSrc++;
        }

        free(tmp);

        *pUDLen = len; //Setting User Data length
	printf("-----------TP UDL %d nDst %d\n", len, nDst);
        //*pUDLen = len; //Setting User Data length

        return nDst;
}


#endif
/**
 * \fn       int gsmEncode8bit(const char* pSrc,unsigned char* pDst,int nSrcLength)
 * \brief  
 * \param  const char* pSrc
 *             unsigned char* pDst
 *            int nSrcLength
 * \return  the length of the pDst.
 */
static int gsmEncode8bit(const char* pSrc,unsigned char* pDst,int nSrcLength)
{
	memcpy(pDst,pSrc,nSrcLength);
	return nSrcLength;
}

static int gsm_is_valid_number(const char* number)
{
	if(NULL == number) {
		return 0;
	}

	if(number[0]!='+' && !(number[0]<='9' && number[0]>='0')) {
		return 0;
	}

	int i;
	for(i=1; number[i] != '\0'; i++) {
		if(number[i]<='9' && number[i]>='0') {
			continue;
		} else {
			return 0;
		}
	}

	return 1;
}
#ifdef PDU_LONG
static int gsm_encode_pdu(const char* SCA, const char* TPA, char TP_PID, char TP_DCS, const char* TP_UD, gsm_sms_pdu* long_pdu, char* pDst){
#else
static int gsm_encode_pdu(const char* SCA, const char* TPA, char TP_PID, char TP_DCS, const char* TP_UD, char* pDst){
#endif

	int nLength; 
	int nDstLength; 	
	unsigned char buf[256];
	char udh_total[16];
	char udh_part_num[16];

	if(!gsm_is_valid_number(TPA)) {
		return 0;
	}

	//SMS Center
	/////////////////////////////////////////////////////////////////////////
	nLength = strlen(SCA); 
	if(nLength) {
		if(!gsm_is_valid_number(SCA)) {
			return 0;
		}
		buf[0] = ((char)((nLength & 1) == 0 ? nLength : nLength + 1))/2 + 1;
		buf[1] = 0x91;
		nDstLength = gsmBytes2String(buf, pDst, 2); 
	} else {
		buf[0] = 0;
		nDstLength = gsmBytes2String(buf, pDst, 1);
	}
	nDstLength += gsmInvertNumbers(SCA, &pDst[nDstLength], nLength); 
	/////////////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////////
	if(TPA[0] == '+' ) {
		nLength = strlen(&(TPA[1])); 
		buf[0] = 0x71;	 
		buf[1] = 0;  
		buf[2] = (char)nLength; 	
		buf[3] = 0x91;	
		nDstLength += gsmBytes2String(buf, &pDst[nDstLength], 4);
		nDstLength += gsmInvertNumbers(&(TPA[1]), &pDst[nDstLength], nLength);
	} else {
		nLength = strlen(TPA); 
		buf[0] = 0x71;
		buf[1] = 0;
		buf[2] = (char)nLength;
		buf[3] = 0x81;	
		nDstLength += gsmBytes2String(buf, &pDst[nDstLength], 4);
		nDstLength += gsmInvertNumbers(TPA, &pDst[nDstLength], nLength);
	}
	////////////////////////////////////////////////////////////////////////////
	
	nLength = strlen(TP_UD);
	buf[0] = TP_PID;
	buf[1] = TP_DCS;
	buf[2] = 0;
	if(TP_DCS == GSM_7BIT) {
                nLength = gsmEncode7bit(TP_UD, &buf[10], nLength, &buf[3]);
/*********************** UDH*************/	
		buf[3] = (char)(buf[3]+7);
                buf[4] = 0x05;
                buf[5] = 0x00;
		buf[6] = 0x03;
		buf[7] = 0x00;
                buf[8] = (char)long_pdu->total_parts; 
                buf[9] = (char)long_pdu->part_num;
/******************************************************************************/
		nLength = nLength + 10;
                nDstLength += gsmBytes2String(buf, &pDst[nDstLength], nLength);

	} else if(TP_DCS == GSM_UCS2) {
		int leng_ucs = 254;
		unsigned char out[255];

		leng_ucs=myUTF8_to_UNICODE(out, (unsigned char*)TP_UD,strlen(TP_UD));
/*********************** UDH*************/
		buf[3] =gsmBytes2String_pdu(out, &buf[10],leng_ucs);
                buf[3] = (char)(buf[3]+6);
                buf[4] = 0x05;
                buf[5] = 0x00;
                buf[6] = 0x03;
                buf[7] = 0x00;
                buf[8] = (char)long_pdu->total_parts;
                buf[9] = (char)long_pdu->part_num;
/******************************************************************************/
/*else
		buf[3] =gsmBytes2String_pdu(out, &buf[4],leng_ucs);
*/
		nLength = buf[3]+4;
		nDstLength += gsmBytes2String(buf, &pDst[nDstLength],nLength);
	} else {
		buf[3] = gsmEncode8bit(TP_UD, &buf[4], nLength);
		nLength = buf[3] + 4; 
		nDstLength += gsmBytes2String(buf, &pDst[nDstLength], nLength); 
	}
	
	return nDstLength;
}
#if 0
int allogsm_encode_pdu_ucs2(const char* SCA, const char* TPA, char* TP_UD, const char* coding,char* pDst)
{
	char mesg[1024];
	
	if(coding == NULL || strlen(coding) == 0)
		code_convert("ASCII","UTF-8",TP_UD,strlen(TP_UD),mesg,1024);
	else
		code_convert(coding,"UTF-8",TP_UD,strlen(TP_UD),mesg,1024);
	
	if(SCA == NULL)
		return gsm_encode_pdu("", TPA, 0, GSM_UCS2, mesg, pDst);
		
	return gsm_encode_pdu(SCA, TPA, 0, GSM_UCS2, mesg, pDst);
}
#else
#ifdef PDU_LONG
int allogsm_encode_pdu_ucs2(const char* smsc, const char* dest, unsigned char* sms_data, const char* coding, gsm_sms_pdu* long_pdu, unsigned char* pdu){
#else
int allogsm_encode_pdu_ucs2(const char* smsc, const char* dest, char* sms_data, const char* coding, char* pdu){
#endif
        char mesg[1024];
        char text_coding[256];
        char TP_DCS;  //UCS2,7BIT or 8BIT
        if(coding == NULL || strlen(coding) == 0) {  //Use default ASCII
                strcpy(text_coding,"ASCII");
        } else {
                strncpy(text_coding,coding,256);
                to_upper_string(text_coding);
        }

        if(strstr(text_coding,"UTF-8")) {
                strncpy(mesg,sms_data,1024);
                TP_DCS = GSM_7BIT; //Assume is ASCII
                unsigned char* p = (unsigned char*)mesg;
                while(*p != '\0') {
                        if(*p >= 128) { //It's not ASCII
                                TP_DCS = GSM_UCS2;
                                break;
                        }
                        p++;
                }
        } else if(strstr(text_coding,"ASCII")) { //ASCII same to UTF-8
                strncpy(mesg,sms_data,1024);
                TP_DCS = GSM_7BIT;
        } else {
                code_convert(text_coding,"UTF-8",sms_data,strlen(sms_data),mesg,1024);
                TP_DCS = GSM_UCS2;
        }

        return gsm_encode_pdu(smsc == NULL ? "" : smsc , dest, 0, TP_DCS, mesg, long_pdu, pdu);
}

#endif
int allogsm_forward_pdu(const char* src_pdu,const char* TPA,const char* SCA, char* pDst)
{
	int nLength; 
	int nDstLength; 	
	unsigned char buf[256]; 

	//SMS Center
	/////////////////////////////////////////////////////////////////////////
	if(0==strcmp(SCA,"0")) {
		SCA = "";
	} else if(0==strcmp(SCA,"1")) {
		//
	}
	
	nLength = strlen(SCA); 
	if( nLength ) {
		buf[0] = ((char)((nLength & 1) == 0 ? nLength : nLength + 1))/2 + 1;
		buf[1] = 0x91;
		nDstLength = gsmBytes2String(buf, pDst, 2); 
	} else {
		buf[0] = 0;
		nDstLength = gsmBytes2String(buf, pDst, 1);
	}
	nDstLength += gsmInvertNumbers(SCA, &pDst[nDstLength], nLength); 
	/////////////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////////
	if(TPA[0] == '+' ) {
		nLength = strlen(&(TPA[1])); 
		buf[0] = 0x31;	 
		buf[1] = 0;  
		buf[2] = (char)nLength; 	
		buf[3] = 0x91;	
		nDstLength += gsmBytes2String(buf, &pDst[nDstLength], 4);
		nDstLength += gsmInvertNumbers(&(TPA[1]), &pDst[nDstLength], nLength);
	} else {
		nLength = strlen(TPA); 
		buf[0] = 0x31;
		buf[1] = 0;
		buf[2] = (char)nLength;
		buf[3] = 0x81;	
		nDstLength += gsmBytes2String(buf, &pDst[nDstLength], 4);
		nDstLength += gsmInvertNumbers(TPA, &pDst[nDstLength], nLength);
	}
	////////////////////////////////////////////////////////////////////////////

	int len;
	char *p = (char*)src_pdu;
	len = gsm_hex2int(p,2);
	p = p+2+len*2+2;  //Point to OA len;
		
	len = gsm_hex2int(p,2);
	if(len&1)	//if odd need add one
		len++;
	
	p = p+2+2+len;	//Point pid;
	pDst[nDstLength++] = *p;
	pDst[nDstLength++] = *(p+1);
	
	p = p+2;		//Point dcs;
	pDst[nDstLength++] = *p;
	pDst[nDstLength++] = *(p+1);
		
	//VP		
	pDst[nDstLength++] = '0';
	pDst[nDstLength++] = '0';
	
	p = p + 2 + 14;		//Point to ULD;
		
	//ULD and UD	
	while (*p != '\0') {
		pDst[nDstLength++] = *(p++);
	}
	
	pDst[nDstLength] = '\0';
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//Freedom Add 2012-01-29 15:48
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
char* pdu_get_send_number(const char* pdu, char* number, int len)
{
	const char* p;
	int number_len;
	char tmp[512];
	p=pdu;
	
	number_len=gsm_hex2int(p,2);

	if(0 == number_len) {
		p += 2+2+2;
	} else {
		p += 2+2+number_len*2+2+2;
	}

	number_len=gsm_hex2int(p,2);

	if(number_len&1)
		number_len+=1;

	strncpy(tmp,p+2+2,number_len);
	tmp[number_len]='\0';

	gsm_dso2string(number, tmp);

	return number;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


