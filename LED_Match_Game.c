#include <stdio.h>
#include <stdlib.h>
#include <system.h>
#include <io.h>
#include <math.h>
#include <time.h>
#include "unistd.h"
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"
#include "altera_avalon_timer_regs.h"
#include "sys/alt_stdio.h"
#include "altera_avalon_jtag_uart_regs.h"
#include "altera_up_avalon_character_lcd.h"
#include "altera_up_avalon_character_lcd_regs.h";

int hex_display[16] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71};
int puzzle_array_singles[5] = {126, 125, 111, 95, 63};//puzzles for single led
int puzzle_array_doubles[12] = {124, 121, 115, 103, 79, 31,//puzzles for double leds
								61, 122, 117, 107, 47, 94};
//hash tables filled with hints at corresponding hash value for each puzzle.
int hints_singles[11] = {0,6,0,0,5,1,0,3,8,0,0};
int hints_doubles[22] = {0,0,0,0,0,15,0,6,0,4,0,12,14,3,11,10,0,0,0,3,0,0};
//Displays the numbers on the seven - segment displays
//global variables that will be user throughout the program
volatile int edge_capture = 0;
int temp = 0;
int count = 0;
int finished_count = 0;
int retry_counter = 0;
int random = 0;
int reset = 0;
int LEDG_on = 0;
int LEDG_off = 0;
int LEDR_on = 0;
int LEDR_off = 0;
int input1 = 1;
int input2 = 0;
int sw_values1 = 0;
int sw_values2 = 0;
int single_double_random = 0;//used for selecting either a single or double puzzle randomly.
int result = 0;
int hint = 0;
int x = 0;
int value = 0;
int LEDG = 0;
int LEDR = 0;
char total_time [] = {"Time: "};
char correct [] = "CORRECT!\0";
int ones_ascii = 0;
int tens_ascii = 0;
int hundreds_ascii = 0;
int DISPLAY_CORRECT = 0;
int DISPLAY_WRONG = 0;
int DISPLAY_RESET = 0;
alt_up_character_lcd_dev * char_lcd_dev;
int ones = 0;
int tens = 0;
int hundreds = 0;
unsigned int milliseconds = 0;

void init_LCD(){
	// open the Character LCD port
	char_lcd_dev = alt_up_character_lcd_open_dev ("/dev/lcd");
	/* Initialize the character display */
	alt_up_character_lcd_init (char_lcd_dev);
}

