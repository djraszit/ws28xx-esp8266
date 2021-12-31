/*
 * ws28xx-esp8266.c
 *
 *  Created on: 31 gru 2017
 *      Author: root
 */

#include "stdint.h"
#include "esp_common.h"
#include "ws28xx-esp8266.h"


#define WSGPIO	BIT5
//uint8 t1 = 2, t2 = 5, t3 = 6, t4 = 3;
uint32 t1 = 8, t2 = 20, t3 = 17, t4 = 15;


struct ws28xx_leds *leds_address;

static int16_t leds;

void ws28xx_address_set(struct ws28xx_leds *addr){
	leds_address = addr;
}

void ws28xx_set_lenght(int16_t len){
	leds = len;
}

void IRAM_ATTR asm_ws28xx_send(void * buffer, int16_t len) {
	ETS_INTR_LOCK();
//	vPortEnterCritical();
	uint8_t *buf = (uint8_t *) buffer;

	len *= 3;
	uint8_t data = 0, bit = 0;
	uint16_t delay;

	__asm__ __volatile__ (
			"start_%=:										\n"
			"			addi	%[len], %[len], -1			\n" //zmniejszenie len o 1
			"			bltz	%[len], end_%=				\n"//jesli len<0 to koncz
			"			l8ui	%[data], %[ptr], 0			\n"//zaladowanie danej ze wskaznika
			"			movi	%[bit], 128					\n"//ustawienie maski 1 << 7
			"loop_%=:										\n"
			"			memw								\n"
			"			s32i.n	%[pin], %[set_addr], 0		\n"//ustawienie WSGPIO
			"			bnone	%[data], %[bit], send0_%=	\n"//jesli bit w "data" jest wyzerowany to skocz
			"send1_%=:										\n"
			"			mov		%[delay], %[t3]				\n"
			"delayH1%=:	addi	%[delay], %[delay], -1		\n"
			"			bnez	%[delay], delayH1%=			\n"
			"			memw								\n"
			"			s32i.n	%[pin], %[clr_addr], 0		\n"//zerowanie WSGPIO
			"			mov		%[delay], %[t4]				\n"
			"delayL1%=:	addi	%[delay], %[delay], -1		\n"
			"			bnez	%[delay], delayL1%=			\n"
			"			j	bitshift_%=						\n"
			"send0_%=:										\n"
			"			mov		%[delay], %[t1]				\n"
			"delayH0%=:	addi	%[delay], %[delay], -1		\n"
			"			bnez	%[delay], delayH0%=			\n"
			"			memw								\n"
			"			s32i.n	%[pin], %[clr_addr], 0		\n"//zerowanie WSGPIO
			"			mov		%[delay], %[t2]				\n"
			"delayL0%=:	addi	%[delay], %[delay], -1		\n"
			"			bnez	%[delay], delayL0%=			\n"
			"bitshift_%=:									\n"
			"			srli	%[bit], %[bit], 1			\n"//przesun bit w prawo o jedna pozycja
			"			bnez	%[bit], loop_%=				\n"//jesli bit != 0 to skocz do loop_
			"			addi	%[ptr], %[ptr], 1			\n"//inaczej zwieksz ptr
			"			j		start_%=					\n"//i skocz do start_
			"end_%=:										\n"
			:[data]"=&a"(data), [bit]"=&a"(bit)
			:[pin]"a"(WSGPIO), [gpio9]"a"(BIT9), [ptr]"a"(buf), [len]"a"(len),
			[set_addr]"D"(PERIPHS_GPIO_BASEADDR+GPIO_OUT_W1TS_ADDRESS),
			[clr_addr]"D"(PERIPHS_GPIO_BASEADDR+GPIO_OUT_W1TC_ADDRESS),
			[delay]"a"(delay), [t1]"a"(t1), [t2]"a"(t2), [t3]"a"(t3), [t4]"a"(t4)
			:
	);
//	vPortExitCritical();
	ETS_INTR_UNLOCK();
}

