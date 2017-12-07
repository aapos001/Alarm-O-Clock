#include <stdint.h> 
#include <stdlib.h> 
#include <stdio.h> 
#include <stdbool.h> 
#include <string.h> 
#include <math.h> 
#include <avr/io.h> 
#include <avr/interrupt.h> 
#include <avr/eeprom.h> 
#include <avr/portpins.h> 
#include <avr/pgmspace.h> 
#include "lcd.h"
 
//FreeRTOS include files 
#include "FreeRTOS.h" 
#include "task.h" 
#include "croutine.h" 
#include "ds3231.h"
#include "i2c_master.h"

#define LEFT (!(PINA & 0x04))
#define RIGHT (!(PINA & 0x08))
#define HBSEN (!(PINA & 0x10))
#define UP (ADC > 750)
#define DOWN (ADC < 200)

enum ClkOutState {ClkOutINIT, ClkOut, ClkBWait, ToMenu} clkOut_state;
enum MenuOutState {MenuOutINIT, MenuWait, MenuBWait, MenuOut1, MenuOut1W, MenuOut2, MenuOut2W, MenuOut3, MenuOut3W, ToAlarm, ToTemp, ToHour, ToClock} menuOut_state;
enum AlarmOutState {AlarmOutINIT, AlarmWait, AlarmBWait, AToClock, AToMenu, DAO1, AO1, AOI1, AOD1, DAO2, AO2, AOI2, AOD2, DAO3, AO3, AOM} alarmOut_state;
enum TempOutState {TempOutINIT, TempWait, TempBWait, TempOut, CToClock, FToClock} tempOut_state;
enum HourOutState {HourOutINIT, HourWait, HourBWait,  HourOut, HTo12Clock, HTo24Clock} hourOut_state;			
enum AlarmPatState {AlarmPatINIT, AlarmPatWait, AlarmPat1, AlarmPat2, AlarmPatReset} alarmPat_state;

/* admin variables */
unsigned char clockO = 0x00; // clock admin
unsigned char menuO = 0x00; // menu admin
unsigned char alarmO = 0x00; // alarm admin
unsigned char tempO = 0x00; // temp admin
unsigned char hourO = 0x00; // hour admin

/* clock admin variables */
unsigned char clktimer = 0x00; // refreshes display

/* hour admin variables */
unsigned char timeset = 0x00; // 0x00 = 12h 0x01 = 24h

/* temp admin variables */
unsigned char tempset = 0x00; // 0x00 = F 0x01 = C

/* alarm admin variables */
uint8_t alarmset_hour = 0xFF;
uint8_t alarmset_min = 0xFF; 
unsigned char alarmset_AMPM = 0xFF;
unsigned char hbeat; // heartbeat sensor time counter
uint8_t alarm_hour; // tens hour
uint8_t alarm_min; // hour
unsigned char alarmAMPM; // AM or PM 0x00 AM 0x01 PM

/* DS3231 variables */
uint8_t ampm, hr, min, sec, year, mnth, day, dt, temp;
uint8_t hrdec, mindec, yeardec, mnthdec, daydec, dtdec;

/* Joystick */
void A2D_init() {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
	// ADEN: Enables analog-to-digital conversion
	// ADSC: Starts analog-to-digital conversion
	// ADATE: Enables auto-triggering, allowing for constant
	//	    analog to digital conversions.
}

void UpdateVars() {
	/* time variables */
	ds3231_get(&hr,&min,&sec,&year,&mnth,&dt,&day);
	if(timeset == 0x00) { // 12 hour
		ampm = hr;
		ampm &= 0x20; // ampm bit
		ampm = (ampm>>5);
		hrdec = bcd2dec(hr & 0x1F);
	}
	else if(timeset == 0x01) { // 24 hour
		hrdec = bcd2dec(hr & 0x3F);
	}
	mindec = bcd2dec(min);
	yeardec = bcd2dec(year);
	mnthdec = bcd2dec(mnth);
	dtdec = bcd2dec(dt);
	
	/* temp variable */ 
	ds3231_getT(&temp);
	if(tempset == 0x00) {// 0x00 F 0x01 C
		temp = (temp * (1.8)) + 32;
	}
}

