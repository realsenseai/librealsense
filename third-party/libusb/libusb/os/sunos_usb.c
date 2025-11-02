/*
 *
 * Copyright (c) 2016, Oracle and/or its affiliates.
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

#include <libusb/config.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <unistd.h>
#include <aio.h>
#include <libdevinfo.h>
#include <sys/nvpair.h>
#include <sys/devctl.h>
#include <sys/usb/clients/ugen/usb_ugen.h>
#include <sys/usb/usba.h>
#include <sys/pci.h>

#include "libusbi.h"
#include "sunos_usb.h"

#define UPDATEDRV_PATH "/usr/sbin/update_drv"
#define UPDATEDRV "update_drv"

#define DEFAULT_LISTSIZE 6

typedef struct {
    int    nargs;
    int    listsize;
    char **string;
} string_list_t;

/*
 * Backend functions
 */
static int  sunos_get_device_list(struct libusb_context *, struct discovered_devs **);
static int  sunos_open(struct libusb_device_handle *);
static void sunos_close(struct libusb_device_handle *);
static int  sunos_get_active_config_descriptor(struct libusb_device *, void *, size_t);
static int  sunos_get_config_descriptor(struct libusb_device *, uint8_t, void *, size_t);
static int  sunos_get_configuration(struct libusb_device_handle *, uint8_t *);
static int  sunos_set_configuration(struct libusb_device_handle *, int);
static int  sunos_claim_interface(struct libusb_device_handle *, uint8_t);
static int  sunos_release_interface(struct libusb_device_handle *, uint8_t);
static int  sunos_set_interface_altsetting(struct libusb_device_handle *, uint8_t, uint8_t);
static int  sunos_clear_halt(struct libusb_device_handle *, unsigned char);
static void sunos_destroy_device(struct libusb_device *);
static int  sunos_submit_transfer(struct usbi_transfer *);
static int  sunos_cancel_transfer(struct usbi_transfer *);
static int  sunos_handle_transfer_completion(struct usbi_transfer *);
static int  sunos_kernel_driver_active(struct libusb_device_handle *, uint8_t);
static int  sunos_detach_kernel_driver(struct libusb_device_handle *, uint8_t);
static int  sunos_attach_kernel_driver(struct libusb_device_handle *, uint8_t);
static int  sunos_usb_open_ep0(sunos_dev_handle_priv_t *hpriv, sunos_dev_priv_t *dpriv);
static int  sunos_usb_ioctl(struct libusb_device *dev, int cmd);

static int sunos_get_link(di_devlink_t devlink, void *arg) {
    walk_link_t *larg = (walk_link_t *)arg;
    const char  *p;
    const char  *q;

    if(larg->path) {
        char *content = (char *)di_devlink_content(devlink);
        char *start   = strstr(content, "/devices/");
        start += strlen("/devices");
        usbi_dbg(NULL, "%s", start);

        /* line content must have minor node */
        if(start == NULL || strncmp(start, larg->path, larg->len) != 0 || start[larg->len] != ':')
            return (DI_WALK_CONTINUE);
    }

    p = di_devlink_path(devlink);
    q = strrchr(p, '/');
    usbi_dbg(NULL, "%s", q);

    *(larg->linkpp) = strndup(p, strlen(p) - strlen(q));

    return (DI_WALK_TERMINATE);
}

static int sunos_physpath_to_devlink(const char *node_path, const char *match, char **link_path) {
    walk_link_t         larg;
    di_devlink_handle_t hdl;

    *link_path  = NULL;
    larg.linkpp = link_path;
    if((hdl = di_devlink_init(NULL, 0)) == NULL) {
        usbi_dbg(NULL, "di_devlink_init failure");
        return (-1);
    }

    larg.len  = strlen(node_path);
    larg.path = (char *)node_path;

    (void)di_devlink_walk(hdl, match, NULL, DI_PRIMARY_LINK, (void *)&larg, sunos_get_link);

    (void)di_devlink_fini(&hdl);

    if(*link_path == NULL) {
        usbi_dbg(NULL, "there is no devlink for this path");
        return (-1);
    }

    return 0;
}

static int sunos_usb_ioctl(struct libusb_device *dev, int cmd) {
    int                   fd;
    nvlist_t             *nvlist;
    char                 *end;
    char                 *phypath;
    char                 *hubpath;
    char                  path_arg[PATH_MAX];
    sunos_dev_priv_t     *dpriv;
    devctl_ap_state_t     devctl_ap_state;
    struct devctl_iocdata iocdata;

    dpriv   = usbi_get_device_priv(dev);
    phypath = dpriv->phypath;

    end = strrchr(phypath, '/');
    if(end == NULL)
        return (-1);
    hubpath = strndup(phypath, end - phypath);
    if(hubpath == NULL)
        return (-1);

    end = strrchr(hubpath, '@');
    if(end == NULL) {
        free(hubpath);
        return (-1);
    }
    end++;
    usbi_dbg(DEVICE_CTX(dev), "unitaddr: %s", end);

    nvlist_alloc(&nvlist, NV_UNIQUE_NAME_TYPE, KM_NOSLEEP);
    nvlist_add_int32(nvlist, "port", dev->port_number);
    // find the hub path
    snprintf(path_arg, sizeof(path_arg), "/devices%s:hubd", hubpath);
    usbi_dbg(DEVICE_CTX(dev), "ioctl hub path: %s", path_arg);

    fd = open(path_arg, O_RDONLY);
    if(fd < 0) {
        usbi_err(DEVICE_CTX(dev), "open failed: errno %d (%s)", errno, strerror(errno));
        nvlist_free(nvlist);
        free(hubpath);
        return (-1);
    }

    memset(&iocdata, 0, sizeof(iocdata));
    memset(&devctl_ap_state, 0, sizeof(devctl_ap_state));

    nvlist_pack(nvlist, (char **)&iocdata.nvl_user, &iocdata.nvl_usersz, NV_ENCODE_NATIVE, 0);

    iocdata.cmd        = DEVCTL_AP_GETSTATE;
    iocdata.flags      = 0;
    iocdata.c_nodename = (char *)"hub";
    iocdata.c_unitaddr = end;
    iocdata.cpyout_buf = &devctl_ap_state;
    usbi_dbg(DEVICE_CTX(dev), "%p, %" PRIuPTR, iocdata.nvl_user, iocdata.nvl_usersz);

    errno = 0;
    if(ioctl(fd, DEVCTL_AP_GETSTATE, &iocdata) == -1) {
        usbi_err(DEVICE_CTX(dev), "ioctl failed: fd %d, cmd %x, errno %d (%s)", fd, DEVCTL_AP_GETSTATE, errno, strerror(errno));
    }
    else {
        usbi_dbg(DEVICE_CTX(dev), "dev rstate: %d", devctl_ap_state.ap_rstate);
        usbi_dbg(DEVICE_CTX(dev), "dev ostate: %d", devctl_ap_state.ap_ostate);
    }

    errno       = 0;
    iocdata.cmd = cmd;
    if(ioctl(fd, (int)cmd, &iocdata) != 0) {
        usbi_err(DEVICE_CTX(dev), "ioctl failed: fd %d, cmd %x, errno %d (%s)", fd, cmd, errno, strerror(errno));
        sleep(2);
    }

    close(fd);
    free(iocdata.nvl_user);
    nvlist_free(nvlist);
    free(hubpath);

    return (-errno);
}

