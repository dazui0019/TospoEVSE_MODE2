#include "evse_cfg.h"

void evse_cfg_erase(void)
{
    /* unlock the flash program/erase controller */
    fmc_unlock();

    /* clear all pending flags */
    fmc_flag_clear(FMC_FLAG_BANK0_END);
    fmc_flag_clear(FMC_FLAG_BANK0_WPERR);
    fmc_flag_clear(FMC_FLAG_BANK0_PGERR);

    /* erase the flash pages */

    fmc_page_erase(CFG_CUR_START_ADDR);
    fmc_flag_clear(FMC_FLAG_BANK0_END);
    fmc_flag_clear(FMC_FLAG_BANK0_WPERR);
    fmc_flag_clear(FMC_FLAG_BANK0_PGERR);

    /* lock the main FMC after the erase operation */
    fmc_lock();
}

void evse_cfg_write_cur(uint32_t cur)
{
    /* unlock the flash program/erase controller */
    fmc_unlock();
    fmc_word_program(CFG_CUR_START_ADDR, cur);
    fmc_flag_clear(FMC_FLAG_BANK0_END);
    fmc_flag_clear(FMC_FLAG_BANK0_WPERR);
    fmc_flag_clear(FMC_FLAG_BANK0_PGERR);
    /* lock the main FMC after the program operation */
    fmc_lock();
}
