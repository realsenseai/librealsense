#!/bin/bash
# Create symbolic links for video nodes and for metadata nodes - /dev/video-rs-[<sensor>|<sensor>-md]-[camera-index]
# This script intended for mipi devices on Jetson and IPU6.
# After running this script in enumeration mode, it will create links as follow for example:
# Example of the output:
#
# Jetson:
# $ ./rs-enum.sh 
# Bus	  Camera	Sensor	Node Type	Video Node	RS Link
# mipi	0	      depth	  Streaming	/dev/video0	/dev/video-rs-depth-0
# mipi	0	      depth	  Metadata	/dev/video1	/dev/video-rs-depth-md-0
# mipi	0	      color	  Streaming	/dev/video2	/dev/video-rs-color-0
# mipi	0	      color	  Metadata	/dev/video3	/dev/video-rs-color-md-0
# mipi	0	      ir	  Streaming	/dev/video4	/dev/video-rs-ir-0
# mipi	0	      ir	  Metadata	/dev/video5	/dev/video-rs-ir-md-0
# mipi	0	      imu	  Streaming	/dev/video6	/dev/video-rs-imu-0
#
# Alderlake:
#$ ./rs-enum.sh 
#Bus	Camera	Sensor	Node Type	Video Node	RS Link
# ipu6	0	depth	  Streaming	/dev/video4 	  /dev/video-rs-depth-0
# ipu6	0	depth	  Metadata	/dev/video5	    /dev/video-rs-depth-md-0
# ipu6	0	ir	    Streaming	/dev/video8	    /dev/video-rs-ir-0
# ipu6	0	imu	    Streaming	/dev/video9	    /dev/video-rs-imu-0
# ipu6	0	color	  Streaming	/dev/video6	    /dev/video-rs-color-0
# ipu6	0	color	  Metadata	/dev/video7	    /dev/video-rs-color-md-0
# i2c 	0	d4xx   	Firmware 	/dev/d4xx-dfu-a	/dev/d4xx-dfu-0
# ipu6	2	depth	  Streaming	/dev/video36	  /dev/video-rs-depth-2
# ipu6	2	depth	  Metadata	/dev/video37  	/dev/video-rs-depth-md-2
# ipu6	2	ir	    Streaming	/dev/video40	  /dev/video-rs-ir-2
# ipu6	2	imu	    Streaming	/dev/video41	  /dev/video-rs-imu-2
# ipu6	2	color 	Streaming	/dev/video38	  /dev/video-rs-color-2
# ipu6	2	color 	Metadata	/dev/video39	  /dev/video-rs-color-md-2
# i2c 	2	d4xx   	Firmware 	/dev/d4xx-dfu-c	/dev/d4xx-dfu-2

# Dependency: v4l-utils
v4l2_util=$(which v4l2-ctl)
media_util=$(which media-ctl)
if [ -z ${v4l2_util} ]; then
  echo "v4l2-ctl not found, install with: sudo apt install v4l-utils"
  exit 1
fi
metadata_enabled=1
#
# parse command line parameters
# for '-i' parameter, print links only
while [[ $# -gt 0 ]]; do
  case $1 in
    -i|--info)
      info=1
      shift
    ;;
    -q|--quiet)
      quiet=1
      shift
    ;;
    -m|--mux)
      shift
      mux_param=$1
      shift
    ;;
    -n|--no-metadata)
      metadata_enabled=0
      shift
    ;;
    *)
      info=0
      quiet=0
      shift
    ;;
    esac
done
#set -x
if [[ $info -eq 0 ]]; then
  if [ "$(id -u)" -ne 0 ]; then
          echo "Please run as root." >&2
          exit 1
  fi
fi

mux_list=${mux_param:-'a b c d e f g h'}

declare -A camera_idx=( [a]=0 [b]=1 [c]=2 [d]=3 [e]=4 [f]=5 [g]=6 [h]=7)
declare -A d4xx_vc_named=([depth]=1 [rgb]=3 [ir]=5 [imu]=7)
declare -A camera_names=( [depth]=depth [rgb]=color [ir]=ir [imu]=imu )

