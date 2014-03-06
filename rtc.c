/* Copyright (c) 2014 Matt Evans
 *
 * rtc:  Manage the STM32 real-time clock.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stm32f0xx_rtc.h>
#include <stm32f0xx_rcc.h>
#include <stm32f0xx_pwr.h>
#include "rtc.h"
// Temporary, for fake rtc:
#include "time.h"
#include "input.h"

static int fast = 0;
static time_callback_t tcb;
static uint64_t tus_now = 0;

static void rtc_timer_callback(uint64_t t_now)
{
	if (fast)
		tus_now += 100000;
	else
		tus_now += 10000;
}

void rtc_init(void)
{
	int i = input_get_raw();
	if (i) {
		fast = 1;

		tcb.callback = rtc_timer_callback;
		tcb.period = 10;           /* ms */
		time_callback_periodic(&tcb);
	}

        // Set up RTC according to periph lib example method:
	if (RTC_ReadBackupRegister(RTC_BKP_DR0) != 0xdeadbeef) {
		RTC_InitTypeDef rtci;
		RTC_TimeTypeDef rtct;

		RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
		PWR_BackupAccessCmd(ENABLE);

		RCC_LSEConfig(RCC_LSE_ON);
		/* Wait for LSE to stabilise */
		while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET) { }
		/* Select LSE */
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
  
		RCC_RTCCLKCmd(ENABLE);

		RTC_WaitForSynchro();	// What does this do?

		/* Configure the RTC data register and RTC prescaler */
		rtci.RTC_AsynchPrediv = 0x7f; // These are the defaults anyway
		rtci.RTC_SynchPrediv = 0xff;
		rtci.RTC_HourFormat = RTC_HourFormat_12;
		RTC_Init(&rtci);

		rtct.RTC_H12 = RTC_H12_AM;
		rtct.RTC_Hours = 0;
		rtct.RTC_Minutes = 0;
		rtct.RTC_Seconds = 0;
		RTC_SetTime(RTC_Format_BIN, &rtct);

		RTC_WriteBackupRegister(RTC_BKP_DR0, 0xdeadbeef);
	} else {
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
		PWR_BackupAccessCmd(ENABLE);
		RTC_WaitForSynchro();
	}
}

void rtc_gettime(tod_t *time_out)
{
	if (fast) {
		// Absolute time to TOD:
		time_out->hour 	= ((tus_now/1000000) % (12*60*60)) / (60*60);
		time_out->min  	= ((tus_now/1000000) % (60*60)) / 60;
		time_out->sec 	= (tus_now/1000000) % 60;
		time_out->subsec = ((tus_now*64) % 64000000)/1000000;
		time_out->amnpm = (((tus_now/1000000) % (24*60*60)) /
				   (60*60)) < 12;
	} else {
		RTC_TimeTypeDef rtct;
		RTC_GetTime(RTC_Format_BIN, &rtct);
		time_out->hour 	= rtct.RTC_Hours;
		if (time_out->hour > 11)
			time_out->hour -= 12;
		time_out->min 	= rtct.RTC_Minutes;
		time_out->sec 	= rtct.RTC_Seconds;
		// Formula in DS:  (PREDIV_S - SS) / (PREDIV_S + 1)
		// That's /256, so to give 0-63, /4:
		time_out->subsec = (0xff - RTC_GetSubSecond()) >> 2;
		time_out->amnpm = rtct.RTC_H12 == RTC_H12_AM;
	}
}

void rtc_settime(tod_t *time)
{
	RTC_TimeTypeDef rtct;

	rtct.RTC_H12 = time->amnpm ? RTC_H12_AM : RTC_H12_PM;
	rtct.RTC_Hours = time->hour;
	rtct.RTC_Minutes = time->min;
	rtct.RTC_Seconds = time->sec;

	RTC_SetTime(RTC_Format_BIN, &rtct);
}
