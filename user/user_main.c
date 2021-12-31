/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_common.h"
#include "uart.h"
#include "gpio.h"
#include "hw_timer.h"
#include "espconn.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdio.h"
#include "stdint.h"
#include "ws28xx-esp8266.h"
#include "freertos/semphr.h"
#include "lwip/ipv4/lwip/ip4_addr.h"
#include "types_enums.h"
#include "misc_functions.h"

#define FX_PARAM_ADDR			0x3fa000
#define NET_PARAM_ADDR			0x3f9000
#define RCV_DATA_SHADOW_SIZE	1500

struct FX_param{
	int16_t leds;
};

struct message{
	struct FX_param *param;
	struct ws28xx_leds *leds;
	void *conn;
};

#define LOCAL_PORT	8000

uint16_t local_port = LOCAL_PORT;

volatile uint8_t config_mode = NORMAL_MODE;

struct espconn udp_conn;
struct espconn tcp_conn;
struct espconn log_conn;

const uint8 log_server_ip[4] = { 192, 168, 1, 1 };
const uint16 log_server_port = 515;

uint8 configured = 0;

struct message _message;
struct FX_param fx_param;
char *rcv_data_shadow;


#define REPLY_MESSAGE_SIZE		256
char *reply_message;

void print_hex(char *name, void *buff, uint16_t size); 

uint8 gateway[4] = { 192, 168, 1, 1 };
uint8 ipaddr[4] = { 192, 168, 1, 87 };
uint8 netmask[4] = { 255, 255, 255, 0 };

network_settings *network_settings_s;

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
 *******************************************************************************/
uint32 user_rf_cal_sector_set(void) {
	flash_size_map size_map = system_get_flash_size_map();
	uint32 rf_cal_sec = 0;

	switch (size_map) {
		case FLASH_SIZE_4M_MAP_256_256:
			rf_cal_sec = 128 - 5;
			break;

		case FLASH_SIZE_8M_MAP_512_512:
			rf_cal_sec = 256 - 5;
			break;

		case FLASH_SIZE_16M_MAP_512_512:
		case FLASH_SIZE_16M_MAP_1024_1024:
			rf_cal_sec = 512 - 5;
			break;

		case FLASH_SIZE_32M_MAP_512_512:
		case FLASH_SIZE_32M_MAP_1024_1024:
			rf_cal_sec = 1024 - 5;
			break;
		case FLASH_SIZE_64M_MAP_1024_1024:
			rf_cal_sec = 2048 - 5;
			break;
		case FLASH_SIZE_128M_MAP_1024_1024:
			rf_cal_sec = 4096 - 5;
			break;
		default:
			rf_cal_sec = 0;
			break;
	}

	return rf_cal_sec;
}

void wifi_station_config(network_settings *ns) {
	struct station_config *st_conf = (struct station_config*) zalloc(
			sizeof(struct station_config));
	struct ip_info ip_info;
	if (ns->dhcp == 0) {
		wifi_station_dhcpc_stop();
	}
	sprintf(st_conf->ssid, ns->ap_name);
	if (strlen(ns->ap_passwd) >= 8) {
		sprintf(st_conf->password, ns->ap_passwd);
	}
	wifi_station_set_config(st_conf);
	if (ns->dhcp == 0) {
		IP4_ADDR(&ip_info.gw, ns->gateway[0], ns->gateway[1], ns->gateway[2],
				ns->gateway[3]);
		IP4_ADDR(&ip_info.ip, ns->ip_addr[0], ns->ip_addr[1], ns->ip_addr[2],
				ns->ip_addr[3]);
		IP4_ADDR(&ip_info.netmask, ns->netmask[0], ns->netmask[1],
				ns->netmask[2], ns->netmask[3]);
		wifi_set_ip_info(STATION_IF, &ip_info);
		wifi_get_ip_info(STATION_IF, &ip_info);
	}
	free(st_conf);
}

void wifi_softap_config() {
	struct softap_config *ap_config = (struct softap_config*) zalloc(
			sizeof(struct softap_config));
	wifi_softap_get_config(ap_config);
	sprintf(ap_config->ssid, "ESP8266-config");
	sprintf(ap_config->password, "12345678");
	ap_config->authmode = AUTH_OPEN;
	ap_config->ssid_len = 14;
	ap_config->max_connection = 4;
	wifi_softap_set_config(ap_config);
	free(ap_config);
}

