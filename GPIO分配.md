# GPIO分配以及功能实现逻辑

## ADC

1. NTC1_ADC(`PA1`): 插头的NTC
2. PLUG_ADC(`PA2`): 枪头的NTC, 目前没接
3. CURRENT_ADC(`PA3`): 电流
4. VOL_IN_ADC(`PA4`): 火线输入电压
5. VOL_OUT_ADC(`PA5`): 火线输出电压
6. VON_OUT_ADC(`PA6`): 零线输出电压, 粘连检测
7. GNDING_ADC(`PA7`): 接地检测
8. NTC0_ADC(`PB0`): 板载温度检测
9. CP_ADC(`PB1`): CP检测

### 电压检测

1. FREQ_IN(`PB6`): 用来指示交流电的零点(上升沿表示一个周期的开始)
2. VOL_IN_ADC(`PA4`): 火线输入电压
3. VOL_OUT_ADC(`PA5`): 火线输出电压
4. VON_OUT_ADC(`PA6`): 零线输出电压, 粘连检测

由于需要采样的电压是交流信号, 所以需要对信号进行周期采样。在`FREQ_IN`的上升沿时, 开启采样; 在下一个`FREQ_IN`上升沿到来时(表示一个周期结束, 下一个周期开始)停止ADC采样。
并且需要对一个周期进行多次采样, 然后计算平均值。所以实际的ADC采样是由定时器来触发, 在`FREQ_IN`的上升沿到来时, 首先开启定时器, 定时器以1ms的频率来触发ADC采样(可能还需要一个DMA), 在下一个`FREQ_IN`的上升沿到来时，关闭定时器处理采样数据。

### 温度检测

1. NTC0_ADC(`PB0`): 板载温度检测
2. NTC1_ADC(`PA1`): 插头的NTC

这个感觉放到ADC2里面用软件触发就行。

## CP 输出

CP_PWM(`PB5(TIMER2_CH1)`): 可以考虑用`TIMER2_TRGO`来触发CP的ADC采样(常规序列)。
接地检测和CP检测对实时性要求较高, 考虑将这两个放到同一个序列里采样。

## RCD

1. RCD_TEST(`PB14`): 未知作用
2. RCD_TRIP(`PB13`): 未知作用
3. RCD_ZERO(`PB12`): 未知作用
4. RCD_RMS(`PB2`):  未知作用