static void keys_isr(void* context, alt_u32 id){
	volatile int* edge_capture_ptr = (volatile int*) context;
	/*
	* Read the edge capture register on the button PIO.
	* Store value.
	*/
	*edge_capture_ptr = IORD_ALTERA_AVALON_PIO_EDGE_CAP(KEY_BASE);
	if(edge_capture & 0x1){//if Key 0 reset
		retry_counter = 0;
		LEDG = 0;
		LEDG_on = 0;
		LEDG_off = 0;
		LEDR = 0;
		LEDR_on = 0;
		LEDR_off = 0;
		input1 = 1;
		input2 = 0;
		count = 0;
		IOWR_ALT_UP_CHARACTER_LCD_COMMAND(LCD_BASE, 0x01);//Reset the LCD display
		IOWR_16DIRECT(HEX_0_BASE, 0, 0x7F);
		IOWR_16DIRECT(HEX_3_BASE, 0, 0x7F);
		IOWR_16DIRECT(HEX_1_BASE, 0, hex_display[8]);
		IOWR_16DIRECT(HEX_2_BASE, 0, hex_display[8]);
		IOWR_16DIRECT(HEX_4_BASE, 0, hex_display[8]);
		IOWR_16DIRECT(HEX_5_BASE, 0, hex_display[8]);
		IOWR_16DIRECT(HEX_6_BASE, 0, hex_display[8]);
		IOWR_16DIRECT(HEX_7_BASE, 0, hex_display[8]);
		IOWR_16DIRECT(LED_BASE, 0, 0);
		IOWR_16DIRECT(LEDG_BASE, 0, 0);
		IOWR_ALTERA_AVALON_TIMER_CONTROL(SYSTEM_TIMER_BASE, 0x9);//stop timer
		IOWR_ALTERA_AVALON_TIMER_PERIODL(SYSTEM_TIMER_BASE, 0xF080);//load low
		IOWR_ALTERA_AVALON_TIMER_PERIODH(SYSTEM_TIMER_BASE, 0x2FA);//load high
	}else if(edge_capture & 0x2){//if Key 1 start
		single_double_random = rand() % 2;
		if(single_double_random == 1){
			random = rand() % 5;//select from single leds
			IOWR_16DIRECT(HEX_3_BASE, 0, puzzle_array_singles[random]);
		}else{
			random = rand() % 12;//select from double leds
			IOWR_16DIRECT(HEX_3_BASE, 0, puzzle_array_doubles[random]);
		}
		IOWR_ALTERA_AVALON_TIMER_CONTROL(SYSTEM_TIMER_BASE, 0x7);//start timer
	}else if(edge_capture & 0x4){//if Key 2 hint
		if(single_double_random == 1){//if single led used
			x = puzzle_array_singles[random] % 11;//x holds index for the hint value
			value = hex_display[hints_singles[x]];//value stores number to be display
			IOWR_16DIRECT(HEX_0_BASE, 0, value);
			sw_values1 = hints_singles[x];
			input1 = 0;
			input2 = 1;
		}else{//if double leds used
			x = puzzle_array_doubles[random] % 22;//x holds index for the hint value
			value = hex_display[hints_doubles[x]];//value stores number to be displayed
			IOWR_16DIRECT(HEX_0_BASE, 0, value);
			sw_values1 = hints_doubles[x];
			input1 = 0;
			input2 = 1;
		}
	}else if(edge_capture & 0x8){//if Key 3 enter
		if(input1){
			sw_values1 = IORD_32DIRECT(SW_BASE, 0) & 0xFF;
			IOWR_16DIRECT(HEX_0_BASE, 0, hex_display[sw_values1]);
			input1 = 0;
			input2 = 1;
		}else if(input2){
			sw_values2 = IORD_32DIRECT(SW_BASE, 0) & 0xFF;
			result = (hex_display[sw_values1]) ^ (hex_display[sw_values2]);
			result = ~(result) & 0x7F;
			IOWR_16DIRECT(HEX_0_BASE, 0, result);
			input1 = 1;
			input2 = 0;
			if(result == IORD_16DIRECT(HEX_3_BASE, 0)){
				LEDG = 1;
				LEDG_on = 1;
				LEDG_off = 0;
				LEDR = 0;
				alt_up_character_lcd_string(char_lcd_dev, "CORRECT!\0");//Display "CORRECT!" to LCD
				alt_up_character_lcd_set_cursor_pos(char_lcd_dev, 0, 1);//Change cursor to first column second row
				alt_up_character_lcd_string(char_lcd_dev, "Time:");
			    char buff[24];//converting integer into a string
			    itoa(count, buff, 10);//string stored in buff
				alt_up_character_lcd_string(char_lcd_dev, buff);
				alt_up_character_lcd_string(char_lcd_dev, "seconds\0");
				count = 0;
			}else{
				LEDR = 1;
				LEDR_on = 1;
				LEDR_off = 0;
				LEDG = 0;
				count = 0;
				alt_up_character_lcd_string(char_lcd_dev, "Not Quite");//"Not Quite" to LCD
				alt_up_character_lcd_set_cursor_pos(char_lcd_dev, 0, 1);//Change cursor to first column second row
				alt_up_character_lcd_string(char_lcd_dev, "Try Again\0");//"Try Again" to LCD
			}
		}
	}
	edge_capture = 0;
	/* Write to the edge capture register to reset it. */
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(KEY_BASE, 0);
	/* reset interrupt capability for the Button PIO. */
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(KEY_BASE, 0xF);
}
/* Initialize the button_pio. */
static void init_keys(){
	/* Recast the edge_capture pointer to match the
	alt_irq_register() function prototype. */
	void* edge_capture_ptr = (void*) &edge_capture;
	/* Enable all 4 button interrupts. */
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(KEY_BASE, 0xF);
	/* Reset the edge capture register. */
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(KEY_BASE, 0x0);
	/* Register the ISR. */
	alt_ic_isr_register(KEY_IRQ_INTERRUPT_CONTROLLER_ID,KEY_IRQ, keys_isr, edge_capture_ptr, 0x0);
}

