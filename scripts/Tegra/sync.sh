#!/bin/bash
set -e

# SPDX-FileCopyrightText: Copyright (c) 2012-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# This script sync's NVIDIA's version of
# 1. the kernel source
# from nv-tegra, NVIDIA's public git repository.
# The script also provides opportunities to the sync to a specific tag
# so that the binaries shipped with a release can be replicated.
#
# Usage:
# By default it will download all the listed sources
# ./sync.sh
# Use the -t <TAG> option to provide the TAG to be used to sync all the sources.
# Use the -k <TAG> option to download only the kernel and device tree repos and optionally sync to TAG
# For detailed usage information run with -h option.
#

# verify that git is installed
if  ! which git > /dev/null  ; then
  echo "ERROR: git is not installed. If your linux distro is 10.04 or later,"
  echo "git can be installed by 'sudo apt-get install git-core'."
  exit 1
fi

# script name, dir
SCRIPT_NAME=`readlink -f $0`
SCRIPT_DIR=`dirname $SCRIPT_NAME`
# source dir
LDK_DIR=$SCRIPT_DIR
# info about sources.
# NOTE: *Add only kernel repos here. Add new repos separately below. Keep related repos together*
# NOTE: nvethrnetrm.git should be listed after "linux-nv-oot.git" due to nesting of sync path
source $SCRIPT_DIR/repos

# exit on error on sync
# after processing SOURCE_INFO
NSOURCES=0
declare -a SOURCE_INFO_PROCESSED
# download all?
DALL=1

function Usages {
	local ScriptName=$1
	local LINE
	local OP
	local DESC
	local PROCESSED=()
	local i

	echo "Use: $1 [options]"
	echo "Available general options are,"
	echo "     -h : help"
	echo "     -d [DIR] : root of source is DIR"
	echo "     -t [TAG] : Git tag that will be used to sync all the sources"
	echo ""
	echo "By default, all sources are downloaded."
	echo "Only specified sources are downloaded, if one or more of the following options are mentioned."
	echo ""
	echo "$SOURCE_INFO" | while read LINE; do
		if [ ! -z "$LINE" ]; then
			OP=$(echo "$LINE" | cut -f 1 -d ':')
			DESC=$(echo "$LINE" | cut -f 2 -d ':')
			if [[ ! " ${PROCESSED[@]} " =~ " ${OP} " ]]; then
				echo "     -${OP} [TAG]: Download $DESC source and optionally sync to TAG"
				PROCESSED+=("${OP}")
			else
				echo "           and download $DESC source and sync to the same TAG"
			fi
		fi
	done
	echo ""
}

function ProcessSwitch {
	local SWITCH="$1"
	local TAG="$2"
	local i
	local found=0

	for ((i=0; i < NSOURCES; i++)); do
		local OP=$(echo "${SOURCE_INFO_PROCESSED[i]}" | cut -f 1 -d ':')
		if [ "-${OP}" == "$SWITCH" ]; then
			SOURCE_INFO_PROCESSED[i]="${SOURCE_INFO_PROCESSED[i]}${TAG}:y"
			DALL=0
			found=1
		fi
	done

	if [ "$found" == 1 ]; then
		return 0
	fi

	echo "Terminating... wrong switch: ${SWITCH}" >&2
	Usages "$SCRIPT_NAME"
	exit 1
}

function UpdateTags {
	local SWITCH="$1"
	local TAG="$2"
	local i

	for ((i=0; i < NSOURCES; i++)); do
		local OP=$(echo "${SOURCE_INFO_PROCESSED[i]}" | cut -f 1 -d ':')
		if [ "${OP}" == "$SWITCH" ]; then
			SOURCE_INFO_PROCESSED[i]=$(echo "${SOURCE_INFO_PROCESSED[i]}" \
				| awk -F: -v OFS=: -v var="${TAG}" '{$4=var; print}')
		fi
	done
}