static int sunos_kernel_driver_active(struct libusb_device_handle *dev_handle, uint8_t interface) {
    sunos_dev_priv_t *dpriv = usbi_get_device_priv(dev_handle->dev);

    UNUSED(interface);

    usbi_dbg(HANDLE_CTX(dev_handle), "%s", dpriv->ugenpath);

    return (dpriv->ugenpath == NULL);
}

/*
 * Private functions
 */
static int _errno_to_libusb(int);
static int sunos_usb_get_status(struct libusb_context *ctx, int fd);

static string_list_t *sunos_new_string_list(void) {
    string_list_t *list;

    list = calloc(1, sizeof(string_list_t));
    if(list == NULL)
        return (NULL);
    list->string = calloc(DEFAULT_LISTSIZE, sizeof(char *));
    if(list->string == NULL) {
        free(list);
        return (NULL);
    }
    list->nargs    = 0;
    list->listsize = DEFAULT_LISTSIZE;

    return (list);
}

static int sunos_append_to_string_list(string_list_t *list, const char *arg) {
    char *str = strdup(arg);

    if(str == NULL)
        return (-1);

    if((list->nargs + 1) == list->listsize) { /* +1 is for NULL */
        char **tmp = realloc(list->string, sizeof(char *) * (list->listsize + 1));
        if(tmp == NULL) {
            free(str);
            return (-1);
        }
        list->string                   = tmp;
        list->string[list->listsize++] = NULL;
    }
    list->string[list->nargs++] = str;

    return (0);
}

static void sunos_free_string_list(string_list_t *list) {
    int i;

    for(i = 0; i < list->nargs; i++) {
        free(list->string[i]);
    }

    free(list->string);
    free(list);
}

static char **sunos_build_argv_list(string_list_t *list) {
    return (list->string);
}

static int sunos_exec_command(struct libusb_context *ctx, const char *path, string_list_t *list) {
    pid_t  pid;
    int    status;
    int    waitstat;
    int    exit_status;
    char **argv_list;

    argv_list = sunos_build_argv_list(list);
    if(argv_list == NULL)
        return (-1);

    pid = fork();
    if(pid == 0) {
        /* child */
        execv(path, argv_list);
        _exit(127);
    }
    else if(pid > 0) {
        /* parent */
        do {
            waitstat = waitpid(pid, &status, 0);
        } while((waitstat == -1 && errno == EINTR) || (waitstat == 0 && !WIFEXITED(status) && !WIFSIGNALED(status)));

        if(waitstat == 0) {
            if(WIFEXITED(status))
                exit_status = WEXITSTATUS(status);
            else
                exit_status = WTERMSIG(status);
        }
        else {
            usbi_err(ctx, "waitpid failed: errno %d (%s)", errno, strerror(errno));
            exit_status = -1;
        }
    }
    else {
        /* fork failed */
        usbi_err(ctx, "fork failed: errno %d (%s)", errno, strerror(errno));
        exit_status = -1;
    }

    return (exit_status);
}

static int sunos_detach_kernel_driver(struct libusb_device_handle *dev_handle, uint8_t interface_number) {
    struct libusb_context *ctx = HANDLE_CTX(dev_handle);
    string_list_t         *list;
    char                   path_arg[PATH_MAX];
    sunos_dev_priv_t      *dpriv;
    int                    r;

    UNUSED(interface_number);

    dpriv = usbi_get_device_priv(dev_handle->dev);
    snprintf(path_arg, sizeof(path_arg), "\'\"%s\"\'", dpriv->phypath);
    usbi_dbg(HANDLE_CTX(dev_handle), "%s", path_arg);

    list = sunos_new_string_list();
    if(list == NULL)
        return (LIBUSB_ERROR_NO_MEM);

    /* attach ugen driver */
    r = 0;
    r |= sunos_append_to_string_list(list, UPDATEDRV);
    r |= sunos_append_to_string_list(list, "-a");     /* add rule */
    r |= sunos_append_to_string_list(list, "-i");     /* specific device */
    r |= sunos_append_to_string_list(list, path_arg); /* physical path */
    r |= sunos_append_to_string_list(list, "ugen");
    if(r) {
        sunos_free_string_list(list);
        return (LIBUSB_ERROR_NO_MEM);
    }

    r = sunos_exec_command(ctx, UPDATEDRV_PATH, list);
    sunos_free_string_list(list);
    if(r < 0)
        return (LIBUSB_ERROR_OTHER);

    /* reconfigure the driver node */
    r = 0;
    r |= sunos_usb_ioctl(dev_handle->dev, DEVCTL_AP_DISCONNECT);
    r |= sunos_usb_ioctl(dev_handle->dev, DEVCTL_AP_CONFIGURE);
    if(r)
        usbi_warn(HANDLE_CTX(dev_handle), "one or more ioctls failed");

    snprintf(path_arg, sizeof(path_arg), "^usb/%x.%x", dev_handle->dev->device_descriptor.idVendor, dev_handle->dev->device_descriptor.idProduct);
    sunos_physpath_to_devlink(dpriv->phypath, path_arg, &dpriv->ugenpath);

    if(access(dpriv->ugenpath, F_OK) == -1) {
        usbi_err(HANDLE_CTX(dev_handle), "fail to detach kernel driver");
        return (LIBUSB_ERROR_IO);
    }

    return sunos_usb_open_ep0(usbi_get_device_handle_priv(dev_handle), dpriv);
}

static int sunos_attach_kernel_driver(struct libusb_device_handle *dev_handle, uint8_t interface_number) {
    struct libusb_context *ctx = HANDLE_CTX(dev_handle);
    string_list_t         *list;
    char                   path_arg[PATH_MAX];
    sunos_dev_priv_t      *dpriv;
    int                    r;

    UNUSED(interface_number);

    /* we open the dev in detach driver, so we need close it first. */
    sunos_close(dev_handle);

    dpriv = usbi_get_device_priv(dev_handle->dev);
    snprintf(path_arg, sizeof(path_arg), "\'\"%s\"\'", dpriv->phypath);
    usbi_dbg(HANDLE_CTX(dev_handle), "%s", path_arg);

    list = sunos_new_string_list();
    if(list == NULL)
        return (LIBUSB_ERROR_NO_MEM);

    /* detach ugen driver */
    r = 0;
    r |= sunos_append_to_string_list(list, UPDATEDRV);
    r |= sunos_append_to_string_list(list, "-d");     /* add rule */
    r |= sunos_append_to_string_list(list, "-i");     /* specific device */
    r |= sunos_append_to_string_list(list, path_arg); /* physical path */
    r |= sunos_append_to_string_list(list, "ugen");
    if(r) {
        sunos_free_string_list(list);
        return (LIBUSB_ERROR_NO_MEM);
    }

    r = sunos_exec_command(ctx, UPDATEDRV_PATH, list);
    sunos_free_string_list(list);
    if(r < 0)
        return (LIBUSB_ERROR_OTHER);

    /* reconfigure the driver node */
    r = 0;
    r |= sunos_usb_ioctl(dev_handle->dev, DEVCTL_AP_CONFIGURE);
    r |= sunos_usb_ioctl(dev_handle->dev, DEVCTL_AP_DISCONNECT);
    r |= sunos_usb_ioctl(dev_handle->dev, DEVCTL_AP_CONFIGURE);
    if(r)
        usbi_warn(HANDLE_CTX(dev_handle), "one or more ioctls failed");

    return 0;
}

