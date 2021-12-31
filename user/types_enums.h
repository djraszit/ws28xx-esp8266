
#define MODULE_NAME_LEN	24

enum OP_MODE {
	SEPARATED_CHANNELS, MIXED_CHANNELS
};

enum MODE {
	DISABLED_MODE, OFF_MODE, ON_MODE, MONO_MODE, TWILIGHT_SENSOR_MODE
};

enum CONFIG_MODE {
	NORMAL_MODE, CONFIG_MODE
};

#define LOG_INTERRUPT		(1 << 0)
#define LOG_CHANGE_MODE		(1 << 1)
#define LOG_ADC				(1 << 2)
#define LOG_RESET_REASON	(1 << 3)
#define LOG_DARK			(1 << 4)


typedef struct {
	uint32_t out1_time;
	uint32_t out2_time;
	uint8_t out1_mode;
	uint8_t out2_mode;
	uint16_t adc;
	uint16_t op_mode;
	char module_name[MODULE_NAME_LEN];
	uint16_t adc_hister;
} settings;

typedef struct {
	uint8_t ip_addr[4];
	uint8_t gateway[4];
	uint8_t netmask[4];
	uint16_t port;
	uint16_t dhcp;
	char ap_passwd[64];
	char ap_name[64];
} network_settings;

typedef struct {
	uint8_t ip_addr[4];
	uint16_t port;
	uint16_t log_mask;
} log_server;

typedef struct {
	settings SETTINGS;
	network_settings NETWORK_SETTINGS;
	log_server LOG_SERVER;
} all_settings;
