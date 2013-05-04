# Makefile for detector

NAME	   = detector
OBJECTS    = $(NAME).o
DEVICE     = attiny4313
CLOCK      = 4000000
FUSES      = -U hfuse:w:0xDF:m -U lfuse:w:0xE2:m -F
PROGRAMMER = avrispmkII -P usb

AVRDUDE = avrdude -c $(PROGRAMMER) -p t4313
COMPILE = avr-gcc -Wall -Os -DF_CPU=$(CLOCK) -mmcu=$(DEVICE)

# symbolic targets:
all:	$(NAME).bin $(NAME).hex

.c.o:
	$(COMPILE) -c $< -o $@

flash:	all
	$(AVRDUDE) -U flash:w:$(NAME).hex:i

fuse:
	$(AVRDUDE) $(FUSES)

clean:
	rm -f $(NAME).hex $(NAME).elf $(OBJECTS) ${NAME}.bin

# file targets:
$(NAME).bin: $(NAME).elf
	avr-objcopy -j .text -j .data -O binary -R .eeprom $(NAME).elf $(NAME).bin

$(NAME).elf: $(OBJECTS)
	$(COMPILE) -o $(NAME).elf $(OBJECTS)

$(NAME).hex: $(NAME).elf
	rm -f $(NAME).hex
	avr-objcopy -j .text -j .data -O ihex $(NAME).elf $(NAME).hex


