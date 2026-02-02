#Break execution on any error received
function require_package {
	package_name=$1
	printf "\e[32mPackage required %s: \e[0m" "${package_name}"
	#Supress error code and message in case the package is not installed
	exec 3>&2
	exec 2>/dev/null
	set +e
	installed=$(dpkg-query -W -f='${Status}' ${package_name} | grep -c "ok installed" || true)
	exec 2>&3
	set -e
	
	if [ $installed -eq 0 ];
	then
		echo -e "\e[31m - not found, installing now...\e[0m"
		sudo apt-get install ${package_name} -y
		echo -e "\e[32mMissing package installed\e[0m"
	else
		echo -e "\e[32m - found\e[0m"
	fi
}

#Ubuntu focal repo : https://kernel.ubuntu.com/git/ubuntu/ubuntu-focal.git/
#	Branch		Commit message
#	master		UBUNTU: Ubuntu-5.4.0-21.25
function choose_kernel_branch {

	# Split the kernel version string
	IFS='.' read -a kernel_version <<< "$1"

	if [ "$2" == "focal" ]; 				# Ubuntu 20
	then
		case "${kernel_version[0]}.${kernel_version[1]}" in
		"5.4")									# kernel 5.4
			echo master
			;;
		"5.8")									# kernel 5.8
			echo hwe-5.8
			;;
		"5.11")									# kernel 5.11
			echo hwe-5.11
			;;
		"5.13")
			echo hwe-5.13
			;;
		"5.15")
			echo hwe-5.15
			;;
		*)
			#error message shall be redirected to stderr to be printed properly
			echo -e "\e[31mUnsupported kernel version $1 . The Focal patches are maintained for Ubuntu LTS with kernel 5.4, 5.8, 5.11 only\e[0m" >&2
			exit 1
			;;
		esac
	elif [ "$2" == "jammy" ]; 				# Ubuntu 22
	then
		case "${kernel_version[0]}.${kernel_version[1]}" in
		"5.15")
			echo hwe-5.15
			;;
		"5.19")
			echo hwe-5.19
			;;
		"6.2")
			echo hwe-6.2
			;;
		"6.5")
			echo hwe-6.5
			;;
		"6.8")
			echo hwe-6.8
			;;
		*)
			#error message shall be redirected to stderr to be printed properly
			echo -e "\e[31mUnsupported kernel version $1 . The Jammy patches are maintained for Ubuntu LTS with kernel 5.15, 5.19 only\e[0m" >&2
			exit 1
			;;
		esac
	elif [ "$2" == "noble" ]; 				# Ubuntu 24
	then
		case "${kernel_version[0]}.${kernel_version[1]}" in
		"6.8")
			echo hwe-6.8
			;;
		"6.11")
			echo hwe-6.11
			;;
		"6.14")
			echo hwe-6.14
			;;
		*)
			#error message shall be redirected to stderr to be printed properly
			echo -e "\e[31mUnsupported kernel version $1 . The Noble patches are maintained for Ubuntu LTS with kernel 6.8, 6.11, 6.14 only\e[0m" >&2
			exit 1
			;;
		esac
	else
		echo -e "\e[31mUnsupported distribution $2, kernel version $1 . The patches are maintained for Ubuntu 20/22/24 LTS\e[0m" >&2
		exit 1
	fi
}

function try_unload_module {
	unload_module_name=$1
	op_failed=0

	sudo modprobe -r ${unload_module_name} || op_failed=$?

	if [ $op_failed -ne 0 ];
	then
		echo -e "\e[31mFailed to unload module $unload_module_name. Error type $op_failed. Try rebooting\e[0m" >&2
		exit 1
	fi
}

function try_load_module {
	load_module_name=$1
	op_failed=0

	if [ $(lsmod | grep ^${load_module_name} | wc -l) -eq 0 ]; then
		sudo modprobe ${load_module_name} || op_failed=$?
	else
		printf "\e[32mn/a\e[0m"
	fi
	
	if [ $op_failed -ne 0 ];
	then
		echo -e "\e[31mFailed to reload module $load_module_name. Error type $op_failed. Operation is aborted\e[0m"  >&2
		exit 1
	fi
}

