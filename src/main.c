/******************************************************************************
 *
 * Copyright:
 *    (C) 2000 - 2007 Embedded Artists AB
 *
 * Description:
 *    Main program for LPC2148 Education Board test program
 *
 *****************************************************************************/

#include "pre_emptive_os/api/osapi.h"
#include "pre_emptive_os/api/general.h"
#include <printf_P.h>
#include <ea_init.h>
#include <lpc2xxx.h>
#include <consol.h>
#include "pca9532.h"
#include "i2c.h"
#include "adc.h"
#include "lcd.h"
#include "framework.h"
#include "ea_97x60c.h"
#include "key.h"
#include <fiq.h>
#include "eeprom.h"

#define LEDS_STACK_SIZE 1024
#define BUTTONS_STACK_SIZE 1024
#define INIT_STACK_SIZE  400

#define CRYSTAL_FREQUENCY FOSC
#define PLL_FACTOR        PLL_MUL
#define VPBDIV_FACTOR     PBSD

static tU8 ledsStack[LEDS_STACK_SIZE];
static tU8 buttonsStack[BUTTONS_STACK_SIZE];
static tU8 initStack[INIT_STACK_SIZE];
static tU8 pid1;
static tU8 pid2;

static void leds(void* arg);
static void buttons(void* arg);
static void initProc(void* arg);

static void delayMs(tU32 delayInMs);

void appTick(tU32 elapsedTime);
volatile tU32 msClock;

tU16 actualLed = 0;
static tU16 lives = 8;
static tU8 gameStarted = 0;
tU16 delay = 1000;

static char buffer[6];
static char buffer2[3];
static char buffer3[6];

unsigned long turns = 0;

void saveToEpprom(int score) {
	tU8 tmp[2];
	tmp[0] = score & 0xFF;
	tmp[1] = (score >> 8) & 0xFF;
	eepromPoll();
	eepromWrite(0x0000, tmp, 2);
	eepromPoll();
}

void clearEEPROM() {
	tU8 tmp[2];
	tmp[0] = 0 & 0xFF;
	tmp[1] = (0 >> 8) & 0xFF;
	eepromPoll();
	eepromWrite(0x0000, tmp, 2);
	eepromPoll();
}

int get_score() {
	int score = 0;
	tU8 buffer[2];
	eepromPoll();
	eepromPageRead(0x0000, buffer, 2);
	eepromPoll();
	score = (buffer[1] << 8) + buffer[0];
	return score;
}

void my_utoa(int dataIn, char* bffr) {
	int temp_dataIn;
	temp_dataIn = dataIn;
	int stringLen = 1;

	while ((int) temp_dataIn / 10 != 0) {
		temp_dataIn = (int) temp_dataIn / 10;
		stringLen++;
	}

	temp_dataIn = dataIn;
	do {
		*(bffr + stringLen - 1) = (temp_dataIn % 10) + '0';
		temp_dataIn = (int) temp_dataIn / 10;
	} while (stringLen--);
}

/*****************************************************************************
 *
 * Description:
 *    The first function to execute 
 *
 ****************************************************************************/
int main(void) {
	tU8 error;
	tU8 pid;

	osInit();
	osCreateProcess(initProc, initStack, INIT_STACK_SIZE, &pid, 1, NULL, &error);
	osStartProcess(pid, &error);

	osStart();

	return 0;
}

inline void turnDelay() {
	if (turns % 5 == 0) {
		delay = delay * 0.8;
	}
}

/*****************************************************************************
 *
 * Description:
 *    A process entry function 
 *
 * Params:
 *    [in] arg - This parameter is not used in this application. 
 *
 ****************************************************************************/

tU16 rand(unsigned *bit, tU16 *lfsr) {
	bit = ((*lfsr >> 0) ^ (*lfsr >> 2) ^ (*lfsr >> 3) ^ (*lfsr >> 5)) & 1;
	return *lfsr = (*lfsr >> 1) | (*bit << 15);
}

static void toggleLed(tU32 address, tU16 _actualLed, tU16 delay) {
	actualLed = _actualLed;
	IOCLR1 = address;
	delayMs(delay);
	IOSET1 = address;
}

static tU32 LED_ADRESS_1 = 0x00010000;
static tU32 LED_ADRESS_2 = 0x00020000;
static tU32 LED_ADRESS_3 = 0x00040000;
static tU32 LED_ADRESS_4 = 0x00080000;

