#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "hciattach_sprd.h"

#define _FILE_PARSE_DEBUG_
#define  CMD_ITEM_TABLE(ITEM, MEM_OFFSET, TYPE)    { ITEM,   (unsigned int)( &(  ((BT_PSKEY_CONFIG_T *)(0))->MEM_OFFSET )),   TYPE }
#define ALOGE printf

typedef struct
{
	char item[64];
	long int  par[32];
	int  num;
}cmd_par;

typedef struct
{
	char *item;
	unsigned int mem_offset;
	int type;
}cmd_par_table;

static cmd_par_table g_pskey_table[] =
{
	CMD_ITEM_TABLE("pskey_cmd", pskey_cmd, 1),
	CMD_ITEM_TABLE("g_dbg_source_sink_syn_test_data", g_dbg_source_sink_syn_test_data, 1),
	CMD_ITEM_TABLE("g_sys_sleep_in_standby_supported", g_sys_sleep_in_standby_supported, 1),
	CMD_ITEM_TABLE("g_sys_sleep_master_supported", g_sys_sleep_master_supported, 1),
	CMD_ITEM_TABLE("g_sys_sleep_slave_supported", g_sys_sleep_slave_supported, 1),
	CMD_ITEM_TABLE("default_ahb_clk", default_ahb_clk, 4),
	CMD_ITEM_TABLE("device_class", device_class, 4),
	CMD_ITEM_TABLE("win_ext", win_ext, 4),
	CMD_ITEM_TABLE("g_aGainValue", g_aGainValue, 4),
	CMD_ITEM_TABLE("g_aPowerValue", g_aPowerValue, 4),
	CMD_ITEM_TABLE("feature_set", feature_set, 1),
	CMD_ITEM_TABLE("device_addr", device_addr, 1),
	CMD_ITEM_TABLE("g_sys_sco_transmit_mode", g_sys_sco_transmit_mode, 1),
	CMD_ITEM_TABLE("g_sys_uart0_communication_supported", g_sys_uart0_communication_supported, 1),
	CMD_ITEM_TABLE("edr_tx_edr_delay", edr_tx_edr_delay, 1),
	CMD_ITEM_TABLE("edr_rx_edr_delay", edr_rx_edr_delay, 1),
	CMD_ITEM_TABLE("g_PrintLevel", g_PrintLevel, 4),
	CMD_ITEM_TABLE("uart_rx_watermark", uart_rx_watermark, 2),
	CMD_ITEM_TABLE("uart_flow_control_thld", uart_flow_control_thld, 2),
	CMD_ITEM_TABLE("comp_id", comp_id, 4),
	CMD_ITEM_TABLE("pcm_clk_divd", pcm_clk_divd, 2),
	CMD_ITEM_TABLE("half_word_reserved", half_word_reserved, 2),
	CMD_ITEM_TABLE("pcm_config", pcm_config, 4),
	CMD_ITEM_TABLE("ref_clk", ref_clk, 1),
	CMD_ITEM_TABLE("FEM_status", FEM_status, 1),
	CMD_ITEM_TABLE("gpio_cfg", gpio_cfg, 1),
	CMD_ITEM_TABLE("gpio_PA_en", gpio_PA_en, 1),
	CMD_ITEM_TABLE("wifi_tx", wifi_tx, 1),
	CMD_ITEM_TABLE("bt_tx", bt_tx, 1),
	CMD_ITEM_TABLE("wifi_rx", wifi_rx, 1),
	CMD_ITEM_TABLE("bt_rx", bt_rx, 1),
	CMD_ITEM_TABLE("wb_lna_bypass", wb_lna_bypass, 1),
	CMD_ITEM_TABLE("gain_LNA", gain_LNA, 1),
	CMD_ITEM_TABLE("IL_wb_lna_bypass", IL_wb_lna_bypass, 1),
	CMD_ITEM_TABLE("Rx_adaptive", Rx_adaptive, 1),
	CMD_ITEM_TABLE("up_bypass_switching_point0", up_bypass_switching_point0, 1),
	CMD_ITEM_TABLE("low_bypass_switching_point0", low_bypass_switching_point0, 1),

	CMD_ITEM_TABLE("bt_reserved", reserved[0], 4),

};

static int getFileSize(char *file)
{
	struct stat temp;
	stat(file, &temp);
	return temp.st_size;
}

static int find_type(char key)
{
	if( (key >= 'a' && key <= 'w') || (key >= 'y' && key <= 'z') || (key >= 'A' && key <= 'W') || (key >= 'Y' && key <= 'Z') || ('_' == key) )
		return 1;
	if( (key >= '0' && key <= '9') || ('-' == key) )
		return 2;
	if( ('x' == key) || ('X' == key) || ('.' == key) )
		return 3;
	if( (key == '\0') || ('\r' == key) || ('\n' == key) || ('#' == key) )
		return 4;
	return 0;
}

