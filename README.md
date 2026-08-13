# C-GBoy
A Gameboy (not color!!!) emulador made from scratch with the C language and the SDL2 lib

this is my first attempt at making something involving emulation, I've tried to keep the IA use athe the minimum (only bug fixes and help understanding the documentation)

## Demos

### Alleyway: 
![alleyway](images/alleyway.gif)

### Tetris: 
![tetris](images/tetris.gif)

### The Legend of Zelda: Link's Awakening:
![zelda](images/zelda.gif)

## The Anatomy of the emulator

### CPU
The CPU is made from a list of 256 functions + 256 prefix CB functions, 7 8-bit registers plus 3 16-bit registers made from the union of 2 8-bit registers, timed cycles to run roms with precision and all of that with unit tests to assure the expected behavior

### RAM

