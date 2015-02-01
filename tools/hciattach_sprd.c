#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <linux/kernel.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <termios.h>
#include <time.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <dirent.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "hciattach.h"
#include <sys/stat.h>

#include "hciattach_sprd.h"

#ifdef SPRD_DBG
#define LOGD(...)
#else
#define LOGD printf
#endif


#define PROPERTY_VALUE_MAX 256

typedef unsigned int   UWORD32;
typedef unsigned short UWORD16;
typedef unsigned char  UWORD8;


// pskey file structure default value
BT_PSKEY_CONFIG_T bt_para_setting={
5,
0,
0,
0,
0,
0x18cba80,
0x001f00,
0x1e,
{0x7a00,0x7600,0x7200,0x5200,0x2300,0x0300},
{0XCe418CFE,
 0Xd0418CFE,0Xd2438CFE,
 0Xd4438CFE,0xD6438CFE},
{0xFF, 0xFF, 0x8D, 0xFE, 0x9B, 0xFF, 0x79, 0x83,
  0xFF, 0xA7, 0xFF, 0x7F, 0x00, 0xE0, 0xF7, 0x3E},
{0x11, 0xFF, 0x0, 0x22, 0x2D, 0xAE},
0,
1,
5,
0x0e,
0xFFFFFFFF,
0x30,
0x3f,
0,
{0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000}
};

static unsigned int get_krandom(void)
{
    unsigned int ticks=0;
    struct timeval tv;
    int fd = 0;
    LOGD("enter:get_krandom\n");

    fd = open("/dev/urandom",O_RDONLY);
    if(fd>=0)
    {
        unsigned int r;
        int i;
        for(i=0;i<20;i++)
        {
            read(fd,&r,sizeof(r));
            ticks += r;
        }
        close(fd);
    }
    LOGD("ticks:[0x%x]1\n",ticks);

    gettimeofday(&tv,NULL);
    ticks = ticks + tv.tv_sec + tv.tv_usec;

    LOGD("ticks:[0x%x]2\n",ticks);

    return ticks;
}

static int create_mac_folder(void)
{
	DIR *dp;
	int err;

	dp = opendir(BD_MAC_FILE_PATH);
	if (dp == NULL) {
		if (mkdir(BD_MAC_FILE_PATH, 0755) < 0) {
			err = -errno;
			LOGD("%s:  mkdir: %s(%d)",__FUNCTION__, strerror(-err), -err);
		}
		return -1;
	}

	closedir(dp);
	return 0;
}

static void mac_rand(char *btmac)
{
	int fd,i, j, k;
	char buf[80];
	int size = 0,counter = 80;
	unsigned int randseed;
	unsigned int randseed2;

	LOGD("mac_rand");

	memset(buf, 0, sizeof(buf));

	if(access(BT_MAC_FILE, F_OK) == 0) {
		LOGD("%s: %s exists",__FUNCTION__, BT_MAC_FILE);
		fd = open(BT_MAC_FILE, O_RDWR);
		if(fd>=0) {
			size = read(fd, buf, sizeof(buf));
			LOGD("%s: read %s %s, size=%d",__FUNCTION__, BT_MAC_FILE, buf, size);
			if(size == BT_RAND_MAC_LENGTH){
				LOGD("bt mac already exists, no need to random it");
				strcpy(btmac, buf);
				close(fd);
				LOGD("%s: read btmac=%s",__FUNCTION__, btmac);
				return;
			}
			close(fd);
		}
	}
        LOGD("%s: there is no bt mac, random it", __FUNCTION__);
	k=0;
	for(i=0; i<counter; i++)
		k += buf[i];

	//rand seed
	randseed = (unsigned int) time(NULL) + k*fd*counter + buf[counter-2];
	randseed2 = get_krandom();
	randseed = randseed + randseed2;

	LOGD("%s: randseed=%d",__FUNCTION__, randseed);
	srand(randseed);

	//FOR BT
	i=rand(); j=rand();
	LOGD("%s:  rand i=0x%x, j=0x%x",__FUNCTION__, i,j);
	sprintf(btmac, "00:%02x:%02x:%02x:%02x:%02x", \
							(unsigned char)((i>>8)&0xFF), \
							(unsigned char)((i>>16)&0xFF), \
							(unsigned char)((j)&0xFF), \
							(unsigned char)((j>>8)&0xFF), \
							(unsigned char)((j>>16)&0xFF));
}


