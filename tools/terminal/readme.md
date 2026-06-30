# rs-terminal Tool

## Goal
`rs-terminal` tool can be used to send debug commands to camera firmware. The tool can operate in two modes - if you have access to the specification of debug commands for the exact firmware you are using (provided in XML form) the tool will provide help and auto-completion based on the information within the XML.  
If you don't have access to the spec, you can still operate the tool in HEX mode, sending binary commands directly. This mode of operation is intended to allow RealSense Support team to troubleshoot the hardware remotely by providing the user the exact binary commands to input.

## Usage
After installing `librealsense` run `rs-terminal` to launch the tool in HEX mode.

* `--version` - print the tool's version.
* `-l <filename>` - load commands specification file.
* `-d <number>` - choose device by device number
* `-n <serial>` - choose device by serial number
* `-a` - broadcast to all connected devices
* `-c` - execute a sequence of FW commands listed in a file. (The file must be ANSI/UTF-8)
* `-v` - turn on verbose logs
* `--debug` - turn on SDK logs.

Example:  
>`rs-terminal -l cmd.xml -c sequence.txt`

Run the tool, load the FW command specifications/mapping file and then iterate over and execute the FW commands listed in `sequence.txt` file

Possible content for `sequence.txt`:
```
gvd
oemr
oemw /usr/tmp/tbd.bin
gvd
```

> Note use 'exit' command to exit the tool