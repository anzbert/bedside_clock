# Alarm Clock

## Background

The idea for this project started with my girlfriend buying a cheap ass bedside clock at Kmart. She quickly got annoyed with the room-illuminating brightness of the display at night, and there being no way to turn it down at all. On top of that, this horrible clock broke after a few weeks.

Amazing! I happily jumped at the opportunity to use my growing collection of electronic parts and micro controllers! My plan was roughly to build a clock with some kind of dimmable display, a slappable arcade button on top for snoozing and a speaker for the alarm. And I was going to use an Arduino Micro Pro (like Leonardo) for the job.

## Challenges

- Making a somewhat intuitive UI with only a 7-segment display and limited input capabilities
- Small scale MP3 sound playback with an Arduino
- Acurate time keeping with an Arduino that persists through power cuts

## Solution

### Materials

- 1x Craft Box from the Hardware Store (Bunnings)
- 1x Arduino Micro Pro
- 1x Micro USB Cable
- 1x 4 Ohm 3 Watt Speaker
- 1x Rotary Encoder with Push Button
- 1x 4-digit 7-segment display (controlled by TM1637)
- 1x DFPlayer Mini - Tiny SD Card MP3 player unit
- 1x persistent RTC Clock unit(DS1307)
- 1x SPST switch with LED
- 1x Sanwa 25mm Arcade Button
- 2x 220 Ohm Resistors for the Button LED and the data channel of the DFPlayer
- Cables / PCB / JST plugs & sockets
- Clear matte varnish spray

### Build

The craft box looked a lot nicer once I cut it right in the middle, which gave it a somewhat classy look?! Not sure if others would agree with that. 😅
Otherwise it only needed a few holes for buttons and encoder and a rectancular cut for the display. I also drilled a bunch of holes on the back for the sound of the speaker. Finally I sprayed some varnish on the box to protect the MDF from dirt and for a more finished look.

The electronic side was also fairly straightforward. I first assembled everything on a breadboard for testing and once it worked, soldered the buttons, resistors, JST connectors for the external units and speaker, as well as connectors for the Arduino to a PCB.

The trickiest bit was to get the DFPlayer Mini to work reliably without crackling sounds on idle, a problem that seems to be pretty common. In the end I found a soldering hack online (which I am struggling to find again), which involved soldering a jumper on the back of the unit.

### Code

I made use of some excellent libraries from the Arduino ecosystem, such as for the TM1637 display, the DFPlayer Mini and the DS1307 RTC unit. Each of these chips have several library options available.

This is the main functionality I implemented in software:

- The main menu is controlled with the rotary encoder, encoder push button and snooze button. The menu layout took a number of iterations to improve ease of use.
- All settings, including time and alarm time, persist when the device is unplugged. The RTC module remembers the time and the Arduino EEPROM stores the settings.
- The clock and alarm can be set with the encoder. The alarm is activated with the switch and the LED on the switch blinks 3 times to confirm that it is armed.
- There is an SD card in the unit with several folders and styles of music and alarm tones. Specific songs can be chosen and there is a random alarm sound function for each folder.
- The display brightess can be dimmed with the encoder. It can be switched on temporarily for 3 seconds when the snooze button is hit, and it turns on automatically when the alarm goes off.

## Conclusion

The clock works really quite well. The controla are intuitive enough and it's nice not to rely on your phone for everything, for once. The only thing I would replace is the RTC chip. The DS1307 is not quite precise enough and is slow by about 5 minutes per months. In hindsight, I should have used an RTC under the name of DS3231, which is supposed to be a lot more accurate.

I also wish there was a higher quality mp3 solution than the DFPlayer Mini. It does the job but it was limited in its serial based API and a pain to get to work with decent sound.

Overall I am happy with the result and if you are planning a similar project, I am happy to answer any question to save you a little bit of time on your build.

## Links

[Source Code](https://github.com/anzbert/bedside_clock)
