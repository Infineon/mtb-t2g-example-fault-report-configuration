/**********************************************************************************************************************
 * \file main.c
 * \copyright Copyright (C) Infineon Technologies AG 2019
 *
 * Use of this file is subject to the terms of use agreed between (i) you or the company in which ordinary course of
 * business you are acting and (ii) Infineon Technologies AG or its licensees. If and as long as no such terms of use
 * are agreed, use of this file is subject to following:
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and
 * accompanying documentation covered by this license (the "Software") to use, reproduce, display, distribute, execute,
 * and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the
 * Software is furnished to do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including the above license grant, this restriction
 * and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all
 * derivative works of the Software, unless such copies or derivative works are solely in the form of
 * machine-executable object code generated by a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *********************************************************************************************************************/
/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "cybsp.h"
#include "cy_pdl.h"
#include "cy_retarget_io.h"
#include "mtb_hal.h"

/*********************************************************************************************************************/
/*-------------------------------------------------Data Structures---------------------------------------------------*/
/*********************************************************************************************************************/
/* Data type for 128bit variable */
typedef struct
{
    uint64_t u64[2];
} Uint128Type;

/* Number of error injection bits */
typedef enum
{
    INJECT_NONE = 0,
    INJECT_1BIT,
    INJECT_2BIT,
} InjectType;

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/* Target SRAM address */
#define RAM_0_ADDRESS                  ((uint32_t)&g_test)

/* Offset of target SRAM from top of SRAM0 */
#define RAM_0_OFFSET                   ((uint32_t)&g_test - CY_SRAM_BASE)

/* Value to be used on error injection */
#define RAM_0_CORRECT_DATA             (0x5A5A5A5A5A5A5A5Aull)
#define RAM_0_ONEBIT_DATA              (RAM_0_CORRECT_DATA ^ 1)
#define RAM_0_TWOBIT_DATA              (RAM_0_CORRECT_DATA ^ 3)

/* Parity to be used on error injection */
#define RAM_0_CORRECT_PARITY(p)        (generate_Parity(make_Word_For_Sram_Ecc(p)))
#define RAM_0_ONEBIT_PARITY(p)         (RAM_0_CORRECT_PARITY(p) ^ 1)
#define RAM_0_TWOBIT_PARITY(p)         (RAM_0_CORRECT_PARITY(p) ^ 3)

#define LED_BLINK_INTERVAL_MS          (500)
#define SLEEP_INTERVAL_MS              (50)

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* Variable for using as the testing target address */
uint64_t g_test;

/* Fault configuration for interrupt */
const cy_stc_SysFault_t FAULT_CFG_INTR =
{
    .ResetEnable   = false,
    .OutputEnable  = true,
    .TriggerEnable = true,
};

/* Fault configuration for reset*/
const cy_stc_SysFault_t FAULT_CFG_RESET =
{
    .ResetEnable   = true,
    .OutputEnable  = true,
    .TriggerEnable = true,
};

/* Configure Interrupt for Fault structure */
const cy_stc_sysint_t IRQ_CFG =
{
    .intrSrc = ((NvicMux3_IRQn << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) | cpuss_interrupts_fault_0_IRQn),
    .intrPriority = 2UL
};

/* For the Retarget -IO (Debug UART) usage */
static cy_stc_scb_uart_context_t    UART_context;           /** UART context */
static mtb_hal_uart_t               UART_hal_obj;           /** Debug UART HAL object  */

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
/**********************************************************************************************************************
 * Function Name: handle_Fault_IRQ
 * Summary:
 *  Fault interrupt handler function. Display the information of the fault on terminal and blinking LED.
 * Parameters:
 *  none
 * Return:
 *  none
 **********************************************************************************************************************
 */
