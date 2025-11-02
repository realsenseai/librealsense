/*
 * Haiku Backend for libusb
 * Copyright © 2014 Akshay Jaggi <akshay1994.leo@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <new>
#include <vector>

#include "haiku_usb.h"

static int _errno_to_libusb(int status) {
    return status;
}

USBTransfer::USBTransfer(struct usbi_transfer *itransfer, USBDevice *device) {
    fUsbiTransfer   = itransfer;
    fLibusbTransfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    fUSBDevice      = device;
    fCancelled      = false;
}

USBTransfer::~USBTransfer() {}

struct usbi_transfer *USBTransfer::UsbiTransfer() {
    return fUsbiTransfer;
}

void USBTransfer::SetCancelled() {
    fCancelled = true;
}

bool USBTransfer::IsCancelled() {
    return fCancelled;
}

void USBTransfer::Do(int fRawFD) {
    switch(fLibusbTransfer->type) {
    case LIBUSB_TRANSFER_TYPE_CONTROL: {
        struct libusb_control_setup *setup = (struct libusb_control_setup *)fLibusbTransfer->buffer;
        usb_raw_command              command;
        command.control.request_type = setup->bmRequestType;
        command.control.request      = setup->bRequest;
        command.control.value        = setup->wValue;
        command.control.index        = setup->wIndex;
        command.control.length       = setup->wLength;
        command.control.data         = fLibusbTransfer->buffer + LIBUSB_CONTROL_SETUP_SIZE;
        if(fCancelled)
            break;
        if(ioctl(fRawFD, B_USB_RAW_COMMAND_CONTROL_TRANSFER, &command, sizeof(command)) || command.control.status != B_USB_RAW_STATUS_SUCCESS) {
            fUsbiTransfer->transferred = -1;
            usbi_err(TRANSFER_CTX(fLibusbTransfer), "failed control transfer");
            break;
        }
        fUsbiTransfer->transferred = command.control.length;
    } break;
    case LIBUSB_TRANSFER_TYPE_BULK:
    case LIBUSB_TRANSFER_TYPE_INTERRUPT: {
        usb_raw_command command;
        command.transfer.interface = fUSBDevice->EndpointToInterface(fLibusbTransfer->endpoint);
        command.transfer.endpoint  = fUSBDevice->EndpointToIndex(fLibusbTransfer->endpoint);
        command.transfer.data      = fLibusbTransfer->buffer;
        command.transfer.length    = fLibusbTransfer->length;
        if(fCancelled)
            break;
        if(fLibusbTransfer->type == LIBUSB_TRANSFER_TYPE_BULK) {
            if(ioctl(fRawFD, B_USB_RAW_COMMAND_BULK_TRANSFER, &command, sizeof(command)) || command.transfer.status != B_USB_RAW_STATUS_SUCCESS) {
                fUsbiTransfer->transferred = -1;
                usbi_err(TRANSFER_CTX(fLibusbTransfer), "failed bulk transfer");
                break;
            }
        }
        else {
            if(ioctl(fRawFD, B_USB_RAW_COMMAND_INTERRUPT_TRANSFER, &command, sizeof(command)) || command.transfer.status != B_USB_RAW_STATUS_SUCCESS) {
                fUsbiTransfer->transferred = -1;
                usbi_err(TRANSFER_CTX(fLibusbTransfer), "failed interrupt transfer");
                break;
            }
        }
        fUsbiTransfer->transferred = command.transfer.length;
    } break;
    // IsochronousTransfers not tested
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS: {
        usb_raw_command command;
        command.isochronous.interface    = fUSBDevice->EndpointToInterface(fLibusbTransfer->endpoint);
        command.isochronous.endpoint     = fUSBDevice->EndpointToIndex(fLibusbTransfer->endpoint);
        command.isochronous.data         = fLibusbTransfer->buffer;
        command.isochronous.length       = fLibusbTransfer->length;
        command.isochronous.packet_count = fLibusbTransfer->num_iso_packets;
        int                        i;
        usb_iso_packet_descriptor *packetDescriptors = new usb_iso_packet_descriptor[fLibusbTransfer->num_iso_packets];
        for(i = 0; i < fLibusbTransfer->num_iso_packets; i++) {
            if((fLibusbTransfer->iso_packet_desc[i]).length > (unsigned int)INT16_MAX) {
                fUsbiTransfer->transferred = -1;
                usbi_err(TRANSFER_CTX(fLibusbTransfer), "failed isochronous transfer");
                break;
            }
            packetDescriptors[i].request_length = (int16)(fLibusbTransfer->iso_packet_desc[i]).length;
        }
        if(i < fLibusbTransfer->num_iso_packets)
            break;  // TODO Handle this error
        command.isochronous.packet_descriptors = packetDescriptors;
        if(fCancelled)
            break;
        if(ioctl(fRawFD, B_USB_RAW_COMMAND_ISOCHRONOUS_TRANSFER, &command, sizeof(command)) || command.isochronous.status != B_USB_RAW_STATUS_SUCCESS) {
            fUsbiTransfer->transferred = -1;
            usbi_err(TRANSFER_CTX(fLibusbTransfer), "failed isochronous transfer");
            break;
        }
        for(i = 0; i < fLibusbTransfer->num_iso_packets; i++) {
            (fLibusbTransfer->iso_packet_desc[i]).actual_length = packetDescriptors[i].actual_length;
            switch(packetDescriptors[i].status) {
            case B_OK:
                (fLibusbTransfer->iso_packet_desc[i]).status = LIBUSB_TRANSFER_COMPLETED;
                break;
            default:
                (fLibusbTransfer->iso_packet_desc[i]).status = LIBUSB_TRANSFER_ERROR;
                break;
            }
        }
        delete[] packetDescriptors;
        // Do we put the length of transfer here, for isochronous transfers?
        fUsbiTransfer->transferred = command.transfer.length;
    } break;
    default:
        usbi_err(TRANSFER_CTX(fLibusbTransfer), "Unknown type of transfer");
    }
}

bool USBDeviceHandle::InitCheck() {
    return fInitCheck;
}

status_t USBDeviceHandle::TransfersThread(void *self) {
    USBDeviceHandle *handle = (USBDeviceHandle *)self;
    handle->TransfersWorker();
    return B_OK;
}

void USBDeviceHandle::TransfersWorker() {
    while(true) {
        status_t status = acquire_sem(fTransfersSem);
        if(status == B_BAD_SEM_ID)
            break;
        if(status == B_INTERRUPTED)
            continue;
        fTransfersLock.Lock();
        USBTransfer *fPendingTransfer = (USBTransfer *)fTransfers.RemoveItem((int32)0);
        fTransfersLock.Unlock();
        fPendingTransfer->Do(fRawFD);
        usbi_signal_transfer_completion(fPendingTransfer->UsbiTransfer());
    }
}

status_t USBDeviceHandle::SubmitTransfer(struct usbi_transfer *itransfer) {
    USBTransfer *transfer                                = new USBTransfer(itransfer, fUSBDevice);
    *((USBTransfer **)usbi_get_transfer_priv(itransfer)) = transfer;
    BAutolock locker(fTransfersLock);
    fTransfers.AddItem(transfer);
    release_sem(fTransfersSem);
    return LIBUSB_SUCCESS;
}

status_t USBDeviceHandle::CancelTransfer(USBTransfer *transfer) {
    transfer->SetCancelled();
    fTransfersLock.Lock();
    bool removed = fTransfers.RemoveItem(transfer);
    fTransfersLock.Unlock();
    if(removed)
        usbi_signal_transfer_completion(transfer->UsbiTransfer());
    return LIBUSB_SUCCESS;
}

USBDeviceHandle::USBDeviceHandle(USBDevice *dev) : fUSBDevice(dev), fClaimedInterfaces(0), fTransfersThread(-1), fInitCheck(false) {
    fRawFD = open(dev->Location(), O_RDWR | O_CLOEXEC);
    if(fRawFD < 0) {
        usbi_err(NULL, "failed to open device");
        return;
    }
    fTransfersSem    = create_sem(0, "Transfers Queue Sem");
    fTransfersThread = spawn_thread(TransfersThread, "Transfer Worker", B_NORMAL_PRIORITY, this);
    resume_thread(fTransfersThread);
    fInitCheck = true;
}

USBDeviceHandle::~USBDeviceHandle() {
    if(fRawFD > 0)
        close(fRawFD);
    for(int i = 0; i < 32; i++) {
        if(fClaimedInterfaces & (1U << i))
            ReleaseInterface(i);
    }
    delete_sem(fTransfersSem);
    if(fTransfersThread > 0)
        wait_for_thread(fTransfersThread, NULL);
}

int USBDeviceHandle::ClaimInterface(uint8 inumber) {
    int status = fUSBDevice->ClaimInterface(inumber);
    if(status == LIBUSB_SUCCESS)
        fClaimedInterfaces |= (1U << inumber);
    return status;
}

int USBDeviceHandle::ReleaseInterface(uint8 inumber) {
    fUSBDevice->ReleaseInterface(inumber);
    fClaimedInterfaces &= ~(1U << inumber);
    return LIBUSB_SUCCESS;
}

int USBDeviceHandle::SetConfiguration(uint8 config) {
    int config_index = fUSBDevice->CheckInterfacesFree(config);
    if(config_index == LIBUSB_ERROR_BUSY || config_index == LIBUSB_ERROR_NOT_FOUND)
        return config_index;
    usb_raw_command command;
    command.config.config_index = config_index;
    if(ioctl(fRawFD, B_USB_RAW_COMMAND_SET_CONFIGURATION, &command, sizeof(command)) || command.config.status != B_USB_RAW_STATUS_SUCCESS) {
        return _errno_to_libusb(command.config.status);
    }
    fUSBDevice->SetActiveConfiguration((uint8)config_index);
    return LIBUSB_SUCCESS;
}

int USBDeviceHandle::SetAltSetting(uint8 inumber, uint8 alt) {
    usb_raw_command command;
    command.alternate.config_index    = fUSBDevice->ActiveConfigurationIndex();
    command.alternate.interface_index = inumber;
    if(ioctl(fRawFD, B_USB_RAW_COMMAND_GET_ACTIVE_ALT_INTERFACE_INDEX, &command, sizeof(command)) || command.alternate.status != B_USB_RAW_STATUS_SUCCESS) {
        usbi_err(NULL, "Error retrieving active alternate interface");
        return _errno_to_libusb(command.alternate.status);
    }
    if(command.alternate.alternate_info == (uint32)alt) {
        usbi_dbg(NULL, "Setting alternate interface successful");
        return LIBUSB_SUCCESS;
    }
    command.alternate.alternate_info = alt;
    if(ioctl(fRawFD, B_USB_RAW_COMMAND_SET_ALT_INTERFACE, &command, sizeof(command))
       || command.alternate.status != B_USB_RAW_STATUS_SUCCESS) {  // IF IOCTL FAILS DEVICE DISCONNECTED PROBABLY
        usbi_err(NULL, "Error setting alternate interface");
        return _errno_to_libusb(command.alternate.status);
    }
    usbi_dbg(NULL, "Setting alternate interface successful");
    return LIBUSB_SUCCESS;
}

int USBDeviceHandle::ClearHalt(uint8 endpoint) {
    usb_raw_command command;
    command.control.request_type = USB_REQTYPE_ENDPOINT_OUT;
    command.control.request      = USB_REQUEST_CLEAR_FEATURE;
    command.control.value        = USB_FEATURE_ENDPOINT_HALT;
    command.control.index        = endpoint;
    command.control.length       = 0;

    if(ioctl(fRawFD, B_USB_RAW_COMMAND_CONTROL_TRANSFER, &command, sizeof(command)) || command.control.status != B_USB_RAW_STATUS_SUCCESS) {
        return _errno_to_libusb(command.control.status);
    }
    return LIBUSB_SUCCESS;
}

USBDevice::USBDevice(const char *path)
    : fClaimedInterfaces(0),
      fConfigurationDescriptors(NULL),
      fActiveConfiguration(0),  // 0?
      fPath(NULL),
      fEndpointToIndex(NULL),
      fEndpointToInterface(NULL),
      fInitCheck(false) {
    fPath = strdup(path);
    Initialise();
}

USBDevice::~USBDevice() {
    free(fPath);
    if(fConfigurationDescriptors) {
        for(uint8 i = 0; i < fDeviceDescriptor.num_configurations; i++) {
            if(fConfigurationDescriptors[i])
                delete fConfigurationDescriptors[i];
        }
        delete[] fConfigurationDescriptors;
    }
    if(fEndpointToIndex)
        delete[] fEndpointToIndex;
    if(fEndpointToInterface)
        delete[] fEndpointToInterface;
}

bool USBDevice::InitCheck() {
    return fInitCheck;
}

const char *USBDevice::Location() const {
    return fPath;
}

uint8 USBDevice::CountConfigurations() const {
    return fDeviceDescriptor.num_configurations;
}

const usb_device_descriptor *USBDevice::Descriptor() const {
    return &fDeviceDescriptor;
}

const usb_configuration_descriptor *USBDevice::ConfigurationDescriptor(uint8 index) const {
    if(index > CountConfigurations())
        return NULL;
    return (usb_configuration_descriptor *)fConfigurationDescriptors[index];
}

const usb_configuration_descriptor *USBDevice::ActiveConfiguration() const {
    return (usb_configuration_descriptor *)fConfigurationDescriptors[fActiveConfiguration];
}

uint8 USBDevice::ActiveConfigurationIndex() const {
    return fActiveConfiguration;
}

int USBDevice::ClaimInterface(uint8 interface) {
    if(interface > ActiveConfiguration()->number_interfaces)
        return LIBUSB_ERROR_NOT_FOUND;
    if(fClaimedInterfaces & (1U << interface))
        return LIBUSB_ERROR_BUSY;
    fClaimedInterfaces |= (1U << interface);
    return LIBUSB_SUCCESS;
}

int USBDevice::ReleaseInterface(uint8 interface) {
    fClaimedInterfaces &= ~(1U << interface);
    return LIBUSB_SUCCESS;
}

int USBDevice::CheckInterfacesFree(uint8 config) {
    if(fConfigToIndex.count(config) == 0)
        return LIBUSB_ERROR_NOT_FOUND;
    if(fClaimedInterfaces == 0)
        return fConfigToIndex[config];
    return LIBUSB_ERROR_BUSY;
}

void USBDevice::SetActiveConfiguration(uint8 config_index) {
    fActiveConfiguration = config_index;
}

uint8 USBDevice::EndpointToIndex(uint8 address) const {
    return fEndpointToIndex[fActiveConfiguration][address];
}

uint8 USBDevice::EndpointToInterface(uint8 address) const {
    return fEndpointToInterface[fActiveConfiguration][address];
}

int USBDevice::Initialise()  // Do we need more error checking, etc? How to report?
{
    int fRawFD = open(fPath, O_RDWR | O_CLOEXEC);
    if(fRawFD < 0)
        return B_ERROR;
    usb_raw_command command;
    command.device.descriptor = &fDeviceDescriptor;
    if(ioctl(fRawFD, B_USB_RAW_COMMAND_GET_DEVICE_DESCRIPTOR, &command, sizeof(command)) || command.device.status != B_USB_RAW_STATUS_SUCCESS) {
        close(fRawFD);
        return B_ERROR;
    }

    fConfigurationDescriptors = new(std::nothrow) unsigned char *[fDeviceDescriptor.num_configurations];
    fEndpointToIndex          = new(std::nothrow) map<uint8, uint8>[fDeviceDescriptor.num_configurations];
    fEndpointToInterface      = new(std::nothrow) map<uint8, uint8>[fDeviceDescriptor.num_configurations];
    for(uint8 i = 0; i < fDeviceDescriptor.num_configurations; i++) {
        usb_configuration_descriptor tmp_config;
        command.config.descriptor   = &tmp_config;
        command.config.config_index = i;
        if(ioctl(fRawFD, B_USB_RAW_COMMAND_GET_CONFIGURATION_DESCRIPTOR, &command, sizeof(command)) || command.config.status != B_USB_RAW_STATUS_SUCCESS) {
            usbi_err(NULL, "failed retrieving configuration descriptor");
            close(fRawFD);
            return B_ERROR;
        }
        fConfigToIndex[tmp_config.configuration_value] = i;
        fConfigurationDescriptors[i]                   = new(std::nothrow) unsigned char[tmp_config.total_length];

        command.config_etc.descriptor   = (usb_configuration_descriptor *)fConfigurationDescriptors[i];
        command.config_etc.length       = tmp_config.total_length;
        command.config_etc.config_index = i;
        if(ioctl(fRawFD, B_USB_RAW_COMMAND_GET_CONFIGURATION_DESCRIPTOR_ETC, &command, sizeof(command))
           || command.config_etc.status != B_USB_RAW_STATUS_SUCCESS) {
            usbi_err(NULL, "failed retrieving full configuration descriptor");
            close(fRawFD);
            return B_ERROR;
        }

        for(uint8 j = 0; j < tmp_config.number_interfaces; j++) {
            command.alternate.config_index    = i;
            command.alternate.interface_index = j;
            if(ioctl(fRawFD, B_USB_RAW_COMMAND_GET_ALT_INTERFACE_COUNT, &command, sizeof(command)) || command.config.status != B_USB_RAW_STATUS_SUCCESS) {
                usbi_err(NULL, "failed retrieving number of alternate interfaces");
                close(fRawFD);
                return B_ERROR;
            }
            uint8 num_alternate = (uint8)command.alternate.alternate_info;
            for(uint8 k = 0; k < num_alternate; k++) {
                usb_interface_descriptor tmp_interface;
                command.interface_etc.config_index    = i;
                command.interface_etc.interface_index = j;
                command.interface_etc.alternate_index = k;
                command.interface_etc.descriptor      = &tmp_interface;
                if(ioctl(fRawFD, B_USB_RAW_COMMAND_GET_INTERFACE_DESCRIPTOR_ETC, &command, sizeof(command))
                   || command.config.status != B_USB_RAW_STATUS_SUCCESS) {
                    usbi_err(NULL, "failed retrieving interface descriptor");
                    close(fRawFD);
                    return B_ERROR;
                }
                for(uint8 l = 0; l < tmp_interface.num_endpoints; l++) {
                    usb_endpoint_descriptor tmp_endpoint;
                    command.endpoint_etc.config_index    = i;
                    command.endpoint_etc.interface_index = j;
                    command.endpoint_etc.alternate_index = k;
                    command.endpoint_etc.endpoint_index  = l;
                    command.endpoint_etc.descriptor      = &tmp_endpoint;
                    if(ioctl(fRawFD, B_USB_RAW_COMMAND_GET_ENDPOINT_DESCRIPTOR_ETC, &command, sizeof(command))
                       || command.config.status != B_USB_RAW_STATUS_SUCCESS) {
                        usbi_err(NULL, "failed retrieving endpoint descriptor");
                        close(fRawFD);
                        return B_ERROR;
                    }
                    fEndpointToIndex[i][tmp_endpoint.endpoint_address]     = l;
                    fEndpointToInterface[i][tmp_endpoint.endpoint_address] = j;
                }
            }
        }
    }
    close(fRawFD);
    fInitCheck = true;
    return B_OK;
}