void parse_data(char *pdata, unsigned short plen, char *message) {
	char* ptr;
	int x = 0;
	union rgb c;

	if (strncmp(pdata, "RAWDATA:", 8) == 0){
		ptr = &pdata[8];
		memcpy(_message.leds, ptr, fx_param.leds * 3);
		ws2812_push((uint8_t*) _message.leds, fx_param.leds * 3);
		sprintf(message, "RAWDATA OK\n\r");
		return;
	}

	if (strncmp(pdata, "RAW:", 4) == 0) {
		if (strncmp(&pdata[4], "RED:", 4) == 0){
			ptr = &pdata[8];
			for (x = 0; x < fx_param.leds; x++){
				c.red = *(ptr++);
				ws2812_set_pixel(_message.leds, x, c.c,
						COLOR_RED_MASK | COLOR_MIX_OVER, 100);
			}
			sprintf(message, "RED OK\n\r");
			return;
		}
		if (strncmp(&pdata[4], "GRN:", 4) == 0){
			ptr = &pdata[8];
			for (x = 0; x < fx_param.leds; x++){
				c.green = *(ptr++);
				ws2812_set_pixel(_message.leds, x, c.c,
						COLOR_GREEN_MASK | COLOR_MIX_OVER, 100);
			}
			sprintf(message, "GRN OK\n\r");
			return;
		}
		if (strncmp(&pdata[4], "BLU:", 4) == 0){
			ptr = &pdata[8];
			for (x = 0; x < fx_param.leds; x++){
				c.blue = *(ptr++);
				ws2812_set_pixel(_message.leds, x, c.c,
						COLOR_BLUE_MASK | COLOR_MIX_OVER, 100);
			}
			sprintf(message, "BLU OK\n\r");
			return;
		}
		if (strncmp(&pdata[4], "PUSH", 4) == 0){
			ws2812_push((uint8_t*) _message.leds, fx_param.leds * 3);
			sprintf(message, "PUSH OK\n\r");
		}
		return;
	}

	print_hex("Received udp data", pdata, plen);


	if (strncmp(pdata, "reboot", 6) == 0) {
		system_restart();
	}

//określenie ilości ledów w łańcuchu
	if (strncmp(pdata, "leds ", 4) == 0){
		uint16_t val = 0;
		ptr = &pdata[4];
		char *s = find_number(ptr);
		if (s == NULL){//jeśli nie znajdzie liczb 0-9 to kończy
			sprintf(message, "error\n\r");
			return;
		}
		fx_param.leds = atoi(s);
		sprintf(message, "please save param and reboot to continue\n\r");
		return;
	}

	if (strncmp(pdata, "get ", 4) == 0) {
		ptr = pdata + 4;

		if (strncmp(ptr, "params", 6) == 0) {
			int i = strlen(message);
			sprintf(&message[i], "leds = %d\n\rOK\n\r", fx_param.leds);
			return;
		}

		if (strncmp(ptr, "wifi", 4) == 0) {
			sprintf(message, "ap name:%s\n\r"
					"passwd: ????????\n\r", network_settings_s->ap_name);
			return;
		}

		if (strncmp(ptr, "network", 7) == 0) {
			sprintf(message, "ip addr: %d.%d.%d.%d\n\r"
					"netmask: %d.%d.%d.%d\n\r"
					"gateway: %d.%d.%d.%d\n\r"
					"port: %d\n\r"
					"dhcp: ", network_settings_s->ip_addr[0],
					network_settings_s->ip_addr[1],
					network_settings_s->ip_addr[2],
					network_settings_s->ip_addr[3],
					network_settings_s->netmask[0],
					network_settings_s->netmask[1],
					network_settings_s->netmask[2],
					network_settings_s->netmask[3],
					network_settings_s->gateway[0],
					network_settings_s->gateway[1],
					network_settings_s->gateway[2],
					network_settings_s->gateway[3],
					network_settings_s->port);
			if (network_settings_s->dhcp == 1) {
				strcat(message, "enabled\n\r");
			} else {
				strcat(message, "disabled\n\r");
			}
			return;
		}
	}

	if (strncmp(pdata, "set ", 4) == 0) {
		char *str = pdata + 4;
		int32_t seek = 0;
		if (strncmp(str, "ap_name:", 8) == 0) {
			str += 8;
			if ((seek = strcspn(str, "\n\r"))) {
				memset(network_settings_s->ap_name, 0, 63);
				memcpy(network_settings_s->ap_name, str, seek);
				sprintf(message, "OK\n\r");
				return;
			}
		}
		if (strncmp(str, "ap_passwd:", 10) == 0) {
			str += 10;
			if ((seek = strcspn(str, "\n\r"))) {
				memset(network_settings_s->ap_passwd, 0, 64);
				memcpy(network_settings_s->ap_passwd, str, seek);
				sprintf(message, "OK\n\r");
				return;
			}
		}
		if (strncmp(str, "ip_addr:", 8) == 0) {
			str += 7;
			char *pch;
			char *s = find_number(str);
			if (s == NULL){
				sprintf(message, "ip_addr error\n\r");
				return;
			}
			memset(network_settings_s->ip_addr, 0, 4);
			pch = strtok(s, ".");
			network_settings_s->ip_addr[0] = atoi(pch);
			pch = strtok(NULL, ".");
			network_settings_s->ip_addr[1] = atoi(pch);
			pch = strtok(NULL, ".");
			network_settings_s->ip_addr[2] = atoi(pch);
			pch = strtok(NULL, ".");
			network_settings_s->ip_addr[3] = atoi(pch);
			pch = strtok(NULL, ".");
			sprintf(message, "OK\n\r");
			return;
		}
		if (strncmp(str, "netmask:", 8) == 0) {
			str += 7;
			char *pch;
			char *s = find_number(str);
			if (s == NULL){
				sprintf(message, "netmask error\n\r");
				return;
			}
			memset(network_settings_s->netmask, 0, 4);
			pch = strtok(s, ".");
			network_settings_s->netmask[0] = atoi(pch);
			pch = strtok(NULL, ".");
			network_settings_s->netmask[1] = atoi(pch);
			pch = strtok(NULL, ".");
			network_settings_s->netmask[2] = atoi(pch);
			pch = strtok(NULL, ".");
			network_settings_s->netmask[3] = atoi(pch);
			pch = strtok(NULL, ".");
			sprintf(message, "OK\n\r");
			return;
		}
		if (strncmp(str, "gateway:", 8) == 0) {
			str += 7;
			char *pch;
			char *s = find_number(str);
			if (s == NULL){
				sprintf(message, "gateway error\n\r");
				return;
			}
			memset(network_settings_s->gateway, 0, 4);
			pch = strtok(s, ".");
			network_settings_s->gateway[0] = atoi(pch);
			pch = strtok(NULL, ".");
			network_settings_s->gateway[1] = atoi(pch);
			pch = strtok(NULL, ".");
			network_settings_s->gateway[2] = atoi(pch);
			pch = strtok(NULL, ".");
			network_settings_s->gateway[3] = atoi(pch);
			pch = strtok(NULL, ".");
			sprintf(message, "OK\n\r");
			return;
		}
		if (strncmp(str, "port:", 5) == 0) {
			str += 4;
			char *pch;
			char *s = find_number(str);
			if (s == NULL){
				sprintf(message, "port error\n\r");
				return;
			}
			network_settings_s->port = atoi(s);
			sprintf(message, "OK\n\r");
			return;
		}
		if (strncmp(str, "dhcp", 4) == 0) {
			network_settings_s->dhcp = 1;
			sprintf(message, "OK\n\r");
			return;
		}
		if (strncmp(str, "static", 6) == 0) {
			network_settings_s->dhcp = 0;
			sprintf(message, "OK\n\r");
			return;
		}
	}

	if (strncmp(pdata, "save network", 12) == 0) {
		if (spi_flash_erase_sector(NET_PARAM_ADDR / 4096) == SPI_FLASH_RESULT_OK) {
			os_printf("erase sector ok\n\r");
			if (spi_flash_write(NET_PARAM_ADDR, (uint32*) network_settings_s,
						sizeof(network_settings)) == SPI_FLASH_RESULT_OK) {
				os_printf("flash write ok\n\r");
				sprintf(message, "network config saved\n\r");
			} else {
				os_printf("flash write error\n\r");
			}
		} else {
			os_printf("erase sector error\n\r");
		}
		return;
	}
	if (strncmp(pdata, "save param", 10) == 0) {
		if (spi_flash_erase_sector(FX_PARAM_ADDR / 4096) == SPI_FLASH_RESULT_OK) {
			os_printf("erase sector ok\n\r");
			if (spi_flash_write(FX_PARAM_ADDR, (uint32*) &fx_param,
						sizeof(struct FX_param)) == SPI_FLASH_RESULT_OK) {
				os_printf("flash write ok\n\r");
				sprintf(message, "fx params saved\n\r");
			} else {
				os_printf("flash write error\n\r");
			}
		} else {
			os_printf("erase sector error\n\r");
		}
		return;
	}
	strcat(message, "UNKNOWN COMMAND\n\r");
}