void handle_Fault_IRQ(void)
{
    uint32_t faultAddress, faultInfo;

    /* Get Fault data */
    faultAddress = Cy_SysFault_GetFaultData(FAULT_STRUCT0, CY_SYSFAULT_DATA0);
    faultInfo = Cy_SysFault_GetFaultData(FAULT_STRUCT0, CY_SYSFAULT_DATA1);
    cy_en_SysFault_source_t errorSource = Cy_SysFault_GetErrorSource(FAULT_STRUCT0);

    /* Display the fault information */
    if(errorSource == CY_SYSFAULT_RAMC0_C_ECC)
    {
        printf("CY_SYSFAULT_RAMC0_C_ECC fault detected in structure 0:\r\n");
        printf("Address:     0x%08lx\r\n", faultAddress);
        printf("Information: 0x%08lx\r\n\n", faultInfo);
    }
    else if (errorSource == CY_SYSFAULT_NO_FAULT)
    {
        printf("CY_SYSFAULT_NO_FAULT fault detected in structure 0:\r\n");
    }
    else
    {
        printf("Fault detected\r\n");
        printf("GetErrorSource: 0x%08lx\r\n", (uint32_t)errorSource);
    }

    /* Blink LED 3 times */
    Cy_GPIO_Inv(CYBSP_LED1_PORT, CYBSP_LED1_PIN);
    Cy_SysLib_Delay(LED_BLINK_INTERVAL_MS);      
    Cy_GPIO_Inv(CYBSP_LED1_PORT, CYBSP_LED1_PIN);
    Cy_SysLib_Delay(LED_BLINK_INTERVAL_MS);      
    Cy_GPIO_Inv(CYBSP_LED1_PORT, CYBSP_LED1_PIN);
    Cy_SysLib_Delay(LED_BLINK_INTERVAL_MS);      
    Cy_GPIO_Inv(CYBSP_LED1_PORT, CYBSP_LED1_PIN);
    Cy_SysLib_Delay(LED_BLINK_INTERVAL_MS);      
    Cy_GPIO_Inv(CYBSP_LED1_PORT, CYBSP_LED1_PIN);
    Cy_SysLib_Delay(LED_BLINK_INTERVAL_MS);      
    Cy_GPIO_Inv(CYBSP_LED1_PORT, CYBSP_LED1_PIN);

    /* Clear Interrupt flag */
    Cy_SysFault_ClearInterrupt(FAULT_STRUCT0);

    /* Disable ECC error Injection */
    CPUSS->ECC_CTL = 0;
    CPUSS->RAM0_CTL0 &= ~(1 << CPUSS_RAM0_CTL0_ECC_INJ_EN_Pos);
}

/**********************************************************************************************************************
 * Function Name: init_Fault
 * Summary:
 *  Intialize RAM0_ECC faults.
 * Parameters:
 *  config - fault control configuration
 * Return:
 *  none
 **********************************************************************************************************************
 */
