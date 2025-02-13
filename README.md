# ORCΛ

Orca is an [esoteric programming language](https://en.wikipedia.org/wiki/Esoteric_programming_language) and live editor designed to quickly create procedural sequencers. Every letter of the alphabet is an operation, lowercase letters execute on `*bang*`, and uppercase letters execute each frame.

This is the C implementation of the [ORCΛ](https://wiki.xxiivv.com/site/orca.html) language and terminal livecoding environment. It's designed to be power efficient. It can handle large files, even if your terminal is small.

Orca is not a synthesizer, but a flexible livecoding environment capable of sending MIDI, OSC, and UDP to your audio/visual interfaces like Ableton, Renoise, VCV Rack, or SuperCollider.

<img src='https://raw.githubusercontent.com/wiki/hundredrabbits/Orca-c/PREVIEW.jpg' width='600'/>

| Main git repo | GitHub mirror |
| ------------- | ------------- |
| [git.sr.ht/~rabbits/orca](https://git.sr.ht/~rabbits/orca) | [github.com/hundredrabbits/Orca-c](https://github.com/hundredrabbits/Orca-c) |

# About this fork

I'll try to add new operators based on my needs. I'm not a professional programmer, just a hobbyist, so most of the code here is generated by ChatGPT, based on 'painfully' extensive directions I provided. I've tried to include comments as much as possible to remember what I did and why, but I'm fairly certain it's not the most elegant code that adheres to proper standards. You should consider this fork to be 'somewhat' functional. Currently I'm focused on Orca-c only, since it's the main variant I use (on my Raspberry Pi Zero 2W, connected to my iPad Pro 11", accessed through SSH, and outputting notes through USB, acting as a MIDI Gadget).

## Scale Operator (`^`):
Outputs note and octave based on the provided root note, scale and degree.

| Operator | Octave | RootNote | Scale | Degree |
|:------:|:--------:|:--------:|:-----:|:------:|
|   ^    |    O     |    R     |   S   |   D    |

Example:
- ^3C02
- Input '3': Octave 3 (optional)
- Input 'C': Root note C
- Input '0': Minor scale
- Input '2': 3rd degree
- Output: '3d' (D#3)

Example without octave:
- ^.C02
- Input '.': No octave
- Input 'C': Root note C
- Input '0': Minor scale  
- Input '2': 3rd degree
- Output: 'd' (D#)


| Scale | Type |
|:-----:|------|
|   0   | Minor Scale |
|   1   | Major Scale |
|   2   | Minor Pentatonic Scale |
|   3   | Major Pentatonic Scale |
|   4   | Blues Minor Scale |
|   5   | Blues Major Scale |
|   6   | Phrygian Scale |
|   7   | Lydian Scale |
|   8   | Locrian Scale |
|   9   | Super Locrian Scale |
|   a   | Neapolitan Minor Scale |
|   b   | Neapolitan Major Scale |
|   c   | Hex Phrygian Scale |
|   d   | Whole Scale |
|   e   | Diminished Scale |
|   f   | Pelog Scale |
|   g   | Spanish Scale |
|   h   | Bhairav Scale |
|   i   | Ahirbhairav Scale |
|   j   | Augmented2 Scale |
|   k   | Purvi Scale |
|   l   | Marva Scale |
|   m   | Enigmatic Scale |
|   n   | Scriabin Scale |
|   o   | Indian Scale |

Scales and offset values taken from
https://patchstorage.com/author/amiika/


## Midipoly Operator (`|`):
Extension of Midi operator with 3 input notes. The first note processed is always considered the root note. Every following note is assumed to be "higher" than the previous one. (So no chord inversions sorry)

| Midipoly | Channel | Octave | Note 1 | Note 2 | Note 3 | Velocity | Duration |
|:---------:|:-------:|:------:|:------:|:------:|:------:|:--------:|:--------:|
|     \|     |    C    |    O   |   N1   |   N2   |   N3   |    V     |    D     |

Example 1:
- |03CEGff
- Plays C3 E3 G3

Example 2:
- |03GECff
- Plays G3 E3 C4

Example 3:
- |03CCCff
- Plays C3 C4 C5

## Midichord Operator (`=`)
The Midichord operator outputs MIDI notes to form common chord types based on a root note. It supports various chord types from basic triads to extended chords, including jazz voicings, making it useful for harmonic progressions and complex chord sequences. (This replaces OSC operator, as I neve ruse it)

| Operator | Channel | Octave | Root Note | Chord Type | Velocity | Duration |
|:--------:|:-------:|:------:|:---------:|:----------:|:--------:|:--------:|
|    =     |    C    |   O    |     R     |     T      |    V     |    D     |

### Example
- `=13C1ff`
- Plays C major chord (C-E-G) on channel 1, octave 3, full velocity and duration

### Available Chord Types

| Value | Chord Type |
|:-----:|------------|
| 0 | Minor |
| 1 | Major |
| 2 | Minor 7 |
| 3 | Major 7 |
| 4 | Minor 9 |
| 5 | Major 9 |
| 6 | Dominant 7 |
| 7 | Minor 6 |
| 8 | Major 6 |
| 9 | Sus2 |
| a | Sus4 |
| b | Minor add9 |
| c | Major add9 |
| d | Augmented |
| e | Augmented 7 |
| f | Minor Major 7 |
| g | Diminished |
| h | Diminished 7 |
| i | Half Diminished |
| j | Minor 6/9 |
| k | Major 6/9 |
| l | Minor First Inversion |
| m | Major First Inversion |
| n | Minor Second Inversion |
| o | Major Second Inversion |
| p | Minor 7b5 |
| q | Minor 11 |
| r | Dominant 9 |
| s | Dominant 7b9 |
| t | Dominant 7#9 |
| u | Major 7#11 |
| v | Minor add11 |

## Random Unique Operator (`$`):
Requires bang. Similar to the Random Operator, but designed to avoid producing identical outputs on consecutive bangs in a creative (in other words, "hacky") manner.


## MIDI Arpeggiator Operator (`&`):
The MIDI Arpeggiator operator (`&`) is designed to generate arpeggiated sequences from a set of input notes across specified octave ranges. It supports dynamic direction control, allowing sequences to ascend, descend, or both based on the input parameters. It only sends MIDI when it's "banged". This bears the possibility of coming up with quite unique patterns since it decouples when a note is "selected" and when a note is "played".

| Parameter     | Description                                               |
|---------------|-----------------------------------------------------------|
| Arp Pattern   | Selects the arpeggiation pattern.                         |
| Note to Play  | Determines the current note in the arpeggio pattern.      |
| Octave Range  | Sets the range and direction of octaves for arpeggiation. |
| Operator      | "&"                                                       |
| Channel       | MIDI channel for output.                                  |
| Base Octave   | Starting octave for the first note in the pattern.        |
| Note 1        | First note in the arpeggio sequence.                      |
| Note 2        | Second note in the arpeggio sequence.                     |
| Note 3        | Third note in the arpeggio sequence.                      |
| Velocity      | MIDI velocity of the played notes.                        |
| Duration      | Length of each note in the sequence.                      |

### Inputs

| Arp Pattern | Note to Play | Octave Range | Operator | Channel | Base Octave | Note 1 | Note 2 | Note 3 | Velocity | Duration |
|:-----------:|:------------:|:------------:|:--------:|:-------:|:-----------:|:------:|:------:|:------:|:--------:|:--------:|
|      P      |       N      |       R      |    &     |    C    |      O      |   N1   |   N2   |   N3   |    V     |    D     |

- `P`: Arpeggio Pattern Index (0-9 for predefined patterns)
- `N`: Note to play (based on selected arpeggio pattern's offset)
- `R`: Octave range and direction (1-2-3-4 for ascending monophonic, 5-6-7-8 for ascending polyphonic, a-b-c-d for descending monophonic, e-f-g-h for descending polyphonic)
- `C`, `O`, `N1`, `N2`, `N3`, `V`, `D`: Similar to the Midipoly operator

### Example

- `02a&04CEGf3`: This example uses arpeggio pattern `2`, plays the first note in the pattern, spans across 1 octave in a descending direction, on channel `0`, starting from octave `4`, with the notes `C`, `E`, `G`, velocity `f`, and duration `3`.

This operator generates a MIDI arpeggiated sequence based on the input parameters, allowing for intricate rhythmic patterns to be easily created and manipulated live. Adjust the `Arp Pattern`, `Note to Play`, and `Octave Range` to explore different musical ideas.

### Arpeggio Patterns

Each pattern is defined by a sequence of steps that dictate the order of arpeggiation. Below are the currently available patterns and their descriptions:

To use a pattern, select its index as the `Arp Pattern` input for the MIDI Arpeggiator operator (`&`). The `Note to Play` input determines which step in the selected pattern to play, allowing the sequence to progress. Suggestions: Connect `Note to Play` to a Clock operator, or a Track operator, or a Random Unique operator for complex and/or unexpected patterns.

| Pattern Index | Notes Sequence       | Description               |
|---------------|----------------------|---------------------------|
| 0             | 1, 2, 3              | Up                        |
| 1             | 3, 2, 1              | Down                      |
| 2             | 1, 3, 2              | Converge up               |
| 3             | 3, 1, 2              | Converge down             |
| 4             | 2, 1, 3              | Diverge up                |
| 5             | 2, 3, 1              | Diverge down              |
| 6             | 1, 2, 3, 2           | Up bounce triangle        |
| 7             | 3, 2, 1, 2           | Down bounce triangle      |
| 8             | 1, 2, 3, 3, 2, 1     | Up bounce sine            |
| 9             | 3, 2, 1, 1, 2, 3     | Down bounce sine          |
| a             | 1, 2, 3, 0           | Up with rest              |
| b             | 3, 2, 1, 0           | Down with rest            |
| c             | 1, 3, 2, 0           | Converge up with rest     |
| d             | 3, 1, 2, 0           | Converge down with rest   |
| e             | 2, 1, 3, 0           | Diverge up with rest      |
| f             | 2, 3, 1, 0           | Diverge down with rest    |
| g             | 1, 2, 3, 2, 0        | Up bounce triangle with rest |
| h             | 3, 2, 1, 2, 0        | Down bounce triangle with rest |
| i             | 1, 0, 2, 3, 0        | Riff with rests           |
| j             | 1, 0, 3, 2, 0        | Alternate riff with rests |
| k             | 1, 2, 0, 3, 0        | Riff variation with rests |
| l             | 1, 3, 0, 2, 0        | Another riff variation with rests |
| m             | 1, 2, 0, 1, 3        | Riff with internal rest   |
| n             | 1, 3, 0, 1, 2        | Riff alternate with internal rest |
| o             | 1, 2, 0, 1, 3, 0     | Extended riff with rests  |
| p             | 1, 0, 2, 1, 0, 3     | Complex riff with rests   |
| q             | 1, 0, 3, 1, 0, 2     | Complex alternate riff with rests |


*IMPORTANT: Arp patterns `0` to `9` are most likely to be permanent. You can come up with complex sequences using only them and banging the operator in various timings. But the patterns `a` and above are just some experimental combinations and they're likely to change in future updates.


## Bouncer (`;`) (A rudimentary LFO interpretation)

The bouncer operator creates smooth transitions between two values using various waveform patterns. Useful for creating continuous value changes and modulations. Each waveform has a resolution of 128 steps (some repeating so not super precise), Rate input basically skips every Nth step to make it "scan" through the waveform faster. (e.g: 2 skips every other step, 5 skip every 4 step etc).

### Inputs

| Start Value | End Value | Operator | Rate | Shape |
|:------:|:------:|:--------:|:------:|:------:|
|   A    |   B    |    ;     |   R    |   S    |

- `A`: Start value (0-z)
- `B`: End value (0-z)
- `R`: Rate - Speed of transition (0-z, higher = faster)
- `S`: Shape (0-7) - Selects waveform pattern

### Example

- `;3a2C`
- Transitions between values 3 and a using waveform pattern 2 (Sine) at speed C

#### Waveform Shapes

| Value | Pattern           | Description                                    |
|-------|------------------|------------------------------------------------|
| 0     | Triangle         | Linear up then down                            |
| 1     | Inv. Triangle    | Linear down then up                           |
| 2     | Sine            | Smooth curved transition up then down          |
| 3     | Inv. Sine       | Smooth curved transition down then up          |
| 4     | Square          | Instant switch between min and max            |
| 5     | Inv. Square     | Instant switch between max and min            |
| 6     | Saw             | Linear up, instant down                       |
| 7     | Inv. Saw        | Linear down, instant up                       |

Output value cycles through the chosen waveform pattern between start and end values at the specified rate. Perfect for creating LFO-like modulations or smooth parameter changes.

## MIDI CC Operator (`!`) (Refactored)
The MIDI CC operator sends MIDI Control Change messages with optional interpolation between values. The control number is specified in hexadecimal (00-FF) using two inputs for high and low nibbles. There's also a "Lerp" input, which compensates a little bit for ORCA's low resolution (base36) output, which "sort of" makes changes slightly smoother. (e.g: when combined witn the Bouncer operator, it can act as a rudimentary LFO for controlling parameters on your synth)

### Inputs

| Operator | Channel | Control High | Control Low | Value | Lerp |
|:--------:|:-------:|:-----------:|:-----------:|:-----:|:----:|
|    !     |    C    |     Ch      |     Cl      |   V   |  L   |

- `C`: MIDI channel (0-F)
- `Ch`: High nibble of control number in hex (0-F)
- `Cl`: Low nibble of control number in hex (0-F) 
- `V`: Target value (0-z maps to 0-127)
- `L`: Interpolation amount (0-z)

### Example

- `!34ACf`
- Sends MIDI CC #4A (74) on channel 3 with value C, rate f
- Rate > 0 enables interpolation between current and target value
- Higher Lerp value means more steps of interpolation (which can result in slower but higher resolution changes)
- Rate of 0 or `.` disables interpolation

Uses a 128-step resolution for interpolation. The rate determines how many steps to skip per tick - higher rates mean faster but coarser transitions, lower rates mean smoother but slower transitions.


## Teleport Operator (`X`) (Refactored)
Teleport operator can teleport more than 1 glyph now. There's a new input to the left of vertical and horizontal offset inputs, which sets the "length of glyphs" to teleport (works similar to how Konkat or Query operators fetch multiple cells).

Quick Tip: You can use this new 'multiglyph' Teleport along with the Scale operator and teleport the result (which can now include an octave value as well) to a midi operator!

| Length | Vertical Offset | Horizontal Offset | Operator |
|:--------:|:-------------:|:-----------------:|:-----------:|
|    L     |       V       |         H         |      X      |


## Query Operator (`Q`) (Refactored)
This is a very minor change where the "length" parameter is moved to the left of offset parameters to match the layout of extended Teleport Operator.
(Previously the input order was Vertical Offset, Horizontal Offset, Length. Now it's Length, Vertical Offset, Horizontal Offset)

| Length | Vertical Offset | Horizontal Offset | Operator |
|:--------:|:-------------:|:-----------------:|:-----------:|
|    L     |       V       |         H         |      Q      |