void print_hex(char *name, void *buff, uint16_t size) {
	uint16_t x;
	uint8_t *buf = (uint8_t*) buff;
	os_printf("\n\rName: %s\n\r", name);
	os_printf("\n\r"
			"       0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f");
	for (x = 0; x < size; x++) {
		if (x % 16 == 0) {
			os_printf("\n\r%04x: ", x);
		}
		os_printf("%02x ", buf[x]);
	}
	os_printf("\n\r");
}

void print_conn_info(struct espconn* conn) {
	os_printf("tcp remote ip:%d.%d.%d.%d:%d\n\r", conn->proto.tcp->remote_ip[0],
			conn->proto.tcp->remote_ip[1], conn->proto.tcp->remote_ip[2],
			conn->proto.tcp->remote_ip[3], conn->proto.tcp->remote_port);

	os_printf("udp remote ip:%d.%d.%d.%d:%d\n\r", conn->proto.udp->remote_ip[0],
			conn->proto.udp->remote_ip[1], conn->proto.udp->remote_ip[2],
			conn->proto.udp->remote_ip[3], conn->proto.udp->remote_port);

	remot_info *info;
	espconn_get_connection_info(conn, &info, 0);

	os_printf("remote ip:%d.%d.%d.%d\n\r", info->remote_ip[0],
			info->remote_ip[1], info->remote_ip[2], info->remote_ip[3]);
	os_printf("remote port: %d\n\r", info->remote_port);
}