void ClkOut_Init(){
	clkOut_state = ClkOutINIT;
}

void MenuOut_Init() {
	menuOut_state = MenuOutINIT;
}

void AlarmOut_Init(){
	alarmOut_state = AlarmOutINIT;
}

void TempOut_Init() {
	tempOut_state = TempOutINIT;
}

void HourOut_Init() {
	hourOut_state = HourOutINIT;
}

void AlarmPat_Init() {
	alarmPat_state = AlarmPatINIT;
}

void ClkOut_Tick(){
	
	//Transitions
	switch(clkOut_state){
		
		case ClkOutINIT:
			clkOut_state = ClkOut; // admin
		break;
		
		case ClkOut: // LCDOut admin
			clkOut_state = ClkBWait;
		break;
		
		case ClkBWait:
			if(LEFT && !RIGHT) { // admin to menu tick
				clkOut_state = ToMenu;
			}
			else if(clktimer >= 150) {
				clkOut_state = ClkOut;
			}
			else { // do nothing
				clkOut_state = ClkBWait;
			}
		break;
		
		case ToMenu: //menu tick admin
			if(clockO == 0x01 && menuO == 0x00) { 
				clkOut_state = ClkOut; //admin to LCDOut tick
			}
			else {
				clkOut_state = ToMenu; //do nothing
			}
		break;
		
		default: 
			clkOut_state = ClkOutINIT;
		break;
	}
	
	//Actions
	switch(clkOut_state){
		
		case ClkOutINIT:
		break;
		
		case ClkOut:
			clktimer = 0;
			UpdateVars();
			clockO = 0x01;
			LCD_ClearScreen();
			SLCD_WriteData(1,(hrdec / 10) + '0'); // tens hours
			SLCD_WriteData(2, (hrdec % 10) + '0'); // hours
			SLCD_WriteData(3, ':');
			SLCD_WriteData(4, (mindec / 10) + '0'); // tens minutes
			SLCD_WriteData(5, (mindec % 10) + '0'); // minutes
			if((timeset == 0x00) && (ampm == 1)) {
				LCD_DisplayString(6, "PM");
			}
			else if((timeset == 0x00) && (ampm == 0)){
				LCD_DisplayString(6, "AM");
			}
			SLCD_WriteData(9, (temp / 10) + '0'); // tens temp
			SLCD_WriteData(10, (temp % 10) + '0'); // temp
			if(tempset == 0x00) {
				SLCD_WriteData(11, 'F');				
			}
			else {
				SLCD_WriteData(11, 'C');			
			}
			SLCD_WriteData(17, (mnthdec / 10) + '0'); // tens months
			SLCD_WriteData(18, (mnthdec % 10) + '0');
			SLCD_WriteData(19, '/');
			SLCD_WriteData(20, (dtdec / 10) + '0');
			SLCD_WriteData(21, (dtdec % 10) + '0');
			LCD_DisplayString(22, "/20");
			SLCD_WriteData(25, (yeardec / 10) + '0');
			SLCD_WriteData(26, (yeardec % 10) + '0');
			switch(day) {
				case 1:
					LCD_DisplayString(28, "SUN");
				break;
				case 2:
					LCD_DisplayString(28, "MON");
				break;
				case 3:
					LCD_DisplayString(28, "TUE");
				break;
				case 4:
					LCD_DisplayString(28, "WED");
				break;
				case 5:
					LCD_DisplayString(28, "THU");
				break;
				case 6:
					LCD_DisplayString(28, "FRI");
				break;
				case 7:
					LCD_DisplayString(28, "SAT");
				break;
				default:
					LCD_DisplayString(28, "broke");
				break;
			}
		break;
		// wait for input
		case ClkBWait:
		clktimer++;
		break;
		//if clock -> menu, clear screen give menu admin
		case ToMenu:
			if(clockO) {		
				clockO = 0x00;
				menuO = 0x01;
			}
		clktimer++;
		break;
		
		default:
			clockO = 0x01;
		break;
	}
}

