/*
 * libusb example program to manipulate U.are.U 4000B fingerprint scanner.
 * Copyright © 2007 Daniel Drake <dsd@gentoo.org>
 * Copyright © 2016 Nathan Hjelm <hjelmn@mac.com>
 * Copyright © 2020 Chris Dickens <christopher.a.dickens@gmail.com>
 *
 * Basic image capture program only, does not consider the powerup quirks or
 * the fact that image encryption may be enabled. Not expected to work
 * flawlessly all of the time.
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

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libusb.h"

#if defined(_MSC_VER)
#define snprintf _snprintf
#endif

#if defined(DPFP_THREADED)
#if defined(PLATFORM_POSIX)
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define THREAD_RETURN_VALUE NULL
typedef sem_t    *semaphore_t;
typedef pthread_t thread_t;

static inline semaphore_t semaphore_create(void) {
    sem_t *semaphore;
    char   name[50];

    snprintf(name, sizeof(name), "/org.libusb.example.dpfp_threaded:%d", (int)getpid());
    semaphore = sem_open(name, O_CREAT | O_EXCL, 0, 0);
    if(semaphore == SEM_FAILED)
        return NULL;
    /* Remove semaphore so that it does not persist after process exits */
    (void)sem_unlink(name);
    return semaphore;
}

static inline void semaphore_give(semaphore_t semaphore) {
    (void)sem_post(semaphore);
}

static inline void semaphore_take(semaphore_t semaphore) {
    (void)sem_wait(semaphore);
}

static inline void semaphore_destroy(semaphore_t semaphore) {
    (void)sem_close(semaphore);
}

static inline int thread_create(thread_t *thread, void *(*thread_entry)(void *arg), void *arg) {
    return pthread_create(thread, NULL, thread_entry, arg) == 0 ? 0 : -1;
}

static inline void thread_join(thread_t thread) {
    (void)pthread_join(thread, NULL);
}
#elif defined(PLATFORM_WINDOWS)
#define THREAD_RETURN_VALUE 0
typedef HANDLE semaphore_t;
typedef HANDLE thread_t;

#if defined(__CYGWIN__)
typedef DWORD  thread_return_t;
#else
#include <process.h>
typedef unsigned thread_return_t;
#endif

static inline semaphore_t semaphore_create(void) {
    return CreateSemaphore(NULL, 0, 1, NULL);
}

static inline void semaphore_give(semaphore_t semaphore) {
    (void)ReleaseSemaphore(semaphore, 1, NULL);
}

static inline void semaphore_take(semaphore_t semaphore) {
    (void)WaitForSingleObject(semaphore, INFINITE);
}

static inline void semaphore_destroy(semaphore_t semaphore) {
    (void)CloseHandle(semaphore);
}

static inline int thread_create(thread_t *thread, thread_return_t(__stdcall *thread_entry)(void *arg), void *arg) {
#if defined(__CYGWIN__)
    *thread = CreateThread(NULL, 0, thread_entry, arg, 0, NULL);
#else
    *thread = (HANDLE)_beginthreadex(NULL, 0, thread_entry, arg, 0, NULL);
#endif
    return *thread != NULL ? 0 : -1;
}

static inline void thread_join(thread_t thread) {
    (void)WaitForSingleObject(thread, INFINITE);
    (void)CloseHandle(thread);
}
#endif
#endif

#define EP_INTR (1 | LIBUSB_ENDPOINT_IN)
#define EP_DATA (2 | LIBUSB_ENDPOINT_IN)
#define CTRL_IN (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN)
#define CTRL_OUT (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT)
#define USB_RQ 0x04
#define INTR_LENGTH 64

enum {
    MODE_INIT             = 0x00,
    MODE_AWAIT_FINGER_ON  = 0x10,
    MODE_AWAIT_FINGER_OFF = 0x12,
    MODE_CAPTURE          = 0x20,
    MODE_SHUT_UP          = 0x30,
    MODE_READY            = 0x80,
};

static int next_state(void);

enum {
    STATE_AWAIT_MODE_CHANGE_AWAIT_FINGER_ON = 1,
    STATE_AWAIT_IRQ_FINGER_DETECTED,
    STATE_AWAIT_MODE_CHANGE_CAPTURE,
    STATE_AWAIT_IMAGE,
    STATE_AWAIT_MODE_CHANGE_AWAIT_FINGER_OFF,
    STATE_AWAIT_IRQ_FINGER_REMOVED,
};

static int                     state = 0;
static libusb_device_handle   *devh  = NULL;
static unsigned char           imgbuf[0x1b340];
static unsigned char           irqbuf[INTR_LENGTH];
static struct libusb_transfer *img_transfer = NULL;
static struct libusb_transfer *irq_transfer = NULL;
static int                     img_idx      = 0;
static volatile sig_atomic_t   do_exit      = 0;