static void getCmdOneline(unsigned char *str, cmd_par *cmd)
{
	int i, j, bufType, cType, flag;
	char tmp[128];
	char c;
	bufType = -1;
	cType = 0;
	flag = 0;
	memset( cmd, 0, sizeof(cmd_par) );
	for(i = 0, j = 0; ; i++)
	{
		c = str[i];
		cType = find_type(c);
		if( (1 == cType) || ( 2 == cType) || (3 == cType)  )
		{
			tmp[j] = c;
			j++;
			if(-1 == bufType)
			{
				if(2 == cType)
					bufType = 2;
				else
					bufType = 1;
			}
			else if(2 == bufType)
			{
				if(1 == cType)
					bufType = 1;
			}
			continue;
		}
		if(-1 != bufType)
		{
			tmp[j] = '\0';

			if((1 == bufType) && (0 == flag) )
			{
				strcpy(cmd->item, tmp);
				flag = 1;
			}
			else
			{
				cmd->par[cmd->num] = atoll(tmp);
				cmd->num++;
			}
			bufType = -1;
			j = 0;
		}
		if(0 == cType )
			continue;
		if(4 == cType)
			return;
	}
	return;
}

static int getDataFromCmd(cmd_par_table *pTable, cmd_par *cmd,  void *pData)
{
	int i;
	unsigned char  *p;
	if( (1 != pTable->type)  && (2 != pTable->type) && (4 != pTable->type) )
		return -1;
	p = (unsigned char *)(pData) + pTable->mem_offset;
#ifdef _FILE_PARSE_DEBUG_
	char tmp[192] = {0};
	char string[16] = {0};
	sprintf(tmp, "###[pskey]%s, offset:%d, num:%d, value:   ", pTable->item, pTable->mem_offset, cmd->num);
	for(i=0; i<cmd->num; i++)
	{
		memset(string, 0, 16);
		sprintf(string, "0x%x, ", cmd->par[i] );
		strcat(tmp, string);
	}
	ALOGE("%s\n", tmp);
#endif
	for(i = 0; i < cmd->num;  i++)
	{
		if(1 == pTable->type)
			*((unsigned char *)p + i) = (unsigned char)(cmd->par[i]);
		else if(2 == pTable->type)
			*((unsigned short *)p + i) = (unsigned short)(cmd->par[i]);
		else if(4 == pTable->type)
			*( (unsigned int *)p + i) = (unsigned int)(cmd->par[i]);
		else
			ALOGE("%s, type err\n", __func__);
	}
	return 0;
}

static cmd_par_table *cmd_table_match(cmd_par *cmd)
{
	int i;
	cmd_par_table *pTable = NULL;
	int len = sizeof(g_pskey_table) / sizeof(cmd_par_table);
	if(NULL == cmd->item)
		return NULL;
	for(i = 0; i < len; i++)
	{
		if(NULL == g_pskey_table[i].item)
			continue;
		if( 0 != strcmp( g_pskey_table[i].item, cmd->item ) )
			continue;
		pTable = &g_pskey_table[i];
		break;
	}
	return pTable;
}


static int getDataFromBuf(void *pData, unsigned char *pBuf, int file_len)
{
	int i, p;
	cmd_par cmd;
	cmd_par_table *pTable = NULL;
	if((NULL == pBuf) || (0 == file_len) || (NULL == pData) )
		return -1;
	for(i = 0, p = 0; i < file_len; i++)
	{
		if( ('\n' == *(pBuf + i)) || ( '\r' == *(pBuf + i)) || ( '\0' == *(pBuf + i) )   )
		{
			if(5 <= (i - p) )
			{
				getCmdOneline((pBuf + p), &cmd);
				pTable = cmd_table_match(&cmd);
				if(NULL != pTable)
				{
					getDataFromCmd(pTable, &cmd, pData);
				}
			}
			p = i + 1;
		}

	}
	return 0;
}

