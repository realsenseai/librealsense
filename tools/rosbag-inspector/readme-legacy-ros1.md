> **Legacy Documentation:** This document describes the `rs-rosbag-inspector` tool for legacy ROS1 `.bag` files. For inspecting `.db3` recordings, see the [current readme](./readme.md).

# ROS Bag Inspector (Legacy ROS1)



## Overview

The rosbag inspector allows you to view the topics and messages of `.bag` files.

> NOTE: This application will show topics for any `.bag` file, but does not support all message types; content of unknown messages will not be visible.


### Loading files

Files can be loaded by either:
1. Drag and drop `.bag` files to the application window
   ![realsense-rosbag-inspector-08_02_18-11_38_42](https://user-images.githubusercontent.com/22654243/35965851-cbc7ac3c-0cc4-11e8-8d9e-163c87b00482.gif)

2. Using the `File` menu
    ![realsense-rosbag-inspector-08_02_18-11_34_24](https://user-images.githubusercontent.com/22654243/35965602-18e47c1c-0cc4-11e8-8e08-677b5e876d85.gif)

### Inspecting Rosbag Files
#### Files List

The tool allows loading multiple files into the application. Loading the files will not load the entire file's data to memory, rather - only its metadata and indexing (Connection Infos):

![image](https://user-images.githubusercontent.com/22654243/35966006-47fd1e4a-0cc5-11e8-9c42-2f70ce63dc89.png)

The `X` button next to each file allows the removal of the file from the application, and will release its handle (The application opens the file for `read`).

#### File Details

The application displays a summary of the selected file's details at the top of the window:

![image](https://user-images.githubusercontent.com/22654243/35965911-f4e3833e-0cc4-11e8-8129-fceaf442ac21.png)

Below the details is a collapsable `Topics` tab which allows the inspection of the file's data:

![image](https://user-images.githubusercontent.com/22654243/35966960-11e5d0ce-0cc8-11e8-8ba8-371ec5ca51ec.png)

This will display all topics in the files, along with the number of messages for that topic, and the type of the messages for that topic (In case of multiple types, the first is displayed).

Clicking any topic will open it and display its messages:
![realsense-rosbag-inspector-08_02_18-11_52_04 1](https://user-images.githubusercontent.com/22654243/35966514-a99e8a7a-0cc6-11e8-9088-9afb31ec4383.gif)

In case a topic has too many messages, a `Show More` button will be available to allow more messages to be loaded into the application (and memory):


![realsense-rosbag-inspector-08_02_18-11_55_07](https://user-images.githubusercontent.com/22654243/35966651-263d4ddc-0cc7-11e8-9c66-d81ff4f91e40.gif)
