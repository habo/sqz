////////////////////////////////////////////////////////////
// detector.c -Schrödiger Quanten Zähler = SQZ            //
// copyright 2013 Stefan Riepenhausen rhn@gmx.net         //
// licensed under the GNU GPL v2 or newer                 //
////////////////////////////////////////////////////////////

#include <inttypes.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <avr/pgmspace.h>

/*
wanted
VU-Meter (PWM): DONE
Red LED: DONE
Piezo: DONE
Button "sensor": DONE
Button ON: SITCH
ISP Header connector (seriel connector)

1. on Taster schaltet Gerät
1.1 externes kabel angeschlossen. langer fieser piezzo und dann aus
2. VU-Meter bewegt sich kurz, piezo "tickt"
3. LED ist an
4. timer für autoauschalten ist aktiv (nach 4 min schaltet es sich selbst aus)
5. wird "sensor" gedrückt:
5.0 timer wird resettet
5.1. VU-Meter bewegt sich mehrmals und bleibt dann irgendwo stehen
5.2. LED blinkt + SENSORLED faded
5.3. piezzo ertönt
6. warten auf "sensor" drücken oder timer ablauf

 ------------------------------------------
 \ 13 12 11 10 09 08 07 06 05 04 03 02 01 /
  \ 25 24 23 22 21 20 19 18 17 16 15 14  /
   --------------------------------------
Connector Pin
 1: VCC
 2: SCK
 3: MISO
 4: MOSI
 5: RST
 6: GND
12: PINBUTTONSENSOR
13: GND



*/
// input (detector)
#define PINBUTTONSENSOR	PD4
#define PINBUTTONCONNECTED PA0
// output
// aka RED LED
#define PINPOWER 	PD5
// aka Piezo
#define PINBEEP		PB0
#define PINSLIDE	PB2 // muss so sein wegen pwm0 an oc0a
#define PINVU		PB3 // muss so sein wegen pwm1 an oc1a
#define PINVULIGHT	PD3
#define PINCABELCHECK   PDx
#define PINSENSORLED	PDy
#define true		0
#define false		1

#define TIMEOUT 120 // timeout in seconds

#define MODEINIT	0
#define MODEWAIT	1
#define MODESENSOR	2
#define MODENOTHING	3

uint8_t b1=0;
uint8_t mode=MODEINIT;
uint16_t beepduration=0;
uint16_t cnt=0;
uint16_t seconds=0;

#define DATAMAX		13
const uint8_t data[DATAMAX] ={5,2,6,30,24,25,3,5,15,14,12,6,7};

#define MELODIEMAX	6
const uint8_t melodie[MELODIEMAX][2] ={
	{10,200},
	{50,200},
	{100,200},
	{50,200},
	{100,200},
	{50,200},
};

ISR(TIMER0_COMPA_vect)
{
	cnt++;
	if (cnt%2==0 && beepduration>0)
		beepduration--;
	if (beepduration>0)
		PORTB ^= (1<<PINBEEP);  
}

void slide(uint8_t pos, uint8_t dur){
	int i,p;
	cli(); // interrupte aus
	for(p=0;p<1*dur;p++){
		PORTB |= (1<<PINSLIDE);
		for(i=0; i<pos; i++) _delay_us(3);
		PORTB &=~(1<<PINSLIDE);
		for(; i<255; i++) _delay_us(3);
	}
	sei();
}
void slidefade(){
	int i;
	for(i=0;i<200;i++) {
		slide(i,3);
	}
	for(i=200;i>0;i--) {
		slide(i,3);
	}
}
void vu(uint8_t pos){
	OCR1A = pos;
}

void beep(uint8_t f, uint16_t duration){
	beepduration=duration;
	OCR0A = f;
}

void playmelody()
{
	int i,d;
	for(i=0; i<MELODIEMAX; i++)
	{
		beep(melodie[i][0],melodie[i][1]);
		for(d=0;d<melodie[i][1]+10;d++)
			_delay_ms(1);
	}
	
}