function DownloadAndSync {
	local WHAT_SOURCE="$1"
	local LDK_SOURCE_DIR="$2"
	local REPO_URL="$3"
	local TAG="$4"
	local OPT="$5"

	if [ -z "$TAG" ]; then
		echo -n "Please enter a tag to sync $2 source to (enter nothing to skip): "
		read TAG
		UpdateTags $OPT $TAG
	fi

	if [ -d "${LDK_SOURCE_DIR}" ]; then
		echo "Directory for $WHAT: ${LDK_SOURCE_DIR}, already exists!"
		if ! git -C ${LDK_SOURCE_DIR} status 2>&1 >/dev/null; then
			echo -e "\e[33m...but the directory is not a git repository -- clean it up first\e[0m\n"
			false
		fi
		ACTUAL_REPO=$(git -C ${LDK_SOURCE_DIR} config remote.origin.url)
		if [[ $REPO_URL != $ACTUAL_REPO ]]; then
			echo -e "\e[33mRepo URL are different:"
			echo config: $REPO_URL
			echo actual: $ACTUAL_REPO
			echo -e "Consider removing ${LDK_SOURCE_DIR} folder and start all over\e[0m"
		fi
		git -C ${LDK_SOURCE_DIR} tag -l 2>/dev/null | grep -q -P "^$TAG\$" || \
			git -C ${LDK_SOURCE_DIR} fetch --all 2>&1 >/dev/null || true
	else
		echo "Downloading default $WHAT source..."
		if ! git clone "$REPO_URL" -n ${LDK_SOURCE_DIR} 2>&1 >/dev/null; then
			echo -e "\e[31msource sync with ${REPO_URL} failed!\e]0m\n"
			false
		fi
		echo "The default $WHAT source is downloaded in: ${LDK_SOURCE_DIR}"
	fi

	if [ ! -z "$TAG" ]; then
		if git -C ${LDK_SOURCE_DIR} tag -l 2>/dev/null | grep -q -P "^$TAG\$"; then
			echo "Syncing up with tag $TAG..."
			git -C ${LDK_SOURCE_DIR} checkout -b mybranch_$(date +%Y-%m-%d-%s) $TAG
			echo -e "\e[32m$2 source sync'ed to tag $TAG successfully!\e[0m"
		else
			echo -e "\e[31mCouldn't find tag $TAG. $2 source sync to tag $TAG failed!\e[0m"
			false
		fi
	fi
	return 0
}

# prepare processing ....
GETOPT=":ehd:t:"

OIFS="$IFS"
IFS=$(echo -en "\n\b")
SOURCE_INFO_PROCESSED=($(echo "$SOURCE_INFO"))
IFS="$OIFS"
NSOURCES=${#SOURCE_INFO_PROCESSED[*]}

for ((i=0; i < NSOURCES; i++)); do
	OP=$(echo "${SOURCE_INFO_PROCESSED[i]}" | cut -f 1 -d ':')
	GETOPT="${GETOPT}${OP}:"
done

# parse the command line first
while getopts "$GETOPT" opt; do
	case $opt in
		d)
			case $OPTARG in
				-[A-Za-z]*)
					Usages "$SCRIPT_NAME"
					exit 1
					;;
				*)
					LDK_DIR="$OPTARG"
					;;
			esac
			;;
		h)
			Usages "$SCRIPT_NAME"
			exit 1
			;;
		t)
			TAG="$OPTARG"
			PROCESSED=()
			for ((i=0; i < NSOURCES; i++)); do
				OP=$(echo "${SOURCE_INFO_PROCESSED[i]}" | cut -f 1 -d ':')
				if [[ ! " ${PROCESSED[@]} " =~ " ${OP} " ]]; then
					UpdateTags $OP $TAG
					PROCESSED+=("${OP}")
				fi
			done
			;;
		[A-Za-z])
			case $OPTARG in
				-[A-Za-z]*)
					eval arg=\$$((OPTIND-1))
					case $arg in
						-[A-Za-Z]-*)
							Usages "$SCRIPT_NAME"
							exit 1
							;;
						*)
							ProcessSwitch "-$opt" ""
							OPTIND=$((OPTIND-1))
							;;
					esac
					;;
				*)
					ProcessSwitch "-$opt" "$OPTARG"
					;;
			esac
			;;
		:)
			case $OPTARG in
				#required arguments
				d)
					Usages "$SCRIPT_NAME"
					exit 1
					;;
				#optional arguments
				[A-Za-z])
					ProcessSwitch "-$OPTARG" ""
					;;
			esac
			;;
		\?)
			echo "Terminating... wrong switch: $@" >&2
			Usages "$SCRIPT_NAME"
			exit 1
			;;
	esac
done
shift $((OPTIND-1))

for ((i=0; i < NSOURCES; i++)); do
	OPT=$(echo "${SOURCE_INFO_PROCESSED[i]}" | cut -f 1 -d ':')
	WHAT=$(echo "${SOURCE_INFO_PROCESSED[i]}" | cut -f 2 -d ':')
	REPO=$(echo "${SOURCE_INFO_PROCESSED[i]}" | cut -f 3 -d ':')
	TAG=$(echo "${SOURCE_INFO_PROCESSED[i]}" | cut -f 4 -d ':')
	DNLOAD=$(echo "${SOURCE_INFO_PROCESSED[i]}" | cut -f 5 -d ':')

	if [ $DALL -eq 1 -o "x${DNLOAD}" == "xy" ]; then
		DownloadAndSync "$WHAT" "${LDK_DIR}/${WHAT}" "https://${REPO}" "${TAG}" "${OPT}"
		echo
	fi
done

if [[ -d ${LDK_DIR}/nvidia-oot/drivers/net/ethernet/nvidia/nvethernet ]]; then
	ln -sf ../../../../../../nvethernetrm ${LDK_DIR}/nvidia-oot/drivers/net/ethernet/nvidia/nvethernet/nvethernetrm
fi

exit 0
