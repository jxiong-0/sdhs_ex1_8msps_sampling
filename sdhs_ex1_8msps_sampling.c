/* --COPYRIGHT--,BSD
 * Copyright (c) 2017, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
//******************************************************************************
//   sdhs_ex1_8msps_sampling.c - SDHS sampling at 8MSPS and DTC transfer results to RAM
//
//   Description: Configure the SDHS for stand-alone use (register mode) at 8MSPS.
//   Use the DTC to transfer the results to an array in RAM.
//
//           MSP430FR6047
//         ---------------
//     /|\|               |
//      | |               |
//      --|RST            |
//        |           P1.0|---> LED
//        |               |-USSXTIN
//        |               |-USSXTOUT
//        |         CH0_IN|<--- input signal
//
//   Wallace Tran
//   Texas Instruments Inc.
//   June 2017
//******************************************************************************
#include "driverlib.h"

#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(results, ".leaRAM")
#pragma RETAIN(results)
uint16_t results[1024] = {0};
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma location = 0x2C00
__no_init uint16_t results[1024];
#pragma required = results
#elif defined(__GNUC__)
uint16_t results[1024] __attribute__ ((section(".leaRAM"), used));
#else
#error Compiler not supported!
#endif

void main (void)
{
    //Stop WDT
    WDT_A_hold(WDT_A_BASE);

    //PA.x output
    GPIO_setAsOutputPin(GPIO_PORT_PJ, GPIO_PIN1);
    //Disable the GPIO power-on default high-impedance mode to activate previously configured port settings
    PMM_unlockLPM5();

    // //Set all PA pins HI
    // GPIO_setOutputHighOnPin(
    //     GPIO_PORT_PJ,
    //     GPIO_PIN1
    //     );

    /*Clock Setup*/
    FRAMCtl_A_configureWaitStateControl(FRAMCTL_A_ACCESS_TIME_CYCLES_1);    //necessary for clock operating above 8MHz
    //Set Divider to 4 before changing frequency to prevent out of spec opreation from overshoot transient
    CS_initClockSignal(CS_ACLK, CS_VLOCLK_SELECT, CS_CLOCK_DIVIDER_4);
	CS_initClockSignal(CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_4);
	CS_initClockSignal(CS_MCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_4);
    //Set DCO to 16 MHz and wait for DCO to settle
    CS_setDCOFreq(CS_DCORSEL_1, CS_DCOFSEL_4); //rsel: 0 = low, 1 = hi; fsesl6: low = 5.33MHz, hi = 16MHz
    __delay_cycles(60); //(10 us / (1/4MHz)) + 20 cycles
    // Set all dividers to 1 for 16MHz operation
	// MCLK = SMCLK = 16MHz
	CS_initClockSignal(CS_ACLK, CS_VLOCLK_SELECT, CS_CLOCK_DIVIDER_1);
	CS_initClockSignal(CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);
	CS_initClockSignal(CS_MCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);
    /*End of Clock Setup*/

    /*Setup SAPH for Register Mode*/
    SAPH_unlock(SAPH_BASE);
	SAPH_configPHYBiasParam saphParam = {0};
	saphParam.enableChargePump = SAPH_PHY_MULTIPLEXER_CHARGEPUMP_ENABLE;
	saphParam.biasSwitchASQ = SAPH_PHY_BIAS_SWITCH_CONTROLLED_BY_REGISTER;
	saphParam.biasPGA = SAPH_PHY_PGA_BIAS_NOMINAL_VALUE;
	saphParam.biasExcitation = SAPH_PHY_EXCITATION_BIAS_NOMINAL_VALUE;
	SAPH_configurePHYBias(SAPH_BASE, &saphParam);
	SAPH_configurePHYMultiplexer(SAPH_BASE, SAPH_PHY_DUMMYLOAD_DISABLE, SAPH_PHY_SOURCE_CONTROLLED_BY_REGISTER, SAPH_PHY_INPUT_CHANNEL_0);
	SAPH_lock(SAPH_BASE);
    /*end of SAPH Setup*/ 

    /*Initialize HSPLL*/
    //Configure USSXT Oscilator to operator at 8MHz
    HSPLL_xtalInitParam xtalParam;
	xtalParam.oscillatorType = HSPLL_XTAL_OSCTYPE_XTAL;
	xtalParam.oscillatorEnable = HSPLL_XTAL_ENABLE;
	xtalParam.xtlOutput = HSPLL_XTAL_OUTPUT_ENABLE;
	HSPLL_xtalInit(HSPLL_BASE, &xtalParam);
	// Check if oscillator is stable
	while(HSPLL_getOscillatorStatus(HSPLL_BASE) == HSPLL_OSCILLATOR_NOT_STABILIZED);
    //set up timer to wait for crystal to stabilize: 4096 clocks for crystal resonator --> For 8MHz XTAL, 4096 clocks = 512us. Using VLO = 9.4kHz, wait 5 timer clock cycles = 532us
    Timer_A_initUpModeParam timerParam = {0};
	timerParam.timerPeriod = 5;
	timerParam.captureCompareInterruptEnable_CCR0_CCIE = TIMER_A_CCIE_CCR0_INTERRUPT_ENABLE;
	// Timer sourced from ACLK (VLO)
	timerParam.clockSource = TIMER_A_CLOCKSOURCE_ACLK;
	// Clear timer
	timerParam.timerClear = TIMER_A_DO_CLEAR;
	timerParam.startTimer = true;
	Timer_A_initUpMode(TA4_BASE, &timerParam); //Timer A4 -- disable interrupts after waiting for crystal to stabilize
    // Enter LPM3 w/interrupts enabled
	__bis_SR_register(LPM3_bits | GIE);
    // For debugger
	__no_operation();
    //Initilize PLL
    HSPLL_initParam hspllParam;
	hspllParam.multiplier = PLLM_19_H;                                      //multiply by 19 to get desired 80 MHz output clock frequency: 80MHz x 2 = 8MHz x (19+1)
	hspllParam.frequency = HSPLL_GREATER_THAN_6MHZ;                         //HSPLL will be greater than 6 MHz
	HSPLL_init(HSPLL_BASE, &hspllParam);
    //Power up UUPS to start PLL and wait
    UUPS_turnOnPower(UUPS_BASE, UUPS_POWERUP_TRIGGER_SOURCE_USSPWRUP);
    while(UUPS_getPowerModeStatus(UUPS_BASE) != UUPS_POWERMODE_READY);  //wait for UUPS to power up
    while(HSPLL_isLocked(HSPLL_BASE) == HSPLL_UNLOCKED);                //wait for PLL to lock
    /*End of HSPLL Configuration*/
    
    /*Setup SDHS (Sigma-Delta High Speed ADC)*/
    SDHS_initParam sdhsParam = {0}; //create config struct

    //All SDHS parameters, brief descriptions and defaults are commented
    sdhsParam.triggerSourceSelect = SDHS_REGISTER_CONTROL_MODE;             // Trigger source select - SDHS_REGISTER_CONTROL_MODE
    //sdhsParam.msbShift = SDHS_NO_SHIFT;                                   // Selects MSB shift from filter out - SDHS_NO_SHIFT
    //sdhsParam.outputBitResolution = SDHS_OUTPUT_RESOLUTION_12_BIT;        // Selects the output bit resolution - SDHS_OUTPUT_RESOLUTION_12_BIT
    sdhsParam.dataFormat = SDHS_DATA_FORMAT_OFFSET_BINARY;                  // Select data format - SDHS_DATA_FORMAT_TWOS_COMPLEMENT
    sdhsParam.dataAlignment = SDHS_DATA_ALIGNED_LEFT;                       // Selects the data format - SDHS_DATA_ALIGNED_LEFT
    //sdhsParam.interruptDelayGeneration = SDHS_DELAY_SAMPLES_1 ;           // Selects the data format - SDHS_DELAY_SAMPLES_1
    sdhsParam.autoSampleStart = SDHS_AUTO_SAMPLE_START_DISABLED;            // Selects the Auto Sample Start - SDHS_AUTO_SAMPLE_START_DISABLED
    sdhsParam.oversamplingRate = SDHS_OVERSAMPLING_RATE_10;                 // 80MHz / 10 (OSR) = 8 MSPS; MCLK must be >= 8 MHz - SDHS_OVERSAMPLING_RATE_10
    //sdhsParam.dataTransferController = SDHS_DATA_TRANSFER_CONTROLLER_ON;  // Selects the Data Transfer Controller State - SDHS_DATA_TRANSFER_CONTROLLER_ON
    //sdhsParam.windowComparator = SDHS_WINDOW_COMPARATOR_DISABLE;          // Selects the Window Comparator State - SDHS_WINDOW_COMPARATOR_DISABLE
    //sdhsParam.sampleSizeCounting = SDHS_SMPSZ_USED;                       // Selects the Sample Size Counting - SDHS_SMPSZ_USED

    // Initialize SDHS module
    SDHS_init(SDHS_BASE, &sdhsParam);                                                       //initilzie SDHS based on above parameters
    SDHS_setTotalSampleSize(SDHS_BASE, 256);                                                //Set to 256 samples
    SDHS_setPGAGain(SDHS_BASE, 0x1A);                                                       //PGA Gain of 0.1 dB
    SDHS_setModularOptimization(SDHS_BASE, SDHS_OPTIMIZE_PLL_OUTPUT_FREQUENCY_77_80MHz);    //Using 80 MHz PLL so optimize for 80 MHz
    SDHS_setDTCDestinationAddress(SDHS_BASE, 0);                                            //Set destination address as the start fo LEA RAM for the DTC (Data Transfer Controller)
    SDHS_enableInterrupt(SDHS_BASE, SDHS_ACQUISITION_DONE_INTERRUPT);                       //Enable the aquisition done interrupt (i.e. after 256 samples are transferred)                    //enable acquisition done interrupt
    SDHS_enableTrigger(SDHS_BASE);                                                          //Allow system to react to event
    SDHS_enable(SDHS_BASE);                                                                 //turn on
    SDHS_startConversion(SDHS_BASE);                                                        //Start conversion
    __delay_cycles(320);                                                                    //Delay for worst case PGA settling time: 40 us = 1/(8MHz) * 320
    
    /* ---Begin configure TA2.1 for 1/sec to trigger the pulse generation and toggle LED--- */
	Timer_A_initUpModeParam timerParam2 = {0};
	timerParam2.timerPeriod = 9400;                     //timerPeriod in clock cycles, 9400 cycles / (9.4 kHz) -> 1 s
	TA2CCR1 = 4700;                                     //interrupt occurs 0.5 s --> enter routine at frequency of 2 Hz --> GPIO toggles at frequency of 1 Hz
	TA2CCTL1 = OUTMOD_7 | CCIE; 	                    // Enable output signal to trigger PPG, enable interrupt
	// Timer sourced from ACLK (VLO)
	timerParam2.clockSource = TIMER_A_CLOCKSOURCE_ACLK; //VLO has frequency of 9.4 kHz from page 42 of data sheet
	// Clear timer
	timerParam2.timerClear = TIMER_A_DO_CLEAR;
	timerParam2.startTimer = true;
	Timer_A_initUpMode(TA2_BASE, &timerParam2);
	/* ---End configure TA2.1 for 1/sec to trigger the pulse*/

    /*Timer A.0 Setup for Toggling GPIO PJ.1
    //configure Timer0_A -- Toggling GPIO PJ.1
    Timer_A_initUpModeParam upConfig = {0};
    upConfig.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    upConfig.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_4; //set TimerA0 to SMCLK/4 = 4MHz
    upConfig.timerPeriod = 10000; //each # is 500ns
    upConfig.timerInterruptEnable_TAIE = TIMER_A_TAIE_INTERRUPT_DISABLE; //disable overflow interrupt
    upConfig.captureCompareInterruptEnable_CCR0_CCIE = TIMER_A_CCIE_CCR0_INTERRUPT_ENABLE; //enable CCR0 interrupt
    upConfig.timerClear = TIMER_A_DO_CLEAR;

    //initialize Timer0_A
    Timer_A_initUpMode(TIMER_A0_BASE, &upConfig);

    //enable Timer0_A0 interrupt
    __enable_interrupt(); //do i need this???

    //start Timer0_A
    Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);
    */

    while(1){
        //Enter LPM4 w/interrupts enabled
        __bis_SR_register(LPM4_bits + GIE);

        //For debugger
        __no_operation();
    }
    
}

