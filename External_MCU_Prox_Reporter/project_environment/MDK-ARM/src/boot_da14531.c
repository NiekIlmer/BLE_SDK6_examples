/***********************************************************************************************************************
 * File Name    : boot_da14531.c
 * Description  : Contains UART functions definition.
 **********************************************************************************************************************/
/***********************************************************************************************************************
 * DISCLAIMER
 * This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
 * other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
 * applicable laws, including copyright laws.
 * THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
 * THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
 * EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
 * SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
 * SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
 * this software. By using this software, you agree to the additional terms and conditions found by accessing the
 * following link:
 * http://www.renesas.com/disclaimer
 *
 * Copyright (C) 2021 Renesas Electronics Corporation. All rights reserved.
 ***********************************************************************************************************************/

#include "common_utils.h"
#include "boot_da14531.h"
#include "r_sci_uart.h"
#include "r_uart_api.h"
#include "prox_reporter_image.h" //This image is just used as an example, any other image for the DA14531 can be used

#define ACK 		(uint8_t)(0x06)
#define NACK		(uint8_t)(0x15)
#define STX			(uint8_t)(0x02)
#define SOH			(uint8_t)(0x01)

enum uart_boot
{
    DA14531_BOOT_SEND_LENGTH ,  
    DA14531_BOOT_SEND_DATA,
    DA14531_BOOT_CHECK_CRC       
};

enum uart_boot uart_state;

/*******************************************************************************************************************//**
 * @addtogroup boot_da14531
 * @{
 **********************************************************************************************************************/

/*
 * Private function declarations
 */
/*
 * Private global variables
 */
/* Flag for user callback */
volatile uint8_t g_uart_event = RESET_VALUE;

bsp_io_level_t BSP_IO_PORT_02_PIN_07_status ;

/* Flag to check whether data is received or not */
static volatile uint8_t g_data_received_flag = false;
#define HEADER_SIZE		(size_t)(3)

extern const unsigned char CODELESS_CRC ;

/*****************************************************************************************************************
 *  @brief       DA14531 Boot project to demonstrate the functionality
 *  @param[in]   None
 *  @retval      FSP_SUCCESS     Upon success
 *  @retval      Any Other Error code apart from FSP_SUCCESS
 ****************************************************************************************************************/
fsp_err_t boot_da14531_demo(void)
{

    while (true)
    {
        if (g_data_received_flag)
        {
            g_data_received_flag = false;
        }
    }
}


/*******************************************************************************************************************//**
 * @brief       Initialize  UART.
 * @param[in]   None
 * @retval      FSP_SUCCESS         Upon successful open and start of timer
 * @retval      Any Other Error code apart from FSP_SUCCESS  Unsuccessful open
 ***********************************************************************************************************************/
fsp_err_t uart_initialize(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Initialize UART channel with baud rate 115200 */
    err = R_SCI_UART_Open (&g_uart0_ctrl, &g_uart0_cfg);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n**  R_SCI_UART_Open API failed  **\r\n");
    }
    return err;
}

/*******************************************************************************************************************//**
 *  @brief       Deinitialize SCI UART module
 *  @param[in]   None
 *  @retval      None
 **********************************************************************************************************************/
void deinit_uart(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Close module */
    err = R_SCI_UART_Close (&g_uart0_ctrl);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n**  R_SCI_UART_Close API failed  ** \r\n");
    }
}

/*****************************************************************************************************************
 *  @brief      UART user callback
 *  @param[in]  p_args
 *  @retval     None
 ****************************************************************************************************************/

void user_uart_callback(uart_callback_args_t * p_args)
{
    fsp_err_t uartStatus = FSP_SUCCESS;
    uint8_t header[HEADER_SIZE ];


#if defined (ONE_WIRE)
    uint8_t buffer;
    uint8_t clearBuffer;
#endif	

    uint8_t crc_buffer;

    if (UART_EVENT_RX_CHAR == p_args->event)
    {
        g_data_received_flag = true;

        if (STX == p_args->data && 0 == DA14531_BOOT_SEND_LENGTH)
        {

            //Prepare header
            header[0] = SOH;
            header[1] = (uint8_t) (sizeof(PROXREPORTER) & 0xFF); //Get lower byte of the size
            header[2] = (uint8_t) ((sizeof(PROXREPORTER) >> 8) & 0xFF); //Get upper byte of the size

            uartStatus = R_SCI_UART_Write (&g_uart0_ctrl, &header[0], HEADER_SIZE);
            assert(FSP_SUCCESS == uartStatus);

#if defined (ONE_WIRE)

            {
                //Because of 1 wire UART the buffer needs to be cleared after a transmit
                uartStatus = R_SCI_UART_Read (&g_uart0_ctrl, &buffer, sizeof(buffer));
                assert(FSP_SUCCESS == uartStatus);
            }
#endif

            APP_PRINT("\r\n**  Done: sends the header for the data to the DA14531 and waits for the ACK  ** \r\n");
            uart_state = DA14531_BOOT_SEND_DATA;

        }

        else if (DA14531_BOOT_SEND_DATA == uart_state && ACK == p_args->data)
        {
            uartStatus = R_SCI_UART_Write (&g_uart0_ctrl, PROXREPORTER, sizeof(PROXREPORTER));
            assert(FSP_SUCCESS == uartStatus);

#if defined (ONE_WIRE)

            {
                //Because of 1 wire UART the buffer needs to be cleared after a transmit
                uartStatus = R_SCI_UART_Read (&g_uart0_ctrl, &clearBuffer, sizeof(clearBuffer));
                assert(FSP_SUCCESS == uartStatus);
            }
#endif

            APP_PRINT("\r\n**  Done: sends the data to the DA14531  ** \r\n");
            uart_state = DA14531_BOOT_CHECK_CRC;
        }

        else if (DA14531_BOOT_CHECK_CRC == uart_state && PROX_CRC == p_args->data)

        {
            crc_buffer = ACK;
            uartStatus = R_SCI_UART_Write (&g_uart0_ctrl, &crc_buffer, sizeof(crc_buffer));
            assert(FSP_SUCCESS == uartStatus);

#if defined (ONE_WIRE)

            {
                //Because of 1 wire UART the buffer needs to be cleared after a transmit
                uartStatus = R_SCI_UART_Read (&g_uart0_ctrl, &crc_buffer, sizeof(crc_buffer));
                assert(FSP_SUCCESS == uartStatus);
            }
#endif

            APP_PRINT("\r\n**  Done: gets the CRC from the DA14531 and checks if it matches the given CRC  ** \r\n");
        }

    }

    if (UART_EVENT_RX_COMPLETE == p_args->event)
    {
        g_uart_event = UART_EVENT_RX_COMPLETE;
    }

    if (UART_EVENT_TX_COMPLETE == p_args->event)
    {
        /* Toggle GPIO when the Transmission is done */
        R_BSP_PinAccessEnable ();
        R_BSP_PinWrite (BSP_IO_PORT_09_PIN_14, 1U);
        R_BSP_PinAccessDisable ();
        R_BSP_PinAccessEnable ();
        R_BSP_PinWrite (BSP_IO_PORT_09_PIN_14, 0U);
        R_BSP_PinAccessDisable ();
        g_uart_event = UART_EVENT_TX_COMPLETE;
    }

}

/*******************************************************************************************************************//**
 * @} (end addtogroup boot_da14531)
 **********************************************************************************************************************/
