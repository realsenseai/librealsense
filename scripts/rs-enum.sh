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


# Check for Tegra devices by looking for DS5 mux in v4l2-ctl output
rs_devices=$(${v4l2_util} --list-devices | grep -E "vi-output, DS5 mux [0-9]+-[0-9a-fA-F]+")

# For Jetson we have `simple` method
if [ -n "${rs_devices}" ]; then
  echo "DEBUG: Tegra DS5 devices detected"
  [[ $quiet -eq 0 ]] && printf "Bus\tCamera\tSensor\tNode Type\tVideo Node\tRS Link\n"
  
  cam_id=0
  # Parse each DS5 mux device
  while IFS= read -r ds5_line; do
    if [[ -z "${ds5_line}" ]]; then
      continue
    fi
    
    echo "DEBUG: Processing DS5 line: ${ds5_line}"
    
    # Extract the I2C address from the DS5 mux line
    i2c_addr=$(echo "${ds5_line}" | grep -oE '[0-9]+-[0-9a-fA-F]+' | head -1)
    echo "DEBUG: Extracted I2C address: ${i2c_addr}"
    
    # Get the video devices for this DS5 mux
    # Extract I2C address for matching (e.g., "30-001a" from the line)
    i2c_pattern=$(echo "${i2c_addr}")
    echo "DEBUG: Looking for I2C pattern: ${i2c_pattern}"
    
    # Extract video devices
    vid_devices=$(${v4l2_util} --list-devices | awk -v pattern="${i2c_pattern}" '
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
    ')
    
    echo "DEBUG: Video devices for ${i2c_addr}: ${vid_devices}"
    
    if [[ -z "${vid_devices}" ]]; then
      echo "DEBUG: No video devices found for ${i2c_addr}, skipping"
      continue
    fi
    
    # Convert video devices to array
    vid_dev_arr=(${vid_devices})
    echo "DEBUG: Video device array: ${vid_dev_arr[*]}"
    
    # Process each video device in the expected order
    sens_id=0
    for vid in "${vid_dev_arr[@]}"; do
      [[ ! -c "${vid}" ]] && echo "DEBUG: Video device ${vid} not found, skipping" && continue
      
      # Check if this is a valid tegra video device
      dev_name=$(${v4l2_util} -d ${vid} -D 2>/dev/null | grep 'Driver name' | head -n1 | awk -F' : ' '{print $2}')
      echo "DEBUG: Video device ${vid} driver name: ${dev_name}"
      
      bus="mipi"
      
      # Handle streaming devices
      if [ "${dev_name}" = "tegra-video" ]; then
        if [[ ${sens_id} -lt ${#camera_vid[@]} ]]; then
          dev_ln="/dev/video-rs-${camera_vid[${sens_id}]}-${cam_id}"
          type="Streaming"
          sensor_name=$(echo "${camera_vid[${sens_id}]}" | awk -F'-' '{print $1}')
          
          echo "DEBUG: Creating streaming link: ${vid} -> ${dev_ln}"
          [[ $quiet -eq 0 ]] && printf '%s\t%d\t%s\t%s\t%s\t%s\n' ${bus} ${cam_id} ${sensor_name} ${type} ${vid} ${dev_ln}

          # create link only in case we choose not only to show it
          if [[ $info -eq 0 ]]; then
            [[ -e $dev_ln ]] && unlink $dev_ln
            ln -s $vid $dev_ln
          fi
          
          sens_id=$((sens_id+1))
        fi
      # Handle metadata devices  
      elif [ "${dev_name}" = "tegra-embedded" ]; then
        if [[ ${sens_id} -lt ${#camera_vid[@]} ]]; then
          dev_md_ln="/dev/video-rs-${camera_vid[${sens_id}]}-${cam_id}"
          type="Metadata"
          sensor_name=$(echo "${camera_vid[${sens_id}]}" | awk -F'-' '{print $1}')
          
          echo "DEBUG: Creating metadata link: ${vid} -> ${dev_md_ln}"
          [[ $quiet -eq 0 ]] && printf '%s\t%d\t%s\t%s\t%s\t%s\n' ${bus} ${cam_id} ${sensor_name} ${type} ${vid} ${dev_md_ln}

          # create link only in case we choose not only to show it
          if [[ $info -eq 0 ]]; then
            [[ -e $dev_md_ln ]] && unlink $dev_md_ln
            ln -s $vid $dev_md_ln
          fi
          
          sens_id=$((sens_id+1))
        fi
      else
        echo "DEBUG: Unrecognized driver ${dev_name} for ${vid}, skipping"
      fi
    done
    
    # Create DFU device link for camera on jetson
    # Try to find matching DFU device based on I2C address
    i2c_bus=$(echo "${i2c_addr}" | cut -d'-' -f1)
    i2c_device_addr=$(echo "${i2c_addr}" | cut -d'-' -f2)
    
    echo "DEBUG: Looking for DFU device with I2C bus ${i2c_bus} and address ${i2c_device_addr}"
    
    # Look for d4xx class devices that might match
    dfu_candidates=$(ls -1 /sys/class/d4xx-class/ 2>/dev/null || true)
    echo "DEBUG: DFU candidates: ${dfu_candidates}"
    
    if [[ -n "${dfu_candidates}" ]]; then
      # For now, map cameras by order found
      dfu_array=(${dfu_candidates})
      if [[ ${cam_id} -lt ${#dfu_array[@]} ]]; then
        i2cdev="${dfu_array[${cam_id}]}"
        dev_dfu_name="/dev/${i2cdev}"
        dev_dfu_ln="/dev/d4xx-dfu-${cam_id}"
        
        echo "DEBUG: Creating DFU link: ${dev_dfu_name} -> ${dev_dfu_ln}"
        
        if [[ $info -eq 0 ]]; then
          [[ -e $dev_dfu_ln ]] && unlink $dev_dfu_ln
          ln -s $dev_dfu_name $dev_dfu_ln
        fi
        [[ $quiet -eq 0 ]] && printf '%s\t%d\t%s\tFirmware \t%s\t%s\n' " i2c " ${cam_id} "d4xx   " $dev_dfu_name $dev_dfu_ln
      fi
    fi
    
    cam_id=$((cam_id+1))
  done <<< "${rs_devices}"
  
  echo "DEBUG: Processed ${cam_id} Tegra cameras"
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

