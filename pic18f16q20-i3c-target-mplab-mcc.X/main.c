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
© [2023] Microchip Technology Inc. and its subsidiaries.

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


volatile bool button = false;
volatile uint16_t numberOfBytesReceived = 0;
volatile bool isDataReceived = false;
volatile bool isDataSent = false;
volatile bool isIBICompleted = false;

uint16_t MWLSize;
uint8_t mandatoryByte;

enum I3C_TARGET_BUFFER_RECEIVE_ERROR receiveError;
enum I3C_TARGET_BUFFER_TRANSMIT_ERROR sendError;
enum I3C_TARGET_IBI_REQUEST_ERROR ibiError;

enum states
{
    INIT,
    RECEIVE_DAA,
    WAIT_FOR_ENTDAA,
    WAIT_FOR_SETMWL,
    WAIT_FOR_DATA,        
    RECEIVE_DATA,
    WAIT_TO_SEND,
    SEND_SETUP,
    SEND_IBI,
    WAIT_IBI,
    END
};

enum states state = INIT;

void debounce(void);
void terminalLine(void);
void I3COpModeDetails(void);
void I3CTargetDetails(void);
void I3CReceiveData(void);
void BusErrorCallback(void);
void TransactionCompleteCallback(struct I3C_TARGET_TRANSACTION_COMPLETE_STATUS *transactionCompleteStatus);
void IBIDoneCallback(void);


