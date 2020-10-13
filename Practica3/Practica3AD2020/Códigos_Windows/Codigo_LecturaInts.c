#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

char chr=0;
int acum=0;

void main() {
	while (chr!=13) {
		if (kbhit()) {
			chr=getch();
			if (chr!=13) {
				if ((chr>='0')&&(chr<='9')) {
					acum*=10;
					acum+=(chr-'0');
					printf("%c",chr);
				}
			}
		}
	}
	printf("\nFin --> %d\n",acum);
}