void init_Fault(cy_stc_SysFault_t *config)
{
    /* clear status */
    Cy_SysFault_ClearStatus(FAULT_STRUCT0);
    Cy_SysFault_SetMaskByIdx(FAULT_STRUCT0, CY_SYSFAULT_RAMC0_C_ECC);
    Cy_SysFault_SetMaskByIdx(FAULT_STRUCT0, CY_SYSFAULT_RAMC0_NC_ECC);
    Cy_SysFault_SetInterruptMask(FAULT_STRUCT0);
    if (Cy_SysFault_Init(FAULT_STRUCT0, config) != CY_SYSFAULT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* set up the interrupt */
    Cy_SysInt_Init(&IRQ_CFG, &handle_Fault_IRQ);
    NVIC_SetPriority((IRQn_Type) NvicMux3_IRQn, 2UL);
    NVIC_EnableIRQ((IRQn_Type) NvicMux3_IRQn);
}

/**********************************************************************************************************************
 * Function Name: make_Word_For_Sram_Ecc
 * Summary:
 *  Genrate code word used for parity calculation
 * Parameters:
 *  addr - target address for ECC error injection
 * Return:
 *  Uint128Type - code word
 **********************************************************************************************************************
 */
static Uint128Type make_Word_For_Sram_Ecc(uint64_t *addr)
{
    /* CODEWORD_SW [63:0] = ACTUALWORD [63:0]; */
    /* Other bits = 0 */
    Uint128Type word = {{*addr, 0ul}};

    /* CODEWORD_SW [ADDR_WIDTH+60:64] = ADDR [ADDR_WIDTH-1:3]; */
    word.u64[1] = (uint64_t)(((uint32_t)addr - CY_SRAM_BASE) & (CPUSS_SRAM0_SIZE * 1024 - 8)) >> 3;

    return word;
}

/**********************************************************************************************************************
 * Function Name: do_AND_128bit
 * Summary:
 *  Do logical AND operation on two 128bit values
 * Parameters:
 *  a - 1st value
 *  b - 2nd value
 * Return:
 *  Uint128Type - code word
 **********************************************************************************************************************
 */
static Uint128Type do_AND_128bit(Uint128Type a, Uint128Type b)
{
    Uint128Type result;
    result.u64[0] = a.u64[0] & b.u64[0];
    result.u64[1] = a.u64[1] & b.u64[1];
    return result;
}

/**********************************************************************************************************************
 * Function Name: do_Reduction_XOR_128bit
 * Summary:
 *  Do reduction XOR operation on specified 128bit value and get the parity
 * Parameters:
 *  data - target value
 * Return:
 *  uint8_t - parity
 **********************************************************************************************************************
 */
static uint8_t do_Reduction_XOR_128bit(Uint128Type data)
{
    uint64_t parity = 0;
    uint64_t bit    = 0;
    for(uint64_t iPos = 0; iPos < 64; iPos++)
    {
        bit = (data.u64[0] & (1ull << iPos)) >> iPos;
        parity ^= bit;
    }

    for(uint64_t iPos = 0; iPos < 64; iPos++)
    {
        bit = (data.u64[1] & (1ull << iPos)) >> iPos;
        parity ^= bit;
    }

    return (uint8_t)parity;
}

/**********************************************************************************************************************
 * Function Name: generate_Parity
 * Summary:
 *  calculate parity for specified 128bit value
 * Parameters:
 *  word - target value
 * Return:
 *  uint8_t - parity
 **********************************************************************************************************************
 */
static uint8_t generate_Parity(Uint128Type word)
{
    static const Uint128Type ECC_P[8] =
    {
        {{0x44844a88952aad5bull, 0x01bfbb75be3a72dcull}},
        {{0x1108931126b3366dull, 0x02df76f9dd99b971ull}},
        {{0x06111c2238c3c78eull, 0x04efcf9f9ad5ce97ull}},
        {{0x9821e043c0fc07f0ull, 0x08f7ecf6ed674e6cull}},
        {{0xe03e007c00fff800ull, 0x10fb7baf6ba6b5a6ull}},
        {{0xffc0007fff000000ull, 0x20fdb7cef36cab5bull}},
        {{0xffffff8000000000ull, 0x40fedd7b74db55abull}},
        {{0xd44225844ba65cb7ull, 0x807f000007ffffffull}},
    };

    uint8_t ecc = 0;
    for (uint32_t cnt = 0; cnt < (sizeof(ECC_P) / sizeof(ECC_P[0])); cnt++)
    {
        ecc |= (do_Reduction_XOR_128bit(do_AND_128bit(word, ECC_P[cnt])) << cnt);
    }

    return ecc;
}

/**********************************************************************************************************************
 * Function Name: inject_ECC_SRAM0_ECC_error
 * Summary:
 *  inject ECC error
 * Parameters:
 *  ramAddr - target RAM address (should be point to SRAM0 region)
 *  injectVal - num of error bits to be injected for value
 *  injectParity - num of error bits to be injected for parity
 * Note:
 *  Errors with a total of 3 bits or more cannot be set because they cannot be detected correctly
 * Return:
 *  none
 **********************************************************************************************************************
 */
void inject_ECC_SRAM0_ECC_error(uint64_t *ramAddr, InjectType injectVal, InjectType injectParity)
{
    uint8_t  parityCorrect, parity1bit, parity2bit;
    uint8_t  *parity;
    volatile static uint64_t ramVal;

    /* first, set correct data to calculate specified parity */
    *ramAddr = RAM_0_CORRECT_DATA;
    parityCorrect = RAM_0_CORRECT_PARITY(ramAddr);
    parity1bit = RAM_0_ONEBIT_PARITY(ramAddr);
    parity2bit = RAM_0_TWOBIT_PARITY(ramAddr);

    /* calculate specified parity */
    /* correct parity */
    if (injectParity == INJECT_NONE)
    {
        parity = &parityCorrect;
    }
    /* 1bit inverted parity */
    else if (injectParity == INJECT_1BIT)
    {
        /* 2bit inverted value is specified */
        if (injectVal == INJECT_2BIT)
        {
            /* ECC will not work well when more than 2bit are inverted */
            CY_ASSERT(0);
        }
        parity = &parity1bit;
    }
    /* 2bit inverted parity */
    else
    {
        /* incorrect value is specified */
        if (injectVal != INJECT_NONE)
        {
            /* ECC will not work well when more than 2bit are inverted */
            CY_ASSERT(0);
        }
        parity = &parity2bit;
    }

    /* set specified value to target RAM */
    /* correct value */
    if (injectVal == INJECT_NONE)
    {
        *ramAddr = RAM_0_CORRECT_DATA;
    }
    /* 1bit inverted value */
    else if (injectVal == INJECT_1BIT)
    {
        *ramAddr = RAM_0_ONEBIT_DATA;
    }
    /* 2bit inverted value */
    else
    {
        *ramAddr = RAM_0_TWOBIT_DATA;
    }
    /* keep the specified value */
    ramVal = *ramAddr;

    /* Specifying RAM address for ECC injection (Bits [23:0]) */
    CPUSS->ECC_CTL = (CPUSS_ECC_CTL_WORD_ADDR_Msk & (RAM_0_OFFSET >> 3));

    /* Specifying Parity word for RAM address (Bits [7:0])*/
    CPUSS->ECC_CTL |= ((uint32_t)*parity << CPUSS_ECC_CTL_PARITY_Pos);

    /* Enable RAM0 CTL0 ECC Injection*/
    CPUSS->RAM0_CTL0 |= ((1 << CPUSS_RAM0_CTL0_ECC_INJ_EN_Pos) |
            (1 << CPUSS_RAM0_CTL0_ECC_EN_Pos) | (1 << CPUSS_RAM0_CTL0_ECC_AUTO_CORRECT_Pos));

    /* SRAM 0 data write to set specified parity */
    *ramAddr = ramVal;
    
    Cy_SysLib_Delay(SLEEP_INTERVAL_MS);

    /* Reads SRAM so that checks are performed with the set parity */
    ramVal = *ramAddr;
}

/**********************************************************************************************************************
 * Function Name: generate_ECC_SRAM0_correctable_error_by_parity
 * Summary:
 *  inject ECC 1bit parity error
 * Parameters:
 *  none
 * Return:
 *  none
 **********************************************************************************************************************
 */
void generate_ECC_SRAM0_correctable_error_by_parity(void)
{
    inject_ECC_SRAM0_ECC_error(&g_test, INJECT_NONE, INJECT_1BIT);
}

/**********************************************************************************************************************
 * Function Name: main
 * Summary:
 *  This is the main function.
 * Parameters:
 *  none
 * Return:
 *  int
 **********************************************************************************************************************
 */
int main(void)
{
    cy_rslt_t result;

    /* Variable for storing character read from terminal */
    uint8_t uartReadValue;

    /* Board init */
    if (cybsp_init() != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Disabled I/D-cache */
    SCB_DisableICache();
    SCB_DisableDCache();

    /* Debug UART init */
    result = (cy_rslt_t)Cy_SCB_UART_Init(UART_HW, &UART_config, &UART_context);

    /* UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_SCB_UART_Enable(UART_HW);

    /* Setup the HAL UART */
    result = mtb_hal_uart_setup(&UART_hal_obj, &UART_hal_config, &UART_context, NULL);

    /* HAL UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    result = cy_retarget_io_init(&UART_hal_obj);

    /* HAL retarget_io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("****************** "
           "Fault report configuration example "
           "****************** \r\n\n");

    /* Check if CY_SYSFAULT_RAMC0_C_ECC reset was caused by FAULT_STRUCT0 */
    if(FAULT_STRUCT0->STATUS == CY_SYSFAULT_RAMC0_C_ECC)
    {
        printf("Reset caused by FAULT_STRUCT0.\r\n");
        printf("Detected fault: CY_SYSFAULT_RAMC0_C_ECC\r\n");
        printf("Address:     0x%08lx\r\n", Cy_SysFault_GetFaultData(FAULT_STRUCT0, CY_SYSFAULT_DATA0));
        printf("Information: 0x%08lx\r\n\n", Cy_SysFault_GetFaultData(FAULT_STRUCT0, CY_SYSFAULT_DATA1));
    }

    /* Check for SRAM0 area validation */
    if (RAM_0_OFFSET >= CPUSS_SRAM0_SIZE * 1024)
    {
        printf("SRAM for test is not located within SRAM0 region...\r\n");
        CY_ASSERT(0);
    }

    printf("Press 'i' to generate a SRAM0 correctable ECC error interrupt\r\n");
    printf("Press 'r' to generate a SRAM0 correctable ECC error reset\r\n\n");
    for (;;)
    {
        /* Check if 'i' key or 'r' key was pressed */
        uartReadValue = Cy_SCB_UART_Get(UART_HW);
               
        if(uartReadValue != 0xFF)
        {
            if (uartReadValue == 'i')
            {
                printf("Fault interrupt requested\r\n");

                /* Enable interrupt for fault handling */
                init_Fault((cy_stc_SysFault_t *)&FAULT_CFG_INTR);

                /* generate an SRAM0 correctable ECC error */
                generate_ECC_SRAM0_correctable_error_by_parity();
            }
            if (uartReadValue == 'r')
            {
                printf("Fault reset requested\r\n");

                /* Enable reset for Fault handling */
                init_Fault((cy_stc_SysFault_t *)&FAULT_CFG_RESET);

                /* generate an SRAM0 correctable ECC error */
                generate_ECC_SRAM0_correctable_error_by_parity();
            }
        }
        
        Cy_SysLib_Delay(SLEEP_INTERVAL_MS);
    }
}

/* [] END OF FILE */
