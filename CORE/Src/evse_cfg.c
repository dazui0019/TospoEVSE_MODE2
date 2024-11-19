#include "evse_cfg.h"
#include "basic_os.h"

#define LOG_TAG "evse.cfg"
#include "elog.h"

__attribute__((section("CFG_SECTION"), used)) const static evse_cfg_t evse_cfg = {.max_cur_idx=0};
__attribute__((section("BACKUP_SECTION"), used)) const static uint32_t test_data = 0;   // 用于测试，防止弹警告

static uint16_t cfg_write_index = 0;    // 下一个空白位置
static uint32_t* cfg_buff = (uint32_t*)CFG_BASE_ADDR;   // 将配置文件抽象为一个数组

static void task_entry_flash_test(void *parameter)
{
    evse_cfg_t test_cfg = {.max_cur_idx=0};
    for(;;){
        evse_cfg_write(&test_cfg);
        if(test_cfg.max_cur_idx++ == 400){
            test_cfg.max_cur_idx = 0;
        }
        bos_delay_ms(100);
    }
}
// bos_task_export(flash_test, task_entry_flash_test, BOS_MAX_PRIORITY, NULL);

void evse_cfg_erase(void)
{
    drv_flash_erase_page(CFG_CUR_START_ADDR, CFG_CUR_SIZE);
    cfg_write_index = 0;
}

void evse_cfg_write(evse_cfg_t* cfg)
{
    // 判断是否写满
    if(cfg_write_index > (FLASH_PAGE_SIZE / sizeof(evse_cfg_t))){
        log_d("erase evse_cfg.");
        evse_cfg_erase();
    }

    if(FLASH_EMPTY_DATA != cfg_buff[cfg_write_index]){
        evse_cfg_get_last(cfg);
        if(FLASH_EMPTY_DATA != cfg_buff[cfg_write_index]){
            log_e("cannot get evse_cfg.");
            return;
        }
    }

    drv_flash_write_word(CFG_CUR_START_ADDR+cfg_write_index*sizeof(evse_cfg_t), sizeof(evse_cfg_t)>>2, cfg);
    log_d("cfg_write_index: %d", cfg_write_index);
    cfg_write_index++;
}

/**
 * @brief       读取配置文件
 * @param[out]  cfg: 配置文件
 */
void evse_cfg_get_last(evse_cfg_t* cfg)
{
    (void)cfg;
    if(cfg == NULL){
        log_e("cfg is NULL.");
        return;
    }

    size_t cfg_size = sizeof(evse_cfg_t);
    uint32_t offset = cfg_size>>2;
    for (uint32_t i = 0; i < (FMC_PAGE_SIZE / cfg_size); i++){
        uint32_t* address = cfg_buff+(i*offset);
        uint32_t data = *address;
        if (FLASH_EMPTY_DATA != data){
            continue;
        }else{
            cfg_write_index = i;
            if(cfg_write_index == 0){
                *cfg = *(evse_cfg_t*)(address);
            }else{
                *cfg = *(evse_cfg_t*)(address-offset);
            }
            log_d("buffer_index: %d", cfg_write_index);
            break;
        } 
    }
    return;
}
