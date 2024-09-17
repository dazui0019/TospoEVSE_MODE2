#include "basic_os.h"

void bos_port_assert(uint32_t error_id)
{
    while (1)
    {
        bos_delay_ms(10);
    }
}

uint32_t count_idle = 0;
void bos_hook_idle(void)
{
    count_idle ++;
}

void bos_hook_start(void)
{

}

/* ----------------------------- end of file -------------------------------- */
