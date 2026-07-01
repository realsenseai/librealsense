#﻿Copyright 2017 Intel Corporation
#
#Licensed under the Apache License, Version 2.0 (the "License");
#you may not use this file except in compliance with the License.
#You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
#Unless required by applicable law or agreed to in writing, software
#distributed under the License is distributed on an "AS IS" BASIS,
#WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#See the License for the specific language governing permissions and
#limitations under the License.

# The scripts is a part of Metadata enablement for IntelRealsense on Windows10 OS.
#
# The flow is summarized as follows
# Find all the connected Intel Realsense devices
# For each device that supports metadata
#	Check whether the Metadata registry key is present
#	Add the missing key(s) if necessary
# Additionally the script can retrofit the metadata keys for all devices that were registered, but not necessarily connected
# Usage:
#Launch Windows PowerShell, and navigate to the script directory
#PS> .\realsense_metadata_win10.ps1                    - Add metadata reg. keys for the connected Realsense devices
#PS> .\realsense_metadata_win10.ps1 -op install        - See above
#PS> .\realsense_metadata_win10.ps1 -op install_all    - Add metadata reg. keys for all RealSense devices that were previously connected
#PS> .\realsense_metadata_win10.ps1 -op remove         - Remove metadata reg. keys for the connected Realsense devices
#PS> .\realsense_metadata_win10.ps1 -op remove_all     - Remove metadata reg. keys for all RealSense devices that were previously connected


#Parse command-line argument
param (
    [string]$op = "install"
)

# Elevate to admin - https://stackoverflow.com/questions/7690994/powershell-running-a-command-as-administrator
If (-NOT ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator))
{
    # The original script is modified to pass through the command-line parameter
    $arguments = "& '" + $myinvocation.mycommand.definition + "'" + " -op " + $op
    Start-Process powershell -Verb runAs -ArgumentList $arguments
    Break
}

# Base location for the registry keys we need to add
$DevConfigRegBase = "HKLM:\SYSTEM\CurrentControlSet\Control\DeviceClasses"
# The Registry subtrees for the Metadata keys shall be added
$guid1 = "{e5323777-f976-4f5b-9b55-b94699c46e44}";
$guid2 = "{65E8773D-8F56-11D0-A3B9-00A0C9223196}";

$SearchTrees = "$DevConfigRegBase\$guid1", "$DevConfigRegBase\$guid2"

# Multipin devices that need additional key MetadataBufferSizeInKB1
$MultiPinDevices =
    "USB\VID_8086&PID_0AD1&MI_00",# D400(PSR)
    "USB\VID_8086&PID_0AD2&MI_00",# D410(ASR)
    "USB\VID_8086&PID_0AD3&MI_00",# D410+RGB(ASRC)
    "USB\VID_8086&PID_0AD4&MI_00",# D430(AWG)
    "USB\VID_8086&PID_0AD5&MI_00",# D430+MM(AWGT)
    "USB\VID_8086&PID_0AD6&MI_00",# D400/ USB2
    "USB\VID_8086&PID_0AF6&MI_00",# D420 (PWG)
    "USB\VID_8086&PID_0AFE&MI_00",# D420+MM(PWGT)
    "USB\VID_8086&PID_0AFF&MI_00",# D410+MM(ASRT)
    "USB\VID_8086&PID_0B00&MI_00",# D400+MM(PSRT)
    "USB\VID_8086&PID_0B01&MI_00",# D430+RGB+MM(AWGCT)
    "USB\VID_8086&PID_0B03&MI_00",# D460(DS5U)
    "USB\VID_8086&PID_0B07&MI_00",# D430+RGB(AWGC)
    "USB\VID_8086&PID_0B0C&MI_00",# D405
    "USB\VID_8086&PID_0B0D&MI_00",# L500
    "USB\VID_8086&PID_0B3A&MI_00",# D435i
    "USB\VID_8086&PID_0B3D&MI_00",# L515 PRQ
    "USB\VID_8086&PID_0B49&MI_00",# F416
    "USB\VID_8086&PID_0B4B&MI_00",# D430i
    "USB\VID_8086&PID_0B4D&MI_00",# D465
    "USB\VID_8086&PID_0B52&MI_00",# F416+RGB
    "USB\VID_8086&PID_0B5B&MI_00",# D405
    "USB\VID_8086&PID_0B5C&MI_00",# D455
    "USB\VID_8086&PID_0B64&MI_00",# L515
    "USB\VID_8086&PID_0B68&MI_00",# L535
    "USB\VID_8086&PID_0B56&MI_00",# D555
    "USB\VID_8086&PID_1155&MI_00",# D421
    "USB\VID_8086&PID_1156&MI_00", # D436
    "USB\VID_8086&PID_0B6A&MI_00",# D585
    "USB\VID_8086&PID_0B6B&MI_00" # D585S
    # VID_38E5 is RealSense's own vendor ID - its cameras get the full per-pin metadata key set on every video
    # interface (matched generically by VID below), so they need no per-PID entry here.

#Inhibit system warnings and erros, such as permissions or missing values
$ErrorActionPreference = "silentlycontinue"