void check_inputs()
{
	if ( !(PIND & (1<<PINBUTTONSENSOR)) )
		b1++;
	else 
		b1 = 0;
}

void init()
{
	// set output pins
	DDRB |= (1<<PINBEEP); 
	DDRB |= (1<<PINSLIDE); 
	DDRB |= (1<<PINVU); 
	DDRD |= (1<<PINVULIGHT); 
	DDRD |= (1<<PINPOWER); 
	// input mit pullup
	DDRD |= (1<<PINBUTTONSENSOR); 
	PORTD |= (1<<PINBUTTONSENSOR);

	// LED oder selbsthaltung einschalten auf PD5
	PORTD |= (1<<PINPOWER);

	// setup Timer0 for beep
	TCCR0A |= _BV(WGM01);	// Set CTC mode
	TCCR0B |= (1<<CS00) | (1<<CS01);
	// ((4.000.000/8)/1000) = 64
	OCR0A = 64;		
	TIMSK |= (1<<OCIE0A);

	// timer0
//	DDRB |= (1<<PINSLIDE);  // control OCR0A
//	TCCR0A |= (1<<WGM10) | (1<<COM1A1); // PWM, phase correct, 8 bit.
//	TCCR0B |= (1<<CS11) | (1<<CS10); // Prescaler 64 = Enable counter

	// timer1
	DDRB |= (1<<PINVU);  // control OCR1A
	TCCR1A |= (1<<WGM10) | (1<<COM1A1); // PWM, phase correct, 8 bit.
	TCCR1B |= (1<<CS11) | (1<<CS10); // Prescaler 64 = Enable counter

	sei();
}

void shutdown(uint8_t dodelay){
	if (dodelay==true) {
		beep(200,200);
		_delay_ms(500);
	}
	// hier soll alles ausgeschaltet werden 
	PORTD &= ~(1<<PINPOWER);
	vu(0);
	cli(); // interrupte aus
	PORTA =0;
	PORTB =0;
	PORTD =0;
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_mode();
}

void cron(){
	uint8_t i;
	switch (mode){
		case MODEINIT:	// booting 
			//beep(10,10); // tick
			// wenn nicht kabel nicht angeschlossen 
/*
			if ( !(PINA & (1<<PINBUTTONCONNECTED)) ) { 
				beep(8,15000); // hoch, aber noch nicht fies
				_delay_ms(12000);
				shutdown(false);
			}
*/
			playmelody();
//			PORTD |= (1<<PINVULIGHT);
			vu(20);
			_delay_ms(200);
			vu(0);
			mode=MODEWAIT;
		break;
		case MODEWAIT:	// waiting for input
			beep(10,10); // tick
			if (b1>0) {
				mode=MODESENSOR;
			}
		break;
		case MODESENSOR:	// sensor active
			for(i=0;i<3;i++) {
				slidefade();
			}
			PORTD |= (1<<PINVULIGHT);
			for(i=0;i<DATAMAX;i++) {
				_delay_ms(300);
				PORTD ^= (1<<PINVULIGHT);  
				beep(10,10); // tick
				vu(data[i]);
			}
			// sensor done...
			PORTD |= (1<<PINVULIGHT);
			vu((seconds+b1)%30);
			_delay_ms(100);
			beep(130,100);
			mode=MODENOTHING;
		break;
		case MODENOTHING:	// all done
		break;
	}
	// selbsthaltung ausschalten z.B.nach 120s
	if (seconds>TIMEOUT) {
		shutdown(true);
	}
}

void main_loop() { 
	uint16_t i=0;
	for (;;) {
		i++;
		check_inputs();
		_delay_ms(1);
		if (i>1000) {
			seconds++;
			cron(seconds);
			i=0;
		}
	}
}

int main (void)
{
	init();
	main_loop();
	return (0);
}