void udp_receive_callback(void *arg, char *pdata, unsigned short len) {
	struct espconn *conn = (struct espconn*) arg;
	remot_info *info;
	espconn_get_connection_info(conn, &info, 0);
	memcpy(conn->proto.udp->remote_ip, info->remote_ip, 4);
	conn->proto.udp->remote_port = info->remote_port;

	if (len > 2) {
		memset(rcv_data_shadow, 0, RCV_DATA_SHADOW_SIZE);
		if (len > RCV_DATA_SHADOW_SIZE) {
			memcpy(rcv_data_shadow, pdata, RCV_DATA_SHADOW_SIZE);
		} else {
			memcpy(rcv_data_shadow, pdata, len);
		}
		rcv_data_shadow[RCV_DATA_SHADOW_SIZE - 1] = 0;
		parse_data(rcv_data_shadow, len, reply_message);
	} else {
		sprintf(reply_message, "ECHO OK\n\r");
	}

	espconn_send(&udp_conn, reply_message, strlen(reply_message)); //echo
}

void tcp_sent_callback(void *arg) {
	os_printf("tcp sent success\n\r");
}

void tcp_receiver_callback(void *arg, char *pdata, unsigned short len) {
	struct espconn *conn = (struct espconn*) arg;
	remot_info *info;
	espconn_get_connection_info(conn, &info, 0);
	memcpy(conn->proto.tcp->remote_ip, info->remote_ip, 4);
	conn->proto.tcp->remote_port = info->remote_port;

	if (len > 2) {
		memset(rcv_data_shadow, 0, RCV_DATA_SHADOW_SIZE);
		if (len > RCV_DATA_SHADOW_SIZE) {
			memcpy(rcv_data_shadow, pdata, RCV_DATA_SHADOW_SIZE);
		} else {
			memcpy(rcv_data_shadow, pdata, len);
		}
		rcv_data_shadow[RCV_DATA_SHADOW_SIZE - 1] = 0;
		parse_data(rcv_data_shadow, len, reply_message);
	} else {
		sprintf(reply_message, "ECHO OK\n\r");
	}
	espconn_regist_sentcb(conn, tcp_sent_callback);
	espconn_send(conn, reply_message, strlen(reply_message)); //echo
}

void tcp_disconnect_callback(void *arg) {
	struct espconn* conn = (struct espconn*) arg;
	os_printf("disconnected ip:%d.%d.%d.%d, port:%d\n\r",
			conn->proto.tcp->remote_ip[0], conn->proto.tcp->remote_ip[1],
			conn->proto.tcp->remote_ip[2], conn->proto.tcp->remote_ip[3],
			conn->proto.tcp->remote_port);
}

