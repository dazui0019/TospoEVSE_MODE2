#include "drv_spi.h"

GD_StatusTypeDef SPI_Transmit(uint32_t spi_periph, uint8_t *pData, uint16_t Size)
{
    GD_StatusTypeDef errorcode = GD_OK;
    uint16_t TxXferCount, TxXferSize;
    uint8_t* pTxBuffPtr;

    if ((pData == NULL) || (Size == 0U))
    {
        errorcode = GD_ERROR;
        goto error;
    }

    TxXferCount = Size;
    TxXferSize = Size;
    pTxBuffPtr = (uint8_t*)pData;

    while (TxXferCount > 0U){
        if(spi_i2s_flag_get(spi_periph, SPI_FLAG_TBE)){
            SPI_DATA(spi_periph) = *((uint32_t*)pTxBuffPtr);
            TxXferCount--;
            pTxBuffPtr++;
        }
    }

error :
    return errorcode;
}