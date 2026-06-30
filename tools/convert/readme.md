# rs-convert Tool

## Goal

Console app for converting RealSense recording files to various formats (currently supported: PNG, RAW, CSV, PLY, BIN, DB3).

This tool can also convert legacy ROS1 `.bag` recordings to the current `.db3` (ROS2 rosbag2) format.

## Command Line Parameters

|Flag   |Description   |Default|
|---|---|---|
|`-i <file>`|Input recording file (`.bag` or `.db3`)|(required)|
|`-D <db3-path>`|Convert legacy `.bag` to `.db3` format||
|`-p <png-path>`|Convert to PNG, set output path to png-path||
|`-v <csv-path>`|Convert to CSV, set output path to csv-path, supported formats: depth, color, imu, pose||
|`-V <csv-path>`|Convert to 3D CSV, set output path to csv-path, supported formats: depth||
|`-r <raw-path>`|Convert to RAW, set output path to raw-path||
|`-l <ply-path>`|Convert to PLY, set output path to ply-path||
|`-b <bin-path>`|Convert to BIN (depth matrix), set output path to bin-path||
|`-T`|Convert to text (frame dump) output to standard out||
|`-d`|Convert depth frames only||
|`-c`|Convert color frames only||

## Usage

### Converting legacy `.bag` to `.db3`

```bash
rs-convert -i recording.bag -D recording.db3
```

### Extracting frames to other formats

**Example**: If you have `1.db3` recorded from the Viewer or from API, launch the command line and enter: `rs-convert -v test -i 1.db3`. This will generate one `.csv` file for each frame inside the recording.

Several converters can be used simultaneously, e.g.:
`rs-convert -i some.db3 -p some_dir/some_file_prefix -r some_another_dir/some_another_file_prefix`
