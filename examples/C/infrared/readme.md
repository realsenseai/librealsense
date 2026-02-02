# rs-infrared Sample

## Overview
This sample demonstrates how to use C API to stream infrared data from both left and right IR cameras and prints a simple text-based representation of the IR images, by breaking them into pixel regions and approximating the intensity levels.

## Key Points
* Demonstrates how to enumerate and differentiate between IR1 (left) and IR2 (right) streams
* Shows how to configure and start infrared streaming with Y8 format
* Provides text-based visualization of IR intensity levels
* Handles multiple IR streams from stereo cameras

## Expected Output
The program will display two text-based representations side by side:
- Left IR camera view
- Right IR camera view

Each view shows intensity levels using ASCII characters where brighter characters represent higher IR intensity.