void MenuOut_Tick() {
	
	//Transitions
	switch(menuOut_state) {
		
		case MenuOutINIT:
			menuOut_state = MenuWait;
		break;
		
		case MenuWait: //wait for admin from clock
			if(menuO == 0x01) {
				menuOut_state = MenuBWait;
			}
			else { 
				menuOut_state = MenuWait;
			}
		break;
		//wait for button to be unpressed
		case MenuBWait:
			if(LEFT || RIGHT) {
				menuOut_state = MenuBWait;
			}
			else {
				menuOut_state = MenuOut1;
			}
		break;
		//L: admin to alarm R: admin to clock  D: Out2 
		case MenuOut1: 
			menuOut_state = MenuOut1W;
		break;
		// wait for input
		case MenuOut1W:
			if(LEFT && !RIGHT && !DOWN) { 
				menuOut_state = ToAlarm;
			}
			else if(RIGHT && !LEFT && !DOWN) { 
				menuOut_state = ToClock;
			}
			//ADD ELSE IF DOWN
			else if(DOWN && !LEFT && !RIGHT) {
				menuOut_state = MenuOut2;
			}
			else {
				menuOut_state = MenuOut1W;
			}
		break;
		//L: admin to temp R: admin to clock U: Out1 D: Out3
		case MenuOut2:
			menuOut_state = MenuOut2W;
		break;
		
		// wait for input
		case MenuOut2W:
			if(LEFT && !RIGHT) {
				menuOut_state = ToTemp;
			}
			else if(RIGHT && !LEFT) { 
				menuOut_state = ToClock;
			}
			else if(UP && !LEFT && !RIGHT) {
				menuOut_state = MenuOut1;
			}
			else if(DOWN && !LEFT && !RIGHT) {
				menuOut_state = MenuOut3;
			}
			
			else {
				menuOut_state = MenuOut2W;
			}
		break;
		
		// L: admin to hour R: admin to clock U: Out2
		case MenuOut3:
			menuOut_state = MenuOut3W;
		break;
		// wait for input
		case MenuOut3W:
			if(LEFT && !RIGHT) {
				menuOut_state = ToHour;
			}
			else if(RIGHT && !LEFT) {
				menuOut_state = ToClock;
			}
			//ADD ELSE IF UP
		    else if(UP && !LEFT && !RIGHT) {
				menuOut_state = MenuOut2;
			}
			else {
				menuOut_state = MenuOut3W;
			}
		break;
		//go to wait state 
		case ToClock:
			menuOut_state = MenuWait;
		break;
		//wait for either admin or return to wait state
		case ToAlarm:
			if(menuO && !alarmO && !clockO) { //admin from alarm
				menuOut_state = MenuOut1;
			}
			else if(clockO && !menuO && !alarmO) { //give admin to clock
				menuOut_state = MenuWait;
			}
			else { //do nothing
				menuOut_state = ToAlarm; 
			}
		break;
		//wait for either admin to return to wait state
		case ToTemp:
			if(menuO && !tempO && !clockO) { //admin from temp
				menuOut_state = MenuOut1;
			}
			else if(clockO && !menuO && !tempO) { //give admin to clock
				menuOut_state = MenuWait;
			}
			else { //do nothing
				menuOut_state = ToTemp;
			}
		break;
		//wait for either admin or return to wait state
		case ToHour:
			if(menuO && !hourO && !clockO) { //admin from hour
				menuOut_state = MenuOut1;
			}
			else if(clockO && !menuO && !hourO) { //give admin to clock
				menuOut_state = MenuWait;
			}
			else { //do nothing
				menuOut_state = ToHour;
			}
		break;
		
		default:
			menuOut_state = MenuOutINIT;
		break;
	}
	
	// Actions
	switch(menuOut_state) {
		case MenuOutINIT:
		break;
		// wait for menuO == 1
		case MenuWait:
		break;
		case MenuBWait:
		break;
		// Display Menu, 1. Alarm
		case MenuOut1:
			LCD_ClearScreen();
			LCD_DisplayString(1, "Menu");
			LCD_DisplayString(17, "1. Alarm <-");
		break;
		// wait for input 
		case MenuOut1W:
		break;
		// Display 1. Alarm, 2. C/F
		case MenuOut2:
			LCD_ClearScreen();
			LCD_DisplayString(1, "1. Alarm");
			LCD_DisplayString(17, "2. F/C <-");
		break;
		// wait for input
		case MenuOut2W:
		break;
		// Display 2. C/F 3. 12/24H
		case MenuOut3:
			LCD_ClearScreen();
			LCD_DisplayString(1, "2. F/C");
			LCD_DisplayString(17, "3. 12/24H <-");
		break;
		// wait for input
		case MenuOut3W:
		break;
		// set clockO to 1, menuO to 0
		case ToClock:
		// clear screen give admin to clock	
			clockO = 1;
			menuO = 0;
		break;
		// if menu-> alarm, set alarmO to 1, menuO to 0
		case ToAlarm:
			if(menuO && !alarmO) {
				alarmO = 0x01;
				menuO = 0x00;			
			}
		break;
		// if menu->temp, set tempO to 1, menuO to 0
		case ToTemp:
			if(menuO && !tempO) {
				tempO = 0x01;
				menuO = 0x00;
			}
		break;
		// if menu->hour, set hourO to 1, menuO to 0
		case ToHour:
			if(menuO && !hourO) {
				hourO = 0x01;
				menuO = 0x00;
			}
		break;
		
		default:
			menuO = 0;
		break;
	}
} 

