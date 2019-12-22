/******************************************************************************************
* EE344 Lab 3
*   This lab create a counter triggered by the gpio switch 2. This is a state machine that
*will ask user to choose the modes of counter: Software counter, Hardware/software mixed 
*counter, and the hardware counter. The outcome functionality will be the same for the
*three counter. Yet, the way they response and triggered are different.
*
* Shao-Peng Yang, 10/24/2019
******************************************************************************************/
#include "MCUType.h"                                                                         //Include project header file
#include "BasicIO.h"
#include "K65TWR_ClkCfg.h"
#include "K65TWR_GPIO.h"

#define STRING_LENGTH 2U
#define ZERO_DI 0U
#define IRQC_FE 10U
#define MAXLZ 1U

static INT16U CalcChkSum(INT8U *startaddr,  INT8U *endaddr);
void PORTA_IRQHandler(void);

typedef enum {CP, SC, CC, HC} UISTATE_T;                                                    //create states for the state machine
static UISTATE_T UICurrentState = CP;                                                       //define and initialize the current state for the state machine


const INT8C StartPrompt[] = "Please enter 's' for software, 'b' for mixed, and 'h' for hardware: ";
const INT8C NewLine[] = "\n\r";

static INT8U Strobe = 0;                                                                    // signal to indicate mcu just entered the handler
static INT32U Count = 0;                                                                    //Global counter for the ISR and used in other two counter


void main(void){
    /*Check sum pointers and variable*/
    INT8U *high_addr = (INT8U *)0x001FFFFF;
    INT8U *low_addr = (INT8U *)0x00000000;
    INT16U check_sum = 0;

    /*user input for choosing counter and quit*/
    INT8C user_input[2];
    INT8C char_in = 'q';

    /*Switch state for software trigger*/
    INT32U prev_sw = 0;                                                             
    INT32U cur_sw = 0;                                                                      //reading directly from the gpio pdir register so it needs 32bits

    K65TWR_BootClock();
    BIOOpen(BIO_BIT_RATE_9600);
    
    /*Checking Memory*/
    check_sum = CalcChkSum(low_addr, high_addr);
    BIOPutStrg("CS: ");
    BIOOutHexHWord(check_sum);
    BIOPutStrg("\n\r");

    /*Create a four-states state machine for the user interface*/
    while(1){
        switch(UICurrentState){
            case CP:
                /*only prompt when reset or return from the counter states*/
                if(char_in == 'q'){
                    BIOPutStrg(StartPrompt);
                    char_in = 0;
                }

                (void)BIOGetStrg(STRING_LENGTH, user_input);                                //no need to use the return value to check error type

                switch(user_input[0]){
                    case('s'):
                        UICurrentState = SC;
                        break;
                    case('b'):
                        UICurrentState = CC;
                        break;
                    case('h'):
                        UICurrentState = HC;
                        break;
                    default:
                        break;
                }
                break;
            case SC:
                Count = 0;
                BIOPutStrg("\r");
                BIOOutDecWord(Count, MAXLZ);

                GpioSw2Init(ZERO_DI);                                                       //initialize switch2 and disable interrupt
                prev_sw &= ~(GPIO_PIN(SW2_BIT));                                            //pre set last previouse switch state to 0, switch2 is indicated in 4th bit of the register
                while(char_in != 'q'){
                    cur_sw = SW2_INPUT;
                    /*checking falling edge*/
                    if(!cur_sw && prev_sw == (1<<4)){
                        Count++;
                        BIOPutStrg("\r");
                        BIOOutDecWord(Count, MAXLZ);
                    }
                    else{
                    }
                    prev_sw = cur_sw;                                                       //update last switch state
                    char_in = BIORead();
                }
                UICurrentState = CP;
                BIOPutStrg(NewLine);
                break;
            case CC:
                Count = 0;
                BIOPutStrg("\r");
                BIOOutDecWord(Count, MAXLZ);

                /*initialize hardware trigger*/
                GpioSw2Init(IRQC_FE);
                SW2_CLR_ISF();
                while(char_in != 'q'){
                    /*check flage for falling edge*/
                    if(SW2_ISF != 0){
                        SW2_CLR_ISF();
                        Count++;
                        BIOPutStrg("\r");
                        BIOOutDecWord(Count, MAXLZ);
                    }
                    else{
                    }
                    char_in = BIORead();
                }
                UICurrentState = CP;
                BIOPutStrg(NewLine);
                break;
            case HC:
                Count = 0;
                BIOPutStrg("\r");
                BIOOutDecWord(Count, MAXLZ);

                /*Initialize NVIC and interrupt in the source*/
                SW2_CLR_ISF();
                NVIC_ClearPendingIRQ(PORTA_IRQn);
                NVIC_EnableIRQ(PORTA_IRQn);
                GpioSw2Init(IRQC_FE);
                while(char_in != 'q'){
                    /*only overwrite when the program just entered the isr*/
                    if(Strobe){
                        BIOPutStrg("\r");
                        BIOOutDecWord(Count, MAXLZ);
                        Strobe = 0;
                    }
                    else{
                    }
                    char_in = BIORead();
                }
                NVIC_DisableIRQ(PORTA_IRQn);                                                    
                UICurrentState = CP;
                BIOPutStrg(NewLine);
                break;
            default:
                UICurrentState = CP;
                break;
        }
    }

}
/***********************************************************************
*CalcChkSum() - Sums up the bytes stored in the memory from
*               the start addr to the end addr, this a private function
*Return Value: the 2bytes unsigned result of the summation
*Arguments: *startaddr is a pointer pointing to the data in the
            start of the memory specified by users
            *endaddr is a pointer pointing to the data in the
            end of the memory specified by users
***********************************************************************/
static INT16U CalcChkSum(INT8U *startaddr, INT8U *endaddr){
    INT16U sum = 0;
    INT8U data =0;
    INT8U over_flow = 0;
    while(startaddr <= endaddr && over_flow == 0)
    {
        data = *startaddr;
        sum += (INT16U)data;
        /*check overflow*/
        if(startaddr == (INT8U *)0xFFFFFFFFU){
            over_flow = 1;
        }
        else{
            startaddr++;
        }
    }
    return sum;
}

void PORTA_IRQHandler(void){
    SW2_CLR_ISF();
    Count++;
    Strobe = 1;
}