camera_vid=("depth" "depth-md" "color" "color-md" "ir" "ir-md" "imu")

depth_dev_counter=0
color_dev_counter=0
ir_dev_counter=0
imu_dev_counter=0
camera_i2c_addrs=()  # Track camera I2C addresses in discovery order for DFU matching

# Helper function: detect RS devices
# Searches v4l2-ctl output for RealSense DS5 mux devices on Tegra platforms
# Returns lines matching the pattern "vi-output, DS5 mux <I2C-address>"
# Example output: "vi-output, DS5 mux 30-001a (platform:tegra-capture-vi:0):"
detect_rs_devices() {
  ${v4l2_util} --list-devices | grep -E "vi-output, DS5 mux [0-9]+-[0-9a-fA-F]+"
}

# Helper function: extract I2C address from RS device line
# Parses the I2C bus address from a DS5 mux device line
# Input example: "vi-output, DS5 mux 30-001a (platform:tegra-capture-vi:0):"
# Output example: "30-001a"
extract_i2c_address() {
  local rs_line="$1"
  echo "${rs_line}" | grep -oE '[0-9]+-[0-9a-fA-F]+' | head -1
}

# Helper function: get video devices for a specific RS device
# Extracts all /dev/videoN devices associated with a specific I2C address
# Uses awk to parse v4l2-ctl output and find video devices under the matching mux
# Input example: "30-001a"
# Output example: "/dev/video0\n/dev/video1\n/dev/video2\n/dev/video3\n/dev/video4\n/dev/video5\n/dev/video6"
get_video_devices_for_rs() {
  local i2c_pattern="$1"
  ${v4l2_util} --list-devices | awk -v pattern="${i2c_pattern}" '
    BEGIN { found=0 }
    /vi-output, DS5 mux/ && $0 ~ pattern { 
      found=1 
      next
    }
    found && /^[[:space:]]*\/dev\/video/ { 
      gsub(/^[[:space:]]+/, "")
      print $1 
    }
    found && /^[[:alpha:]]/ && !/^[[:space:]]/ { 
      found=0 
    }
  '
}

# Helper function: get ordered stream types for cameras from the media controller graph.
# Queries media-ctl --print-dot to find D4XX entity names (e.g. "D4XX depth", "D4XX rgb")
# and their port connections to the DS5 mux, returning types in port order.
# Entity names are set by the driver and unambiguously identify the stream type,
# unlike pixel format heuristics which can be ambiguous (e.g. GREY is used by both IR and safety).
# Handles multi-cam on single deserializer: cameras share the same I2C bus but have
# different device addresses (e.g. 9-001a and 9-002a). Returns types for ALL cameras
# on the bus, sorted by I2C address.
# Input: I2C address (e.g. "9-001a")
# Sets global: stream_types_result (space-separated types), camera_i2c_addrs (appended)
get_stream_types() {
  local i2c_addr="$1"
  local i2c_bus="${i2c_addr%%-*}"  # Extract bus number (e.g. "9" from "9-001a")
  stream_types_result=""

  if [ -z "${media_util}" ]; then
    echo "Error: media-ctl not found, install with: sudo apt install v4l-utils" >&2
    return 1
  fi

  # Find the Tegra media device
  local mdev=$(${v4l2_util} --list-devices | grep -A1 -i tegra | grep '/dev/media' | head -1 | tr -d '[:space:]')
  if [ -z "${mdev}" ]; then
    echo "Error: No Tegra media device found" >&2
    return 1
  fi

  local dot=$(${media_util} -d ${mdev} --print-dot 2>/dev/null | grep -v dashed)
  if [ -z "${dot}" ]; then
    echo "Error: Failed to read media-ctl graph from ${mdev}" >&2
    return 1
  fi

  # Find all unique I2C addresses with D4XX entities on this bus.
  # Multi-cam on single deserializer: multiple addresses share the same bus (e.g. 9-001a, 9-002a)
  local all_addrs=$(echo "${dot}" | grep -oP "D4XX \w+ \K${i2c_bus}-[0-9a-fA-F]+" | sort -u)
  if [ -z "${all_addrs}" ]; then
    echo "Error: No D4XX entities found on I2C bus ${i2c_bus}" >&2
    return 1
  fi

  # Record I2C addresses in order for DFU device matching
  camera_i2c_addrs+=(${all_addrs})

  # For each camera (I2C address), extract stream types in port order
  for addr in ${all_addrs}; do
    unset port_to_type
    declare -A port_to_type

    while IFS= read -r line; do
      local entity_node=$(echo "${line}" | awk '{print $1}')
      local type=$(echo "${line}" | grep -oP 'D4XX \K\w+')
      [[ -z "${entity_node}" || -z "${type}" ]] && continue

      # Find the connection: <entity>:port0 -> <mux>:portN, extract target port number
      local port=$(echo "${dot}" | grep -F "${entity_node}:port0 ->" | head -1 | grep -oP -- '-> \S+:port\K[0-9]+')

      if [[ -n "${port}" ]]; then
        # Map entity type to link name (rgb -> color) using camera_names
        if [[ -n "${camera_names[${type}]+_}" ]]; then
          port_to_type[${port}]="${camera_names[${type}]}"
        else
          port_to_type[${port}]="${type}"
        fi
      fi
    done < <(echo "${dot}" | grep "D4XX .* ${addr}")

    # Append types sorted by port number
    for port in $(echo "${!port_to_type[@]}" | tr ' ' '\n' | sort -n); do
      stream_types_result+="${port_to_type[${port}]} "
    done
  done
}