#if defined(DPFP_THREADED)
static semaphore_t exit_semaphore;
static thread_t    poll_thread;
#endif

static void request_exit(sig_atomic_t code) {
    do_exit = code;
#if defined(DPFP_THREADED)
    semaphore_give(exit_semaphore);
#endif
}

#if defined(DPFP_THREADED)
#if defined(PLATFORM_POSIX)
static void *poll_thread_main(void *arg)
#elif defined(PLATFORM_WINDOWS)
static thread_return_t __stdcall poll_thread_main(void *arg)
#endif
{
    (void)arg;

    printf("poll thread running\n");

    while(!do_exit) {
        struct timeval tv = { 1, 0 };
        int            r;

        r = libusb_handle_events_timeout(NULL, &tv);
        if(r < 0) {
            request_exit(2);
            break;
        }
    }

    printf("poll thread shutting down\n");
    return THREAD_RETURN_VALUE;
}
#endif

static int find_dpfp_device(void) {
    devh = libusb_open_device_with_vid_pid(NULL, 0x05ba, 0x000a);
    if(!devh) {
        errno = ENODEV;
        return -1;
    }
    return 0;
}

static int print_f0_data(void) {
    unsigned char data[0x10];
    size_t        i;
    int           r;

    r = libusb_control_transfer(devh, CTRL_IN, USB_RQ, 0xf0, 0, data, sizeof(data), 0);
    if(r < 0) {
        fprintf(stderr, "F0 error %d\n", r);
        return r;
    }
    if(r < (int)sizeof(data)) {
        fprintf(stderr, "short read (%d)\n", r);
        return -1;
    }

    printf("F0 data:");
    for(i = 0; i < sizeof(data); i++)
        printf(" %02x", data[i]);
    printf("\n");
    return 0;
}

static int get_hwstat(unsigned char *status) {
    int r;

    r = libusb_control_transfer(devh, CTRL_IN, USB_RQ, 0x07, 0, status, 1, 0);
    if(r < 0) {
        fprintf(stderr, "read hwstat error %d\n", r);
        return r;
    }
    if(r < 1) {
        fprintf(stderr, "short read (%d)\n", r);
        return -1;
    }

    printf("hwstat reads %02x\n", *status);
    return 0;
}

static int set_hwstat(unsigned char data) {
    int r;

    printf("set hwstat to %02x\n", data);
    r = libusb_control_transfer(devh, CTRL_OUT, USB_RQ, 0x07, 0, &data, 1, 0);
    if(r < 0) {
        fprintf(stderr, "set hwstat error %d\n", r);
        return r;
    }
    if(r < 1) {
        fprintf(stderr, "short write (%d)\n", r);
        return -1;
    }

    return 0;
}

static int set_mode(unsigned char data) {
    int r;

    printf("set mode %02x\n", data);
    r = libusb_control_transfer(devh, CTRL_OUT, USB_RQ, 0x4e, 0, &data, 1, 0);
    if(r < 0) {
        fprintf(stderr, "set mode error %d\n", r);
        return r;
    }
    if(r < 1) {
        fprintf(stderr, "short write (%d)\n", r);
        return -1;
    }

    return 0;
}

static void LIBUSB_CALL cb_mode_changed(struct libusb_transfer *transfer) {
    if(transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "mode change transfer not completed!\n");
        request_exit(2);
    }

    printf("async cb_mode_changed length=%d actual_length=%d\n", transfer->length, transfer->actual_length);
    if(next_state() < 0)
        request_exit(2);
}

static int set_mode_async(unsigned char data) {
    unsigned char          *buf = malloc(LIBUSB_CONTROL_SETUP_SIZE + 1);
    struct libusb_transfer *transfer;

    if(!buf) {
        errno = ENOMEM;
        return -1;
    }

    transfer = libusb_alloc_transfer(0);
    if(!transfer) {
        free(buf);
        errno = ENOMEM;
        return -1;
    }

    printf("async set mode %02x\n", data);
    libusb_fill_control_setup(buf, CTRL_OUT, USB_RQ, 0x4e, 0, 1);
    buf[LIBUSB_CONTROL_SETUP_SIZE] = data;
    libusb_fill_control_transfer(transfer, devh, buf, cb_mode_changed, NULL, 1000);

    transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK | LIBUSB_TRANSFER_FREE_BUFFER | LIBUSB_TRANSFER_FREE_TRANSFER;
    return libusb_submit_transfer(transfer);
}