static int sunos_fill_in_dev_info(di_node_t node, struct libusb_device *dev) {
    int               proplen;
    int              *i, n, *addr, *port_prop;
    char             *phypath;
    uint8_t          *rdata;
    sunos_dev_priv_t *dpriv = usbi_get_device_priv(dev);
    char              match_str[PATH_MAX];

    /* Device descriptors */
    proplen = di_prop_lookup_bytes(DDI_DEV_T_ANY, node, "usb-dev-descriptor", &rdata);
    if(proplen <= 0) {
        return (LIBUSB_ERROR_IO);
    }
    bcopy(rdata, &dev->device_descriptor, LIBUSB_DT_DEVICE_SIZE);

    /* Raw configuration descriptors */
    proplen = di_prop_lookup_bytes(DDI_DEV_T_ANY, node, "usb-raw-cfg-descriptors", &rdata);
    if(proplen <= 0) {
        usbi_dbg(DEVICE_CTX(dev), "can't find raw config descriptors");

        return (LIBUSB_ERROR_IO);
    }
    dpriv->raw_cfgdescr = calloc(1, proplen);
    if(dpriv->raw_cfgdescr == NULL) {
        return (LIBUSB_ERROR_NO_MEM);
    }
    else {
        bcopy(rdata, dpriv->raw_cfgdescr, proplen);
        dpriv->cfgvalue = ((struct libusb_config_descriptor *)rdata)->bConfigurationValue;
    }

    n = di_prop_lookup_ints(DDI_DEV_T_ANY, node, "reg", &port_prop);

    if((n != 1) || (*port_prop <= 0)) {
        return (LIBUSB_ERROR_IO);
    }
    dev->port_number = *port_prop;

    /* device physical path */
    phypath = di_devfs_path(node);
    if(phypath) {
        dpriv->phypath = strdup(phypath);
        snprintf(match_str, sizeof(match_str), "^usb/%x.%x", dev->device_descriptor.idVendor, dev->device_descriptor.idProduct);
        usbi_dbg(DEVICE_CTX(dev), "match is %s", match_str);
        sunos_physpath_to_devlink(dpriv->phypath, match_str, &dpriv->ugenpath);
        di_devfs_path_free(phypath);
    }
    else {
        free(dpriv->raw_cfgdescr);

        return (LIBUSB_ERROR_IO);
    }

    /* address */
    n = di_prop_lookup_ints(DDI_DEV_T_ANY, node, "assigned-address", &addr);
    if(n != 1 || *addr == 0) {
        usbi_dbg(DEVICE_CTX(dev), "can't get address");
    }
    else {
        dev->device_address = *addr;
    }

    /* speed */
    if(di_prop_lookup_ints(DDI_DEV_T_ANY, node, "low-speed", &i) >= 0) {
        dev->speed = LIBUSB_SPEED_LOW;
    }
    else if(di_prop_lookup_ints(DDI_DEV_T_ANY, node, "high-speed", &i) >= 0) {
        dev->speed = LIBUSB_SPEED_HIGH;
    }
    else if(di_prop_lookup_ints(DDI_DEV_T_ANY, node, "full-speed", &i) >= 0) {
        dev->speed = LIBUSB_SPEED_FULL;
    }
    else if(di_prop_lookup_ints(DDI_DEV_T_ANY, node, "super-speed", &i) >= 0) {
        dev->speed = LIBUSB_SPEED_SUPER;
    }

    usbi_dbg(DEVICE_CTX(dev), "vid=%x pid=%x, path=%s, bus_nmber=0x%x, port_number=%d, speed=%d", dev->device_descriptor.idVendor,
             dev->device_descriptor.idProduct, dpriv->phypath, dev->bus_number, dev->port_number, dev->speed);

    return (LIBUSB_SUCCESS);
}

static int sunos_add_devices(di_devlink_t link, void *arg) {
    struct devlink_cbarg *largs = (struct devlink_cbarg *)arg;
    struct node_args     *nargs;
    di_node_t             myself, dn;
    uint64_t              session_id = 0;
    uint64_t              sid        = 0;
    uint64_t              bdf        = 0;
    struct libusb_device *dev;
    sunos_dev_priv_t     *devpriv;
    int                   n, *j;
    int                   i = 0;
    int                  *addr_prop;
    uint8_t               bus_number = 0;
    uint32_t             *regbuf     = NULL;
    uint32_t              reg;

    UNUSED(link);

    nargs  = (struct node_args *)largs->nargs;
    myself = largs->myself;

    /*
     * Construct session ID.
     * session ID = dev_addr | hub addr |parent hub addr|...|root hub bdf
     * 		8 bits       8bits          8 bits               16bits
     */
    if(myself == DI_NODE_NIL)
        return (DI_WALK_CONTINUE);

    dn = myself;
    /* find the root hub */
    while(di_prop_lookup_ints(DDI_DEV_T_ANY, dn, "root-hub", &j) != 0) {
        usbi_dbg(NULL, "find_root_hub:%s", di_devfs_path(dn));
        n = di_prop_lookup_ints(DDI_DEV_T_ANY, dn, "assigned-address", &addr_prop);
        session_id |= ((addr_prop[0] & 0xff) << i++ * 8);
        dn = di_parent_node(dn);
    }

    /* dn is the root hub node */
    n   = di_prop_lookup_ints(DDI_DEV_T_ANY, dn, "reg", (int **)&regbuf);
    reg = regbuf[0];
    bdf = (PCI_REG_BUS_G(reg) << 8) | (PCI_REG_DEV_G(reg) << 3) | PCI_REG_FUNC_G(reg);
    /* bdf must larger than i*8 bits */
    session_id |= (bdf << i * 8);
    bus_number = (PCI_REG_DEV_G(reg) << 3) | PCI_REG_FUNC_G(reg);

    usbi_dbg(NULL, "device bus address=%s:%x, name:%s", di_bus_addr(myself), bus_number, di_node_name(dn));
    usbi_dbg(NULL, "session id org:%" PRIx64, session_id);

    /* dn is the usb device */
    for(dn = di_child_node(myself); dn != DI_NODE_NIL; dn = di_sibling_node(dn)) {
        usbi_dbg(NULL, "device path:%s", di_devfs_path(dn));
        /* skip hub devices, because its driver can not been unload */
        if(di_prop_lookup_ints(DDI_DEV_T_ANY, dn, "usb-port-count", &addr_prop) != -1)
            continue;
        /* usb_addr */
        n = di_prop_lookup_ints(DDI_DEV_T_ANY, dn, "assigned-address", &addr_prop);
        if((n != 1) || (addr_prop[0] == 0)) {
            usbi_dbg(NULL, "cannot get valid usb_addr");
            continue;
        }

        sid = (session_id << 8) | (addr_prop[0] & 0xff);
        usbi_dbg(NULL, "session id %" PRIX64, sid);

        dev = usbi_get_device_by_session_id(nargs->ctx, sid);
        if(dev == NULL) {
            dev = usbi_alloc_device(nargs->ctx, sid);
            if(dev == NULL) {
                usbi_dbg(NULL, "can't alloc device");
                continue;
            }
            devpriv         = usbi_get_device_priv(dev);
            dev->bus_number = bus_number;

            if(sunos_fill_in_dev_info(dn, dev) != LIBUSB_SUCCESS) {
                libusb_unref_device(dev);
                usbi_dbg(NULL, "get information fail");
                continue;
            }
            if(usbi_sanitize_device(dev) < 0) {
                libusb_unref_device(dev);
                usbi_dbg(NULL, "sanatize failed: ");
                return (DI_WALK_TERMINATE);
            }
        }
        else {
            devpriv = usbi_get_device_priv(dev);
            usbi_dbg(NULL, "Dev %s exists", devpriv->ugenpath);
        }

        if(discovered_devs_append(*(nargs->discdevs), dev) == NULL) {
            usbi_dbg(NULL, "cannot append device");
        }

        /*
         * we alloc and hence ref this dev. We don't need to ref it
         * hereafter. Front end or app should take care of their ref.
         */
        libusb_unref_device(dev);

        usbi_dbg(NULL, "Device %s %s id=0x%" PRIx64 ", devcount:%" PRIuPTR ", bdf=%" PRIx64, devpriv->ugenpath, di_devfs_path(dn), (uint64_t)sid,
                 (*nargs->discdevs)->len, bdf);
    }

    return (DI_WALK_CONTINUE);
}