static void leds(void* arg) {
	tU16 randLed = 0;
	unsigned bit;
	tU16 lfsr = 0xACE1u;

	for (;;) {


		randLed = rand(&bit, &lfsr) % 4;

		switch (randLed) {
		case 0:
			toggleLed(LED_ADRESS_1, 1, delay);
			break;
		case 1:
			toggleLed(LED_ADRESS_2, 2, delay);
			break;
		case 2:
			toggleLed(LED_ADRESS_3, 3, delay);
			break;
		case 3:
			toggleLed(LED_ADRESS_4, 4, delay);
			break;

		}
		actualLed = 0;
		delayMs(delay);
	}
}

void clearBuffer(char *buff, tU16 length) {
	int i;
	for (i = 0; i < length; i = i + 1) {
		if (i != length-1) {
			buffer[i] = ' ';
		} else {
			buffer[i] = '\0';
		}
	}
}

void clearBuffers() {
	clearBuffer(buffer, 6);
	clearBuffer(buffer2, 3);
	clearBuffer(buffer3, 6);
	printf("clearBuffers() called \n");
}

void gameLCD() {
	lcdClrscr();
	lcdGotoxy(16, 10);
	lcdPuts("REFLEX GAME");
}

void displayActualScore() {
	lcdGotoxy(16, 40);
	lcdPuts("Wynik: ");
	lcdPuts(buffer);
	lcdPuts("\n");
	lcdGotoxy(16, 66);
	lcdPuts("Ruchy: ");
	my_utoa(turns, buffer2);
	lcdPuts(buffer2);
	clearBuffers();
}

void disableActualLedAndCountScore(unsigned int ioset, int* result) {
	turns = turns + 1;
	turnDelay();
	IOSET1 = ioset;
	*result = *result + TIMER1_TC / 100000;
	TIMER1_TCR = 0x00;
	my_utoa(*result, buffer);
	printf("%s\n", buffer);
}

void keyPressed(unsigned int ioset, int* result) {
	gameLCD();
	gameStarted = 1;
	disableActualLedAndCountScore(ioset, result);
	displayActualScore();
}

void loseLive() {
	lives = lives - 1;
	osSleep(8);
}

void enableScoreLeds(tU16 l) {

	int j;
	for (j = 0; j < 8; j = j + 1) {
		setPca9532Pin(7 - j, 1);
		setPca9532Pin(8 + j, 1);
	}
	osSleep(4);
	int i;
	for (i = 0; i < l; i = i + 1) {
		setPca9532Pin(7 - i, 0);
		setPca9532Pin(8 + i, 0);
	}
}

void livesHandler() {
	switch (lives) {
			case 8:
				enableScoreLeds(8);
				break;
			case 7:
				enableScoreLeds(7);
				break;
			case 6:
				enableScoreLeds(6);
				break;
			case 5:
				enableScoreLeds(5);
				break;
			case 4:
				enableScoreLeds(4);
				break;
			case 3:
				enableScoreLeds(3);
				break;
			case 2:
				enableScoreLeds(2);
				break;
			case 1:
				enableScoreLeds(1);
				break;
			case 0:
				enableScoreLeds(0);
				break;
			}
}

void correctlyButtonPressedDetectors(int* result) {
	//detect if P1.20 key is pressed
	if ((IOPIN1 & 0x00100000) == 0 && actualLed == 1) {
		keyPressed(0x00010000, result);
	}

	//detect if P1.21 key is pressed
	if ((IOPIN1 & 0x00200000) == 0 && actualLed == 2) {
		keyPressed(0x00020000, result);
	}

	//detect if P1.22 key is pressed
	if ((IOPIN1 & 0x00400000) == 0 && actualLed == 3) {
		keyPressed(0x00040000, result);
	}

	//detect if P1.23 key is pressed
	if ((IOPIN1 & 0x00800000) == 0 && actualLed == 4) {
		keyPressed(0x00080000, result);
	}
}

void badlyButtonPressedDetectors() {
	if ((IOPIN1 & 0x00800000) == 0 && actualLed != 4) {
		loseLive();
	}
	if ((IOPIN1 & 0x00400000) == 0 && actualLed != 3) {
		loseLive();
	}
	if ((IOPIN1 & 0x00200000) == 0 && actualLed != 2) {
		loseLive();
	}
	if ((IOPIN1 & 0x00100000) == 0 && actualLed != 1) {
		loseLive();
	}
}

int buttonsHandler(int *result) {
	if (lives != 0) {
		correctlyButtonPressedDetectors(result);
		badlyButtonPressedDetectors();
		return 0;
	} else {
		return 1;
	}
}

