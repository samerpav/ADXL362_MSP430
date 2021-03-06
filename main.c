/*
P1.0 <--> LED3
P1.1 --> CS
P1.2 --> LED1
P1.3 <-- INT1
P1.4 --> LED2
P1.5 --> CLK
P1.6 --> MOSI
P1.7 <-- MISO
P2.6 --> LED4
*/

#include <msp430.h>

unsigned char transfer(unsigned char);
unsigned char Identify();
void SoftReset();
void WriteByteToRegister(unsigned char, unsigned char);
unsigned char ReadStatusRegister();

unsigned char result = 0x0;

unsigned int converting()
{
	return ADC10CTL1 & ADC10BUSY;
}

int main(void)
{
	WDTCTL = WDTPW + WDTHOLD; 		// Stop watchdog timer

	BCSCTL1 = CALBC1_1MHZ;            // Set range
	DCOCTL = CALDCO_1MHZ;             // Set DCO step + modulation

	// the only INPUT pin is P1.3 (INT1)
	P1DIR = 0xFF;
	P1DIR &= ~BIT3;
	P1OUT = 0;
	P2DIR = 0xFF;
	P2OUT = 0;
	P2SEL = 0;
	//P1DIR = BIT6 | BIT5 | BIT0 | BIT1 | BIT2 | BIT4;		-- old code

	USICTL0 |= USIPE7 + USIPE6 + USIPE5 + USIMST + USIOE;		// setup SPI mode
	USICTL0 |= USISWRST;						// enable software reset to begin spi init

	// setup clock polarity and edge
	USICKCTL &= ~USICKPL;
	USICTL1 = USICKPH;

	USICKCTL |= USIDIV_2 | USISSEL_2;		// SPI clock
	USICTL0 &= ~USISWRST; 		// release USI for operation

	// CS disable, set CS high
	P1OUT |= BIT1;

	SoftReset();

	// Turn on LED P1.2 if ADXL362 is not found
//	result = Identify();
//	if (result==0xAD)
//		P1OUT &= ~BIT2;
//	else
//		P1OUT |= BIT2;

	// turn off LED 1.2
	P1OUT &= ~BIT2;

	// turn off LED P1.4
	P1OUT &= ~BIT4;

	// set activity threshold to 100 mg
	WriteByteToRegister( 0x20, 0x64 );
	WriteByteToRegister( 0x21, 0x0 );

	// Default Mode and Referenced Activity only (don't care about Inactivity)
	WriteByteToRegister( 0x27, 0x3 );

	// map activity status to int1
	WriteByteToRegister( 0x2A, 0x10 );

	// begin measurement in wakeup mode
	WriteByteToRegister( 0x2D, 0x0A );

	// interrupt check on P1.3
	P1IE = BIT3;
	P1IES &= ~BIT3;		// rise edge
	P1IFG &= ~BIT3; 	// P1.3 IFG cleared

	_BIS_SR(LPM4_bits + GIE);
}

#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{
	ADC10CTL0 = ADC10ON | SREF_0 | ADC10SHT_2;
	ADC10CTL1 = INCH_0 | SHS_0 | ADC10SSEL_0 | ADC10DIV_0 | CONSEQ_0;
	ADC10CTL0 |= ENC;
	ADC10AE0 = BIT0;
	ADC10CTL0 |= ADC10SC; 	// start conversion
	while (converting());
	if (ADC10MEM < 340)		// 340 Vss 3.0v, 360 Vss 3.6v
	{
		int j;
		for (j=0; j<40; j++)
		{
			volatile unsigned int i;

			P1OUT ^= BIT2;
			i = 2000;
			do i--;
			while(i != 0);
			P1OUT ^= BIT2;
			i = 3000;
			do i--;
			while(i != 0);

			P1OUT ^= BIT4;
			i = 2000;
			do i--;
			while(i != 0);
			P1OUT ^= BIT4;
			i = 3000;
			do i--;
			while(i != 0);

/*
 * 	P1.0 is used as light sensor
 */

	//		P1OUT ^= BIT0;
	//		i = 2000;
	//		do i--;
	//		while(i != 0);
	//		P1OUT ^= BIT0;
	//		i = 2000;
	//		do i--;
	//		while(i != 0);

			/*
			 *	for dog collar, don't use this one to save power
			 *
			 */
			P2OUT ^= BIT6;
			i = 2000;
			do i--;
			while(i != 0);
			P2OUT ^= BIT6;
			i = 3000;
			do i--;
			while(i != 0);
		}
	} // ADC10MEM

	// acknowledge interrupt by reading STATUS register
	ReadStatusRegister();

	P1IFG &= ~BIT3;                           // P1.3 IFG cleared

	// restart measurement in wakeup mode again after interrupt is cleared
	WriteByteToRegister( 0x2D, 0x0A );
}

/*
 * command structure for writing to ADXL362 is
 * <CS down><0x0A><address><data byte><CS high>
 *
 */
void WriteByteToRegister(unsigned char address, unsigned char value)
{
	P1OUT &= ~BIT1; 	// CS enable; pull CS low
	transfer(0xA);		// WRITE command
	transfer(address);	// register address
	transfer(value);
	P1OUT |= BIT1;		// disable CS; pull CS high
}

unsigned char ReadStatusRegister()
{
	unsigned char s = 0x0;

	P1OUT &= ~BIT1; 	// CS enable; pull CS low
	transfer(0xB);		// READ command
	transfer(0x0B);		// register address for STATUS
	s = transfer(0xFF);	// dummy send; read data from MISO
	P1OUT |= BIT1;		// disable CS; pull CS high

	return s;
}

unsigned char Identify()
{
	unsigned char s = 0x0;

	P1OUT &= ~BIT1; 	// CS enable; pull CS low
	transfer(0xB);		// READ command
	transfer(0x0);		// register address
	s = transfer(0xFF);	// dummy send; read data from MISO
	P1OUT |= BIT1;		// disable CS; pull CS high

	return s;
}

void SoftReset()
{
	WriteByteToRegister(0x1F, 0x0);
}

unsigned char transfer(unsigned char value)
{
	unsigned char r = 0x0;

	USISRL = value;
	USICNT = 8;
	while ( !(USICTL1 & USIIFG) ); // wait for transfer to complete

	r = USISRL;
	return r;
}

