/* Copyright (c) 2014 Matt Evans
 *
 * flashvars:  Store simple configuration data in STM32 flash, with
 * basic 'erase avoidance'.
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

#include "flashvars.h"
#ifndef SIM
#include <stm32f0xx_flash.h>
#endif

/* Some very basic code to maintain a structure of 'environment variables'
   in flash.  A dedicated page (1K at end of flash) is used here.
   
   Although this code maintains multiple records (which are written from the
   top downwards) in order to minimise erase cycles, no effort is made to
   maintain a generation count/multiple pages for reliability.  During
   the page being erased, a powercut will lose the data staged in RAM.

   Applications should check flashvars_at_defaults() to see if the
   variables were read correctly and, if not, set local defaults.
 */

#define FLASH_END	0x08008000
#define FLASH_PAGE_BASE	(FLASH_END-1024)
#define FLASH_EYECATCH	0xbeef

FlashVars env;

#define FLASH_ENV_SIZE_WORDALIGN	((sizeof(FlashVars) + 3) & ~3)

#ifdef SIM
void	flashvars_init(void) {}
void	flashvars_update(void) {}
int	flashvars_at_defaults(void) { return 1; }
#else

static uint16_t	record_offset = ~0;
FlashVars *env_in_flash;

void	flashvars_init(void)
{
	// Search for marker of first record, from bottom UP:
	uint32_t *fl = (uint32_t *)FLASH_PAGE_BASE;

	// Record will be word-aligned!
	for (int i = 0; i < 1024/4; i++) {
		if (((FlashVars *)&fl[i])->_eyecatcher == FLASH_EYECATCH) {
			env_in_flash = (FlashVars *)&fl[i];
			record_offset = i*4;
			// Copy vars to runtime mutable version:
			env = *env_in_flash;
			break;
		}
	}
}

// Returns true if no records found, so variables are not initialised.
int	flashvars_at_defaults(void)
{
	return (record_offset == ~0);
}

void	flashvars_update(void)
{
	// Writes env into flash as new record.  This will be placed
	// below the last-found record unless the bottom of the page is
	// reached, in which case the page is erased and the record
	// written at the top.

	FLASH_Unlock();
	// STM32 examples recommend this:
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

	if ((record_offset > 1024) ||
	    ((record_offset - FLASH_ENV_SIZE_WORDALIGN) < 0)) {
		// Erase first, then write at top:

		if (FLASH_ErasePage(FLASH_PAGE_BASE) != FLASH_COMPLETE) {
			// Oh no, fail!
			// printf("Flash erase failed\r\n");

			// Not much more we can do.  Bail out:
			return;
		}

		record_offset = 1024-FLASH_ENV_SIZE_WORDALIGN;
	} else {
		// There is space for a new record:
		record_offset -= FLASH_ENV_SIZE_WORDALIGN;
	}

	// Now write the struct:
        env._eyecatcher = FLASH_EYECATCH;
	for (int i = 0; i < FLASH_ENV_SIZE_WORDALIGN; i += 4) {
		uint32_t word = ((uint32_t *)&env)[i/4];
		if (FLASH_ProgramWord(FLASH_PAGE_BASE + record_offset + i,
				      word) != FLASH_COMPLETE) {
			// Oh no, more fail!
			// printf("Flash program, offs %d, failed\r\n",
			//		record_offset+i);
			// Again, not much we can do except bail.  Maybe
			// erasing the page again might be good?  Attempt
			// to bodge out the eyecatcher, though, in case
			// the old version is OK:
			FLASH_ProgramWord(FLASH_PAGE_BASE + record_offset, 0);
			return;
		}
	}

	FLASH_Lock();
}

#endif