static int dumpPskey(BT_PSKEY_CONFIG_T *p)
{
	int i;
	char tmp[128];
	char string[16];
	ALOGE("pskey_cmd:0x%x\n", p->pskey_cmd);
	ALOGE("g_dbg_source_sink_syn_test_data:0x%x\n", p->g_dbg_source_sink_syn_test_data);
	ALOGE("g_sys_sleep_in_standby_supported:0x%x\n", p->g_sys_sleep_in_standby_supported);
	ALOGE("g_sys_sleep_master_supported:0x%x\n", p->g_sys_sleep_master_supported);
	ALOGE("g_sys_sleep_slave_supported:0x%x\n", p->g_sys_sleep_slave_supported);
	ALOGE("default_ahb_clk:0x%x\n",  p->default_ahb_clk);
	ALOGE("device_class:0x%x\n", p->device_class);
	ALOGE("win_ext:0x%x\n", p->win_ext);
	memset(tmp,0, 128);
	memset(string,0,16);
	sprintf(tmp, "g_aGainValue: ");
	for(i=0; i<6; i++)
	{
		sprintf(string, "0x%x, ", p->g_aGainValue[i]);
		strcat(tmp, string);
	}
	ALOGE("%s\n", tmp);

	memset(tmp,0, 128);
	memset(string,0,16);
	sprintf(tmp, "g_aPowerValue: ");
	for(i=0; i<5; i++)
	{
		sprintf(string, "0x%x, ", p->g_aPowerValue[i]);
		strcat(tmp, string);
	}
	ALOGE("%s\n", tmp);

	memset(tmp,0, 128);
	memset(string,0,16);
	sprintf(tmp, "feature_set: ");
	for(i=0; i<16; i++)
	{
		sprintf(string, "0x%x, ", p->feature_set[i]);
		strcat(tmp, string);
	}
	ALOGE("%s\n", tmp);


	memset(tmp,0, 128);
	memset(string,0,16);
	sprintf(tmp, "device_addr: ");
	for(i=0; i<6; i++)
	{
		sprintf(string, "0x%x, ", p->device_addr[i]);
		strcat(tmp, string);
	}
	ALOGE("%s\n", tmp);

	ALOGE("g_sys_sco_transmit_mode:0x%x\n", p->g_sys_sco_transmit_mode);
	ALOGE("g_sys_uart0_communication_supported:0x%x\n",  p->g_sys_uart0_communication_supported);
	ALOGE("edr_tx_edr_delay:0x%x\n", p->edr_tx_edr_delay);
	ALOGE("edr_rx_edr_delay:0x%x\n", p->edr_rx_edr_delay);
	ALOGE("g_PrintLevel:0x%x\n", p->g_PrintLevel);
	ALOGE("uart_rx_watermark:0x%x\n", p->uart_rx_watermark);
	ALOGE("uart_flow_control_thld:0x%x\n", p->uart_flow_control_thld);
	ALOGE("comp_id:0x%x\n", p->comp_id);
	ALOGE("pcm_clk_divd:0x%x\n", p->pcm_clk_divd);
	ALOGE("half_word_reserved:0x%x\n", p->half_word_reserved);
	ALOGE("pcm_config:0x%x\n", p->pcm_config);

	ALOGE("ref_clk:0x%x\n", p->ref_clk);
	ALOGE("ref_clk:0x%x\n", p->ref_clk);
	ALOGE("gpio_cfg:0x%x\n", p->gpio_cfg);
	ALOGE("gpio_PA_en:0x%x\n", p->gpio_PA_en);
	ALOGE("wifi_tx:0x%x\n", p->wifi_tx);
	ALOGE("bt_tx:0x%x\n", p->bt_tx);
	ALOGE("wifi_rx:0x%x\n", p->wifi_rx);
	ALOGE("bt_rx:0x%x\n", p->bt_rx);
	ALOGE("wb_lna_bypass:0x%x\n", p->wb_lna_bypass);
	ALOGE("gain_LNA:0x%x\n", p->gain_LNA);
	ALOGE("IL_wb_lna_bypass:0x%x\n", p->IL_wb_lna_bypass);
	ALOGE("Rx_adaptive:0x%x\n", p->Rx_adaptive);
	ALOGE("up_bypass_switching_point0:0x%x\n", p->up_bypass_switching_point0);
	ALOGE("low_bypass_switching_point0:0x%x\n", p->low_bypass_switching_point0);

	memset(tmp,0, 128);
	memset(string,0,16);
	sprintf(tmp, "reserved: ");
	for(i=0; i<4; i++)
	{
		sprintf(string, "0x%x, ", p->reserved[i]);
		strcat(tmp, string);
	}
	ALOGE("%s\n", tmp);


	return 0;
}

int getPskeyFromFile(void *pData)
{
	int ret = -1;
	int fd;
	int len;
	unsigned char *pBuf = NULL;

	char *CFG_2351_PATH = "/usr/etc/bluetooth/connectivity_configure.ini";

	fd = open(CFG_2351_PATH, O_RDONLY, 0644);
	if(-1 != fd)
	{
		len = getFileSize(CFG_2351_PATH);
		pBuf = (unsigned char *)malloc(len);
		ret = read(fd, pBuf, len);
		ALOGE("%s read %s size:%d fd2:%d\n", __FUNCTION__, CFG_2351_PATH, len, fd);
	}
	else
	{
		ALOGE("%s open %s ret:%d\n", __FUNCTION__, CFG_2351_PATH, fd);
	}

	ret = getDataFromBuf(pData, pBuf, len);
	free(pBuf);

	if(-1 == ret)
	{
		return -1;
	}

	dumpPskey((BT_PSKEY_CONFIG_T *)pData);
	return 0;
}


