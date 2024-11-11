#include "evse_rgb.h"
#include "stdlib.h"
#include "printf.h"
#include "stdbool.h"
#include "basic_os.h"
#include "drv_delay.h"
#include "EventRecorder.h"
#include "utils.h"

#define LOG_TAG "evse.rgb"
#include  "elog.h"

#define RGB_NUM 12

static void evse_rgb_memset(uint32_t arr[], uint32_t val, int n);

const uint8_t GammaTable[] ={
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2,
    2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5,
    6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11,
    11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18,
    19, 19, 20, 21, 21, 22, 22, 23, 23, 24, 25, 25, 26, 27, 27, 28,
    29, 29, 30, 31, 31, 32, 33, 34, 34, 35, 36, 37, 37, 38, 39, 40,
    40, 41, 42, 43, 44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 54,
    55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70,
    71, 72, 73, 74, 76, 77, 78, 79, 80, 81, 83, 84, 85, 86, 88, 89,
    90, 91, 93, 94, 95, 96, 98, 99,100,102,103,104,106,107,109,110,
    111,113,114,116,117,119,120,121,123,124,126,128,129,131,132,134,
    135,137,138,140,142,143,145,146,148,150,151,153,155,157,158,160,
    162,163,165,167,169,170,172,174,176,178,179,181,183,185,187,189,
    191,193,194,196,198,200,202,204,206,208,210,212,214,216,218,220,
    222,224,227,229,231,233,235,237,239,241,244,246,248,250,252,255
};

const uint8_t GammaTable2[] = {
    10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105,
    110, 115, 120, 125, 130, 135, 140, 145, 150, 155, 160, 165, 170, 175, 180, 185, 190,
    195, 200, 205, 210, 215, 220, 225, 230, 235, 240, 245, 250, 255
};

static ws2812b_handle_t ws2812b;
static uint8_t led_buffer[4096];                // 用2个字节来代表一个WS2812比特

/* 特殊效果对应的颜色数组 */
static uint32_t fluid_buffer_fault[2][RGB_NUM] = {
    {
        0x000000U, 0x000000U, 0x000000U, 0x0F0000U,
        0x2F0000U, 0x3F0000U, 0x3F0000U, 0x2F0000U,
        0x0F0000U, 0x000000U, 0x000000U, 0x000000U
    },
    {
        0x9F0000U, 0x9F0000U, 0x9F0000U, 0x9F0000U,
        0x9F0000U, 0x9F0000U, 0x9F0000U, 0x9F0000U,
        0x9F0000U, 0x9F0000U, 0x9F0000U, 0x9F0000U
    }
};
static uint32_t fluid_buffer_green[2*RGB_NUM] = {
    0x0000A8U, 0x000098U, 0x000088U, 0x000078U,
    0x000068U, 0x000058U, 0x000048U, 0x000038U,
    0x000028U, 0x000018U, 0x000010U, 0x000008U
};
static uint32_t fluid_buffer_blue[RGB_NUM] = {0xFFFFFFU, 0x000000U, 0x000000U, 0x000000U};
// static uint32_t fluid_buffer_rainbow[RGB_NUM] = {
//     COLOR_RGB888_SKYBLUE, COLOR_RGB888_VIOLET, COLOR_RGB888_BROWN,
//     COLOR_RGB888_GOLD, COLOR_RGB888_SEAGREEN, COLOR_RGB888_FORESTGREEN,
//     COLOR_RGB888_SEAGREEN, COLOR_RGB888_CRIMSON, COLOR_RGB888_PINK
// };

static evse_state_t evse_state = EVSE_CHARGING;

/**
 * @brief   更新RGB
 */
