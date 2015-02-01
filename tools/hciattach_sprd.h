#ifndef HCIATTACH_SPRD_H__
#define HCIATTACH_SPRD_H__

#define MAC_ERROR		"FF:FF:FF:FF:FF:FF"

#define	BD_MAC_FILE_PATH	"/csa/bluetooth"
#define BT_MAC_FILE	"/csa/bluetooth/.bd_addr"
#define BT_RAND_MAC_LENGTH   17


typedef unsigned char uint8;
typedef unsigned int uint32;
typedef unsigned short uint16;

#define BT_ADDRESS_SIZE    6


// add by longting.zhao for pskey NV
// pskey file structure
typedef struct SPRD_BT_PSKEY_INFO_T{
    uint8	pskey_cmd;//add h4 cmd 5 means pskey cmd
    uint8   g_dbg_source_sink_syn_test_data;
    uint8   g_sys_sleep_in_standby_supported;
    uint8   g_sys_sleep_master_supported;
    uint8   g_sys_sleep_slave_supported;
    uint32  default_ahb_clk;
    uint32  device_class;
    uint32  win_ext;
    uint32  g_aGainValue[6];
    uint32  g_aPowerValue[5];
    uint8   feature_set[16];
    uint8   device_addr[6];
    uint8  g_sys_sco_transmit_mode; //0: DMA 1: UART 2:SHAREMEM
    uint8  g_sys_uart0_communication_supported; //true use uart0, otherwise use uart1 for debug
    uint8 edr_tx_edr_delay;
    uint8 edr_rx_edr_delay;
    uint32 g_PrintLevel;
    uint16 uart_rx_watermark;
    uint16 uart_flow_control_thld;
    uint32 comp_id;
	uint16 pcm_clk_divd;
	uint16 half_word_reserved;
	uint32 pcm_config;
/**********bt&wif public*********************/
	uint8 ref_clk;
	uint8 FEM_status;
	uint8 gpio_cfg;
	uint8 gpio_PA_en;
	uint8 wifi_tx;
	uint8 bt_tx;
	uint8 wifi_rx;
	uint8 bt_rx;
	uint8 wb_lna_bypass;
	uint8 gain_LNA;
	uint8 IL_wb_lna_bypass;
	uint8 Rx_adaptive;
	uint16 up_bypass_switching_point0;
	uint16 low_bypass_switching_point0;
/***************************************/
	uint32  reserved[4];
}BT_PSKEY_CONFIG_T;

int getPskeyFromFile(void *pData);

#endif /* HCIATTACH_SPRD_H__ */