void tcp_connect_callback(void *arg) {
	struct espconn* conn = (struct espconn*) arg;
	os_printf("connected ip:%d.%d.%d.%d, port:%d\n\r",
			conn->proto.tcp->remote_ip[0], conn->proto.tcp->remote_ip[1],
			conn->proto.tcp->remote_ip[2], conn->proto.tcp->remote_ip[3],
			conn->proto.tcp->remote_port);
	int error;
	error = espconn_regist_recvcb(conn, tcp_receiver_callback);
	if (error != 0) {
		os_printf("error esp regist recvcb %d\n\r", error);
	}
	espconn_regist_disconcb(conn, tcp_disconnect_callback);
	espconn_set_opt(conn, ESPCONN_KEEPALIVE);
}

void udp_server_config() {
	udp_conn.type = ESPCONN_UDP;
	udp_conn.state = ESPCONN_NONE;
	udp_conn.proto.udp = (esp_udp*) os_zalloc(sizeof(esp_udp));
	udp_conn.proto.udp->local_port = local_port;
	espconn_regist_recvcb(&udp_conn, udp_receive_callback);
	espconn_create(&udp_conn);
}

void tcp_server_config() {
	sint8 error;
	tcp_conn.type = ESPCONN_TCP;
	tcp_conn.state = ESPCONN_LISTEN;
	tcp_conn.proto.tcp = (esp_tcp*) os_zalloc(sizeof(esp_tcp));
	tcp_conn.proto.tcp->local_port = local_port;
	error = espconn_regist_connectcb(&tcp_conn, tcp_connect_callback);
	if (error != 0) {
		os_printf("error esp regist connectcb %d\n\r", error);
	}

	error = espconn_accept(&tcp_conn);
	if (error != 0) {
		os_printf("error esp accept %d\n\r", error);
	}
	error = espconn_regist_time(&tcp_conn, 120, 0);
	if (error != 0) {
		os_printf("error esp regist time %d\n\r", error);
	}
	os_printf("tcp server config\n\r");

}

void log_server_config() {
	log_conn.type = ESPCONN_UDP;
	log_conn.state = ESPCONN_NONE;
	log_conn.proto.udp = (esp_udp*) os_zalloc(sizeof(esp_udp));
	log_conn.proto.udp->remote_port = log_server_port;
	memcpy(log_conn.proto.udp->remote_ip, log_server_ip, 4);
	espconn_create(&log_conn);
	configured = 1;
}

void scan_done(void *arg, STATUS status) {
	uint8 ssid[33];
	if (status == OK) {
		struct bss_info *bss_link = (struct bss_info *) arg;
		while (bss_link != NULL ) {
			memset(ssid, 0, 33);
			if (strlen(bss_link->ssid) <= 32)
				memcpy(ssid, bss_link->ssid, strlen(bss_link->ssid));
			else
				memcpy(ssid, bss_link->ssid, 32);
			printf("(%d,\"%s\",%d,\""MACSTR"\",%d)\r\n", bss_link->authmode,
					ssid, bss_link->rssi, MAC2STR(bss_link->bssid),
					bss_link->channel);
			bss_link = bss_link->next.stqe_next;
		}
	} else {
		printf("scan fail !!!\r\n");
	}
}

void sprint_reset_reason(char *string) {
	char *ptr = string;
	int size = 0;
	struct rst_info *rtc_info = system_get_rst_info();
	size = sprintf(ptr, "reset reason: %x\n", rtc_info->reason);
	ptr += size;

	if (rtc_info->reason == REASON_WDT_RST
			|| rtc_info->reason == REASON_EXCEPTION_RST
			|| rtc_info->reason == REASON_SOFT_WDT_RST) {
		if (rtc_info->reason == REASON_EXCEPTION_RST) {
			size = sprintf(ptr, "Fatal exception (%d):\n", rtc_info->exccause);
			ptr += size;
		}
		size =
			sprintf(ptr,
					"epc1=0x%08x, epc2=0x%08x, epc3=0x%08x, excvaddr=0x%08x, depc=0x%08x\n",
					rtc_info->epc1, rtc_info->epc2, rtc_info->epc3,
					rtc_info->excvaddr, rtc_info->depc); //The address of the last crash is printed,
		//which is used to debug garbledoutput.
	}
}

