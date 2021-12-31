/*
 * ws28xx-esp8266.h
 *
 *  Created on: 31 gru 2017
 *      Author: root
 */

#ifndef USER_WS28XX_ESP8266_H_
#define USER_WS28XX_ESP8266_H_

#define LEDS	192


#define GAMMA_R	0.8
#define GAMMA_G	0.6
#define GAMMA_B 0.5

#define COLOR_RED_MASK		0x01
#define COLOR_GREEN_MASK	0x02
#define COLOR_BLUE_MASK		0x04
#define COLOR_ALL_MASK		0x07
#define COLOR_MIX_OR		0x08
#define COLOR_MIX_XOR		0x10
#define COLOR_MIX_OVER		0x20

struct ws28xx_leds {
	uint8_t b;
	uint8_t r;
	uint8_t g;
};



struct rgb_color {
	uint8_t blue;
	uint8_t green;
	uint8_t red;
	uint8_t alpha;
};

union rgb {
	uint32_t c;
	struct rgb_color color;
	struct {
		uint8_t blue;
		uint8_t green;
		uint8_t red;
		uint8_t alpha;
	};
};

void ws28xx_set_lenght(int16_t len);

void ws28xx_address_set(struct ws28xx_leds *addr);
void ws2812_set_pixel(struct ws28xx_leds *p, uint16_t pos, uint32_t color, uint32_t color_mask,
		int brightness);
void ws2812_fill_color(struct ws28xx_leds * pasek, int16_t start_pos, int16_t leds, uint32_t color, int brightness);
void mixcolor(struct ws28xx_leds * pasek, uint16_t lenght, void * color1, void * color2);
void shift_buf_fwd(struct ws28xx_leds* buf, int lenght);
void shift_buf_bwd(struct ws28xx_leds* buf, int lenght);
double pow_(double x, double y);
union rgb RGB_compute(int xrgb, int yrgb, float zrgb, int max_x,
		int max_y);
void ws28xx_send_buf(void *leds, int size);
void ws28xx_asm_send_buf(void *leds, int size);
void fade_out(struct ws28xx_leds *buf, uint16_t lenght, uint8_t how_much);
void fade_out_pixel(struct ws28xx_leds *buf, int pos, uint32_t color_mask, uint8_t b);
void fade_out_(struct ws28xx_leds *buf, uint16_t lenght, uint32_t color_mask, uint8_t b);
uint8_t gamma_correction(double value, double max, double gamma);

#endif /* USER_WS28XX_ESP8266_H_ */