void AlarmOut_Tick() {
	
	//Transitions
	switch(alarmOut_state) {
		
		case AlarmOutINIT:
			alarmOut_state = AlarmWait;
		break;
    
		// wait for admin from menu
		case AlarmWait:
			if(alarmO == 0x01) {
				alarmOut_state = AlarmBWait;
			}
			else {
				alarmOut_state = AlarmWait;
			}
		break;

		// wait for button to be unpressed 
		case AlarmBWait:
			if(LEFT || RIGHT) { 
				alarmOut_state = AlarmBWait;
			}
			else {
				alarmOut_state = AO1;
			}
		break;
    
		// display updated hours
		case DAO1:
			alarmOut_state = AO1;
		break;
    
		// L: increment cursor R: decrement cursor U: increment hours D: decrement hours
		case AO1:
			if(LEFT && !RIGHT && !UP && !DOWN) { 
				alarmOut_state = DAO2;
			}
			else if(RIGHT && !LEFT && !UP && !DOWN) {
				alarmOut_state = AToMenu;
			}
			else if(UP && !LEFT && !RIGHT && !DOWN) {
				alarmOut_state = AOI1;
			}
			else if(DOWN && !LEFT && !RIGHT && !UP) {
				alarmOut_state = AOD1;
			}
			else {
				alarmOut_state = AO1;
			}
		break;
    
	    // increment hours number
	    case AOI1:
			alarmOut_state = DAO1;
	    break;
    
	    // decrement hours number
	    case AOD1:
			alarmOut_state = DAO1;
	    break;
		
		// display updated minutes number
		case DAO2:
			alarmOut_state = AO2;
		break;
		
		// L: go to AMPM/clock R: go to hours U: increment minutes number D: decrement minutes number
		case AO2:
		if(LEFT && !RIGHT && !UP && !DOWN) { // 0x01 24h 0x00 12h
			if(timeset) {
				alarmOut_state = AToClock;
			}
			else {
				alarmOut_state = DAO3;
			}
		}
		else if(RIGHT && !LEFT && !UP && !DOWN) {
			alarmOut_state = DAO1;
		}
		else if(UP && !LEFT && !RIGHT && !DOWN) {
			alarmOut_state = AOI2;
		}
		else if(DOWN && !LEFT && !RIGHT && !UP) {
			alarmOut_state = AOD2;
		}
		else {
			alarmOut_state = AO2;
		}
		break;
		
		// increment hours number
		case AOI2:
			alarmOut_state = DAO2;
		break;
		
		// decrement hours number
		case AOD2:
			alarmOut_state = DAO2;
		break;
		
		// display AM/PM choice
		case DAO3:
			alarmOut_state = AO3;
		break;
		
		// wait for input
		case AO3:
			if(LEFT && !RIGHT && !UP && !DOWN) {
				alarmOut_state = AToClock;
			}
			else if(RIGHT && !LEFT && !UP && !DOWN) {
				alarmOut_state = DAO2;
			}
			else if((UP || DOWN) && !LEFT && !RIGHT) {
				alarmOut_state = AOM;
			}
			else {
				alarmOut_state = AO3;
			}
		break;
		
		// change display to AM or PM
		case AOM:
			alarmOut_state = DAO3;
		break;
    
		// set alarm, go back to clock
		case AToClock:
			alarmOut_state = AlarmWait;
		break;
    
		// cancel alarm, go back to menu
		case AToMenu:
			alarmOut_state = AlarmWait;
		break;
		
		default:
			alarmOut_state = AlarmOutINIT;
		break;
	}
	//Actions
	switch(alarmOut_state) {
		
		case AlarmOutINIT:
		break;
    
		// wait for admin
		case AlarmWait:
		break;
    
		// display alarm
		case AlarmBWait:
			alarm_hour = 12;
			alarm_min = 0;
			alarmAMPM = 0;
			LCD_ClearScreen();
			LCD_DisplayString(1, "Set Alarm");
			if(timeset) { // 24 hours
				LCD_DisplayString(17, "12:00");
			}
			else {
				LCD_DisplayString(17, "12:00AM");				
			}
			LCD_Cursor(17);
		break;
    
		// wait for input
		case AO1:
		break;
		
		// output AO1 
		case DAO1:
			LCD_Cursor(17);
			LCD_WriteData((alarm_hour / 10) + '0');
			LCD_Cursor(18);
			LCD_WriteData((alarm_hour % 10) + '0');
		break;
		
		// increment alarm_hour
		case AOI1:
			alarm_hour++;
			if(timeset) { // 24 overflows to 0
				if(alarm_hour > 23) {
					alarm_hour = 0;
				}
			}
			else {
				if(alarm_hour > 12) { // 12 overflows to 1
					alarm_hour = 1;
				}				
			}
			break;
			
		// decrement alarm_hour
		case AOD1:
			if(timeset && alarm_hour <= 0) { // 0 underflows to 24
					alarm_hour = 24;
			}
			else if(!timeset && alarm_hour <= 1) { // 1 underflows to 12
					alarm_hour = 12;
			}
			else {
				alarm_hour--;
			}
		break;
		
		// wait for input
		case AO2:
		break;
		
		// output AO2
		case DAO2:
			LCD_Cursor(20);
			LCD_WriteData((alarm_min / 10) + '0');
			LCD_Cursor(21);
			LCD_WriteData((alarm_min % 10) + '0');
		break;
		
		// increment alarm_minute
		case AOI2:
			alarm_min++;
			if(alarm_min > 59) {
				alarm_min = 0;
			}	
		break;
		
		// decrement alarm_minute
		case AOD2:
			if(alarm_min <= 0) {
				alarm_min = 59;
			}
			else {
				alarm_min--;
			}
		break;
		
		// display AM or PM
		case DAO3:
			if(alarmAMPM) { // 0x00 AM 0x01 PM
				LCD_DisplayString(22, "PM");
			}
			else {
				LCD_DisplayString(22, "AM");
			}
		break;
		
		case AO3:
		break;
		
		// invert alarmAMPM
		case AOM:
			alarmAMPM ^= 1;
		break;
		
		// set alarmset hour to 24 hours give admin to clock
		case AToClock:
			alarmset_hour = alarm_hour;
			alarmset_min = alarm_min;
			alarmset_AMPM = alarmAMPM;
			alarmO = 0x00;
			clockO = 0x01;
		break;

		// cancel, go back to menu
		case AToMenu:
			alarmO = 0x00;
			menuO = 0x01;
		break;
		
		default:
			alarmO = 0x00;
		break;
	}
}