# Helper function: get the number of devices of specific type identified
get_dev_num() {
  local DEVICE="$1"
  if [[ "$DEVICE" == "depth" ]]; then
    echo $depth_dev_counter
  elif [[ "$DEVICE" == "color" ]]; then
    echo $color_dev_counter
  elif [[ "$DEVICE" == "ir" ]]; then
    echo $ir_dev_counter
  elif [[ "$DEVICE" == "imu" ]]; then
    echo $imu_dev_counter
  fi
}

# Helper function: increment the number of devices of specific type identified
increment_dev_num() {
  local DEVICE="$1"
  if [[ "$DEVICE" == "depth" ]]; then
    depth_dev_counter=$((depth_dev_counter+1))
  elif [[ "$DEVICE" == "color" ]]; then
    color_dev_counter=$((color_dev_counter+1))
  elif [[ "$DEVICE" == "ir" ]]; then
    ir_dev_counter=$((ir_dev_counter+1))
  elif [[ "$DEVICE" == "imu" ]]; then
    imu_dev_counter=$((imu_dev_counter+1))
  fi
}

# Helper function: create video device link
# Creates a symbolic link from /dev/videoN to a standardized RS device name
# Handles both info display and actual link creation based on global flags
# Input example: create_video_link "/dev/video0" "/dev/video-rs-depth-0" "mipi" "0" "depth" "Streaming"
# Creates: /dev/video-rs-depth-0 -> /dev/video0
create_video_link() {
  local vid="$1"
  local dev_ln="$2"
  local bus="$3"
  local cam_id="$4"
  local sensor_name="$5"
  local type="$6"
  
  echo "DEBUG: Creating ${type} link: ${vid} -> ${dev_ln}"
  [[ $quiet -eq 0 ]] && printf '%s\t%d\t%s\t%s\t%s\t%s\n' "${bus}" "${cam_id}" "${sensor_name}" "${type}" "${vid}" "${dev_ln}"

  if [[ $info -eq 0 ]]; then
    [[ -e "$dev_ln" ]] && unlink "$dev_ln"
    ln -s "$vid" "$dev_ln"
  fi
}

