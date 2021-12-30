# CEC-Tiny-Pro
Support for the HDMI CEC protocol with minimal hardware, running on an ATMEGA processor.

I've got myself into HDMI-CEC attempting to control the power state (on/off) of a vintage TV based on the state of a HDMI signal source. That is described in the [CEC-Tiny](https://github.com/SzymonSlupik/CEC-Tiny) project. This "Pro" version is intended to serve as a generic CEC playground. It can either run as an active CEC node or as a passive sniffer. I find it very useful as a sniffer to debug other CEC devices, as it can be inserted as an "in-the-middle" device, supporting serial terminal as an output.

I went through a dozen or so HDMI-CEC projects and all of them were quite complex (e.g., requiring a separation of CEC-IN and CEC-OUT as separate signals) and despite several efforts I could not get any of them up an running.
Then I came across the [CEC Volume Control project by Thomas Sowell](https://blog.ldtlb.com/2020/10/14/pioneer-sx950-hdmi-cec-volume.html) from which I derived the idea for this implemetation.

The code is a farily straightforward fork of [Thomas'es code](https://github.com/tsowell/avr-hdmi-cec-volume), with minor modifications:
* Changed to make it compatible with the Arduino IDE (this brings much greater flexibility in terms of hardware)
* Changed timer "ticks" to the system "micros()" function to make the code independent (to some extent) from the exact oscillator frequency. This change also makes it it easier to port to the low-end ATTINY processors, which do not have a 16-bit timer).
* Added support for some additional CEC messages.
* Added a hack to force the Chromecast to send power and volume commands on the CEC bus (to do that you need to pretend you are a TV - in CEC terms). This also works with Apple TV.

The INO file does not have any dependencies nor requires any libraries. Although this has been tested with both external, crystal-based clock as well as with an internal oscillator. Remember to select a proper option in the IDE and "Tools -> Burn Bootloader", which will set the AVR fuses accordingly.

Thomas argues for using a crystal oscillator, as the CEC protocol is very time-sensitive. I too do recommend for the first device you make to be crystal-based. This will  save you from debugging frustration. Please note the crystal must match the Arduino IDE settings for your board / processor. 16MHz is recommended. Once you have one board working, you may consider iterating by removing the crystal, switching to the internal oscillator and tweaking the OSCCAL variable.

The highlight of this CEC implementation is the CEC line (pin 13 on a HDMI port) is fed directly to a GPIO pin on an AVR processor. The pin is configured as a High-Z (no pull-up) input when receiving and is toggled between low output (active) and High-Z input (inactive) when transmitting. This approach reduces the necessary hardware to minimum. In fact in the most simplified version the only part needed is the ATTINY25 processor (and an LED serving as an output). I thik this can be claimed the tiniest HDMI-CEC implementation possible. The code uses ~1600 bytes of program memory and 9 bytes of variables.

Below are photos of an example setup. I've taken the [Beetle board](https://www.dfrobot.com/product-1075.html), which is one of the smallest fully-featured Arduino and glued it on top of a [Male-Female HDMI "IR extender"](https://www.aliexpress.com/i/32852599971.html), which offers an easy access to both CEC and +5V lines (if using this one, remember to bridge the CEC line from one side to the other, as it is originally cut by a switch).


![alt text](https://github.com/SzymonSlupik/CEC-Tiny-Pro/blob/main/Beetle%20Leonardo%20on%20HDMI.JPG?raw=true "Adding a USB sniffer to HDMI")
![alt text](https://github.com/SzymonSlupik/CEC-Tiny-Pro/blob/main/HDMI%20CEC%20to%20Beetle%20Schematic.jpg?raw=true "HDMI CEC to Beetle Schematic")
![alt text](https://github.com/SzymonSlupik/CEC-Tiny-Pro/blob/main/HDMI%20Pass-Through%20with%20a%20Sniffer.JPG?raw=true "Adding a USB sniffer to HDMI")
![alt text](https://github.com/SzymonSlupik/CEC-Tiny-Pro/blob/main/HDMI%20CEC%20Sniffer%20Console.png?raw=true "HDMI CEC Sniffer console")