static void task_entry_rgb_upgrade(void *parameter)
{
    __IO uint8_t gamma = 10;
    __IO uint8_t flag = 0;
    
    /* 延时计数 */
    __IO uint8_t fault_cnt = 0;
    evse_rgb_init();
    // evse_rgb_set_color(COLOR_RGB888_GRAY, 100, 0xFF);
    for(;;){
        switch (evse_state)
        {
        case EVSE_REBOOT:
            if(flag == 0)
                {if((++gamma) == 100) {flag = 1;}}
            else if(flag == 1)
                {if((--gamma) == 10) {flag = 0;}}
            evse_rgb_set_color(COLOR_RGB888_GRAY, gamma, 0xFF);
            break;
        /* 空闲状态 */
        case EVSE_IDLE:     // 蓝灯呼吸
            if(flag == 0)
                {if((++gamma) == 35) {flag = 1;}}
            else if(flag == 1)
                {if((--gamma) == 5) {flag = 0;}}
            evse_rgb_set_color(COLOR_RGB888_NAVY, GammaTable2[gamma], 0xFF);
            break;
        /* 已刷卡的状态(即插即用款相当于IDLE状态) */
        case EVSE_WAIT_PLUGIN:  // 蓝灯常亮
            evse_rgb_set_color(COLOR_RGB888_NAVY, 100, 0xFF);
            break;
        case EVSE_9V_PWM:   // 绿灯常亮
            evse_rgb_set_color(COLOR_RGB888_GREEN, 100, 0xFF);
            break;
        /* 充电中 */
        case EVSE_CHARGING:
            left_shift(fluid_buffer_green, 2*RGB_NUM);
            ws2812b_write(&ws2812b, fluid_buffer_green, RGB_NUM, led_buffer, 4096);
            break;
        case EVSE_DONE:
        case EVSE_STOP:
            evse_rgb_set_color(COLOR_RGB888_YELLOW, 100, 0xFF);
            break;
        /* 故障 */
        case EVSE_FAULT:
            if(++fault_cnt < 5)
                break;
            fault_cnt = 0;

            if(flag == 0){
                ws2812b_write(&ws2812b, fluid_buffer_fault[0], RGB_NUM, led_buffer, 4096);
                flag = 1;
            }
            else if(flag == 1){
                // evse_rgb_set_color(COLOR_RGB888_RED, 0xFF, 0xFF);
                ws2812b_write(&ws2812b, fluid_buffer_fault[1], RGB_NUM, led_buffer, 4096);
                flag = 0;
            }
            // if(flag == 0){
            //     for(int i = RGB_NUM; i >= 0; i--){
            //         fluid_buffer_fault[i] += 0x050000U;
            //         if(fluid_buffer_fault[i] > 0xFF0000) fluid_buffer_fault[i] = 0xFF0000;
            //     }
            //     if((++gamma) == 20) {flag = 1;}
            // }else if(flag == 1){
            //     for(int i = RGB_NUM; i >= 0; i--){
            //         fluid_buffer_fault[i] -= 0x050000U;
            //         if(fluid_buffer_fault[i] > 0xFF0000) fluid_buffer_fault[i] = 0x00;
            //     }
            //     if((--gamma) == 0) {flag = 0;}
            // }
            // ws2812b_write(&ws2812b, fluid_buffer_fault, RGB_NUM, led_buffer, 4096);
            break;
        default:
            evse_rgb_set_color(COLOR_RGB888_BLACK, 0xFF, 0xFF);
            break;
        }
        bos_delay_ms(50);
    }
}
bos_task_export(rgb_upgrade, task_entry_rgb_upgrade, BOS_MAX_PRIORITY, NULL);

void evse_rgb_init()
{
    uint8_t res;

    DRIVER_WS2812B_LINK_INIT(&ws2812b, ws2812b_handle_t);
    DRIVER_WS2812B_LINK_SPI_10MHZ_INIT(&ws2812b, ws2812b_spi_init);
    DRIVER_WS2812B_LINK_SPI_DEINIT(&ws2812b, ws2812b_spi_deinit);
    DRIVER_WS2812B_LINK_SPI_WRITE_COMMAND(&ws2812b, ws2812b_write_cmd);
    DRIVER_WS2812B_LINK_DELAY_MS(&ws2812b, bos_delay_ms);
    DRIVER_WS2812B_LINK_DEBUG_PRINT(&ws2812b, printf);

    /* ws2812b initialization */
    res = ws2812b_init(&ws2812b);
    if (res != 0){
        log_e("ws2812b: init failed.");
    }
}

// index: ws2812 num
void evse_rgb_set_color(uint32_t color, uint8_t Brightness, uint8_t index)
{
    uint8_t Red, Green, Blue;
    static uint32_t led_color[RGB_NUM] = {0x00};

    Red = ((color & 0xFF0000) >> 16) * Brightness / 255 ;
    Green = ((color & 0x00FF00) >> 8) * Brightness / 255;
    Blue = (color & 0x0000FF) * Brightness / 255;

#if WS28XX_ORDER == WS28XX_ORDER_RGB
    color = ((Red << 16) | (Green << 8) | Blue);
#elif WS28XX_ORDER == WS28XX_ORDER_BGR
    color = ((Blue << 16) | (Green << 8) | Red);
#elif WS28XX_ORDER == WS28XX_ORDER_GRB
    color = ((Green << 16) | (Red << 8) | Blue);
#elif WS28XX_ORDER == WS28XX_ORDER_RBG
    color = ((Red << 16) | (Blue << 8) | Green);
#endif

    if(index > (RGB_NUM-1))
        if(index != 0xFF) return;

    if(index == 0xFF){
        evse_rgb_memset(led_color, color, RGB_NUM);
    }else{
        led_color[index] = color;
    }

    ws2812b_write(&ws2812b, led_color, RGB_NUM, led_buffer, 2048);
}

// void evse_rgb_clear(uint8_t index)
// {
//     if(index > (RGB_NUM-1))
//         if(index != 0xFF) return;

//     if(index == 0xFF){
//         evse_rgb_memset(led_color, 0x000000U, RGB_NUM);
//     }else{
//         led_color[index] = 0x000000U;
//     }

//     ws2812b_write(&ws2812b, led_color, RGB_NUM, led_buffer, 2048);
// }

static void evse_rgb_memset(uint32_t arr[], uint32_t val, int n)
{
    for(int i = 0; i < n; i++){
        arr[i] = val;
    }
}

void evse_rgb_state_update(evse_state_t state){
    evse_state = state;
}
