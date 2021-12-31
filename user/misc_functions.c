
char* find_char_or_number(char *str) {
	char *c = (char*) str;
	char cc;
	while ((cc = *c) != 0) {
		if ((cc >= 0x21 && cc <= 0x39) || (cc >= 0x40 && cc <= 0x7e)) {
			return c;
		} else {
			c++;
		}
	}
	return 0;
}

char *find_hex_number(char *str){
	char *c = (char*) str;
	char cc;
	while ((cc = *c) != 0) {
		if ((cc >= 0x30 && cc <= 0x39) || (cc >= 0x41 && cc <= 0x46) ||
				(cc >= 0x61 && cc<= 0x66)) {
			return c;
		} else {
			c++;
		}
	}
	return 0;
}

char *find_number(char *str){
	char *c = (char*) str;
	char cc;
	while ((cc = *c) != 0) {
		if ((cc >= 0x30 && cc <= 0x39)){
			return c;
		} else {
			c++;
		}
	}
	return 0;
}