static int do_sync_intr(unsigned char *data) {
    int r;
    int transferred;

    r = libusb_interrupt_transfer(devh, EP_INTR, data, INTR_LENGTH, &transferred, 1000);
    if(r < 0) {
        fprintf(stderr, "intr error %d\n", r);
        return r;
    }
    if(transferred < INTR_LENGTH) {
        fprintf(stderr, "short read (%d)\n", r);
        return -1;
    }

    printf("recv interrupt %04x\n", *((uint16_t *)data));
    return 0;
}

static int sync_intr(unsigned char type) {
    int           r;
    unsigned char data[INTR_LENGTH];

    while(1) {
        r = do_sync_intr(data);
        if(r < 0)
            return r;
        if(data[0] == type)
            return 0;
    }
}

static int save_to_file(unsigned char *data) {
    FILE *f;
    char  filename[64];

    snprintf(filename, sizeof(filename), "finger%d.pgm", img_idx++);
    f = fopen(filename, "w");
    if(!f)
        return -1;

    fputs("P5 384 289 255 ", f);
    (void)fwrite(data + 64, 1, 384 * 289, f);
    fclose(f);
    printf("saved image to %s\n", filename);
    return 0;
}

static int next_state(void) {
    int r = 0;

    printf("old state: %d\n", state);
    switch(state) {
    case STATE_AWAIT_IRQ_FINGER_REMOVED:
        state = STATE_AWAIT_MODE_CHANGE_AWAIT_FINGER_ON;
        r     = set_mode_async(MODE_AWAIT_FINGER_ON);
        break;
    case STATE_AWAIT_MODE_CHANGE_AWAIT_FINGER_ON:
        state = STATE_AWAIT_IRQ_FINGER_DETECTED;
        break;
    case STATE_AWAIT_IRQ_FINGER_DETECTED:
        state = STATE_AWAIT_MODE_CHANGE_CAPTURE;
        r     = set_mode_async(MODE_CAPTURE);
        break;
    case STATE_AWAIT_MODE_CHANGE_CAPTURE:
        state = STATE_AWAIT_IMAGE;
        break;
    case STATE_AWAIT_IMAGE:
        state = STATE_AWAIT_MODE_CHANGE_AWAIT_FINGER_OFF;
        r     = set_mode_async(MODE_AWAIT_FINGER_OFF);
        break;
    case STATE_AWAIT_MODE_CHANGE_AWAIT_FINGER_OFF:
        state = STATE_AWAIT_IRQ_FINGER_REMOVED;
        break;
    default:
        printf("unrecognised state %d\n", state);
    }
    if(r < 0) {
        fprintf(stderr, "error detected changing state\n");
        return r;
    }

    printf("new state: %d\n", state);
    return 0;
}

static void LIBUSB_CALL cb_irq(struct libusb_transfer *transfer) {
    unsigned char irqtype = transfer->buffer[0];

    if(transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "irq transfer status %d?\n", transfer->status);
        goto err_free_transfer;
    }

    printf("IRQ callback %02x\n", irqtype);
    switch(state) {
    case STATE_AWAIT_IRQ_FINGER_DETECTED:
        if(irqtype == 0x01) {
            if(next_state() < 0)
                goto err_free_transfer;
        }
        else {
            printf("finger-on-sensor detected in wrong state!\n");
        }
        break;
    case STATE_AWAIT_IRQ_FINGER_REMOVED:
        if(irqtype == 0x02) {
            if(next_state() < 0)
                goto err_free_transfer;
        }
        else {
            printf("finger-on-sensor detected in wrong state!\n");
        }
        break;
    }
    if(libusb_submit_transfer(irq_transfer) < 0)
        goto err_free_transfer;

    return;

err_free_transfer:
    libusb_free_transfer(transfer);
    irq_transfer = NULL;
    request_exit(2);
}

static void LIBUSB_CALL cb_img(struct libusb_transfer *transfer) {
    if(transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "img transfer status %d?\n", transfer->status);
        goto err_free_transfer;
    }

    printf("Image callback\n");
    save_to_file(imgbuf);
    if(next_state() < 0)
        goto err_free_transfer;

    if(libusb_submit_transfer(img_transfer) < 0)
        goto err_free_transfer;

    return;

err_free_transfer:
    libusb_free_transfer(transfer);
    img_transfer = NULL;
    request_exit(2);
}

static int init_capture(void) {
    int r;

    r = libusb_submit_transfer(irq_transfer);
    if(r < 0)
        return r;

    r = libusb_submit_transfer(img_transfer);
    if(r < 0) {
        libusb_cancel_transfer(irq_transfer);
        while(irq_transfer)
            if(libusb_handle_events(NULL) < 0)
                break;
        return r;
    }

    /* start state machine */
    state = STATE_AWAIT_IRQ_FINGER_REMOVED;
    return next_state();
}