void TempOut_Tick() {
	
	//Transitions
	switch(tempOut_state) {
		case TempOutINIT:
			tempOut_state = TempWait;
		break;
		// wait for admin from menu
		case TempWait:
			if(tempO == 0x01) { 
				tempOut_state = TempBWait;
			}
			else {
				tempOut_state = TempWait;
			}
		break;
		// wait for button to be unpressed 
		case TempBWait:
			if(LEFT || RIGHT) {
				tempOut_state = TempBWait;
			}
			else {
				tempOut_state = TempOut;
			}
		break;
		// L:F R:C
		case TempOut:
			if(LEFT && !RIGHT) {
				tempOut_state = FToClock;
			}
			else if(RIGHT && !LEFT) {
				tempOut_state = CToClock;
			}
			else {
				tempOut_state = TempOut;
			}
		break;
		// give admin back to clock
		case FToClock:
			tempOut_state = TempWait;
		break;
		// give admin back to clock
		case CToClock:
			tempOut_state = TempWait;
		break;
		
		default:
			tempOut_state = TempOutINIT;
		break;
	}
	
	//Actions
	switch(tempOut_state) {
		
		case TempOutINIT:
		break;
		// wait for admin from menu
		case TempWait:
		break;
		// display choices
		case TempBWait:
			LCD_ClearScreen();
			LCD_DisplayString(1, "L:Fahrenheit");
			LCD_DisplayString(17, "R:Celsius");
		break;
		// wait for choice
		case TempOut:
		break;
		// set tempset to F, give admin back to clock
		case FToClock:
			tempset = 0x00;
			tempO = 0x00;
			clockO = 0x01;
		break;
		// set tempset to C, give admin back to clock
		case CToClock:
			tempset = 0x01;
			tempO = 0x00;
			clockO = 0x01;
		break;
		
		default:
			tempO = 0x00;
		break;
	}
}