# Helper function: process video devices for a RS camera
# Processes all video devices for one RealSense camera, determining device types.
# Uses media-ctl entity names to identify stream types (depth/color/ir/imu) reliably.
# Maps devices to sensors based on driver names (tegra-video=streaming, tegra-embedded=metadata).
# On Tegra, the media graph doesn't expose per-stream video node mappings — individual
# /dev/videoN nodes aren't linked to specific D4XX entities. The driver creates video nodes
# in deterministic DS5 mux port order, so positional assignment by streaming node index works.
# Expected device order: depth, depth-md, color, color-md, ir, ir-md, imu
# Input: "$vid_devices" "$i2c_addr"
process_rs_video_devices() {
  local vid_devices="$1"
  local i2c_addr="$2"

  # Convert video devices to array
  local vid_dev_arr=(${vid_devices})
  echo "DEBUG: Video device array: ${vid_dev_arr[*]}"

  # Get ordered stream types from media-ctl for this camera
  # Called directly (not in subshell) so global camera_i2c_addrs is populated for DFU matching
  get_stream_types "${i2c_addr}"
  if [[ -z "${stream_types_result}" ]]; then
    echo "Error: Could not discover stream types for ${i2c_addr}"
    return 1
  fi
  local stream_types=(${stream_types_result})
  local stream_type_idx=0
  echo "DEBUG: Stream types from media-ctl: ${stream_types[*]}"

  # Process each video device in the expected order
  local bus="mipi"
  local sensor_name=""
  local sensor_idx=0

  for vid in "${vid_dev_arr[@]}"; do
    [[ ! -c "${vid}" ]] && echo "DEBUG: Video device ${vid} not found, skipping" && continue

    # Check if this is a valid tegra video device
    local dev_name=$(${v4l2_util} -d ${vid} -D 2>/dev/null | grep 'Driver name' | head -n1 | awk -F' : ' '{print $2}')
    echo "DEBUG: Video device ${vid} driver name: ${dev_name}"
    # Handle streaming devices
    if [ "${dev_name}" = "tegra-video" ]; then
      if [[ ${stream_type_idx} -ge ${#stream_types[@]} ]]; then
        echo "DEBUG: More streaming nodes than expected stream types, skipping ${vid}"
        continue
      fi
      sensor_name="${stream_types[${stream_type_idx}]}"
      stream_type_idx=$((stream_type_idx+1))
      echo "DEBUG: Stream type for ${vid}: ${sensor_name}"
      sensor_idx=$(get_dev_num $sensor_name)
      local dev_ln="/dev/video-rs-${sensor_name}-${sensor_idx}"
      create_video_link "$vid" "$dev_ln" "$bus" "$sensor_idx" "$sensor_name" "Streaming"
      increment_dev_num $sensor_name
    # Handle metadata devices
    elif [ "${dev_name}" = "tegra-embedded" ]; then
      if [[ -z "$sensor_name" ]]; then
        echo "DEBUG: Could not identify sensor type for ${vid}, skipping"
        continue
      fi
      local dev_md_ln="/dev/video-rs-${sensor_name}-md-${sensor_idx}"
      create_video_link "$vid" "$dev_md_ln" "$bus" "$sensor_idx" "$sensor_name" "Metadata"
    else
      echo "DEBUG: Unrecognized driver ${dev_name} for ${vid}, skipping"
    fi
  done
}

# Helper function: create DFU device link
# Creates symbolic link for firmware update (DFU) device based on camera I2C address
# Matches the DFU device by I2C address to ensure correct camera-to-DFU mapping
# Input: cam_id (e.g. "0"), i2c_addr (e.g. "9-001a")
# Creates: /dev/d4xx-dfu-0 -> /dev/d4xx-dfu-9-001a
create_dfu_link() {
  local cam_id="$1"
  local i2c_addr="$2"

  local dfu_dev="d4xx-dfu-${i2c_addr}"
  local dev_dfu_name="/dev/${dfu_dev}"
  local dev_dfu_ln="/dev/d4xx-dfu-${cam_id}"

  echo "DEBUG: Looking for DFU device for camera ${cam_id} (${i2c_addr})"

  if [[ ! -e "/sys/class/d4xx-class/${dfu_dev}" ]]; then
    echo "DEBUG: DFU device ${dfu_dev} not found for camera ${cam_id}"
    return
  fi

  echo "DEBUG: Creating DFU link: ${dev_dfu_name} -> ${dev_dfu_ln}"

  if [[ $info -eq 0 ]]; then
    [[ -e $dev_dfu_ln ]] && unlink $dev_dfu_ln
    ln -s $dev_dfu_name $dev_dfu_ln
  fi
  [[ $quiet -eq 0 ]] && printf '%s\t%d\t%s\tFirmware \t%s\t%s\n' " i2c " ${cam_id} "d4xx   " $dev_dfu_name $dev_dfu_ln
}

# Helper function: process a single RS device
# Orchestrates complete processing of one RealSense DS5 mux device
# Extracts I2C address, finds video devices, creates links, and handles DFU
# Input example: "vi-output, DS5 mux 30-001a (platform:tegra-capture-vi:0):" "0"
process_single_rs_device() {
  local rs_line="$1"
  
  echo "DEBUG: Processing RS line: ${rs_line}"
  
  # Extract the I2C address from the RS mux line
  local i2c_addr=$(extract_i2c_address "$rs_line")
  echo "DEBUG: Extracted I2C address: ${i2c_addr}"
  
  if [[ -z "${i2c_addr}" ]]; then
    echo "DEBUG: Could not extract I2C address from ${rs_line}, skipping"
    return
  fi
  
  # Get the video devices for this RS mux
  echo "DEBUG: Looking for I2C pattern: ${i2c_addr}"
  local vid_devices=$(get_video_devices_for_rs "${i2c_addr}")
  echo "DEBUG: Video devices for ${i2c_addr}: ${vid_devices}"
  
  if [[ -z "${vid_devices}" ]]; then
    echo "DEBUG: No video devices found for ${i2c_addr}, skipping"
    return
  fi
  
  # Process video devices
  process_rs_video_devices "$vid_devices" "$i2c_addr"
}

# Check for Tegra devices by looking for RS mux in v4l2-ctl output
rs_devices=$(detect_rs_devices)

# For Jetson we have `simple` method
if [ -n "${rs_devices}" ]; then
  echo "DEBUG: Tegra RS devices detected"
  [[ $quiet -eq 0 ]] && printf "Bus\tCamera\tSensor\tNode Type\tVideo Node\tRS Link\n"
  
  # Parse each RS mux device
  while IFS= read -r rs_line; do
    if [[ -z "${rs_line}" ]]; then
      continue
    fi
    
    process_single_rs_device "$rs_line"
  done <<< "${rs_devices}"

  # Create DFU device links for all detected cameras, matched by I2C address
  for ((i=0; i<${depth_dev_counter}; i++)); do
    if [[ ${i} -lt ${#camera_i2c_addrs[@]} ]]; then
      create_dfu_link "$i" "${camera_i2c_addrs[$i]}"
    fi
  done

  echo "DEBUG: Processed ${depth_dev_counter} Tegra cameras"
  exit 0 # exit for Tegra
fi # done for Jetson

#ADL-P IPU6
mdev=$(${v4l2_util} --list-devices | grep -A1 ipu6 | grep media)
if [ -n "${mdev}" ]; then
[[ $quiet -eq 0 ]] && printf "Bus\tCamera\tSensor\tNode Type\tVideo Node\tRS Link\n"
# cache media-ctl output
dot=$(${media_util} -d ${mdev} --print-dot | grep -v dashed)
# for all d457 muxes a, b, c and d
for camera in $mux_list; do
  create_dfu_dev=0
  vpad=0
  if [ "${camera}" \> "d" ]; then
	  vpad=6
  fi
  for sens in "${!d4xx_vc_named[@]}"; do
    # get sensor binding from media controller
    d4xx_sens=$(echo "${dot}" | grep "D4XX $sens $camera" | awk '{print $1}')

    [[ -z $d4xx_sens ]] && continue; # echo "SENS $sens NOT FOUND" && continue

    d4xx_sens_mux=$(echo "${dot}" | grep $d4xx_sens:port0 | awk '{print $3}' | awk -F':' '{print $1}')
    csi2=$(echo "${dot}" | grep $d4xx_sens_mux:port0 | awk '{print $3}' | awk -F':' '{print $1}')
    be_soc=$(echo "${dot}" | grep $csi2:port1 | awk '{print $3}' | awk -F':' '{print $1}')
    vid_nd=$(echo "${dot}" | grep -w "$be_soc:port$((${d4xx_vc_named[${sens}]}+${vpad}))" | awk '{print $3}' | awk -F':' '{print $1}')
    [[ -z $vid_nd ]] && continue; # echo "SENS $sens NOT FOUND" && continue

    vid=$(echo "${dot}" | grep "${vid_nd}" | grep video | tr '\\n' '\n' | grep video | awk -F'"' '{print $1}')
    [[ -z $vid ]] && continue;
    dev_ln="/dev/video-rs-${camera_names["${sens}"]}-${camera_idx[${camera}]}"
    dev_name=$(${v4l2_util} -d $vid -D | grep Model | awk -F':' '{print $2}')

    [[ $quiet -eq 0 ]] && printf '%s\t%d\t%s\tStreaming\t%s\t%s\n' "$dev_name" ${camera_idx[${camera}]} ${camera_names["${sens}"]} $vid $dev_ln

    # create link only in case we choose not only to show it
    if [[ $info -eq 0 ]]; then
      [[ -e $dev_ln ]] && unlink $dev_ln
      ln -s $vid $dev_ln
      # activate ipu6 link enumeration feature
      ${v4l2_util} -d $dev_ln -c enumerate_graph_link=1
    fi
    create_dfu_dev=1 # will create DFU device link for camera
    # metadata link
    if [ "$metadata_enabled" -eq 0 ]; then
        continue;
    fi
    # skip IR metadata node for now.
    [[ ${camera_names["${sens}"]} == 'ir' ]] && continue
    # skip IMU metadata node.
    [[ ${camera_names["${sens}"]} == 'imu' ]] && continue

    vid_num=$(echo $vid | grep -o '[0-9]\+')
    dev_md_ln="/dev/video-rs-${camera_names["${sens}"]}-md-${camera_idx[${camera}]}"
    dev_name=$(${v4l2_util} -d "/dev/video$(($vid_num+1))" -D | grep Model | awk -F':' '{print $2}')

    [[ $quiet -eq 0 ]] && printf '%s\t%d\t%s\tMetadata\t/dev/video%s\t%s\n' "$dev_name" ${camera_idx[${camera}]} ${camera_names["${sens}"]} $(($vid_num+1)) $dev_md_ln
    # create link only in case we choose not only to show it
    if [[ $info -eq 0 ]]; then
      [[ -e $dev_md_ln ]] && unlink $dev_md_ln
      ln -s "/dev/video$(($vid_num+1))" $dev_md_ln
      ${v4l2_util} -d $dev_md_ln -c enumerate_graph_link=3
    fi
  done
  # create DFU device link for camera
  if [[ ${create_dfu_dev} -eq 1 ]]; then
    dev_dfu_name="/dev/d4xx-dfu-${camera}"
    dev_dfu_ln="/dev/d4xx-dfu-${camera_idx[${camera}]}"
    if [[ $info -eq 0 ]]; then
      [[ -e $dev_dfu_ln ]] && unlink $dev_dfu_ln
      ln -s $dev_dfu_name $dev_dfu_ln
    else
      [[ $quiet -eq 0 ]] && printf '%s\t%d\t%s\tFirmware \t%s\t%s\n' " i2c " ${camera_idx[${camera}]} "d4xx   " $dev_dfu_name $dev_dfu_ln
    fi
  fi
done
fi
# end of file