void WiFi_handle_event_cb(System_Event_t *evt) {
	printf("event %x\n", evt->event_id);

	switch (evt->event_id) {
		case EVENT_STAMODE_CONNECTED:
			printf("connect to ssid %s, channel %d\n",
					evt->event_info.connected.ssid,
					evt->event_info.connected.channel);

			break;
		case EVENT_STAMODE_DISCONNECTED:
			printf("disconnect from ssid %s, reason %d\n",
					evt->event_info.disconnected.ssid,
					evt->event_info.disconnected.reason);
			struct station_config *st = (struct station_config*) zalloc(
					sizeof(struct station_config));
			wifi_station_get_config(st);
			os_printf("stored ssid:%s\n\r", st->ssid);
			os_printf("stored password:%s\n\r", st->password);
			free(st);
			//		wifi_station_config();
			//		wifi_station_connect();
			break;
		case EVENT_STAMODE_AUTHMODE_CHANGE:
			printf("mode: %d -> %d\n", evt->event_info.auth_change.old_mode,
					evt->event_info.auth_change.new_mode);
			break;
		case EVENT_STAMODE_GOT_IP:
			printf("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR,
					IP2STR(&evt->event_info.got_ip.ip),
					IP2STR(&evt->event_info.got_ip.mask),
					IP2STR(&evt->event_info.got_ip.gw));
			printf("\n");
			char *reset_reason = zalloc(128);
			sprint_reset_reason(reset_reason);
			espconn_send(&log_conn, reset_reason, strlen(reset_reason));
			free(reset_reason);
			break;
		case EVENT_SOFTAPMODE_STACONNECTED:
			printf("station: " MACSTR "join, AID = %d\n",
					MAC2STR(evt->event_info.sta_connected.mac),
					evt->event_info.sta_connected.aid);
			break;
		case EVENT_SOFTAPMODE_STADISCONNECTED:
			printf("station: " MACSTR "leave, AID = %d\n",
					MAC2STR(evt->event_info.sta_disconnected.mac),
					evt->event_info.sta_disconnected.aid);
			break;
		default:
			break;
	}
}

LOCAL void uart0_rx_intr_handler(void *para) {
	/* uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
	 * uart1 and uart0 respectively
	 */
	uint8 RcvChar;
	uint8 uart_no = UART0; //UartDev.buff_uart_no;
	uint8 fifo_len = 0;
	uint8 buf_idx = 0;

	uint32 uart_intr_status = READ_PERI_REG(UART_INT_ST(uart_no));

	while (uart_intr_status != 0x0) {
		if (UART_FRM_ERR_INT_ST == (uart_intr_status & UART_FRM_ERR_INT_ST)) {
			//printf("FRM_ERR\r\n");
			WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_FRM_ERR_INT_CLR);
		} else if (UART_RXFIFO_FULL_INT_ST
				== (uart_intr_status & UART_RXFIFO_FULL_INT_ST)) {
			printf("full\r\n");
			fifo_len = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S)
				& UART_RXFIFO_CNT;
			buf_idx = 0;

			while (buf_idx < fifo_len) {
				uart_tx_one_char(UART0,
						READ_PERI_REG(UART_FIFO(UART0)) & 0xFF);
				buf_idx++;
			}

			WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);
		} else if (UART_RXFIFO_TOUT_INT_ST
				== (uart_intr_status & UART_RXFIFO_TOUT_INT_ST)) {
			printf("tout\r\n");
			fifo_len = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S)
				& UART_RXFIFO_CNT;
			buf_idx = 0;

			while (buf_idx < fifo_len) {
				uart_tx_one_char(UART0,
						READ_PERI_REG(UART_FIFO(UART0)) & 0xFF);
				buf_idx++;
			}

			WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_TOUT_INT_CLR);
		} else if (UART_TXFIFO_EMPTY_INT_ST
				== (uart_intr_status & UART_TXFIFO_EMPTY_INT_ST)) {
			printf("empty\n\r");
			WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_TXFIFO_EMPTY_INT_CLR);
			CLEAR_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);
		} else {
			//skip
		}

		uart_intr_status = READ_PERI_REG(UART_INT_ST(uart_no));
	}
}

void interrupt_handler(void *arg) {
	char *message;
	message = malloc(64*sizeof(char));
	os_printf("Interrupt handler\r\n");
	uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	sprintf(message, "gpio_status = %u\r\n", gpio_status);
	os_printf("%s",message);
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);
	free(message);
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
 *******************************************************************************/

