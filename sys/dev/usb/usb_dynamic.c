/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "implementation/global_implementation.h"

/* function prototypes */
static usb_handle_req_t usb_temp_get_desc_w;
static usb_temp_setup_by_index_t usb_temp_setup_by_index_w;
static usb_temp_unsetup_t usb_temp_unsetup_w;
static usb_test_quirk_t usb_test_quirk_w;
static usb_quirk_ioctl_t usb_quirk_ioctl_w;

/* global variables */
usb_handle_req_t *usb_temp_get_desc_p = &usb_temp_get_desc_w;
usb_temp_setup_by_index_t *usb_temp_setup_by_index_p = &usb_temp_setup_by_index_w;
usb_temp_unsetup_t *usb_temp_unsetup_p = &usb_temp_unsetup_w;
usb_test_quirk_t *usb_test_quirk_p = &usb_test_quirk_w;
usb_quirk_ioctl_t *usb_quirk_ioctl_p = &usb_quirk_ioctl_w;
devclass_t usb_devclass_ptr;

static usb_error_t
usb_temp_setup_by_index_w(struct usb_device *udev, uint16_t index)
{
	return (USB_ERR_INVAL);
}

static uint8_t
usb_test_quirk_w(const struct usbd_lookup_info *info, uint16_t quirk)
{
	return (0);			/* no match */
}

static int
usb_quirk_ioctl_w(unsigned long cmd, caddr_t data, int fflag, struct thread *td)
{
	return (ENOIOCTL);
}

static usb_error_t
usb_temp_get_desc_w(struct usb_device *udev, struct usb_device_request *req, const void **pPtr, uint16_t *pLength)
{
	/* stall */
	return (USB_ERR_STALLED);
}

static void
usb_temp_unsetup_w(struct usb_device *udev)
{
	usbd_free_config_desc(udev, udev->usb_template_ptr);
	udev->usb_template_ptr = NULL;
}

void
usb_quirk_unload(void *arg)
{
	/* reset function pointers */

	usb_test_quirk_p = &usb_test_quirk_w;
	usb_quirk_ioctl_p = &usb_quirk_ioctl_w;

	/* wait for CPU to exit the loaded functions, if any */

	/* XXX this is a tradeoff */

	pause("WAIT", hz);
}

void
usb_temp_unload(void *arg)
{
	/* reset function pointers */

	usb_temp_get_desc_p = &usb_temp_get_desc_w;
	usb_temp_setup_by_index_p = &usb_temp_setup_by_index_w;
	usb_temp_unsetup_p = &usb_temp_unsetup_w;

	/* wait for CPU to exit the loaded functions, if any */

	/* XXX this is a tradeoff */

	pause("WAIT", hz);
}

void
usb_bus_unload(void *arg)
{
	/* reset function pointers */

	usb_devclass_ptr = NULL;

	/* wait for CPU to exit the loaded functions, if any */

	/* XXX this is a tradeoff */

	pause("WAIT", hz);
}
