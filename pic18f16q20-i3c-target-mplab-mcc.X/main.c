 /*
 * MAIN Generated Driver File
 * 
 * @file main.c
 * 
 * @defgroup main MAIN
 * 
 * @brief This is the generated driver implementation file for the MAIN driver.
 *
 * @version MAIN Driver Version 1.0.0
*/

/*
© [2024] Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip 
    software and any derivatives exclusively with Microchip products. 
    You are responsible for complying with 3rd party license terms  
    applicable to your use of 3rd party software (including open source  
    software) that may accompany Microchip software. SOFTWARE IS ?AS IS.? 
    NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS 
    SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT,  
    MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT 
    WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY 
    KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF 
    MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE 
    FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP?S 
    TOTAL LIABILITY ON ALL CLAIMS RELATED TO THE SOFTWARE WILL NOT 
    EXCEED AMOUNT OF FEES, IF ANY, YOU PAID DIRECTLY TO MICROCHIP FOR 
    THIS SOFTWARE.
*/
#include "mcc_generated_files/system/system.h"
#include <string.h>
/*
    Main application
*/

#define I3C_USER_BUFFER_SIZE (50U)

uint8_t receivedData[I3C_USER_BUFFER_SIZE];
uint8_t IBIData[] = {"q20"};
uint8_t LEDOn[6] = {"LED On"};
uint8_t LEDOff[7] = {"LED Off"};

volatile enum I3C_CCC receivedsupportedCCC;
volatile bool supportedCCCreceived = false;

volatile bool button = false;
volatile uint16_t numberOfBytesReceived = 0;
volatile bool isDataReceived = false;
volatile bool isDataSent = false;
volatile bool isIBICompleted = false;
uint8_t mandatoryByte = 0xAA;;

enum I3C_TARGET_BUFFER_RECEIVE_ERROR receiveError;
enum I3C_TARGET_IBI_REQUEST_ERROR ibiError;

enum states
{
    INIT,
    WAIT_FOR_ENTDAA,
    WAIT_FOR_CCC,
    LAST_CCC_RECEIVED,
    SETUP_BUFFER,
    WAIT_FOR_DATA,        
    RECEIVE_PRIVATE_WRITE,
    WAIT_TO_SEND_IBI,
    SEND_IBI
};

enum states state = INIT;

void debounce(void);
void terminalLine(void);
void I3C1_OpModeDetails(void);
void printReceivedData(void);
void TransactionCompleteCallback(struct I3C_TARGET_TRANSACTION_COMPLETE_STATUS *transactionCompleteStatus);
void IBIDoneCallback(void);
void SupportedCCCReceivedCallback(void);