// Timer A2 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER2_A1_VECTOR
__interrupt void Timer2_A1_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER2_A1_VECTOR))) Timer2_A1_ISR(void)
#else
#error Compiler not supported!
#endif
{
	switch(__even_in_range(TA2IV, TAIV__TAIFG))
	{
	case TAIV__NONE:   break;               	// No interrupt
	case TAIV__TACCR1:
    	SDHS_endConversion(SDHS_BASE);
    	SDHS_startConversion(SDHS_BASE);   	    // Start conversion
    	break;
	case TAIV__TAIFG: break;                	// overflow
	default: break;
	}
}

// SDHS interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = SDHS_VECTOR
__interrupt void SDHS_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(SDHS_VECTOR))) SDHS_ISR(void)
#else
#error Compiler not supported!
#endif
{
	switch(__even_in_range(SDHSIIDX, IIDX_6))
	{
	case IIDX_0:   break;               	                // No interrupt
	case IIDX_1:   break;               	                // OVF interrupt
	case IIDX_2:                        	                // ACQDONE interrupt
    	GPIO_toggleOutputOnPin(GPIO_PORT_PJ, GPIO_PIN1); 	// Toggle GPIO J.1 to show new cycle
    	__delay_cycles(10000);                              // 10000 / 9400 = 1.0638s delay?
    	__no_operation();               	                //put breakpoint here to view results
    	break;
	case IIDX_3:   break;               	                // SSTRG interrupt
	case IIDX_4:   break;               	                // DTRDY interrupt
	case IIDX_5:   break;               	                // WINHI interrupt
	case IIDX_6:   break;               	                // WINLO interrupt
	default: break;
	}
}

// Timer A4 interrupt service routine -- Stabilize USSXT
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER4_A0_VECTOR
__interrupt void Timer4_A0_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER4_A0_VECTOR))) Timer4_A0_ISR(void)
#else
#error Compiler not supported!
#endif
{
	// Stop the timer and wake up from LPM
	Timer_A_startCounter(TA4_BASE, TIMER_A_STOP_MODE);          //Stop Timer 4
	__bic_SR_register_on_exit(LPM3_bits | GIE);                 //Disable gloabal interrupts for the rest of SDHS configuration -- will be enabled again in the main loop.
	__no_operation();
}

/*
// Timer0_A0 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer0_A0_ISR (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) Timer0_A0_ISR (void)
#else
#error Compiler not supported!
#endif
{
    Timer_A_clearCaptureCompareInterrupt(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);
    GPIO_toggleOutputOnPin(GPIO_PORT_PJ, GPIO_PIN1);
}
*/

