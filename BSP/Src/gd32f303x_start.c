#include "gd32f303x_start.h"

/* private variables */
static uint32_t GPIO_PORT[LEDn]             = {LED0_GPIO_PORT,  LED1_GPIO_PORT, LED2_GPIO_PORT};
static uint32_t GPIO_PIN[LEDn]              = {LED0_PIN,        LED1_PIN,       LED2_PIN};
static rcu_periph_enum GPIO_CLK[LEDn]       = {LED0_GPIO_CLK,   LED1_GPIO_CLK,  LED2_GPIO_CLK};

static uint32_t KEY_PORT[KEYn]              = {KEY0_GPIO_PORT,          KEY1_GPIO_PORT,         KEY2_GPIO_PORT,         KEY3_GPIO_PORT,       };
static uint32_t KEY_PIN[KEYn]               = {KEY0_PIN,                KEY1_PIN,               KEY2_PIN,               KEY3_PIN,             };
static rcu_periph_enum KEY_CLK[KEYn]        = {KEY0_GPIO_CLK,           KEY1_GPIO_CLK,          KEY2_GPIO_CLK,          KEY3_GPIO_CLK,        };
static exti_line_enum KEY_EXTI_LINE[KEYn]   = {KEY0_EXTI_LINE,          KEY1_EXTI_LINE,         KEY2_EXTI_LINE,         KEY3_EXTI_LINE,       };
static uint8_t KEY_PORT_SOURCE[KEYn]        = {KEY0_EXTI_PORT_SOURCE,   KEY1_EXTI_PORT_SOURCE,  KEY2_EXTI_PORT_SOURCE,  KEY3_EXTI_PORT_SOURCE,};
static uint8_t KEY_PIN_SOURCE[KEYn]         = {KEY0_EXTI_PIN_SOURCE,    KEY1_EXTI_PIN_SOURCE,   KEY2_EXTI_PIN_SOURCE,   KEY3_EXTI_PIN_SOURCE, };
static uint8_t KEY_IRQn[KEYn]               = {KEY0_EXTI_IRQn,          KEY1_EXTI_IRQn,         KEY2_EXTI_IRQn,         KEY3_EXTI_IRQn,       };
/*!
    \brief      configure led GPIO
    \param[in]  lednum: specify the led to be configured
      \arg        LEDx(x = 0, 1...)
    \param[out] none
    \retval     none
*/
void  gd_led_init (led_typedef_enum lednum)
{
    /* enable the led clock */
    rcu_periph_clock_enable(GPIO_CLK[lednum]);
    /* configure led GPIO port */ 
    gpio_init(GPIO_PORT[lednum], GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN[lednum]);

    GPIO_BC(GPIO_PORT[lednum]) = GPIO_PIN[lednum];
}

/*!
    \brief      turn on selected led
    \param[in]  lednum: specify the led to be turned on
      \arg        LEDx(x = 0, 1...)
    \param[out] none
    \retval     none
*/
void gd_led_on(led_typedef_enum lednum)
{
    GPIO_BOP(GPIO_PORT[lednum]) = GPIO_PIN[lednum];
}

/*!
    \brief      turn off selected led
    \param[in]  lednum: specify the led to be turned off
      \arg        LEDx(x = 0, 1...)
    \param[out] none
    \retval     none
*/
void gd_led_off(led_typedef_enum lednum)
{
    GPIO_BC(GPIO_PORT[lednum]) = GPIO_PIN[lednum];
}

/*!
    \brief      toggle selected led
    \param[in]  lednum: specify the led to be toggled
      \arg        LEDx(x = 0, 1...)
    \param[out] none
    \retval     none
*/
void gd_led_toggle(led_typedef_enum lednum)
{
    // todo: 改成异或？
    gpio_bit_write(GPIO_PORT[lednum], GPIO_PIN[lednum], 
                    (bit_status)(1-gpio_input_bit_get(GPIO_PORT[lednum], GPIO_PIN[lednum])));
}

/*!
    \brief      configure key
    \param[in]  key_num: specify the key to be configured
      \arg        KEYx(x = 0,1 ...)
    \param[in]  key_mode: specify button mode
      \arg        KEY_MODE_GPIO: key will be used as simple IO
      \arg        KEY_MODE_EXTI: key will be connected to EXTI line with interrupt
    \param[out] none
    \retval     none
*/
void gd_key_init(key_typedef_enum key_num, keymode_typedef_enum key_mode)
{
    /* enable the key clock */
    rcu_periph_clock_enable(KEY_CLK[key_num]);
    rcu_periph_clock_enable(RCU_AF);

    /* configure button pin as input */
    if(key_num == KEY0){
        gpio_init(KEY_PORT[key_num], GPIO_MODE_IPD, GPIO_OSPEED_50MHZ, KEY_PIN[key_num]);
    }else if(key_num == KEY3){
        gpio_init(KEY_PORT[key_num], GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, KEY_PIN[key_num]);
    }else{gpio_init(KEY_PORT[key_num], GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, KEY_PIN[key_num]);}

    if (key_mode == KEY_MODE_EXTI) {
        /* enable and set key EXTI interrupt to the lowest priority */
        nvic_irq_enable(KEY_IRQn[key_num], 4U, 0U);

        /* connect key EXTI line to key GPIO pin */
        gpio_exti_source_select(KEY_PORT_SOURCE[key_num], KEY_PIN_SOURCE[key_num]);

        /* configure key EXTI line */
        exti_init(KEY_EXTI_LINE[key_num], EXTI_INTERRUPT, EXTI_TRIG_BOTH);
        exti_interrupt_flag_clear(KEY_EXTI_LINE[key_num]);
    }
}

/*!
    \brief      return the selected key state
    \param[in]  key: specify the key to be checked
      \arg        KEYx(x = 0,1 ...)
    \param[out] none
    \retval     the key's GPIO pin value
*/
uint8_t gd_key_state_get(key_typedef_enum key)
{
    return gpio_input_bit_get(KEY_PORT[key], KEY_PIN[key]);
}