static int sunos_walk_minor_node_link(di_node_t node, void *args) {
    di_minor_t           minor = DI_MINOR_NIL;
    char                *minor_path;
    struct devlink_cbarg arg;
    struct node_args    *nargs       = (struct node_args *)args;
    di_devlink_handle_t  devlink_hdl = nargs->dlink_hdl;

    /* walk each minor to find usb devices */
    while((minor = di_minor_next(node, minor)) != DI_MINOR_NIL) {
        minor_path = di_devfs_minor_path(minor);
        arg.nargs  = args;
        arg.myself = node;
        arg.minor  = minor;
        (void)di_devlink_walk(devlink_hdl, "^usb/hub[0-9]+", minor_path, DI_PRIMARY_LINK, (void *)&arg, sunos_add_devices);
        di_devfs_path_free(minor_path);
    }

    /* switch to a different node */
    nargs->last_ugenpath = NULL;

    return (DI_WALK_CONTINUE);
}

int sunos_get_device_list(struct libusb_context *ctx, struct discovered_devs **discdevs) {
    di_node_t           root_node;
    struct node_args    args;
    di_devlink_handle_t devlink_hdl;

    args.ctx           = ctx;
    args.discdevs      = discdevs;
    args.last_ugenpath = NULL;
    if((root_node = di_init("/", DINFOCPYALL)) == DI_NODE_NIL) {
        usbi_dbg(ctx, "di_int() failed: errno %d (%s)", errno, strerror(errno));
        return (LIBUSB_ERROR_IO);
    }

    if((devlink_hdl = di_devlink_init(NULL, 0)) == NULL) {
        di_fini(root_node);
        usbi_dbg(ctx, "di_devlink_init() failed: errno %d (%s)", errno, strerror(errno));

        return (LIBUSB_ERROR_IO);
    }
    args.dlink_hdl = devlink_hdl;

    /* walk each node to find USB devices */
    if(di_walk_node(root_node, DI_WALK_SIBFIRST, &args, sunos_walk_minor_node_link) == -1) {
        usbi_dbg(ctx, "di_walk_node() failed: errno %d (%s)", errno, strerror(errno));
        di_fini(root_node);

        return (LIBUSB_ERROR_IO);
    }

    di_fini(root_node);
    di_devlink_fini(&devlink_hdl);

    usbi_dbg(ctx, "%zu devices", (*discdevs)->len);

    return ((*discdevs)->len);
}

static int sunos_usb_open_ep0(sunos_dev_handle_priv_t *hpriv, sunos_dev_priv_t *dpriv) {
    char filename[PATH_MAX + 1];

    if(hpriv->eps[0].datafd > 0) {
        return (LIBUSB_SUCCESS);
    }
    snprintf(filename, PATH_MAX, "%s/cntrl0", dpriv->ugenpath);

    usbi_dbg(NULL, "opening %s", filename);
    hpriv->eps[0].datafd = open(filename, O_RDWR);
    if(hpriv->eps[0].datafd < 0) {
        return (_errno_to_libusb(errno));
    }

    snprintf(filename, PATH_MAX, "%s/cntrl0stat", dpriv->ugenpath);
    hpriv->eps[0].statfd = open(filename, O_RDONLY);
    if(hpriv->eps[0].statfd < 0) {
        close(hpriv->eps[0].datafd);
        hpriv->eps[0].datafd = -1;

        return (_errno_to_libusb(errno));
    }

    return (LIBUSB_SUCCESS);
}

static void sunos_usb_close_all_eps(sunos_dev_handle_priv_t *hdev) {
    int i;

    /* not close ep0 */
    for(i = 1; i < USB_MAXENDPOINTS; i++) {
        if(hdev->eps[i].datafd != -1) {
            (void)close(hdev->eps[i].datafd);
            hdev->eps[i].datafd = -1;
        }
        if(hdev->eps[i].statfd != -1) {
            (void)close(hdev->eps[i].statfd);
            hdev->eps[i].statfd = -1;
        }
    }
}

static void sunos_usb_close_ep0(sunos_dev_handle_priv_t *hdev) {
    if(hdev->eps[0].datafd >= 0) {
        close(hdev->eps[0].datafd);
        close(hdev->eps[0].statfd);
        hdev->eps[0].datafd = -1;
        hdev->eps[0].statfd = -1;
    }
}

static uchar_t sunos_usb_ep_index(uint8_t ep_addr) {
    return ((ep_addr & LIBUSB_ENDPOINT_ADDRESS_MASK) + ((ep_addr & LIBUSB_ENDPOINT_DIR_MASK) ? 16 : 0));
}

static int sunos_find_interface(struct libusb_device_handle *hdev, uint8_t endpoint, uint8_t *interface) {
    struct libusb_config_descriptor *config;
    int                              r;
    int                              iface_idx;

    r = libusb_get_active_config_descriptor(hdev->dev, &config);
    if(r < 0) {
        return (LIBUSB_ERROR_INVALID_PARAM);
    }

    for(iface_idx = 0; iface_idx < config->bNumInterfaces; iface_idx++) {
        const struct libusb_interface *iface = &config->interface[iface_idx];
        int                            altsetting_idx;

        for(altsetting_idx = 0; altsetting_idx < iface->num_altsetting; altsetting_idx++) {
            const struct libusb_interface_descriptor *altsetting = &iface->altsetting[altsetting_idx];
            int                                       ep_idx;

            for(ep_idx = 0; ep_idx < altsetting->bNumEndpoints; ep_idx++) {
                const struct libusb_endpoint_descriptor *ep = &altsetting->endpoint[ep_idx];
                if(ep->bEndpointAddress == endpoint) {
                    *interface = iface_idx;
                    libusb_free_config_descriptor(config);

                    return (LIBUSB_SUCCESS);
                }
            }
        }
    }
    libusb_free_config_descriptor(config);

    return (LIBUSB_ERROR_INVALID_PARAM);
}