void HourOut_Tick() {
	
	//Transitions
	switch(hourOut_state) {
		
		case HourOutINIT:
			hourOut_state = HourWait;
		break;
		// wait for admin from menu
		case HourWait:
			if(hourO == 0x01) {
				hourOut_state = HourBWait;
			}
			else {
				hourOut_state = HourWait;
			}
		break;
		// wait for button to be unpressed
		case HourBWait:
			if(LEFT || RIGHT) {
				hourOut_state = HourBWait;
			}
			else {
				hourOut_state = HourOut;
			}
		break;
		// L:12 R:24
		case HourOut:
			if(LEFT && !RIGHT) {
				hourOut_state = HTo12Clock;
			}
			else if(RIGHT && !LEFT) {
				hourOut_state = HTo24Clock;
			}
			else {
				hourOut_state = HourOut;	
			}
		break;
		// give admin back to clock
		case HTo12Clock:
			hourOut_state = HourWait;
		break;
		// give admin back to clock
		case HTo24Clock:
			hourOut_state = HourWait;
		break;
		
		default:
			hourOut_state = HourOutINIT;
		break;
	}
	//Actions
	switch(hourOut_state) {
		
		case HourOutINIT:
		break;
		// wait for hourO == 0x01
		case HourWait:
		break;
		// display choices
		case HourBWait:
			LCD_ClearScreen();
			LCD_DisplayString(1, "L:12H");
			LCD_DisplayString(17, "R:24H");
		break;
		// wait for choice to be made
		case HourOut:
		break;
		// set shared variable to 12h
		case HTo12Clock:
			timeset = 0x00;
			ds3231_setHr(timeset, hr);
			if(alarmset_hour > 12) {
				alarmset_hour -= 12;
			} 
			hourO = 0x00;
			clockO = 0x01;
		break;
		
		// set shared variable to 24h 
		case HTo24Clock:
			timeset = 0x01;
			ds3231_setHr(timeset, hr);
			if(alarmset_AMPM) {
				alarmset_hour += 12;
			}
			hourO = 0x00;
			clockO = 0x01;
		break;
		
		default:
			hourO = 0x00;
		break;
	}
}