int main(void)
{
    SYSTEM_Initialize();
    
    SW0_SetInterruptHandler(&debounce);
    
    I3C1_TransactionCompleteCallbackRegister(TransactionCompleteCallback);
    
    /* For In-Band Interrupt Test */
    mandatoryByte = 0xAA;
    I3C1_IBIMandatoryDataByteSet(mandatoryByte);
    I3C1_IBIPayloadSizeSet(sizeof(IBIData));
    INTERRUPT_GlobalInterruptEnable();
    
    __delay_ms(1000);
    
    printf("PIC18Q20 as an I3C Target Device Demonstration\r\n");
    printf("---------------------------------------------------------\r\n");
    printf("\nBefore any transactions occur... \r\n");
    
    __delay_ms(1000);

    while(1)
    {      
        switch(state) 
        {
            case INIT:
                I3COpModeDetails();
                isDataReceived = false;
                numberOfBytesReceived = 0;  
                printf("\r\nThe Q20 is waiting to be given a dynamic address on the bus...\r\n");
                terminalLine();
                
                state = WAIT_FOR_ENTDAA;
                break;
                
            case WAIT_FOR_ENTDAA:
                while(I3C1_OperatingModeGet() != I3C_TARGET_OPERATING_MODE_I3C_SDR);
                state = RECEIVE_DAA;
                break;
                
            case RECEIVE_DAA:
                /* Target is awaiting a Dynamic Address */
                receiveError = I3C1_Target_BufferReceive(receivedData, sizeof(receivedData));
                if(receiveError == I3C_TARGET_BUFFER_RECEIVE_BUFFER_SIZE_EXCEEDED)
                {
                    printf("Receive Buffer Size Exceeded\r\n");
                    break;
                }
                else if(receiveError == I3C_TARGET_BUFFER_RECEIVE_NO_ERROR)
                {
                    printf("\r\nDynamic Address Assignment is complete.\r\n");
                    printf("The Q20 Target has been given the dynamic address: 7'h%x.\n", I3C1DADR);
                }
                state = WAIT_FOR_SETMWL;
                I3COpModeDetails();
                terminalLine();
                break;
                
            case WAIT_FOR_SETMWL:
                if(I3C1_LastCCCReceivedGet() == I3C_CCC_SETMWL_D) {
                    I3C1_MaxReadLengthSet(MWLSize);
                    printf("\r\nThe Controller set a new MWL for the Target.\r\n");
                    terminalLine();
                    isDataReceived = false;
                    state = WAIT_FOR_DATA;
                }
                
            case WAIT_FOR_DATA:
                if(isDataReceived)
                {
                    isDataReceived = false;
                    state = RECEIVE_DATA;
                }
                break;
            
            case RECEIVE_DATA:
                if(memcmp(receivedData, LEDOn, numberOfBytesReceived) == 0) {
                    LED0_SetLow();
                    state = WAIT_FOR_DATA;
                }
                if(memcmp(receivedData, LEDOff, numberOfBytesReceived) == 0) {
                    LED0_SetHigh();
                    state = WAIT_TO_SEND;
                }
                
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
                
                break;

            case WAIT_TO_SEND:
                if(button) {
                    state = SEND_IBI;
                    button = false;
                }
                break;
                
            case SEND_SETUP:
                printf("\r\nSending data from Target to Controller\r\n");
                sendError = I3C1_Target_BufferTransmit(LEDOff, sizeof(LEDOff));
                if(sendError == I3C_TARGET_BUFFER_TRANSMIT_BUFFER_SIZE_EXCEEDED)
                {
                    printf("DMA buffer exceeded\r\n");
                    state = END;
                }
                else if(sendError == I3C_TARGET_BUFFER_TRANSMIT_NO_ERROR)
                {
                    printf("No error during send setup\r\n");
                    state = END;
                }
                break;
                
            case SEND_IBI:
                isDataReceived = false;
                numberOfBytesReceived = 0;
                I3C1_IBIMandatoryDataByteSet(mandatoryByte);

                if(I3C1_IsIBIEnabledOnBus())
                {
                    /* Send IBI request */
                    printf("Sending IBI request\r\n");
                    ibiError = I3C1_IBIRequest(IBIData, sizeof(IBIData));

                    if(ibiError == I3C_TARGET_IBI_REQUEST_NOT_IN_I3C_MODE)
                    {
                        printf("I3C_IBI_NOT_IN_I3C_MODE\r\n");
                        state = END;
                        break;
                    }
                    else if(ibiError == I3C_TARGET_IBI_REQUEST_IBI_DISABLED_ON_BUS)
                    {
                        printf("I3C_IBI_IBI_DISABLED_ON_BUS\r\n");
                        state = END;
                        break;
                    }
                    else if(ibiError == I3C_TARGET_IBI_REQUEST_SEND_BUFFER_SIZE_EXCEEDED)
                    {
                        printf("I3C_IBI_SEND_BUFFER_SIZE_EXCEEDED\r\n");
                        state = END;
                        break;
                    }
                    else if(ibiError == I3C_TARGET_IBI_REQUEST_NO_ERROR)
                    {
                        printf("\r\nIn-Band Interrupt successfully sent to Controller\r\n");
                        LED0_SetLow();
                        terminalLine();
                        state = WAIT_IBI;
                    }

                }
                else
                {
                    printf("IBI is not enabled on the bus\r\n");
                    state = END;
                    break;
                }
                break;
                
            case WAIT_IBI:
                if(button) {
                    state = SEND_IBI;
                    button = false;
                }
                break;
                
            case END:
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

void I3COpModeDetails(void) {
    switch(I3C1_OperatingModeGet()) {
        case 0:
            printf("\r\nThe Q20 is a Target is operating in Legacy I2C mode\r\n");
            printf("The Bus is operating in Single Data Rate (SDR) mode.\r\n");
            break;
        case 1:
            printf("\r\nThe Q20 Target is now operating in I3C mode.\r\n");
            printf("The bus is operating in Single Data Rate (SDR) mode\r\n");
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

void I3CTargetDetails() {
    printf("\nThe target's dynamic address is %x.\n", I3C1DADR);
    printf("The target's operating mode is %d\n", I3C1STAT0bits.OPMD);
    printf("The last CCC code received was %x\n\n", I3C1CCC);
    printf("Bus error %x\n", I3C1BSTAT);

    printf("The Maximum Write Length is %d bytes\n", I3C1MWL);
    printf("The Maximum Read Length is %d bytes\n", I3C1MRL);
}

void TransactionCompleteCallback(struct I3C_TARGET_TRANSACTION_COMPLETE_STATUS *transactionCompleteStatus)
{
    if(transactionCompleteStatus->dataFlowDirection == I3C_TARGET_DATA_RECEIVED)
    {
        isDataReceived = true;  
        numberOfBytesReceived = transactionCompleteStatus->numOfBytesReceived;
    }
    else
    {
        isDataSent = true;
    }
}

void IBIDoneCallback(void)
{
    isIBICompleted = true;
}