void mixcolor(struct ws28xx_leds * pasek, uint16_t lenght, void * color1, void * color2) {
	struct ws28xx_leds *p = (struct ws28xx_leds*) pasek;
	struct rgb_color *c1 = (struct rgb_color*) color1;
	struct rgb_color *c2 = (struct rgb_color*) color2;

	float r = (float) (abs(c1->red - c2->red)) / (lenght * 1.0);
	float g = (float) (abs(c1->green - c2->green)) / (lenght * 1.0);
	float b = (float) (abs(c1->blue - c2->blue)) / (lenght * 1.0);
	uint16_t pos = 0;
	for (pos = 0; pos < lenght; pos++) {
		if (c1->red > c2->red)
			p[lenght - pos - 1].r = gamma_correction(r * pos + c2->red, 255,
					GAMMA_R);
		if (c1->red <= c2->red)
			p[pos].r = gamma_correction(r * pos + c1->red, 255, GAMMA_R);
		if (c1->green > c2->green)
			p[lenght - pos - 1].g = gamma_correction(g * pos + c2->green, 255,
					GAMMA_G);
		if (c1->green <= c2->green)
			p[pos].g = gamma_correction(g * pos + c1->green, 255, GAMMA_G);
		if (c1->blue > c2->blue)
			p[lenght - pos - 1].b = gamma_correction(b * pos + c2->blue, 255,
					GAMMA_B);
		if (c1->blue <= c2->blue)
			p[pos].b = gamma_correction(b * pos + c1->blue, 255, GAMMA_B);

	}
}

void ws2812_set_pixel(struct ws28xx_leds *p, uint16_t pos, uint32_t color, uint32_t color_mask,
		int brightness) {
	if (p == 0){
		return;
	}
	if (p < leds_address || (p + pos) >= (leds_address + leds)){
		return;
	}
	if (pos >= leds) {
		return;
	}
	if (color_mask == 0){
		color_mask = COLOR_ALL_MASK | COLOR_MIX_OVER;
	}
	int temp;
//	struct rgb_color *c = (struct rgb_color*) color;
	union rgb c;
	c.c = color;
	if (color_mask & COLOR_MIX_OR) {
		if (color_mask & COLOR_RED_MASK) {
			temp = (int) (c.red * brightness);
			p[pos].r |= (uint8_t) (temp / 100);
		}
		if (color_mask & COLOR_GREEN_MASK) {
			temp = (int) (c.green * brightness);
			p[pos].g |= (uint8_t) (temp / 100);
		}
		if (color_mask & COLOR_BLUE_MASK) {
			temp = (int) (c.blue * brightness);
			p[pos].b |= (uint8_t) (temp / 100);
		}
		return;
	}
	if (color_mask & COLOR_MIX_XOR) {
		if (color_mask & COLOR_RED_MASK) {
			temp = (int) (c.red * brightness);
			p[pos].r ^= (uint8_t) (temp / 100);
		}
		if (color_mask & COLOR_GREEN_MASK) {
			temp = (int) (c.green * brightness);
			p[pos].g ^= (uint8_t) (temp / 100);
		}
		if (color_mask & COLOR_BLUE_MASK) {
			temp = (int) (c.blue * brightness);
			p[pos].b ^= (uint8_t) (temp / 100);
		}
		return;
	}
	if (color_mask & COLOR_MIX_OVER) {
		if (color_mask & COLOR_RED_MASK) {
			temp = (int) (c.red * brightness);
			p[pos].r = (uint8_t) (temp / 100);
		}
		if (color_mask & COLOR_GREEN_MASK) {
			temp = (int) (c.green * brightness);
			p[pos].g = (uint8_t) (temp / 100);
		}
		if (color_mask & COLOR_BLUE_MASK) {
			temp = (int) (c.blue * brightness);
			p[pos].b = (uint8_t) (temp / 100);
		}
		return;
	}
}

void ws2812_fill_color(struct ws28xx_leds * pasek, int16_t start_pos, int16_t leds, uint32_t color, int brightness) {
	int16_t x;
	int16_t end_pos = start_pos + leds;
	for (x = start_pos; x < end_pos; x++) {
		ws2812_set_pixel(pasek, x, color, COLOR_ALL_MASK | COLOR_MIX_OVER, brightness);
	}
}

void shift_buf_fwd(struct ws28xx_leds* buf, int lenght) {
	struct ws28xx_leds temp;
	int x;
	temp.r = buf[lenght - 1].r;
	temp.g = buf[lenght - 1].g;
	temp.b = buf[lenght - 1].b;
//	temp = buf[lenght-1];
	for (x = (lenght - 1); x > 0; x--) {
		buf[x].r = buf[x - 1].r;
		buf[x].g = buf[x - 1].g;
		buf[x].b = buf[x - 1].b;
	}
	buf[0].r = temp.r;
	buf[0].g = temp.g;
	buf[0].b = temp.b;
//	buf[0] = temp;
}