int main(void)
{
    SYSTEM_Initialize();
    
    /* Register callback for button press */
    SW0_SetInterruptHandler(&debounce);
    
    /* Register user callback for Supported CCC */
    I3C1_SupportedCCCReceivedCallbackRegister(SupportedCCCReceivedCallback);
    
    /* Register user call back for Completed Transaction */
    I3C1_TransactionCompleteCallbackRegister(TransactionCompleteCallback);
    
    /* For In-Band Interrupts */
    I3C1_IBIMandatoryDataByteSet(mandatoryByte);
    I3C1_IBIPayloadSizeSet(sizeof(IBIData));
    
    INTERRUPT_GlobalInterruptEnable();
    
    __delay_ms(1000);
    
    printf("PIC18Q20 as an I3C Target Device Demonstration\r\n");
    printf("--------------------------------------------------------------\r\n");
    printf("\nBefore any transactions occur... \r\n");
    
    __delay_ms(1000);

    while(1)
    {      
        switch(state) 
        {
            case INIT:
                I3C1_OpModeDetails();
                printf("and waiting to be given a dynamic address on the bus...\r\n");
                terminalLine();
                state = WAIT_FOR_ENTDAA;
                break;
                
            case WAIT_FOR_ENTDAA:
                while(I3C1_OperatingModeGet() != I3C_TARGET_OPERATING_MODE_I3C_SDR);
                state = LAST_CCC_RECEIVED;
                break;
                
            case WAIT_FOR_CCC:
                if(supportedCCCreceived) {
                    state = LAST_CCC_RECEIVED;
                }
                break;
           
            case LAST_CCC_RECEIVED:
                supportedCCCreceived = false;
                if(receivedsupportedCCC == I3C_CCC_ENTDAA_B) {   
                    printf("\r\nBroadcast CCC - Enter Dynamic Address Assignment (ENTDAA)\r\n");
                    printf("The Q20 Target has been given the dynamic address: 7'h%x.\r\n", I3C1DADR);
                    state = WAIT_FOR_CCC;
                    I3C1_OpModeDetails();
                    terminalLine();
                }
                
                else if(receivedsupportedCCC == I3C_CCC_SETMWL_D) {                   
                    printf("\r\nDirect CCC - Set Maximum Write Length (SETMWL)\r\n");
                    printf("The Q20 Target now has a MWL of %d bytes\r\n", I3C1_MaxWriteLengthGet());
                    terminalLine();
                    state = SETUP_BUFFER;
                }
                
                else {
                    printf("error.");
                }
                break;
                
            case SETUP_BUFFER:
                /* Prepare the buffer for the Private Write Transactions */
                receiveError = I3C1_Target_BufferReceive(receivedData, sizeof(receivedData));
                I3C1_NextPrivateTransactionACK();
                state = WAIT_FOR_DATA;
                break;
                
            case WAIT_FOR_DATA:
                if(isDataReceived)
                {
                    isDataReceived = false;
                    state = RECEIVE_PRIVATE_WRITE;
                }
                break;
            
            case RECEIVE_PRIVATE_WRITE:
                if(memcmp(receivedData, LEDOn, numberOfBytesReceived) == 0) {
                    LED0_SetLow();
                    printReceivedData();
                    state = WAIT_FOR_DATA;
                }
                if(memcmp(receivedData, LEDOff, numberOfBytesReceived) == 0) {
                    LED0_SetHigh();
                    printReceivedData();
                    state = WAIT_TO_SEND_IBI;
                }               
                break;

            case WAIT_TO_SEND_IBI:
                if(button) {
                    state = SEND_IBI;
                    button = false;
                }
                break;
           
            case SEND_IBI:
                if(I3C1_IsIBIEnabledOnBus())
                {
                    /* Send IBI request */
                    printf("Sending IBI request\r\n");
                    ibiError = I3C1_IBIRequest(IBIData, sizeof(IBIData));

                    if(ibiError == I3C_TARGET_IBI_REQUEST_NOT_IN_I3C_MODE)
                    {
                        printf("I3C_IBI_NOT_IN_I3C_MODE\r\n");
                    }
                    else if(ibiError == I3C_TARGET_IBI_REQUEST_IBI_DISABLED_ON_BUS)
                    {
                        printf("I3C_IBI_IBI_DISABLED_ON_BUS\r\n");
                    }
                    else if(ibiError == I3C_TARGET_IBI_REQUEST_SEND_BUFFER_SIZE_EXCEEDED)
                    {
                        printf("I3C_IBI_SEND_BUFFER_SIZE_EXCEEDED\r\n");
                    }
                    else if(ibiError == I3C_TARGET_IBI_REQUEST_NO_ERROR)
                    {
                        printf("\r\nIn-Band Interrupt successfully sent to Controller\r\n");
                        printf("\r\nThe IBI Payload size is %d bytes\r\n", I3C1_IBIPayloadSizeGet());
                        terminalLine();
                        state = WAIT_TO_SEND_IBI;;
                    }
                }
                else
                {
                    printf("IBI is not enabled on the bus\r\n");
                }
                break;
                
            default:
                break;
        }
    }    
}

void debounce() {
    button = true;
}

void terminalLine() {
    printf("\r\n\n--------------------------------------------------------------\r\n\n");
}

void I3C1_OpModeDetails(void) {
    switch(I3C1_OperatingModeGet()) {
        case 0:
            printf("\r\nThe Q20 is a Target is operating in Legacy I2C mode\r\n");
            break;
        case 1:
            printf("\r\nThe Q20 Target is now operating in I3C mode.\r\n");
            break;
        case 2:
            printf("The Target is operating in Legacy I2C mode; The bus is operating in High Data Rate (HDR) mode\r\n");
            break;
        case 3:
            printf("The Target is operating in I3C mode; The bus is operating in High Data Rate (HDR) mode\r\n");
            break;
        default:
            break;
    }
}

void TransactionCompleteCallback(struct I3C_TARGET_TRANSACTION_COMPLETE_STATUS *transactionCompleteStatus)
{
    if(transactionCompleteStatus->dataFlowDirection == I3C_TARGET_DATA_RECEIVED)
    {
        isDataReceived = true;  
        numberOfBytesReceived = transactionCompleteStatus->numOfBytesReceived;
    }
    else if(transactionCompleteStatus->dataFlowDirection == I3C_TARGET_DATA_TRANSMITTED)
    {
        //isDataSent = true;
    }
}

void printReceivedData(void) {
    printf("\r\nThe Controller sent %u bytes\r\n", numberOfBytesReceived);
    printf("\r\nThe Q20 received the following message:\r\n\n");

    for(uint16_t i = 0; i < numberOfBytesReceived; i++)
    {
        printf("%c", receivedData[i]);
    }
    printf("\r\nin hex: ");

    for(uint16_t i = 0; i < numberOfBytesReceived; i++)
    {
        printf("0x%X ", receivedData[i]);
    }
    printf("\r\n");
    terminalLine();
}

void IBIDoneCallback(void)
{
    isIBICompleted = true;
}

void SupportedCCCReceivedCallback(void)
{
    supportedCCCreceived = true;
    receivedsupportedCCC = I3C1_LastCCCReceivedGet();
}

