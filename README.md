# QO-100cat-for-FT-817-8
Arduino sketch for CAT controlling FT-818 with Arduino Nano and small SSD1306 128x32 display

/*
    FT-818 / QO-100 controller PA3ANG - May 2020
    Using Arduino Nano Every with Adafruit SSD1306 128x32 OLED display. The sketch uses knobs from the FT-818 to control the 
    functions, so no external knobs are needed. The knobs used are the CLAR, LOCK and SPLIT. 

    My QO-100 RF hardware consist of SG-Lab componets for uplink (transvertor and amplifier). The FT-818 and SG-Lab transverter are 
    both TXCO stabile. The outdoor LNB has an TCXO build inbut does drift a bit based on the environmental temperature. 
    The LNB converts down from 10489 to 432 MHz. Calibration is only needed to accomodate for the outside temperature deviations 
    which are about +- 1kHz on 70cm. 
    
    The FT-818 can be wired directly from the ACC Jack to the Ardunio digital pin 3 (RX D) and 2 (TX D) and GND. No level schifter is 
    needed. 

    The sketch reads VFO A frequency and calculates the associated QO-100 transponder downlink frequency based on
    the LNB_offset (IF) frequency and the calibartion offset (+- 1KHz). 
    Based on the calculated QO_frequency the sketch calculates the uplink TX_frequency for the FT-818 to be programmed in VFO B.
    The TX_frequency is calculated including the TX_LO_frequency (Transvertor IF frequency) and the QO-100 transponder 
    offset 10489.500 => 2400.000 (808950000). 

    The mode from the VFO A (RX) is always copied to the VFO B (TX). Note: VFO A and VFO B can be reversed if so.

    Four different FT-818 statusses (or knobs) are interrogated by the sketch:

    1. PTT.  If the FT-818 is transmitting nothing happens and no display updates are made.
    2. CLAR. If the Clar is switched on, the calibartion procedure will start and the FT-818 VFO A RX frequency is set to the 
             Middle Beacon frequency. Pressing again the Clar knob will stop the calibration process. The new calibration offset 
             will be calculated and the RX_frequency will be set back to the frequency before starting the calibration. 
    3. LOCK  If de calculated uplink TX_frequency deviates from the current TX_frequency in VFO B, the display will show the 
             an up and down arrow symbol. When pressing the LOCK knob on the FT-818 the VFO B mode and frequency will be updated.
    4. SPLIT If the Split mode is switched off on the FT-818, the sketch will automatically switch it on again and program in both VFO A
             and B the Home frequency with Mode USB. 

    The calibration process works as follows:
    RX is set to LSB and the Middle Beacon frequency. Adjust to zero beat (you will hear a low tone +- 200Hz) and when switching
    to USB the tone should be the same +- 200Hz. You need to experiment a bit but when know know how it works this calibration can
    be done very fast. The program will switch 
    back to USB after pressing Clar again.
*/


