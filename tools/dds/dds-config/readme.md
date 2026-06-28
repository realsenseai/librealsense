# dds-config tool

## Goal
This tool is used to configure Ethernet based devices.

## Description
The dds-config tool offers connection configuration information for connected devices and allows users to manage connection priorities. 
It enables the selection of the preferred connection method, whether Ethernet or USB, when both options are available, and provides the ability to set network parameters.

- For a guide how to set host MTU see [this](../../../doc/stepbystep/setting_mtu_on_host.md)

## Command Line Parameters
| Flag | Description |
|---|---|
|-h --help|Show command line help menu with all available options|
|--version|Displays version information|
|--usb-first|Prioritize USB and fall back to Ethernet after link timeout|
|--eth-first|Prioritize Ethernet and fall back to USB after link timeout|
|--dynamic-priority|Dynamically prioritize the last-working connection method (the default)|
|--serial-number <S/N>|Device serial-number to use, if more than one device is available|
|--reset|Hardware reset without making any changes|
|--link-timeout <milliseconds>|Milliseconds before --eth-first link times out and falls back to USB|
|--dhcp <on/off>|DHCP dynamic IP discovery 'on' or 'off'|
|--ip <address>|Device static IP address to use when DHCP is off|
|--mask <1.2.3.4>|Device static IP network mask to use when DHCP is off|
|--gateway <1.2.3.4>|Gateway to use when DHCP is off|
|--domain-id <0-232>|DDS Domain ID to use (default is 0), note:this is the device domain ID not librealsense domain ID|
|--sdk-domain-id <0-232>|DDS Domain the SDK should use (default is 0). Saved in the configuration file as the default for future SDK use|
|--transient-sdk-domain-id <0-232>|DDS Domain the SDK should use for this run only; NOT saved to the configuration file. Use to reach a device on a non-default domain (e.g. a camera that reverted to its golden DDS domain 0) without persisting a change. Mutually exclusive with `--sdk-domain-id`|
|--dhcp-timeout|Seconds before DHCP times out and falls back to a static IP|
|--mtu|Size per Ethernet packet. 500-9000 in 500 byte steps|
|--transmission-delay|Wait time after each packet is sent before sending next one. 0-144 in 3 microsecond steps|
|--no-reset|Do not hardware reset after changes are made|
|--golden| Show R/O golden values vs. current; mutually exclusive with any changes|
|--factory-reset|Reset settings back to the --golden values|

## Usage example
Prioritize Ethernet connection by using `rs-dds-config.exe --eth-first`:  
```
     Device: [USB] Intel RealSense D555 s/n 338522301774
     MAC address: 98:4f:ee:19:cc:49
     configured: 192.168.11.55 & 255.255.255.0 / 192.168.11.1
     DDS:
         domain ID: 0
         link: 1000 Mbps
         MTU, bytes: 9000
         timeout, ms: 10000
         priority: usb-first --> eth-first
     DHCP: OFF
         timeout, sec: 30
     transmission delay, us: 0

     Successfully changed
     Resetting device...
```
After running `rs-dds-config.exe` we can now see the device changed it's connection to DDS:
```
     Device: [DDS] Intel RealSense D555 s/n 338522301774
     MAC address: 98:4f:ee:19:cc:49
     configured: 192.168.11.55 & 255.255.255.0 / 192.168.11.1
     DDS:
         domain ID: 0
         link: 1000 Mbps
         MTU, bytes: 9000
         timeout, ms: 10000
         priority: eth-first
     DHCP: OFF
         timeout, sec: 30		 
     transmission delay, us: 0
```