static int do_init(void) {
    unsigned char status;
    int           r;

    r = get_hwstat(&status);
    if(r < 0)
        return r;

    if(!(status & 0x80)) {
        r = set_hwstat(status | 0x80);
        if(r < 0)
            return r;
        r = get_hwstat(&status);
        if(r < 0)
            return r;
    }

    status &= ~0x80;
    r = set_hwstat(status);
    if(r < 0)
        return r;

    r = get_hwstat(&status);
    if(r < 0)
        return r;

    r = sync_intr(0x56);
    if(r < 0)
        return r;

    return 0;
}

static int alloc_transfers(void) {
    img_transfer = libusb_alloc_transfer(0);
    if(!img_transfer) {
        errno = ENOMEM;
        return -1;
    }

    irq_transfer = libusb_alloc_transfer(0);
    if(!irq_transfer) {
        errno = ENOMEM;
        return -1;
    }

    libusb_fill_bulk_transfer(img_transfer, devh, EP_DATA, imgbuf, sizeof(imgbuf), cb_img, NULL, 0);
    libusb_fill_interrupt_transfer(irq_transfer, devh, EP_INTR, irqbuf, sizeof(irqbuf), cb_irq, NULL, 0);

    return 0;
}

static void sighandler(int signum) {
    (void)signum;

    request_exit(1);
}

static void setup_signals(void) {
#if defined(PLATFORM_POSIX)
    struct sigaction sigact;

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    (void)sigaction(SIGINT, &sigact, NULL);
    (void)sigaction(SIGTERM, &sigact, NULL);
    (void)sigaction(SIGQUIT, &sigact, NULL);
#else
    (void)signal(SIGINT, sighandler);
    (void)signal(SIGTERM, sighandler);
#endif
}

int main(void) {
    int r;

    r = libusb_init_context(/*ctx=*/NULL, /*options=*/NULL, /*num_options=*/0);
    if(r < 0) {
        fprintf(stderr, "failed to initialise libusb %d - %s\n", r, libusb_strerror(r));
        exit(1);
    }

    r = find_dpfp_device();
    if(r < 0) {
        fprintf(stderr, "Could not find/open device\n");
        goto out;
    }

    r = libusb_claim_interface(devh, 0);
    if(r < 0) {
        fprintf(stderr, "claim interface error %d - %s\n", r, libusb_strerror(r));
        goto out;
    }
    printf("claimed interface\n");

    r = print_f0_data();
    if(r < 0)
        goto out_release;

    r = do_init();
    if(r < 0)
        goto out_deinit;

    /* async from here onwards */
    setup_signals();

    r = alloc_transfers();
    if(r < 0)
        goto out_deinit;

#if defined(DPFP_THREADED)
    exit_semaphore = semaphore_create();
    if(!exit_semaphore) {
        fprintf(stderr, "failed to initialise semaphore\n");
        goto out_deinit;
    }

    r = thread_create(&poll_thread, poll_thread_main, NULL);
    if(r) {
        semaphore_destroy(exit_semaphore);
        goto out_deinit;
    }

    r = init_capture();
    if(r < 0)
        request_exit(2);

    while(!do_exit)
        semaphore_take(exit_semaphore);
#else
    r = init_capture();
    if(r < 0)
        goto out_deinit;

    while(!do_exit) {
        r = libusb_handle_events(NULL);
        if(r < 0)
            request_exit(2);
    }
#endif

    printf("shutting down...\n");

#if defined(DPFP_THREADED)
    thread_join(poll_thread);
    semaphore_destroy(exit_semaphore);
#endif

    if(img_transfer) {
        r = libusb_cancel_transfer(img_transfer);
        if(r < 0)
            fprintf(stderr, "failed to cancel transfer %d - %s\n", r, libusb_strerror(r));
    }

    if(irq_transfer) {
        r = libusb_cancel_transfer(irq_transfer);
        if(r < 0)
            fprintf(stderr, "failed to cancel transfer %d - %s\n", r, libusb_strerror(r));
    }

    while(img_transfer || irq_transfer) {
        if(libusb_handle_events(NULL) < 0)
            break;
    }

    if(do_exit == 1)
        r = 0;
    else
        r = 1;

out_deinit:
    if(img_transfer)
        libusb_free_transfer(img_transfer);
    if(irq_transfer)
        libusb_free_transfer(irq_transfer);
    set_mode(0);
    set_hwstat(0x80);
out_release:
    libusb_release_interface(devh, 0);
out:
    libusb_close(devh);
    libusb_exit(NULL);
    return r >= 0 ? r : -r;
}