void shift_buf_bwd(struct ws28xx_leds* buf, int lenght) {
	struct ws28xx_leds temp;
	int x;
	temp.r = buf[0].r;
	temp.g = buf[0].g;
	temp.b = buf[0].b;
	for (x = 0; x < (lenght-1);x++) {
		buf[x].r = buf[x + 1].r;
		buf[x].g = buf[x + 1].g;
		buf[x].b = buf[x + 1].b;
	}
	buf[lenght - 1].r = temp.r;
	buf[lenght - 1].g = temp.g;
	buf[lenght - 1].b = temp.b;
}

IRAM_ATTR union rgb RGB_compute(int xrgb, int yrgb, float zrgb, int max_x,
		int max_y) {
	int x1, x2, x3, x4, x5;
	x1 = max_x / 6;
	x2 = x1 * 2;
	x3 = x1 * 3;
	x4 = x1 * 4;
	x5 = x1 * 5;
	float y_temp = (float) ((float) yrgb / (float) max_y * 1.0);
	yrgb = (int) (y_temp * 255);

	union rgb c;
	if (xrgb >= 0 && xrgb < x1) {
		c.red = yrgb;
		c.green = (uint8_t) (((xrgb / (x1 - 1.0))) * yrgb * (1.0 - zrgb)
				+ (zrgb * yrgb));
		c.blue = (uint8_t) (zrgb * yrgb);
	}
	if (xrgb >= x1 && xrgb < x2) {
		c.red = (uint8_t) (yrgb
				- ((((xrgb - x1) / (x1 - 1.0)) * yrgb) * (1.0 - zrgb)));
		c.green = yrgb;
		c.blue = (uint8_t) (zrgb * yrgb);
	}
	if (xrgb >= x2 && xrgb < x3) {
		c.red = (uint8_t) (zrgb * yrgb);
		c.green = yrgb;
		c.blue = (uint8_t) (((xrgb - x2) / (x1 - 1.0)) * yrgb * (1.0 - zrgb)
				+ (zrgb * yrgb));
	}
	if (xrgb >= x3 && xrgb < x4) {
		c.red = (uint8_t) (zrgb * yrgb);
		c.green = (uint8_t) (yrgb
				- ((((xrgb - x3) / (x1 - 1.0)) * yrgb) * (1.0 - zrgb)));
		c.blue = yrgb;
	}
	if (xrgb >= x4 && xrgb < x5) {
		c.red = (uint8_t) (((xrgb - x4) / (x1 - 1.0)) * yrgb * (1.0 - zrgb)
				+ (zrgb * yrgb));
		c.green = (uint8_t) (zrgb * yrgb);
		c.blue = yrgb;
	}
	if (xrgb >= x5 && xrgb <= max_x) {
		c.red = yrgb;
		c.green = (uint8_t) (zrgb * yrgb);
		c.blue = (uint8_t) (yrgb
				- ((((xrgb - x5) / (x1 * 1.0)) * yrgb) * (1.0 - zrgb)));
	}
	return c;
}

double fabs(double x) {
	return (x < 0 ? -x : x);
}
double pow(double x, double y) {
	double z, p = 1;
	int i;
	//y<0 ? z=-y : z=y ;
	if (y < 0)
		z = fabs(y);
	else
		z = y;
	for (i = 0; i < z; ++i) {
		p *= x;
	}
	if (y < 0)
		return 1 / p;
	else
		return p;
}

void fade_out(struct ws28xx_leds *buf, uint16_t lenght, uint8_t how_much) {
	uint16_t led;
	for (led = 0; led < lenght; led++) {
		if (buf[led].r >= how_much)
			buf[led].r -= how_much;
		if (buf[led].r < how_much)
			buf[led].r = 0;
		if (buf[led].g >= how_much)
			buf[led].g -= how_much;
		if (buf[led].g < how_much)
			buf[led].g = 0;
		if (buf[led].b >= how_much)
			buf[led].b -= how_much;
		if (buf[led].b < how_much)
			buf[led].b = 0;
	}
}

void fade_out_pixel(struct ws28xx_leds *buf, int pos, uint32_t color_mask, uint8_t b) {
	if (pos >= leds) {
		return;
	}
	union rgb c = { .red = buf[pos].r, .green = buf[pos].g, .blue = buf[pos].b };
	ws2812_set_pixel(buf, pos, c.c, color_mask, b);
}

void fade_out_(struct ws28xx_leds *buf, uint16_t lenght, uint32_t color_mask, uint8_t b) {
	uint16_t l;
	for (l = 0; l < lenght; l++) {
		fade_out_pixel(buf, l, color_mask, b);
	}
}

uint8_t IRAM_ATTR gamma_correction(double value, double max, double gamma) {
	return max * pow((double) value / max, 1.0 / gamma);
}