static int sunos_check_device_and_status_open(struct libusb_device_handle *hdl, uint8_t ep_addr, int ep_type) {
    char                     filename[PATH_MAX + 1], statfilename[PATH_MAX + 1];
    char                     cfg_num[16], alt_num[16];
    int                      fd, fdstat, mode;
    uint8_t                  ifc = 0;
    uint8_t                  ep_index;
    sunos_dev_handle_priv_t *hpriv;

    usbi_dbg(HANDLE_CTX(hdl), "open ep 0x%02x", ep_addr);
    hpriv    = usbi_get_device_handle_priv(hdl);
    ep_index = sunos_usb_ep_index(ep_addr);
    /* ep already opened */
    if((hpriv->eps[ep_index].datafd > 0) && (hpriv->eps[ep_index].statfd > 0)) {
        usbi_dbg(HANDLE_CTX(hdl), "ep 0x%02x already opened, return success", ep_addr);

        return (0);
    }

    if(sunos_find_interface(hdl, ep_addr, &ifc) < 0) {
        usbi_dbg(HANDLE_CTX(hdl), "can't find interface for endpoint 0x%02x", ep_addr);

        return (EACCES);
    }

    /* create filename */
    if(hpriv->config_index > 0) {
        (void)snprintf(cfg_num, sizeof(cfg_num), "cfg%d", hpriv->config_index + 1);
    }
    else {
        bzero(cfg_num, sizeof(cfg_num));
    }

    if(hpriv->altsetting[ifc] > 0) {
        (void)snprintf(alt_num, sizeof(alt_num), ".%d", hpriv->altsetting[ifc]);
    }
    else {
        bzero(alt_num, sizeof(alt_num));
    }

    (void)snprintf(filename, PATH_MAX, "%s/%sif%d%s%s%d", hpriv->dpriv->ugenpath, cfg_num, ifc, alt_num, (ep_addr & LIBUSB_ENDPOINT_DIR_MASK) ? "in" : "out",
                   (ep_addr & LIBUSB_ENDPOINT_ADDRESS_MASK));
    (void)snprintf(statfilename, PATH_MAX, "%sstat", filename);

    /*
     * In case configuration has been switched, the xfer endpoint needs
     * to be opened before the status endpoint, due to a ugen issue.
     * However, to enable the one transfer mode for an Interrupt-In pipe,
     * the status endpoint needs to be opened before the xfer endpoint.
     * So, open the xfer mode first and close it immediately
     * as a workaround. This will handle the configuration switch.
     * Then, open the status endpoint.  If for an Interrupt-in pipe,
     * write the USB_EP_INTR_ONE_XFER control to the status endpoint
     * to enable the one transfer mode.  Then, re-open the xfer mode.
     */
    if(ep_type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS) {
        mode = O_RDWR;
    }
    else if(ep_addr & LIBUSB_ENDPOINT_IN) {
        mode = O_RDONLY;
    }
    else {
        mode = O_WRONLY;
    }
    /* Open the xfer endpoint first */
    if((fd = open(filename, mode)) == -1) {
        usbi_dbg(HANDLE_CTX(hdl), "can't open %s: errno %d (%s)", filename, errno, strerror(errno));

        return (errno);
    }
    /* And immediately close the xfer endpoint */
    (void)close(fd);

    /*
     * Open the status endpoint.
     * If for an Interrupt-IN pipe, need to enable the one transfer mode
     * by writing USB_EP_INTR_ONE_XFER control to the status endpoint
     * before opening the xfer endpoint
     */
    if((ep_type == LIBUSB_TRANSFER_TYPE_INTERRUPT) && (ep_addr & LIBUSB_ENDPOINT_IN)) {
        char    control = USB_EP_INTR_ONE_XFER;
        ssize_t count;

        /* Open the status endpoint with RDWR */
        if((fdstat = open(statfilename, O_RDWR)) == -1) {
            usbi_dbg(HANDLE_CTX(hdl), "can't open %s RDWR: errno %d (%s)", statfilename, errno, strerror(errno));

            return (errno);
        }
        else {
            count = write(fdstat, &control, sizeof(control));
            if(count != 1) {
                /* this should have worked */
                usbi_dbg(HANDLE_CTX(hdl), "can't write to %s: errno %d (%s)", statfilename, errno, strerror(errno));
                (void)close(fdstat);

                return (errno);
            }
        }
    }
    else {
        if((fdstat = open(statfilename, O_RDONLY)) == -1) {
            usbi_dbg(HANDLE_CTX(hdl), "can't open %s: errno %d (%s)", statfilename, errno, strerror(errno));

            return (errno);
        }
    }

    /* Re-open the xfer endpoint */
    if((fd = open(filename, mode)) == -1) {
        usbi_dbg(HANDLE_CTX(hdl), "can't open %s: errno %d (%s)", filename, errno, strerror(errno));
        (void)close(fdstat);

        return (errno);
    }

    hpriv->eps[ep_index].datafd = fd;
    hpriv->eps[ep_index].statfd = fdstat;
    usbi_dbg(HANDLE_CTX(hdl), "ep=0x%02x datafd=%d, statfd=%d", ep_addr, fd, fdstat);

    return (0);
}

int sunos_open(struct libusb_device_handle *handle) {
    sunos_dev_handle_priv_t *hpriv;
    sunos_dev_priv_t        *dpriv;
    int                      i;
    int                      ret;

    hpriv        = usbi_get_device_handle_priv(handle);
    dpriv        = usbi_get_device_priv(handle->dev);
    hpriv->dpriv = dpriv;

    /* set all file descriptors to "closed" */
    for(i = 0; i < USB_MAXENDPOINTS; i++) {
        hpriv->eps[i].datafd = -1;
        hpriv->eps[i].statfd = -1;
    }

    if(sunos_kernel_driver_active(handle, 0)) {
        /* pretend we can open the device */
        return (LIBUSB_SUCCESS);
    }

    if((ret = sunos_usb_open_ep0(hpriv, dpriv)) != LIBUSB_SUCCESS) {
        usbi_dbg(HANDLE_CTX(handle), "fail: %d", ret);
        return (ret);
    }

    return (LIBUSB_SUCCESS);
}

void sunos_close(struct libusb_device_handle *handle) {
    sunos_dev_handle_priv_t *hpriv;

    usbi_dbg(HANDLE_CTX(handle), " ");

    hpriv = usbi_get_device_handle_priv(handle);

    sunos_usb_close_all_eps(hpriv);
    sunos_usb_close_ep0(hpriv);
}