void AlarmPat_Tick() {
	
	//Transitions
	switch(alarmPat_state) {
		
		case AlarmPatINIT:
			alarmPat_state = AlarmPatWait;
		break;
		
		// wait for alarm timer
		case AlarmPatWait:
			if((alarmset_hour == hrdec) && (alarmset_min == mindec)) { // alarm time
				alarmPat_state = AlarmPat1;
			}
			else {
				alarmPat_state = AlarmPatWait;
			}
		break;
		
		// display pattern until hbsensor > 6
		case AlarmPat1:
			if(HBSEN) {	
				hbeat++;
			}
			if(hbeat > 6) {
				alarmPat_state = AlarmPatReset;
			}
			else {
				alarmPat_state = AlarmPat2;
			}
		break;
		
		case AlarmPat2:
			if(HBSEN) {
				hbeat++;
			}
			if(hbeat > 6) {
				alarmPat_state = AlarmPatReset;
			}
			else {
				alarmPat_state = AlarmPat1;
			}
		break;
		
		case AlarmPatReset:
			alarmPat_state = AlarmPatINIT;
		break;
		
		default:
			alarmPat_state = AlarmPatINIT;
		break;
	}
	
	// Actions
	switch(alarmPat_state) {
		
		case AlarmPatINIT:
		break;
		
		case AlarmPatWait:
		break;
		
		// alarm until hbeat > 6
		case AlarmPat1:
		PORTB = 0xFF;
		break;
		
		// alarm until hbeat > 6
		case AlarmPat2:
		PORTB = 0x00;
		break;
		
		case AlarmPatReset:
		alarmset_hour = 0xFF;
		alarmset_min = 0xFF;
		PORTB = 0x00;
		break;
		
		default:
		break;
	}
}

void ClkOutTask()
{
	ClkOut_Init();
   for(;;) 
   { 	
	ClkOut_Tick();
	vTaskDelay(200); 
   } 
}

void MenuOutTask()
{
	MenuOut_Init();
	for(;;)
	{
		MenuOut_Tick();
		vTaskDelay(200);
	}
}

void AlarmOutTask()
{
	AlarmOut_Init();
	for(;;)
	{
		AlarmOut_Tick();
		vTaskDelay(200);
	}
}

void TempOutTask()
{
	TempOut_Init();
	for(;;)
	{
		TempOut_Tick();
		vTaskDelay(200);
	}
}

void HourOutTask()
{
	HourOut_Init();
	for(;;)
	{
		HourOut_Tick();
		vTaskDelay(200);
	}
}

void AlarmPatTask()
{
	AlarmPat_Init();
	for(;;)
	{
		AlarmPat_Tick();
		vTaskDelay(500);
	}
}

void StartSecPulse(unsigned portBASE_TYPE Priority)
{	
	xTaskCreate(ClkOutTask, (signed portCHAR *)"ClkOutTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(MenuOutTask, (signed portCHAR *)"MenuOutTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(AlarmOutTask, (signed portCHAR *)"AlarmOutTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(TempOutTask, (signed portCHAR *)"TempOutTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(HourOutTask, (signed portCHAR *)"HourOutTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(AlarmPatTask, (signed portCHAR *)"AlarmPatTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}	
 
int main(void) 
{ 
    DDRA = 0x00; PORTA = 0xFF;
    DDRD = 0xFF; PORTD = 0x00;
	DDRB = 0xFF; PORTB = 0x00;
    A2D_init();
    LCD_init();
	ds3231_init();
	_delay_ms(100);
	
	/* hour, minute, second, am/pm, year, month, date, day */
	//ds3231_set(0x07, 0x33, 0x00, 0x01, 0x17, 0x11, 0x28, 0x03);
		
    //Start Tasks  
    StartSecPulse(1);
    //RunSchedular 
    vTaskStartScheduler(); 
    
	return 0; 
}