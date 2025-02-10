#include "drv_flash.h"
#include "basic_os.h"

#define LOG_TAG "drv.flash"
#include "elog.h"

/**
 * @brief      erase page
 * @param[in]  address: erase start address
 * @param[in]  flash_length: flash length(Byte)
 * @retval     ErrStatus
*/
ErrStatus drv_flash_erase_page(uint32_t start_address, uint32_t size){
    uint32_t end_address = start_address + size;
    uint32_t current_address = start_address;
    log_d("Address: 0x%X", start_address);
    log_d("flash_length: 0x%X Byte", size);
    log_d("End address: 0x%X", end_address);
    /* clear pending flags */
    fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
    fmc_unlock();
    while (current_address < end_address){
        if(FMC_READY == fmc_page_erase(current_address)){
            current_address += FLASH_PAGE_SIZE;
        }else{
            log_e("erase page failed.");
            return ERROR;
        }
    }
    fmc_lock();
    return SUCCESS;
}

/**
 * @brief      write 32 bit length data to a given address
 * @param[in]  address: a given address(0x08000000~0x082FFFFF)
 * @param[in]  length: data length, Unit: Word(4-Byte)
 * @param[in]  data_32: data pointer
 * @retval     state of flash operation, refer to FlashStateTypeDef
 * @todo        需要用寄存器优化
*/
ErrStatus drv_flash_write_word(uint32_t address, uint32_t length, void* data_32)
{
    log_d("Address: 0x%X", address);
    uint32_t idx = 0U;
    int32_t* data = (int32_t*)data_32;
    /* unlock the flash program erase controller */
    fmc_unlock();
    /* clear pending flags */
    fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);

    /* write data_32 to the corresponding address */
    for(idx = 0U; idx < length; idx += 1){
        if(FMC_READY == fmc_word_program(address, *(data+idx))){
            address += 4;
        }else{
            log_e("write 32-bit data failed.");
            return ERROR;
        }
    }
    /* lock the flash program erase controller */
    fmc_lock();
    log_d("Write complete.");
    return SUCCESS;
}