static timer_isr(void* context, alt_u32 id){
	count++;
	//Clear the interrupt
	IOWR_ALTERA_AVALON_TIMER_STATUS(SYSTEM_TIMER_BASE, 0);
	if(LEDG == 1){
		if(LEDG_on){//Flash green LEDs
				IOWR_16DIRECT(LEDG_BASE, 0, 0xFF);
				IOWR_16DIRECT(LED_BASE, 0, 0x0);
				LEDG_on = 0;
				LEDG_off = 1;
		}else if(LEDG_off){
				IOWR_16DIRECT(LEDG_BASE, 0, 0x0);
				IOWR_16DIRECT(LED_BASE, 0, 0);
				LEDG_on = 1;
				LEDG_off = 0;
		}
	}else if(LEDR == 1){
		retry_counter++;//Display "RETRY"
		if(LEDR_on){//Flash red LEDs
				IOWR_16DIRECT(LEDG_BASE, 0, 0x0);
				IOWR_16DIRECT(LED_BASE, 0, 0x3FFFF);
				LEDR_on = 0;
				LEDR_off = 1;
		}else if(LEDR_off){
				IOWR_16DIRECT(LEDG_BASE, 0, 0x0);
				IOWR_16DIRECT(LED_BASE, 0, 0);
				LEDR_on = 1;
				LEDR_off = 0;
		}
		if(retry_counter == 4){//waits for four seconds before resetting for a retry.
			retry_counter = 0;
			LEDG = 0;
			LEDG_on = 0;
			LEDG_off = 0;
			LEDR = 0;
			LEDR_on = 0;
			LEDR_off = 0;
			input1 = 1;
			input2 = 0;
			if(single_double_random == 1){
				IOWR_16DIRECT(HEX_3_BASE, 0, puzzle_array_singles[random]);
			}else{
				IOWR_16DIRECT(HEX_3_BASE, 0, puzzle_array_doubles[random]);
			}
			IOWR_ALT_UP_CHARACTER_LCD_COMMAND(LCD_BASE, 0x01);//Reset the LCD display
			IOWR_16DIRECT(HEX_0_BASE, 0, 0x7F);
			IOWR_16DIRECT(HEX_1_BASE, 0, hex_display[8]);
			IOWR_16DIRECT(HEX_2_BASE, 0, hex_display[8]);
			IOWR_16DIRECT(HEX_4_BASE, 0, hex_display[8]);
			IOWR_16DIRECT(HEX_5_BASE, 0, hex_display[8]);
			IOWR_16DIRECT(HEX_6_BASE, 0, hex_display[8]);
			IOWR_16DIRECT(HEX_7_BASE, 0, hex_display[8]);
			IOWR_16DIRECT(LED_BASE, 0, 0);
			IOWR_16DIRECT(LEDG_BASE, 0, 0);
		}
	}
}

static timer_seed_isr(void* context, alt_u32 id){
	IOWR_ALTERA_AVALON_TIMER_STATUS(TIMER_SEED_BASE, 0);
	milliseconds++;
}

void init_timers(void){
	//Register ISR with HAL
	alt_irq_register(SYSTEM_TIMER_IRQ, NULL, timer_isr);
	alt_irq_register(TIMER_SEED_IRQ, NULL, timer_seed_isr);
	IOWR_ALTERA_AVALON_TIMER_CONTROL(SYSTEM_TIMER_BASE, 0x9);//write to control register. stop timer
	IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_SEED_BASE, 0x9);//stop timer
	IOWR_ALTERA_AVALON_TIMER_PERIODL(TIMER_SEED_BASE, 0xC350);//load low
	IOWR_ALTERA_AVALON_TIMER_PERIODH(TIMER_SEED_BASE, 0x0);//load high
	IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_SEED_BASE, 0x7);//start timer
}
int main(){
	for(int i = 0; i < 16; i++){
	hex_display[i] = hex_display[i] ^ 0xFF;//inverting bits to match the active low inputs to the
										   //seven -segment displays.
	}
	//Resetting all seven - segment displays and LEDs
	IOWR_16DIRECT(HEX_0_BASE, 0, 0x7F);
	IOWR_16DIRECT(HEX_3_BASE, 0, 0x7F);
	IOWR_16DIRECT(HEX_1_BASE, 0, hex_display[8]);
	IOWR_16DIRECT(HEX_2_BASE, 0, hex_display[8]);
	IOWR_16DIRECT(HEX_4_BASE, 0, hex_display[8]);
	IOWR_16DIRECT(HEX_5_BASE, 0, hex_display[8]);
	IOWR_16DIRECT(HEX_6_BASE, 0, hex_display[8]);
	IOWR_16DIRECT(HEX_7_BASE, 0, hex_display[8]);
	IOWR_16DIRECT(LED_BASE, 0, 0);
	IOWR_16DIRECT(LEDG_BASE, 0, 0);
	init_LCD();
	init_keys();
	init_timers();

	while(1){
		srand(milliseconds);//constantly passing a new seed into srand() for a new
							//random number to generate
	}
	 return 0;
}

