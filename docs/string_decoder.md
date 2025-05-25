---
layout: default
---

# String Decoder

The String Decoder window is a utility that allows you to decode Guild Wars' encoded strings, which can be useful for advanced users and developers working with the game's internal text system.

## Features

The String Decoder window provides two main functions:
- Decode strings by their encoded ID or hexadecimal representation
- Decode map names by their map ID

## Usage

### Decoding Encoded Strings

You can decode a string in two ways:
1. **By ID**:
   - Enter the hexadecimal ID in the "Encoded string id:" field
   - Press Enter or click "Decode"

2. **By Encoded Hexadecimal**:
   - Enter the encoded hexadecimal values in the "Encoded string:" field
   - You can use formats like `0x1234 0x5678` or `\x1234\x5678`
   - Press Enter or click "Decode"

### Decoding Map Names

To decode a map name:
1. Enter the map ID in the "Map ID:" field
2. Click "Decode Map Name"

## Results

When you decode a string, the result will be displayed in your chat window. This allows you to see the human-readable text that corresponds to the encoded string.

## Notes

- This is an advanced feature primarily intended for developers or users who need to work with Guild Wars' internal text encoding system
- Encoded strings in Guild Wars typically start with `0x` followed by hexadecimal values
- Not all encoded strings will decode to meaningful text, as some may be used for other purposes in the game