int sunos_get_active_config_descriptor(struct libusb_device *dev, void *buf, size_t len) {
    sunos_dev_priv_t                *dpriv = usbi_get_device_priv(dev);
    struct libusb_config_descriptor *cfg;
    int                              proplen;
    di_node_t                        node;
    uint8_t                         *rdata;

    /*
     * Keep raw configuration descriptors updated, in case config
     * has ever been changed through setCfg.
     */
    if((node = di_init(dpriv->phypath, DINFOCPYALL)) == DI_NODE_NIL) {
        usbi_dbg(DEVICE_CTX(dev), "di_int() failed: errno %d (%s)", errno, strerror(errno));
        return (LIBUSB_ERROR_IO);
    }
    proplen = di_prop_lookup_bytes(DDI_DEV_T_ANY, node, "usb-raw-cfg-descriptors", &rdata);
    if(proplen <= 0) {
        usbi_dbg(DEVICE_CTX(dev), "can't find raw config descriptors");

        return (LIBUSB_ERROR_IO);
    }
    dpriv->raw_cfgdescr = realloc(dpriv->raw_cfgdescr, proplen);
    if(dpriv->raw_cfgdescr == NULL) {
        return (LIBUSB_ERROR_NO_MEM);
    }
    else {
        bcopy(rdata, dpriv->raw_cfgdescr, proplen);
        dpriv->cfgvalue = ((struct libusb_config_descriptor *)rdata)->bConfigurationValue;
    }
    di_fini(node);

    cfg = (struct libusb_config_descriptor *)dpriv->raw_cfgdescr;
    len = MIN(len, libusb_le16_to_cpu(cfg->wTotalLength));
    memcpy(buf, dpriv->raw_cfgdescr, len);
    usbi_dbg(DEVICE_CTX(dev), "path:%s len %zu", dpriv->phypath, len);

    return (len);
}

int sunos_get_config_descriptor(struct libusb_device *dev, uint8_t idx, void *buf, size_t len) {
    UNUSED(idx);
    /* XXX */
    return (sunos_get_active_config_descriptor(dev, buf, len));
}

int sunos_get_configuration(struct libusb_device_handle *handle, uint8_t *config) {
    sunos_dev_priv_t *dpriv = usbi_get_device_priv(handle->dev);

    *config = dpriv->cfgvalue;

    usbi_dbg(HANDLE_CTX(handle), "bConfigurationValue %u", *config);

    return (LIBUSB_SUCCESS);
}

int sunos_set_configuration(struct libusb_device_handle *handle, int config) {
    sunos_dev_priv_t        *dpriv = usbi_get_device_priv(handle->dev);
    sunos_dev_handle_priv_t *hpriv;

    usbi_dbg(HANDLE_CTX(handle), "bConfigurationValue %d", config);
    hpriv = usbi_get_device_handle_priv(handle);

    if(dpriv->ugenpath == NULL)
        return (LIBUSB_ERROR_NOT_SUPPORTED);

    if(config < 1)
        return (LIBUSB_ERROR_NOT_SUPPORTED);

    dpriv->cfgvalue     = config;
    hpriv->config_index = config - 1;

    return (LIBUSB_SUCCESS);
}

int sunos_claim_interface(struct libusb_device_handle *handle, uint8_t iface) {
    UNUSED(handle);

    usbi_dbg(HANDLE_CTX(handle), "iface %u", iface);

    return (LIBUSB_SUCCESS);
}

int sunos_release_interface(struct libusb_device_handle *handle, uint8_t iface) {
    sunos_dev_handle_priv_t *hpriv = usbi_get_device_handle_priv(handle);

    usbi_dbg(HANDLE_CTX(handle), "iface %u", iface);

    /* XXX: can we release it? */
    hpriv->altsetting[iface] = 0;

    return (LIBUSB_SUCCESS);
}

int sunos_set_interface_altsetting(struct libusb_device_handle *handle, uint8_t iface, uint8_t altsetting) {
    sunos_dev_priv_t        *dpriv = usbi_get_device_priv(handle->dev);
    sunos_dev_handle_priv_t *hpriv = usbi_get_device_handle_priv(handle);

    usbi_dbg(HANDLE_CTX(handle), "iface %u, setting %u", iface, altsetting);

    if(dpriv->ugenpath == NULL)
        return (LIBUSB_ERROR_NOT_FOUND);

    /* XXX: can we switch altsetting? */
    hpriv->altsetting[iface] = altsetting;

    return (LIBUSB_SUCCESS);
}

static void usb_dump_data(const void *data, size_t size) {
    const uint8_t *p = data;
    size_t         i;

    if(getenv("LIBUSB_DEBUG") == NULL) {
        return;
    }

    (void)fprintf(stderr, "data dump:");
    for(i = 0; i < size; i++) {
        if(i % 16 == 0) {
            (void)fprintf(stderr, "\n%08zx	", i);
        }
        (void)fprintf(stderr, "%02x ", p[i]);
    }
    (void)fprintf(stderr, "\n");
}

static void sunos_async_callback(union sigval arg) {
    struct sunos_transfer_priv *tpriv = (struct sunos_transfer_priv *)arg.sival_ptr;
    struct libusb_transfer     *xfer  = tpriv->transfer;
    struct aiocb               *aiocb = &tpriv->aiocb;
    int                         ret;
    sunos_dev_handle_priv_t    *hpriv;
    uint8_t                     ep;
    libusb_device_handle       *dev_handle;

    dev_handle = xfer->dev_handle;

    /* libusb can forcibly interrupt transfer in do_close() */
    if(dev_handle != NULL) {
        hpriv = usbi_get_device_handle_priv(dev_handle);
        ep    = sunos_usb_ep_index(xfer->endpoint);

        ret = aio_error(aiocb);
        if(ret != 0) {
            xfer->status = sunos_usb_get_status(TRANSFER_CTX(xfer), hpriv->eps[ep].statfd);
        }
        else {
            xfer->actual_length = LIBUSB_TRANSFER_TO_USBI_TRANSFER(xfer)->transferred = aio_return(aiocb);
        }

        usb_dump_data(xfer->buffer, xfer->actual_length);

        usbi_dbg(TRANSFER_CTX(xfer), "ret=%d, len=%d, actual_len=%d", ret, xfer->length, xfer->actual_length);

        /* async notification */
        usbi_signal_transfer_completion(LIBUSB_TRANSFER_TO_USBI_TRANSFER(xfer));
    }
}