tU16 setLivesByKnobValue() {
	tU16 poten = getAnalogueInput(AIN1);
	printf("%d", poten);
	if (poten <= 125) {
		return 1;
	} else if (poten >= 126 && poten <= 250) {
		return 2;
	} else if (poten >= 251 && poten <= 375) {
		return 3;
	} else if (poten >= 376 && poten <= 500) {
		return 4;
	} else if (poten >= 501 && poten <= 625) {
		return 5;
	} else if (poten >= 626 && poten <= 750) {
		return 6;
	} else if (poten >= 751 && poten <= 875) {
		return 7;
	} else if (poten >= 876) {
		return 8;
	}
}

static void buttons(void* arg) {
	tU8 pca9532Present = FALSE;
	clearBuffers();
	int result = 0;

	//check if connection with PCA9532
	pca9532Present = pca9532Init();

	if (TRUE == pca9532Present) {
		printf("lcd dziala");
		lcdInit();
		lcdColor(0xff, 0x00);
		lcdClrscr();
		lcdIcon(16, 0, 97, 60, _ea_97x60c[2], _ea_97x60c[3], &_ea_97x60c[4]);
		lcdGotoxy(16, 66);
		lcdPuts("Designed and");
		lcdGotoxy(20, 80);
		lcdPuts("produced by");
		lcdGotoxy(0, 96);
		lcdPuts("turbo smieszki");
		lcdGotoxy(8, 112);
		lcdPuts("(C)2009 (v1.1)");
	} else {
		printf("Problem z LCD\n");
	}


	for (;;) {
		if (!gameStarted) {
			lives = setLivesByKnobValue();
		}
		livesHandler();
		if (buttonsHandler(&result)) {
			break;
		}
		osSleep(5);
	}
	lcdClrscr();
	lcdPuts("GAME OVER \n");
	lcdPuts("Wynik: ");
	my_utoa(result, buffer);
	lcdPuts(buffer);
	lcdPuts("\nNajlepszy: ");
	int best_score = 0;
	best_score = get_score();
	my_utoa(best_score, buffer3);
	lcdPuts(buffer3);

	lcdPuts("\nJoy UP zapisz\n wynik  ");
	//joystikc w góre
	osSleep(300);
	tU8 pressedKey = checkKey();

	if (pressedKey == KEY_UP) {
		lcdPuts("\nZapisano!");
		osSleep(200);
		saveToEpprom(result);
	}

	if (pressedKey == KEY_DOWN) {
		clearEEPROM();
	}

	lcdClrscr();
	osSleep(300);
	lcdClrscr();
	lcdPuts("Winter is\n coming");
	osSleep(400);

}

/*****************************************************************************
 *
 * Description:
 *    The entry function for the initialization process. 
 *
 * Params:
 *    [in] arg - This parameter is not used in this application. 
 *
 ****************************************************************************/
static void initProc(void* arg) {
	tU8 error;
	eaInit();

	printf("HELLO. DO YOU WANNA PLAY A GAME?");

	initMyFiq(); // P0.14 calls FIQ;

	IODIR1 |= 0x000F0000; //LEDs
	IOSET1 = 0x000F0000; //Turn them off
	IODIR1 &= ~0x00F00000; //Keys
	initKeyProc();
	initAdc();
	i2cInit();
	osCreateProcess(leds, ledsStack, LEDS_STACK_SIZE, &pid1, 3, NULL, &error);
	osStartProcess(pid1, &error);
	osCreateProcess(buttons, buttonsStack, BUTTONS_STACK_SIZE, &pid2, 3, NULL,
			&error);
	osStartProcess(pid2, &error);

	osDeleteProcess();
}

static void delayMs(tU32 delayInMs) {
	/*
	 * setup timer #1 for delay
	 */
	TIMER1_TCR = 0x02; //stop and reset timer
	TIMER1_PR = 0x00; //set prescaler to zero
	TIMER1_MR0 = delayInMs * ((CRYSTAL_FREQUENCY * PLL_FACTOR) / (1000
			* VPBDIV_FACTOR));
	TIMER1_IR = 0xff; //reset all interrrupt flags
	TIMER1_MCR = 0x04; //stop timer on match
	TIMER1_TCR = 0x01; //start timer


	//wait until delay time has elapsed
	while (TIMER1_TCR & 0x01)
		;
}

void appTick(tU32 elapsedTime) {
	msClock += elapsedTime;
}