static void write_btmac2file(char *btmac)
{
	int fd;
	fd = open(BT_MAC_FILE, O_CREAT|O_RDWR|O_TRUNC, 0666);
	LOGD("write_btmac2file open file, fd=%d", fd);
	if(fd >= 0) {
		if(chmod(BT_MAC_FILE,0666) != -1){
			write(fd, btmac, strlen(btmac));
		}
		close(fd);
	}else{
		LOGD("write bt mac to file failed!!");
	}
}

uint8 ConvertHexToBin(
			uint8        *hex_ptr,     // in: the hexadecimal format string
			uint16       length,       // in: the length of hexadecimal string
			uint8        *bin_ptr      // out: pointer to the binary format string
			){
    uint8        *dest_ptr = bin_ptr;
    uint32        i = 0;
    uint8        ch;

    for(i=0; i<length; i+=2){
		    // the bit 8,7,6,5
				ch = hex_ptr[i];
				// digital 0 - 9
				if (ch >= '0' && ch <= '9')
				    *dest_ptr =(uint8)((ch - '0') << 4);
				// a - f
				else if (ch >= 'a' && ch <= 'f')
				    *dest_ptr = (uint8)((ch - 'a' + 10) << 4);
				// A - F
				else if (ch >= 'A' && ch <= 'F')
				    *dest_ptr = (uint8)((ch -'A' + 10) << 4);
				else{
				    return 0;
				}

				// the bit 1,2,3,4
				ch = hex_ptr[i+1];
				// digtial 0 - 9
				if (ch >= '0' && ch <= '9')
				    *dest_ptr |= (uint8)(ch - '0');
				// a - f
				else if (ch >= 'a' && ch <= 'f')
				    *dest_ptr |= (uint8)(ch - 'a' + 10);
				// A - F
				else if (ch >= 'A' && ch <= 'F')
				    *dest_ptr |= (uint8)(ch -'A' + 10);
				else{
			            return 0;
				}

				dest_ptr++;
	  }

    return 1;
}