$ConnectedDev = @()
#Retrieve all connected UVC devices
$DevInReg = Get-ChildItem hklm:\SYSTEM\CurrentControlSet\Services\usbvideo | ForEach-Object {Get-ItemProperty $_.pspath}

#Transform output into a standard container
for ($i=0; $i -lt $DevInReg[0].Count; $i++) { $ConnectedDev +=$DevInReg[0].$i}

#Filter Intel Realsense devices
$ConnectedDev = $ConnectedDev -match "VID_8086|VID_38E5"

#Progress notification
$rs_count = $ConnectedDev.Count
echo "$rs_count connected RealSense devices were found:" $ConnectedDev

#Search each subtree for values that correspond to the requested Intel Realsense devices
foreach ($subtree in $SearchTrees)
{
    "`nProcessing Registry branch $subtree"
    #Get records for all UVC devices records
    $Items = Get-ChildItem $subtree | Foreach-Object {Get-ItemProperty $_.PsPath }

    #Filter Intel RealSense devices
    "There are " + $Items.Count +" total devices listed"
    $Items = $Items | Where { $_.DeviceInstance -match "VID_8086|VID_38E5" }
    "" + $Items.Count + " of them are Intel Realsense"

    $remove_keys = 0
    switch ($op)
    {
        "install"     { $Items =  $Items | Where { $ConnectedDev -contains $_.DeviceInstance }}
        "remove"      { $Items =  $Items | Where { $ConnectedDev -contains $_.DeviceInstance }; $remove_keys = 1 }
        "install_all" { }
        "remove_all"  { $remove_keys = 1 }
        default       { "Aborting: unrecognized argument "" + $op + "" provided.`nPossible values are:";
                        "`t`t -op [install/install_all/remove/remove_all].`nRefer to the installation manual for details"; Sleep 2; Exit }
    }

    foreach  ($item in $Items)
    {
        
        $fullPath = $item.PSPath+'\#global\Device Parameters'

        if ($remove_keys -ge 1)
        {
            "Remove keys for device: " + $item.DeviceInstance.ToString()
            # Non-present value will be ignored as for script execution policy
            Remove-ItemProperty -path $fullPath -name MetadataBufferSizeInKB0
            Remove-ItemProperty -path $fullPath -name MetadataBufferSizeInKB1
			Remove-ItemProperty -path $fullPath -name MetadataBufferSizeInKB2
			Remove-ItemProperty -path $fullPath -name MetadataBufferSizeInKB3
        }
        else
        {
            # Device-instance prefix "USB\VID_xxxx&PID_xxxx&MI_00" identifies the device (strip the instance suffix).
            # Guard against instances shorter than the prefix before matching against the multi-pin list.
            # VID_38E5 is RealSense's own vendor ID - enable the full per-pin metadata key set on every video
            # interface (all MIs) so all streams get metadata, and new PIDs need no list entry.
            $devInstance = $item.DeviceInstance.ToString()
            $prefix = if ($devInstance.Length -ge 27) { $devInstance.Substring(0,27) } else { "" }
            $isMultiPin = ($MultiPinDevices -contains $prefix) -or ($devInstance -match '^USB\\VID_38E5&')

            $val = 0,0,0,0
            $val[0] = Get-ItemPropertyValue -Path $fullPath -Name MetadataBufferSizeInKB0
            $val[1] = Get-ItemPropertyValue -Path $fullPath -Name MetadataBufferSizeInKB1
			$val[2] = Get-ItemPropertyValue -Path $fullPath -Name MetadataBufferSizeInKB2
			$val[3] = Get-ItemPropertyValue -Path $fullPath -Name MetadataBufferSizeInKB3

            if ($val[0] -eq 0)
            {
                "Device " +  $item.DeviceInstance.ToString() + ": adding metadata key"
                Set-ItemProperty -path $fullPath -name MetadataBufferSizeInKB0 -value 5
            }
            else
            {
                "Device " +  $item.DeviceInstance.ToString() + ": skiping - metadata key already exists"
            }

            if ($isMultiPin -and ($val[1] -eq 0))
            {
                # Multi-pin interface requires an additional key
                "Device " +  $item.DeviceInstance.ToString() +": adding extra key for multipin interface"
                Set-ItemProperty -path $fullPath -name MetadataBufferSizeInKB1 -value 5
            }
            if ($isMultiPin -and ($val[2] -eq 0))
            {
                # Multi-pin interface requires an additional key
                "Device " +  $item.DeviceInstance.ToString() +": adding extra key for multipin interface"
                Set-ItemProperty -path $fullPath -name MetadataBufferSizeInKB2 -value 5
            }
            if ($isMultiPin -and ($val[3] -eq 0))
            {
                # Dual-RGB devices route a fourth media-pin (second color) through MI_00
                "Device " +  $item.DeviceInstance.ToString() +": adding extra key for multipin interface"
                Set-ItemProperty -path $fullPath -name MetadataBufferSizeInKB3 -value 5
            }
        }
    }
}

"`nTask Completed"
sleep 1