function try_module_insert {
	module_name=$1
	src_ko=$2
	tgt_ko=$3
	backup_available=1
	dependent_modules=""

	printf "\e[32mReplacing \e[93m\e[1m%s \e[32m -\n\e[0m" ${module_name}

	#Check if the module is loaded, and if does - are there dependent kernel modules.
	#Unload those first, then unload the requsted module and proceed with replacement
	#  lsmod | grep ^videodev
	#videodev              176128  4 uvcvideo,v4l2_common,videobuf2_core,videobuf2_v4l2
	# In the above scenario videodev cannot be unloaded untill all the modules listed on the right are unloaded
	# Note that in case of multiple dependencies we'll remove only modules one by one starting with the first on the list
	# And that the modules stack unwinding will start from the last (i.e leaf) dependency,
	# for instance : videobuf2_core,videobuf2_v4l2,uvcvideo will start with unloading uvcvideo as it should automatically unwind dependent modules
	if [ $(lsmod | grep ^${module_name} | wc -l) -ne 0 ];
	then
		dependencies=$(lsmod | grep ^${module_name} | awk '{printf $4}')
		dependent_module=$(lsmod | grep ^${module_name} | awk '{printf $4}' | awk -F, '{printf $NF}')
		if [ ! -z "$dependencies" ];
		then
			printf "\e[32m\tModule \e[93m\e[1m%s \e[32m\e[21m is used by \e[34m$dependencies\n\e[0m" ${module_name}
		fi
		while [ ! -z "$dependent_module" ]
		do
			printf "\e[32m\tUnloading dependency \e[34m$dependent_module\e[0m\n\t"
			dependent_modules+="$dependent_module "
			try_unload_module $dependent_module
			dependent_module=$(lsmod | grep ^${module_name} | awk '{printf $4}' | awk -F, '{printf $NF}')
		done

		# Unload existing modules if resident
		printf "\e[32mModule is resident, unloading ... \e[0m"
		try_unload_module ${module_name}
		printf "\e[32m succeeded. \e[0m\n"
	fi

	# backup the existing module (if available) for recovery
	if [ -f ${tgt_ko}.zst ];
	then
		sudo mv ${tgt_ko}.zst ${tgt_ko}.zst.bckup
	elif [ -f ${tgt_ko} ];
	then
		sudo mv ${tgt_ko} ${tgt_ko}.bckup
	else
		backup_available=0
	fi

	# copy the patched module to target location
	sudo cp ${src_ko} ${tgt_ko}

	# Run 'depmod' command, so that, it will update the 'modinfo' if incase filename or other details got changed.
	sudo depmod

	# try to load the new module
	modprobe_failed=0
	printf "\e[32m\tApplying the patched module ... \e[0m"
	sudo modprobe ${module_name} || modprobe_failed=$?

	# Check and revert the backup module if 'modprobe' operation crashed
	if [ $modprobe_failed -ne 0 ];
	then
		echo -e "\e[31mFailed to insert the patched module. Operation is aborted, the original module is restored\e[0m"
		echo -e "\e[31mVerify that the current kernel version is aligned to the patched module version\e[0m"
		if [ ${backup_available} -ne 0 ];
		then
			if [ -f ${tgt_ko}.zst.bckup ];
			then
				sudo mv ${tgt_ko}.zst.bckup ${tgt_ko}.zst
			else
				sudo mv ${tgt_ko}.bckup ${tgt_ko}
			fi
			sudo modprobe ${module_name}
			printf "\e[34mThe original \e[33m %s \e[34m module was reloaded\n\e[0m" ${module_name}
		fi
		exit 1
	else
		# Everything went OK, delete backup
		printf "\e[32m succeeded\e[0m"
		sudo rm -f ${tgt_ko}.zst.bckup
		sudo rm -f ${tgt_ko}.bckup
	fi

	# Reload all dependent modules recursively
	if [ ! -z "$dependent_modules" ];
	then
		#Retrieve the list of dependent modules that were unloaded
		modules_list=(${dependent_modules})
		for (( idx=${#modules_list[@]}-1 ; idx>=0 ; idx-- ));
		do
			printf "\e[32m\tReloading dependent kernel module \e[34m${modules_list[idx]} \e[32m... \e[0m"
			try_load_module ${modules_list[idx]}
			printf "\e[32m succeeded. \e[0m\n"
		done
	fi
	printf "\n"
}