void user_init(void) {
	system_update_cpu_freq(160);
	wifi_set_opmode(STATION_MODE);
	wifi_set_sleep_type(NONE_SLEEP_T);
	//	wifi_station_set_reconnect_policy(0);

	UART_SetBaudrate(UART0, BIT_RATE_115200);
	UART_SetPrintPort(UART0);

	//	GPIO_AS_OUTPUT(GPIO_Pin_5);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
	//	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15);
	GPIO_AS_OUTPUT(BIT5 | BIT9 | BIT12 | BIT13 | BIT14);
	gpio16_output_conf();
	GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, BIT12 | BIT13 | BIT14);
	GPIO_AS_INPUT(GPIO_Pin_0); //input pin

	//gpio 5 as config pin
	GPIO_AS_INPUT(GPIO_Pin_5);
	PIN_PULLUP_EN(GPIO_PIN_REG_5);

	network_settings_s = zalloc(sizeof(network_settings));
	if (network_settings_s == 0){
		os_printf("network settings struct not allocated\n\r"
				"can't continue\n\r");
		while(1);
	}
	spi_flash_read(NET_PARAM_ADDR, (uint32_t*) network_settings_s, sizeof(network_settings));
	os_printf("ap_name:%s\n\rap_passwd:%s\n\r", network_settings_s->ap_name,
			network_settings_s->ap_passwd);
	os_printf("ip_addr:%d.%d.%d.%d\n\r", network_settings_s->ip_addr[0],
			network_settings_s->ip_addr[1], network_settings_s->ip_addr[2],
			network_settings_s->ip_addr[3]);


	os_delay_us(65535);

	if (!GPIO_INPUT_GET(5)) {
		config_mode = CONFIG_MODE;
	} else {
		config_mode = NORMAL_MODE;
	}

	if (config_mode == CONFIG_MODE) {
		wifi_set_opmode_current(SOFTAP_MODE);
		wifi_softap_config();
		local_port = LOCAL_PORT;
	} else {
		wifi_set_opmode_current(STATION_MODE);
		wifi_station_config(network_settings_s);
		if (network_settings_s->port != 0){
			local_port = network_settings_s->port;;
		}
	}

	espconn_init();
	udp_server_config();
	tcp_server_config();

	log_server_config();

	os_printf("SDK version:%s\n", system_get_sdk_version());

	uint8_t cpu_freq = system_get_cpu_freq();
	os_printf("CPU Freq = %d\n\r", cpu_freq);

//odczyt parametrów z pamięci flash
	spi_flash_read(FX_PARAM_ADDR, (uint32*) &fx_param, sizeof(struct FX_param));

	if (fx_param.leds > 500){
		fx_param.leds = LEDS;
	}
	if (fx_param.leds < 1){
		fx_param.leds = LEDS;
	}


	char *reset_reason = zalloc(192);
	sprint_reset_reason(reset_reason);
	os_printf("%s\n\r", reset_reason);
	free(reset_reason);

	wifi_set_event_handler_cb(WiFi_handle_event_cb);

	rcv_data_shadow = zalloc(sizeof(char) * RCV_DATA_SHADOW_SIZE);

	reply_message = zalloc(sizeof(char) * REPLY_MESSAGE_SIZE);

	struct ws28xx_leds *leds = (struct ws28xx_leds*) zalloc(
			sizeof(struct ws28xx_leds) * fx_param.leds);
	if (leds) {
		ws28xx_address_set(leds);
		ws2812_init();
		ws28xx_set_lenght(fx_param.leds);
	}


	_message.leds = leds;
	_message.param = &fx_param;

	gpio_intr_handler_register(interrupt_handler, NULL);
	gpio_pin_intr_state_set(GPIO_ID_PIN(0), GPIO_PIN_INTR_NEGEDGE);
	gpio_pin_intr_state_set(GPIO_ID_PIN(5), GPIO_PIN_INTR_NEGEDGE);

	uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);
#define ETS_GPIO_INTR_ENABLE()  _xt_isr_unmask(1 << ETS_GPIO_INUM)  //ENABLE INTERRUPTS
#define ETS_GPIO_INTR_DISABLE() _xt_isr_mask(1 << ETS_GPIO_INUM) //DISABLE INTERRUPTS
	ETS_GPIO_INTR_ENABLE();

	os_printf("Free heap size%d\n\r", system_get_free_heap_size());

}
