# PiXL - a Lua based pixel/chiptune engine

This is a very tiny Lua based game engine for creating Pixel / Chiptune games. It's great for game jams.

* all resources are handeled as strings
* games can be written with simple Lua commands
* true retro feeling with small resolution and chiptune sounds / music
* uses stock Lua 5.3 with all new features like integers and bit operations

## Limitations

* 256x240 resolution (NES screen size)
* 16 colors with a fixes palette
* 8x8 pixel sprites
* 8 audio channels with different waveform generators (square, triangle, sawtooth and noise)

## Goals

* **Ready-to-go package** just create a "game.lua" and you are ready to go
* **Portability** using SDL2 it will run on Windows, MacOS X, Linux and other platforms
* **Easy Distribution** on Windows you need only PiXL.exe / SDL2.dll and your game.lua file

## Documentation

On start PiXL will load and execute the file "game.lua". This behaviour can be overwritten with the **-file** parameter. See "Parameter" section below for more details.

## Callbacks

The following functions must be defined as global functions and will be called by PiXL.

* **init()** This function will be called only once for initialization.
* **update()** PiXL calls this function periodically, 30 times per second.

### Video Drawing Primitives

* **clear([color])** Clear the entire screen with the color. If no color is given black (0) is used.
* **point(color, x, y)** Draw a single pixel on the screen.
* **fill(color, x0, y0, x1, y1)** Fills a portion of the screen with the given color.
* **rect(color, x0, y0, x1, y1)** Draws a single pixel width rectangle on the screen.
* **line(color, x0, y0, x1, y1)** Draws a single pixel line from *x0*, *y0* to *x1*, *y1*.
* **circle(color, x, y, radius[, fill)** Draws a circle on the screen. If *fill* is set to *true* the circle will be filled.
* **translate([x, y])** Sets the translation for all pixels drawn. Returns the current translation values. Please note, that the mouse X, Y values will be translated as well.

## Highlevel Drawing

Sprites are represented as 64 character strings. Every character represents one pixel in the sprite. The colors are hexadecimal encoded (0-9, a-f, A-F).
Other characters will be interpreted as color 0.

* **sprite(x, y, data[, transparent])** Draws the given sprite string on *x*, *y*. If *transparent* color is given, this color will be not drawn.
* **print(color, x, y, string)** Prints the given *string* to *x*, *y* on screen.

## Audio (MML) Routines

To create sounds PiXL uses a MML (Music Macro Language) to represent the song/sound effect to played. There are 8 channels (0-7) available for playback.
MML syntax:
* **C**,**D**,**E**,**F**,**G**,**A**,**B** The letters correspond to the musical pitches and cause the corresponding note to be played. If *+*,*#* is appended the note will be sharp, if *-* is appended the note will be flat. If a length like (1, 2, 4, 8, 16, 32) is appended the note will be played with that length. You have to specify the length as a fraction of a whole note. If *.* is appended to the length, the length will be extended. If *&* is appended the note will be "legato" and bound to the next note.
* **Tn** Sets the tempo in quarter notes per minute.
* **On** Selects the octave the instrument will play in.
* **<**, **>** Step down or step up an octave.
* **Ln** Sets the default length for notes which have no length specified.
* **P**, **R** Pauses the song. You can specify lengths like on notes.
* **W1** Selects pulse square waveform (12.5%).
* **W2** Selects pulse square waveform (25%).
* **W5** Selects pulse square waveform (50%).
* **WT** Selects triangle waveform.
* **WS** Selects sawtooth waveform.
* **WN** Selects noise waveform.

Example:

* **W2 T120 L4 CDEFGAB>C** plays a full octave from C-C. All notes are set to quarter notes and the waveform will be 25% square waves.
* **t80l16>erc-cde32d32c<bara>cerdcc-r8cdrercr<arar4>drfargfer8cef32e32dcc-r8cd<a->e<a->cr<arl4aecdc-c<ag+b>ecdc-c8e8ag+2,l16brg+abb+32b32ag+e8eab+rbag+eg+ab8b+8a8e8e<<ab>cd>frab+rbagr8egrfeg+eg+abrb+rare** simple Tetris A theme


Lua functions:

* **play(channel, string[, looping])** Plays the given MML *string* on the given *channel*. If *looping* is set the MML-string will be looped.
* **stop(channel)** Stops the audio generation on the given *channel*.
* **pause(paused)** Stops the entire audio mixing if *paused* is *true*. This is could be useful if you want to setup a song to be played on multiple channels.

## Input

You can use up to 8 controllers for PiXL. The buttons available for checking are *A*,*B*,*X*,*Y*,*LEFT*,*RIGHT*,*UP*,*DOWN* and *START*.

* **btn(button[, player])** Returns true if the given *button* for *player* is pressed.
* **btnp(button[, player])** Returns true if the given *button* for *player* was pressed since the last frame. This can be used to check inputs for menus.
* **mouse()** Returns the mouse position.

## Misc Functions

* **clipboard([text])** If *text* is given, the text will be set to the clipboard. The function will return the current clipboard content.
* **randomseed([seed])** If *seed* is given the random seed is set to the value. Returns the current random seed value.
* **random([low[, high]])** Returns 0..1 if no value is given. Returns 1..x when only one parameter was given. Returns low..high when both arguments are given. The function behaves similar to Lua's math.random(). This random number generator should be used when you need reproduceable random values across different platforms. Lua's random functions utilize C rand() which behaves not identical on different platforms.
* **quit()** Quits the game's main loop and closes the window.
* **title(title)** Sets the title of the window.

## Parameters for PiXL executable

* **-video driver** Defines which video driver should be used by PiXL (and SDL2). Please check https://wiki.libsdl.org/SDL_HINT_RENDER_DRIVER?highlight=%28%5CbCategoryDefine%5Cb%29%7C%28CategoryHints%29 for possible values.
* **-audio driver** Defines the audio driver which will be used by PiXL (and SDL2).
* **-nosound** Disables sound completely.
* **-file filename** Overrides the Lua file which will be loaded on startup.

## Hot Keys

* **F12** Toggle fullscreen mode.
* **ESC** Quits PiXL.