int sprd_config_init(int fd, char *bdaddr, struct termios *ti)
{
	int i,psk_fd,fd_btaddr,ret = 0,r,size=0,read_btmac=0;
	unsigned char resp[30];
	BT_PSKEY_CONFIG_T bt_para_tmp;
	char bt_mac[30] = {0};
	char bt_mac_tmp[20] = {0};
	uint8 bt_mac_bin[32]     = {0};
#if 0
	/*The below code ment to inform the controller about single connection or multiple connection. */
	/*0 - single connection, 1 - Mulitple connectin*/
	char bt_singleconn[PROPERTY_VALUE_MAX];
	char singlewifion[PROPERTY_VALUE_MAX];
#endif
	fprintf(stderr,"init_sprd_config in \n");

	if(access(BT_MAC_FILE, F_OK) == 0) {
		LOGD("%s: %s exists",__FUNCTION__, BT_MAC_FILE);
		fd_btaddr = open(BT_MAC_FILE, O_RDWR);
		if(fd_btaddr>=0) {
			size = read(fd_btaddr, bt_mac, sizeof(bt_mac));
			LOGD("%s: read %s %s, size=%d",__FUNCTION__, BT_MAC_FILE, bt_mac, size);
			if(size == BT_RAND_MAC_LENGTH){
						LOGD("bt mac already exists, no need to random it");
						fprintf(stderr, "read btmac ok \n");
						read_btmac=1;
			}
			close(fd_btaddr);
		}
		for(i=0; i<6; i++){
				bt_mac_tmp[i*2] = bt_mac[3*(5-i)];
				bt_mac_tmp[i*2+1] = bt_mac[3*(5-i)+1];
		}
		LOGD("====bt_mac_tmp=%s", bt_mac_tmp);
		printf("====bt_mac_tmp=%s\n", bt_mac_tmp);
		ConvertHexToBin(bt_mac_tmp, strlen(bt_mac_tmp), bt_mac_bin);
	}else{
		fprintf(stderr, "btmac.txt not exsit!\n");
		if(create_mac_folder())
			return -1;

		read_btmac=0;
		mac_rand(bt_mac);
		LOGD("bt random mac=%s",bt_mac);
		printf("bt_mac=%s\n",bt_mac);
		write_btmac2file(bt_mac);

		fd_btaddr = open(BT_MAC_FILE, O_RDWR);
		if(fd_btaddr>=0) {
			size = read(fd_btaddr, bt_mac, sizeof(bt_mac));
			LOGD("%s: read %s %s, size=%d",__FUNCTION__, BT_MAC_FILE, bt_mac, size);
			if(size == BT_RAND_MAC_LENGTH){
						LOGD("bt mac already exists, no need to random it");
						fprintf(stderr, "read btmac ok \n");
						read_btmac=1;
			}
			close(fd_btaddr);
		}
		for(i=0; i<6; i++){
				bt_mac_tmp[i*2] = bt_mac[3*(5-i)];
				bt_mac_tmp[i*2+1] = bt_mac[3*(5-i)+1];
		}
		LOGD("====bt_mac_tmp=%s", bt_mac_tmp);
		printf("====bt_mac_tmp=%s\n", bt_mac_tmp);
		ConvertHexToBin(bt_mac_tmp, strlen(bt_mac_tmp), bt_mac_bin);
	}

	/* Reset the BT Chip */

	memset(resp, 0, sizeof(resp));
	memset(&bt_para_tmp, 0, sizeof(BT_PSKEY_CONFIG_T) );
	ret = getPskeyFromFile(  (void *)(&bt_para_tmp) );//ret = get_pskey_from_file(&bt_para_tmp);
		if(ret != 0){
			fprintf(stderr, "get_pskey_from_file faill \n");
			/* Send command from hciattach*/
			if(read_btmac == 1){
				memcpy(bt_para_setting.device_addr, bt_mac_bin, sizeof(bt_para_setting.device_addr));
			}
			if (write(fd, (char *)&bt_para_setting, sizeof(BT_PSKEY_CONFIG_T)) != sizeof(BT_PSKEY_CONFIG_T)) {
				fprintf(stderr, "Failed to write reset command\n");
				return -1;
			}
		}else{
#if 0
			/* Send command from pskey_bt.txt*/
			property_get("ro.system.property.singleconn",bt_singleconn,"");
			property_get("ro.system.property.singleWifiOn",singlewifion,"");

			if((strcmp(bt_singleconn,"true") == 0)
				||(strcmp(singlewifion,"true") == 0))
			{
				LOGD("single true...");
				bt_para_tmp.reserved[0] |= 0x1;
			}
#else
			bt_para_tmp.reserved[0] |= 0x1;

#endif
			if(read_btmac == 1){
				memcpy(bt_para_tmp.device_addr, bt_mac_bin, sizeof(bt_para_tmp.device_addr));
			}
			if (write(fd, (char *)&bt_para_tmp, sizeof(BT_PSKEY_CONFIG_T)) != sizeof(BT_PSKEY_CONFIG_T)) {
				fprintf(stderr, "Failed to write reset command\n");
				return -1;
			}else{
				fprintf(stderr, "get_pskey_from_file down ok\n");
			}
		}

	while (1) {
		r = read(fd, resp, 1);
		if (r <= 0)
			return -1;
		if (resp[0] == 0x05){
			fprintf(stderr, "read response ok \n");
			break;
		}
	}

	return 0;
}