static int sunos_do_async_io(struct libusb_transfer *transfer) {
    int                         ret = -1;
    struct aiocb               *aiocb;
    sunos_dev_handle_priv_t    *hpriv;
    uint8_t                     ep;
    struct sunos_transfer_priv *tpriv;

    usbi_dbg(TRANSFER_CTX(transfer), " ");

    tpriv = usbi_get_transfer_priv(LIBUSB_TRANSFER_TO_USBI_TRANSFER(transfer));
    hpriv = usbi_get_device_handle_priv(transfer->dev_handle);
    ep    = sunos_usb_ep_index(transfer->endpoint);

    tpriv->transfer = transfer;
    aiocb           = &tpriv->aiocb;
    bzero(aiocb, sizeof(*aiocb));
    aiocb->aio_fildes                         = hpriv->eps[ep].datafd;
    aiocb->aio_buf                            = transfer->buffer;
    aiocb->aio_nbytes                         = transfer->length;
    aiocb->aio_lio_opcode                     = ((transfer->endpoint & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) ? LIO_READ : LIO_WRITE;
    aiocb->aio_sigevent.sigev_notify          = SIGEV_THREAD;
    aiocb->aio_sigevent.sigev_value.sival_ptr = tpriv;
    aiocb->aio_sigevent.sigev_notify_function = sunos_async_callback;

    if(aiocb->aio_lio_opcode == LIO_READ) {
        ret = aio_read(aiocb);
    }
    else {
        ret = aio_write(aiocb);
    }

    return (ret);
}

/* return the number of bytes read/written */
static ssize_t usb_do_io(struct libusb_context *ctx, int fd, int stat_fd, void *data, size_t size, int flag, int *status) {
    int     error;
    ssize_t ret = -1;

    usbi_dbg(ctx, "usb_do_io(): datafd=%d statfd=%d size=0x%zx flag=%s", fd, stat_fd, size, flag ? "WRITE" : "READ");

    switch(flag) {
    case READ:
        errno = 0;
        ret   = read(fd, data, size);
        usb_dump_data(data, size);
        break;
    case WRITE:
        usb_dump_data(data, size);
        errno = 0;
        ret   = write(fd, data, size);
        break;
    }

    usbi_dbg(ctx, "usb_do_io(): amount=%zd", ret);

    if(ret < 0) {
        int save_errno = errno;

        usbi_dbg(ctx, "TID=%x io %s errno %d (%s)", pthread_self(), flag ? "WRITE" : "READ", errno, strerror(errno));

        /* sunos_usb_get_status will do a read and overwrite errno */
        error = sunos_usb_get_status(ctx, stat_fd);
        usbi_dbg(ctx, "io status=%d errno %d (%s)", error, save_errno, strerror(save_errno));

        if(status) {
            *status = save_errno;
        }

        return (save_errno);
    }
    else if(status) {
        *status = 0;
    }

    return (ret);
}

static int solaris_submit_ctrl_on_default(struct libusb_transfer *transfer) {
    ssize_t                      ret = -1, setup_ret;
    int                          status;
    sunos_dev_handle_priv_t     *hpriv;
    struct libusb_device_handle *hdl = transfer->dev_handle;
    uint16_t                     wLength;
    uint8_t                     *data = transfer->buffer;

    hpriv   = usbi_get_device_handle_priv(hdl);
    wLength = transfer->length - LIBUSB_CONTROL_SETUP_SIZE;

    if(hpriv->eps[0].datafd == -1) {
        usbi_dbg(TRANSFER_CTX(transfer), "ep0 not opened");

        return (LIBUSB_ERROR_NOT_FOUND);
    }

    if((data[0] & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
        usbi_dbg(TRANSFER_CTX(transfer), "IN request");
        ret = usb_do_io(TRANSFER_CTX(transfer), hpriv->eps[0].datafd, hpriv->eps[0].statfd, data, LIBUSB_CONTROL_SETUP_SIZE, WRITE, &status);
    }
    else {
        usbi_dbg(TRANSFER_CTX(transfer), "OUT request");
        ret =
            usb_do_io(TRANSFER_CTX(transfer), hpriv->eps[0].datafd, hpriv->eps[0].statfd, transfer->buffer, transfer->length, WRITE, (int *)&transfer->status);
    }

    setup_ret = ret;
    if(ret < (ssize_t)LIBUSB_CONTROL_SETUP_SIZE) {
        usbi_dbg(TRANSFER_CTX(transfer), "error sending control msg: %zd", ret);

        return (LIBUSB_ERROR_IO);
    }

    ret = transfer->length - LIBUSB_CONTROL_SETUP_SIZE;

    /* Read the remaining bytes for IN request */
    if((wLength) && ((data[0] & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN)) {
        usbi_dbg(TRANSFER_CTX(transfer), "DATA: %d", transfer->length - (int)setup_ret);
        ret = usb_do_io(TRANSFER_CTX(transfer), hpriv->eps[0].datafd, hpriv->eps[0].statfd, transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE, wLength, READ,
                        (int *)&transfer->status);
    }

    if(ret >= 0) {
        LIBUSB_TRANSFER_TO_USBI_TRANSFER(transfer)->transferred = ret;
    }
    usbi_dbg(TRANSFER_CTX(transfer), "Done: ctrl data bytes %zd", ret);

    /**
     * Sync transfer handling.
     * We should release transfer lock here and later get it back
     * as usbi_handle_transfer_completion() takes its own transfer lock.
     */
    usbi_mutex_unlock(&LIBUSB_TRANSFER_TO_USBI_TRANSFER(transfer)->lock);
    ret = usbi_handle_transfer_completion(LIBUSB_TRANSFER_TO_USBI_TRANSFER(transfer), transfer->status);
    usbi_mutex_lock(&LIBUSB_TRANSFER_TO_USBI_TRANSFER(transfer)->lock);

    return (ret);
}

int sunos_clear_halt(struct libusb_device_handle *handle, unsigned char endpoint) {
    int ret;

    usbi_dbg(HANDLE_CTX(handle), "endpoint=0x%02x", endpoint);

    ret = libusb_control_transfer(handle, LIBUSB_ENDPOINT_OUT | LIBUSB_RECIPIENT_ENDPOINT | LIBUSB_REQUEST_TYPE_STANDARD, LIBUSB_REQUEST_CLEAR_FEATURE, 0,
                                  endpoint, NULL, 0, 1000);

    usbi_dbg(HANDLE_CTX(handle), "ret=%d", ret);

    return (ret);
}

void sunos_destroy_device(struct libusb_device *dev) {
    sunos_dev_priv_t *dpriv = usbi_get_device_priv(dev);

    usbi_dbg(DEVICE_CTX(dev), "destroy everything");
    free(dpriv->raw_cfgdescr);
    free(dpriv->ugenpath);
    free(dpriv->phypath);
}

int sunos_submit_transfer(struct usbi_transfer *itransfer) {
    struct libusb_transfer      *transfer;
    struct libusb_device_handle *hdl;
    int                          err = 0;

    transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    hdl      = transfer->dev_handle;

    err = sunos_check_device_and_status_open(hdl, transfer->endpoint, transfer->type);
    if(err < 0) {

        return (_errno_to_libusb(err));
    }

    switch(transfer->type) {
    case LIBUSB_TRANSFER_TYPE_CONTROL:
        /* sync transfer */
        usbi_dbg(ITRANSFER_CTX(itransfer), "CTRL transfer: %d", transfer->length);
        err = solaris_submit_ctrl_on_default(transfer);
        break;

    case LIBUSB_TRANSFER_TYPE_BULK:
        /* fallthru */
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
        if(transfer->type == LIBUSB_TRANSFER_TYPE_BULK)
            usbi_dbg(ITRANSFER_CTX(itransfer), "BULK transfer: %d", transfer->length);
        else
            usbi_dbg(ITRANSFER_CTX(itransfer), "INTR transfer: %d", transfer->length);
        err = sunos_do_async_io(transfer);
        break;

    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
        /* Isochronous/Stream is not supported */

        /* fallthru */
    case LIBUSB_TRANSFER_TYPE_BULK_STREAM:
        if(transfer->type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS)
            usbi_dbg(ITRANSFER_CTX(itransfer), "ISOC transfer: %d", transfer->length);
        else
            usbi_dbg(ITRANSFER_CTX(itransfer), "BULK STREAM transfer: %d", transfer->length);
        err = LIBUSB_ERROR_NOT_SUPPORTED;
        break;
    }

    return (err);
}

int sunos_cancel_transfer(struct usbi_transfer *itransfer) {
    sunos_xfer_priv_t       *tpriv;
    sunos_dev_handle_priv_t *hpriv;
    struct libusb_transfer  *transfer;
    struct aiocb            *aiocb;
    uint8_t                  ep;
    int                      ret;

    tpriv    = usbi_get_transfer_priv(itransfer);
    aiocb    = &tpriv->aiocb;
    transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
    hpriv    = usbi_get_device_handle_priv(transfer->dev_handle);
    ep       = sunos_usb_ep_index(transfer->endpoint);

    ret = aio_cancel(hpriv->eps[ep].datafd, aiocb);

    usbi_dbg(ITRANSFER_CTX(itransfer), "aio->fd=%d fd=%d ret = %d, %s", aiocb->aio_fildes, hpriv->eps[ep].datafd, ret,
             (ret == AIO_CANCELED) ? strerror(0) : strerror(errno));

    if(ret != AIO_CANCELED) {
        ret = _errno_to_libusb(errno);
    }
    else {
        /*
         * we don't need to call usbi_handle_transfer_cancellation(),
         * because we'll handle everything in sunos_async_callback.
         */
        ret = LIBUSB_SUCCESS;
    }

    return (ret);
}

int sunos_handle_transfer_completion(struct usbi_transfer *itransfer) {
    return usbi_handle_transfer_completion(itransfer, LIBUSB_TRANSFER_COMPLETED);
}

int _errno_to_libusb(int err) {
    usbi_dbg(NULL, "error: %s (%d)", strerror(err), err);

    switch(err) {
    case EIO:
        return (LIBUSB_ERROR_IO);
    case EACCES:
        return (LIBUSB_ERROR_ACCESS);
    case ENOENT:
        return (LIBUSB_ERROR_NO_DEVICE);
    case ENOMEM:
        return (LIBUSB_ERROR_NO_MEM);
    case ETIMEDOUT:
        return (LIBUSB_ERROR_TIMEOUT);
    }

    return (LIBUSB_ERROR_OTHER);
}

/*
 * sunos_usb_get_status:
 *	gets status of endpoint
 *
 * Returns: ugen's last cmd status
 */
static int sunos_usb_get_status(struct libusb_context *ctx, int fd) {
    int     status;
    ssize_t ret;

    usbi_dbg(ctx, "sunos_usb_get_status(): fd=%d", fd);

    ret = read(fd, &status, sizeof(status));
    if(ret == sizeof(status)) {
        switch(status) {
        case USB_LC_STAT_NOERROR:
            usbi_dbg(ctx, "No Error");
            break;
        case USB_LC_STAT_CRC:
            usbi_dbg(ctx, "CRC Timeout Detected\n");
            break;
        case USB_LC_STAT_BITSTUFFING:
            usbi_dbg(ctx, "Bit Stuffing Violation\n");
            break;
        case USB_LC_STAT_DATA_TOGGLE_MM:
            usbi_dbg(ctx, "Data Toggle Mismatch\n");
            break;
        case USB_LC_STAT_STALL:
            usbi_dbg(ctx, "End Point Stalled\n");
            break;
        case USB_LC_STAT_DEV_NOT_RESP:
            usbi_dbg(ctx, "Device is Not Responding\n");
            break;
        case USB_LC_STAT_PID_CHECKFAILURE:
            usbi_dbg(ctx, "PID Check Failure\n");
            break;
        case USB_LC_STAT_UNEXP_PID:
            usbi_dbg(ctx, "Unexpected PID\n");
            break;
        case USB_LC_STAT_DATA_OVERRUN:
            usbi_dbg(ctx, "Data Exceeded Size\n");
            break;
        case USB_LC_STAT_DATA_UNDERRUN:
            usbi_dbg(ctx, "Less data received\n");
            break;
        case USB_LC_STAT_BUFFER_OVERRUN:
            usbi_dbg(ctx, "Buffer Size Exceeded\n");
            break;
        case USB_LC_STAT_BUFFER_UNDERRUN:
            usbi_dbg(ctx, "Buffer Underrun\n");
            break;
        case USB_LC_STAT_TIMEOUT:
            usbi_dbg(ctx, "Command Timed Out\n");
            break;
        case USB_LC_STAT_NOT_ACCESSED:
            usbi_dbg(ctx, "Not Accessed by h/w\n");
            break;
        case USB_LC_STAT_UNSPECIFIED_ERR:
            usbi_dbg(ctx, "Unspecified Error\n");
            break;
        case USB_LC_STAT_NO_BANDWIDTH:
            usbi_dbg(ctx, "No Bandwidth\n");
            break;
        case USB_LC_STAT_HW_ERR:
            usbi_dbg(ctx, "Host Controller h/w Error\n");
            break;
        case USB_LC_STAT_SUSPENDED:
            usbi_dbg(ctx, "Device was Suspended\n");
            break;
        case USB_LC_STAT_DISCONNECTED:
            usbi_dbg(ctx, "Device was Disconnected\n");
            break;
        case USB_LC_STAT_INTR_BUF_FULL:
            usbi_dbg(ctx, "Interrupt buffer was full\n");
            break;
        case USB_LC_STAT_INVALID_REQ:
            usbi_dbg(ctx, "Request was Invalid\n");
            break;
        case USB_LC_STAT_INTERRUPTED:
            usbi_dbg(ctx, "Request was Interrupted\n");
            break;
        case USB_LC_STAT_NO_RESOURCES:
            usbi_dbg(ctx, "No resources available for "
                          "request\n");
            break;
        case USB_LC_STAT_INTR_POLLING_FAILED:
            usbi_dbg(ctx, "Failed to Restart Poll");
            break;
        default:
            usbi_dbg(ctx, "Error Not Determined %d\n", status);
            break;
        }
    }
    else {
        usbi_dbg(ctx, "read stat error: %s", strerror(errno));
        status = -1;
    }

    return (status);
}

const struct usbi_os_backend usbi_backend = {
    .name                         = "Solaris",
    .caps                         = 0,
    .get_device_list              = sunos_get_device_list,
    .get_active_config_descriptor = sunos_get_active_config_descriptor,
    .get_config_descriptor        = sunos_get_config_descriptor,
    .open                         = sunos_open,
    .close                        = sunos_close,
    .get_configuration            = sunos_get_configuration,
    .set_configuration            = sunos_set_configuration,
    .claim_interface              = sunos_claim_interface,
    .release_interface            = sunos_release_interface,
    .set_interface_altsetting     = sunos_set_interface_altsetting,
    .clear_halt                   = sunos_clear_halt,
    .kernel_driver_active         = sunos_kernel_driver_active,
    .detach_kernel_driver         = sunos_detach_kernel_driver,
    .attach_kernel_driver         = sunos_attach_kernel_driver,
    .destroy_device               = sunos_destroy_device,
    .submit_transfer              = sunos_submit_transfer,
    .cancel_transfer              = sunos_cancel_transfer,
    .handle_transfer_completion   = sunos_handle_transfer_completion,
    .device_priv_size             = sizeof(sunos_dev_priv_t),
    .device_handle_priv_size      = sizeof(sunos_dev_handle_priv_t),
    .transfer_priv_size           = sizeof(sunos_xfer_priv_t),
};
