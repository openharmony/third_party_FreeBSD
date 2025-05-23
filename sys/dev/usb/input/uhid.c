/*	$NetBSD: uhid.c,v 1.46 2001/11/13 06:24:55 lukem Exp $	*/

/* Also already merged from NetBSD:
 *	$NetBSD: uhid.c,v 1.54 2002/09/23 05:51:21 simonb Exp $
 */

#include <sys/cdefs.h>
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * HID spec: http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 */

#include "implementation/global_implementation.h"
#include "input/usb_rdesc.h"
#include "implementation/usbdevs.h"
#include "event_hub.h"
#include <dev/usb/usb_generic.h>

#undef USB_DEBUG_VAR
#define	USB_DEBUG_VAR uhid_debug

#ifdef LOSCFG_USB_DEBUG
static int uhid_debug = 0;
#endif

#define	UHID_BSIZE	1024		/* bytes, buffer size */
#define	UHID_FRAME_NUM 	  50		/* bytes, frame number */

#define MOUSE_DATA_LEN 4
#define BTN_LEFT_VALUE(v) ((v) & 0x01)
#define BTN_RIGHT_VALUE(v) (((v) & 0x02)>>1)
#define BTN_MIDDLE_VALUE(v) (((v) & 0x04)>>2)

enum {
	UHID_INTR_DT_WR,
	UHID_INTR_DT_RD,
	UHID_CTRL_DT_WR,
	UHID_CTRL_DT_RD,
	UHID_N_TRANSFER,
};

struct uhid_softc {
	struct usb_fifo_sc sc_fifo;
	struct mtx sc_mtx;

	struct usb_xfer *sc_xfer[UHID_N_TRANSFER];
	struct usb_device *sc_udev;
	void   *sc_repdesc_ptr;

	uint32_t sc_isize;
	uint32_t sc_osize;
	uint32_t sc_fsize;

	InputDevice *input_dev;

	uint16_t sc_repdesc_size;

	uint8_t	sc_iface_no;
	uint8_t	sc_iface_index;
	uint8_t	sc_iid;
	uint8_t	sc_oid;
	uint8_t	sc_fid;
	uint8_t	sc_flags;
#define	UHID_FLAG_IMMED        0x01	/* set if read should be immediate */
#define	UHID_FLAG_STATIC_DESC  0x04	/* set if report descriptors are
					 * static */
};

static const uint8_t uhid_xb360gp_report_descr[] = {UHID_XB360GP_REPORT_DESCR()};
static const uint8_t uhid_graphire_report_descr[] = {UHID_GRAPHIRE_REPORT_DESCR()};
static const uint8_t uhid_graphire3_4x5_report_descr[] = {UHID_GRAPHIRE3_4X5_REPORT_DESCR()};

/* prototypes */

static device_probe_t uhid_probe;
static device_attach_t uhid_attach;
static device_detach_t uhid_detach;

static usb_callback_t uhid_intr_write_callback;
static usb_callback_t uhid_intr_read_callback;
static usb_callback_t uhid_write_callback;
static usb_callback_t uhid_read_callback;

static usb_fifo_cmd_t uhid_start_read;
static usb_fifo_cmd_t uhid_stop_read;
static usb_fifo_cmd_t uhid_start_write;
static usb_fifo_cmd_t uhid_stop_write;
static usb_fifo_open_t uhid_open;
static usb_fifo_close_t uhid_close;
static usb_fifo_ioctl_t uhid_ioctl;
static usb_fifo_ioctl_t uhid_ioctl_post;

static struct usb_fifo_methods uhid_fifo_methods = {
	.f_open = &uhid_open,
	.f_close = &uhid_close,
	.f_ioctl = &uhid_ioctl,
	.f_ioctl_post = &uhid_ioctl_post,
	.f_start_read = &uhid_start_read,
	.f_stop_read = &uhid_stop_read,
	.f_start_write = &uhid_start_write,
	.f_stop_write = &uhid_stop_write,
	.basename[0] = "uhid",
};

static void
uhid_intr_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uhid_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	usb_frlength_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	case USB_ST_SETUP:
tr_setup:
		pc = usbd_xfer_get_frame(xfer, 0);
		if (usb_fifo_get_data(sc->sc_fifo.fp[USB_FIFO_TX], pc,
		    0, usbd_xfer_max_len(xfer), &actlen, 0)) {
			usbd_xfer_set_frame_len(xfer, 0, actlen);
			usbd_transfer_submit(xfer);
		}
		return;

	default:			/* Error */
		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		return;
	}
}

void report_event(InputDevice *input_dev, uint32_t type, uint32_t code, int32_t value)
{
	DPRINTF("%s type = %u, code = %u, value = %d\n", input_dev->devName, type, code, value);
	if (type == EV_SYN || type == EV_KEY) {
		PushOnePackage(input_dev, type, code, value);
	} else if (value) {
		PushOnePackage(input_dev, type, code, value);
	}
}

void mouse_report_events(InputDevice *input_dev, void *buffer, int len)
{
	if (len != MOUSE_DATA_LEN) {
		DPRINTF("%s: invalid data len = %d\n", __func__, len);
		return;
	}

	const char *buf = buffer;
	report_event(input_dev, EV_KEY, BTN_LEFT, BTN_LEFT_VALUE((unsigned char)buf[0]));
	report_event(input_dev, EV_KEY, BTN_RIGHT, BTN_RIGHT_VALUE((unsigned char)buf[0]));
	report_event(input_dev, EV_KEY, BTN_MIDDLE, BTN_MIDDLE_VALUE((unsigned char)buf[0]));
	report_event(input_dev, EV_REL, REL_X, buf[1]);
	report_event(input_dev, EV_REL, REL_Y, buf[2]);
	report_event(input_dev, EV_REL, REL_WHEEL, buf[3]);
	report_event(input_dev, EV_SYN, SYN_REPORT, 0);
}

static void
uhid_intr_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uhid_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int actlen;

	DPRINTF("enter state of xfer is %u!\n", USB_GET_STATE(xfer));

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		pc = usbd_xfer_get_frame(xfer, 0);
		if (sc->input_dev && sc->input_dev->devType == INDEV_TYPE_MOUSE) {
			mouse_report_events(sc->input_dev, pc->buffer, actlen);
		}

	case USB_ST_SETUP:
		usbd_xfer_set_frame_len(xfer, 0, sc->sc_isize);
		usbd_transfer_submit(xfer);
		return;

	default:
		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			usbd_xfer_set_frame_len(xfer, 0, sc->sc_isize);
			usbd_transfer_submit(xfer);
		}
		return;
	}
}

static void
uhid_fill_set_report(struct usb_device_request *req, uint8_t iface_no,
    uint8_t type, uint8_t id, uint16_t size)
{
	req->bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req->bRequest = UR_SET_REPORT;
	USETW2(req->wValue, type, id);
	req->wIndex[0] = iface_no;
	req->wIndex[1] = 0;
	USETW(req->wLength, size);
}

static void
uhid_fill_get_report(struct usb_device_request *req, uint8_t iface_no,
    uint8_t type, uint8_t id, uint16_t size)
{
	req->bmRequestType = UT_READ_CLASS_INTERFACE;
	req->bRequest = UR_GET_REPORT;
	USETW2(req->wValue, type, id);
	req->wIndex[0] = iface_no;
	req->wIndex[1] = 0;
	USETW(req->wLength, size);
}

static void
uhid_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uhid_softc *sc = usbd_xfer_softc(xfer);
	struct usb_device_request req;
	struct usb_page_cache *pc;
	uint32_t size = sc->sc_osize;
	uint32_t actlen;
	uint8_t id;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	case USB_ST_SETUP:
		/* try to extract the ID byte */
		if (sc->sc_oid) {
			pc = usbd_xfer_get_frame(xfer, 0);
			if (usb_fifo_get_data(sc->sc_fifo.fp[USB_FIFO_TX], pc,
			    0, 1, &actlen, 0)) {
				if (actlen != 1) {
					goto tr_error;
				}
				usbd_copy_out(pc, 0, &id, 1);

			} else {
				return;
			}
			if (size) {
				size--;
			}
		} else {
			id = 0;
		}

		pc = usbd_xfer_get_frame(xfer, 1);
		if (usb_fifo_get_data(sc->sc_fifo.fp[USB_FIFO_TX], pc,
		    0, UHID_BSIZE, &actlen, 1)) {
			if (actlen != size) {
				goto tr_error;
			}
			uhid_fill_set_report
			    (&req, sc->sc_iface_no,
			    UHID_OUTPUT_REPORT, id, size);

			pc = usbd_xfer_get_frame(xfer, 0);
			usbd_copy_in(pc, 0, &req, sizeof(req));

			usbd_xfer_set_frame_len(xfer, 0, sizeof(req));
			usbd_xfer_set_frame_len(xfer, 1, size);
			usbd_xfer_set_frames(xfer, size ? 2 : 1);
			usbd_transfer_submit(xfer);
		}
		return;

	default:
tr_error:
		/* bomb out */
		usb_fifo_get_data_error(sc->sc_fifo.fp[USB_FIFO_TX]);
		return;
	}
}

static void
uhid_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uhid_softc *sc = usbd_xfer_softc(xfer);
	struct usb_device_request req;
	struct usb_page_cache *pc;

	DPRINTF("enter state of xfer is %u!\n", USB_GET_STATE(xfer));

	pc = usbd_xfer_get_frame(xfer, 0);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		usb_fifo_put_data(sc->sc_fifo.fp[USB_FIFO_RX], pc, sizeof(req),
		    sc->sc_isize, 1);
		return;

	case USB_ST_SETUP:

		if (usb_fifo_put_bytes_max(sc->sc_fifo.fp[USB_FIFO_RX]) > 0) {
			uhid_fill_get_report
			    (&req, sc->sc_iface_no, UHID_INPUT_REPORT,
			    sc->sc_iid, sc->sc_isize);

			usbd_copy_in(pc, 0, &req, sizeof(req));

			usbd_xfer_set_frame_len(xfer, 0, sizeof(req));
			usbd_xfer_set_frame_len(xfer, 1, sc->sc_isize);
			usbd_xfer_set_frames(xfer, sc->sc_isize ? 2 : 1);
			usbd_transfer_submit(xfer);
		}
		return;

	default:			/* Error */
		/* bomb out */
		usb_fifo_put_data_error(sc->sc_fifo.fp[USB_FIFO_RX]);
		return;
	}
}

static const struct usb_config uhid_config[UHID_N_TRANSFER] = {
	[UHID_INTR_DT_WR] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.flags = {.pipe_bof = 1,.no_pipe_ok = 1, },
		.bufsize = UHID_BSIZE,
		.callback = &uhid_intr_write_callback,
	},

	[UHID_INTR_DT_RD] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.bufsize = UHID_BSIZE,
		.callback = &uhid_intr_read_callback,
	},

	[UHID_CTRL_DT_WR] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.bufsize = sizeof(struct usb_device_request) + UHID_BSIZE,
		.callback = &uhid_write_callback,
		.timeout = 1000,	/* 1 second */
	},

	[UHID_CTRL_DT_RD] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.bufsize = sizeof(struct usb_device_request) + UHID_BSIZE,
		.callback = &uhid_read_callback,
		.timeout = 1000,	/* 1 second */
	},
};

static void
uhid_start_read(struct usb_fifo *fifo)
{
	struct uhid_softc *sc = usb_fifo_softc(fifo);

	if (sc->sc_flags & UHID_FLAG_IMMED) {
		usbd_transfer_start(sc->sc_xfer[UHID_CTRL_DT_RD]);
	} else {
		usbd_transfer_start(sc->sc_xfer[UHID_INTR_DT_RD]);
	}
}

static void
uhid_stop_read(struct usb_fifo *fifo)
{
	struct uhid_softc *sc = usb_fifo_softc(fifo);

	usbd_transfer_stop(sc->sc_xfer[UHID_CTRL_DT_RD]);
	usbd_transfer_stop(sc->sc_xfer[UHID_INTR_DT_RD]);
}

static void
uhid_start_write(struct usb_fifo *fifo)
{
	struct uhid_softc *sc = usb_fifo_softc(fifo);

	if ((sc->sc_flags & UHID_FLAG_IMMED) ||
	    sc->sc_xfer[UHID_INTR_DT_WR] == NULL) {
		usbd_transfer_start(sc->sc_xfer[UHID_CTRL_DT_WR]);
	} else {
		usbd_transfer_start(sc->sc_xfer[UHID_INTR_DT_WR]);
	}
}

static void
uhid_stop_write(struct usb_fifo *fifo)
{
	struct uhid_softc *sc = usb_fifo_softc(fifo);

	usbd_transfer_stop(sc->sc_xfer[UHID_CTRL_DT_WR]);
	usbd_transfer_stop(sc->sc_xfer[UHID_INTR_DT_WR]);
}

static int
uhid_get_report(struct uhid_softc *sc, uint8_t type,
    uint8_t id, void *kern_data, void *user_data,
    uint16_t len)
{
	int err;
	uint8_t free_data = 0;

	if (kern_data == NULL) {
		if (len == 0) {
			err = EPERM;
			goto done;
		}

		kern_data = malloc(len);
		if (kern_data == NULL) {
			err = ENOMEM;
			goto done;
		}
		free_data = 1;
	}
	err = usbd_req_get_report(sc->sc_udev, NULL, kern_data,
	    len, sc->sc_iface_index, type, id);
	if (err) {
		err = ENXIO;
		goto done;
	}
	if (user_data) {
		/* dummy buffer */
		err = copyout(kern_data, user_data, len);
		if (err) {
			goto done;
		}
	}
done:
	if (free_data) {
		free(kern_data);
	}
	return (err);
}

static int
uhid_set_report(struct uhid_softc *sc, uint8_t type,
    uint8_t id, void *kern_data, const void *user_data,
    uint16_t len)
{
	int err;
	uint8_t free_data = 0;

	if (kern_data == NULL) {
		if (len == 0) {
			err = EPERM;
			goto done;
		}

		kern_data = malloc(len);
		if (kern_data == NULL) {
			err = ENOMEM;
			goto done;
		}
		free_data = 1;
		err = copyin(user_data, kern_data, len);
		if (err) {
			goto done;
		}
	}
	err = usbd_req_set_report(sc->sc_udev, NULL, kern_data,
	    len, sc->sc_iface_index, type, id);
	if (err) {
		err = ENXIO;
		goto done;
	}
done:
	if (free_data) {
		free(kern_data);
	}
	return (err);
}

static int
uhid_open(struct usb_fifo *fifo, int fflags)
{
	struct uhid_softc *sc = usb_fifo_softc(fifo);

	/*
	 * The buffers are one byte larger than maximum so that one
	 * can detect too large read/writes and short transfers:
	 */
	if ((unsigned int)fflags & FREAD) {
		/* reset flags */
		mtx_lock(&sc->sc_mtx);
		sc->sc_flags &= ~UHID_FLAG_IMMED;
		mtx_unlock(&sc->sc_mtx);

		if (usb_fifo_alloc_buffer(fifo,
		    sc->sc_isize + 1, UHID_FRAME_NUM)) {
			return (ENOMEM);
		}
	}
	if ((unsigned int)fflags & FWRITE) {
		if (usb_fifo_alloc_buffer(fifo,
		    sc->sc_osize + 1, UHID_FRAME_NUM)) {
			return (ENOMEM);
		}
	}

	return (0);
}

static void
uhid_close(struct usb_fifo *fifo, int fflags)
{
	if ((unsigned int)fflags & (FREAD | FWRITE)) {
		usb_fifo_free_buffer(fifo);
	}
}

static int
uhid_ioctl(struct usb_fifo *fifo, u_long cmd, void *addr,
    int fflags)
{
	struct uhid_softc *sc = usb_fifo_softc(fifo);
	struct usb_gen_descriptor ugd;
	uint32_t size;
	int error = 0;
	uint8_t id;

	switch (cmd) {
	case USB_GET_REPORT_DESC:
		error = copyin((const void *)addr, &ugd, sizeof(struct usb_gen_descriptor));
		if (error != ENOERR) {
			break;
		}
		if (sc->sc_repdesc_size > ugd.ugd_maxlen) {
			size = ugd.ugd_maxlen;
		} else {
			size = sc->sc_repdesc_size;
		}
		ugd.ugd_actlen = size;
		if (ugd.ugd_data == NULL)
			break;		/* descriptor length only */
		error = copyout(sc->sc_repdesc_ptr, ugd.ugd_data, size);
		if (error == ENOERR) {
			error = copyout((const void *)&ugd, addr, sizeof(struct usb_gen_descriptor));
		}
		break;

	case USB_SET_IMMED: {
		int data;
		if (!((unsigned int)fflags & FREAD)) {
			error = EPERM;
			break;
		}
		error = copyin((const void *)addr, &data, sizeof(data));
		if (error != ENOERR) {
			break;
		}
		if (data) {
			/* do a test read */

			error = uhid_get_report(sc, UHID_INPUT_REPORT,
			    sc->sc_iid, NULL, NULL, sc->sc_isize);
			if (error) {
				break;
			}
			mtx_lock(&sc->sc_mtx);
			sc->sc_flags |= UHID_FLAG_IMMED;
			mtx_unlock(&sc->sc_mtx);
		} else {
			mtx_lock(&sc->sc_mtx);
			sc->sc_flags &= ~UHID_FLAG_IMMED;
			mtx_unlock(&sc->sc_mtx);
		}
		break;
	}

	case USB_GET_REPORT:
		if (!((unsigned int)fflags & FREAD)) {
			error = EPERM;
			break;
		}
		error = copyin((const void *)addr, &ugd, sizeof(struct usb_gen_descriptor));
		if (error != ENOERR) {
			break;
		}
		switch (ugd.ugd_report_type) {
		case UHID_INPUT_REPORT:
			size = sc->sc_isize;
			id = sc->sc_iid;
			break;
		case UHID_OUTPUT_REPORT:
			size = sc->sc_osize;
			id = sc->sc_oid;
			break;
		case UHID_FEATURE_REPORT:
			size = sc->sc_fsize;
			id = sc->sc_fid;
			break;
		default:
			return (EINVAL);
		}
		if (id != 0)
			copyin(ugd.ugd_data, &id, 1);
		error = uhid_get_report(sc, ugd.ugd_report_type, id,
		    NULL, ugd.ugd_data, MIN(ugd.ugd_maxlen, size));
		break;

	case USB_SET_REPORT:
		if (!((unsigned int)fflags & FWRITE)) {
			error = EPERM;
			break;
		}
		error = copyin((const void *)addr, &ugd, sizeof(struct usb_gen_descriptor));
		if (error != ENOERR) {
			break;
		}
		switch (ugd.ugd_report_type) {
		case UHID_INPUT_REPORT:
			size = sc->sc_isize;
			id = sc->sc_iid;
			break;
		case UHID_OUTPUT_REPORT:
			size = sc->sc_osize;
			id = sc->sc_oid;
			break;
		case UHID_FEATURE_REPORT:
			size = sc->sc_fsize;
			id = sc->sc_fid;
			break;
		default:
			return (EINVAL);
		}
		if (id != 0)
			copyin(ugd.ugd_data, &id, 1);
		error = uhid_set_report(sc, ugd.ugd_report_type, id,
		    NULL, ugd.ugd_data, MIN(ugd.ugd_maxlen, size));
		break;

	case USB_GET_REPORT_ID: {
		int data = 0;
		error = copyout((const void *)&data, addr, sizeof(data)); /* XXX: we only support reportid 0? */
		break;
	}

	default:
		error = ENOIOCTL;
		break;
	}

	return (error);
}

static int
uhid_ioctl_post(struct usb_fifo *fifo, u_long cmd, void *addr,
    int fflags)
{
	int error;

	switch (cmd) {
	case USB_GET_DEVICEINFO:
		error = ugen_fill_deviceinfo(fifo, addr);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

static const STRUCT_USB_HOST_ID uhid_devs[] = {
	/* generic HID class */
	{USB_IFACE_CLASS(UICLASS_HID),},
	/* the Xbox 360 gamepad doesn't use the HID class */
	{USB_IFACE_CLASS(UICLASS_VENDOR),
	 USB_IFACE_SUBCLASS(UISUBCLASS_XBOX360_CONTROLLER),
	 USB_IFACE_PROTOCOL(UIPROTO_XBOX360_GAMEPAD),},
};

static int
uhid_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	int error;

	DPRINTFN(11, "\n");

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	error = usbd_lookup_id_by_uaa(uhid_devs, sizeof(uhid_devs), uaa);
	if (error)
		return (error);

	if (usb_test_quirk(uaa, UQ_HID_IGNORE))
		return (ENXIO);

	return (0);
}

static int
uhid_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct uhid_softc *sc = device_get_softc(dev);
	int unit = device_get_unit(dev);
	int error = 0;
	int32_t ret;

	DPRINTFN(10, "sc=%p\n", sc);

	device_set_usb_desc(dev);

	mtx_init(&sc->sc_mtx, "uhid lock", NULL, MTX_DEF | MTX_RECURSE);

	sc->sc_udev = uaa->device;

	sc->sc_iface_no = uaa->info.bIfaceNum;
	sc->sc_iface_index = uaa->info.bIfaceIndex;

	error = usbd_transfer_setup(uaa->device,
	    &uaa->info.bIfaceIndex, sc->sc_xfer, uhid_config,
	    UHID_N_TRANSFER, sc, &sc->sc_mtx);

	if (error) {
		DPRINTF("error=%s\n", usbd_errstr(error));
		goto detach;
	}
	if (uaa->info.idVendor == USB_VENDOR_WACOM) {
		/* the report descriptor for the Wacom Graphire is broken */

		if (uaa->info.idProduct == USB_PRODUCT_WACOM_GRAPHIRE) {
			sc->sc_repdesc_size = sizeof(uhid_graphire_report_descr);
			sc->sc_repdesc_ptr = __DECONST(void *, &uhid_graphire_report_descr);
			sc->sc_flags |= UHID_FLAG_STATIC_DESC;

		} else if (uaa->info.idProduct == USB_PRODUCT_WACOM_GRAPHIRE3_4X5) {
			static uint8_t reportbuf[] = {2, 2, 2};

			/*
			 * The Graphire3 needs 0x0202 to be written to
			 * feature report ID 2 before it'll start
			 * returning digitizer data.
			 */
			error = usbd_req_set_report(uaa->device, NULL,
			    reportbuf, sizeof(reportbuf),
			    uaa->info.bIfaceIndex, UHID_FEATURE_REPORT, 2);

			if (error) {
				DPRINTF("set report failed, error=%s (ignored)\n",
				    usbd_errstr(error));
			}
			sc->sc_repdesc_size = sizeof(uhid_graphire3_4x5_report_descr);
			sc->sc_repdesc_ptr = __DECONST(void *, &uhid_graphire3_4x5_report_descr);
			sc->sc_flags |= UHID_FLAG_STATIC_DESC;
		}
	} else if ((uaa->info.bInterfaceClass == UICLASS_VENDOR) &&
	    (uaa->info.bInterfaceSubClass == UISUBCLASS_XBOX360_CONTROLLER) &&
	    (uaa->info.bInterfaceProtocol == UIPROTO_XBOX360_GAMEPAD)) {
		static const uint8_t reportbuf[3] = {1, 3, 0};
		/*
		 * Turn off the four LEDs on the gamepad which
		 * are blinking by default:
		 */
		error = usbd_req_set_report(uaa->device, NULL,
		    __DECONST(void *, reportbuf), sizeof(reportbuf),
		    uaa->info.bIfaceIndex, UHID_OUTPUT_REPORT, 0);
		if (error) {
			DPRINTF("set output report failed, error=%s (ignored)\n",
			    usbd_errstr(error));
		}
		/* the Xbox 360 gamepad has no report descriptor */
		sc->sc_repdesc_size = sizeof(uhid_xb360gp_report_descr);
		sc->sc_repdesc_ptr = __DECONST(void *, &uhid_xb360gp_report_descr);
		sc->sc_flags |= UHID_FLAG_STATIC_DESC;
	}

	if (sc->sc_repdesc_ptr == NULL) {
		error = usbd_req_get_hid_desc(uaa->device, NULL,
		    &sc->sc_repdesc_ptr, &sc->sc_repdesc_size,
		    M_USBDEV, uaa->info.bIfaceIndex);

		if (error) {
			device_printf(dev, "no report descriptor\n");
			goto detach;
		}
	}

	error = usbd_req_set_idle(uaa->device, NULL,
	    uaa->info.bIfaceIndex, 0, 0);

	if (error) {
		DPRINTF("set idle failed, error=%s (ignored)\n",
		    usbd_errstr(error));
	}

	sc->sc_isize = hid_report_size
	    (sc->sc_repdesc_ptr, sc->sc_repdesc_size, hid_input, &sc->sc_iid);

	sc->sc_osize = hid_report_size
	    (sc->sc_repdesc_ptr, sc->sc_repdesc_size, hid_output, &sc->sc_oid);

	sc->sc_fsize = hid_report_size
	    (sc->sc_repdesc_ptr, sc->sc_repdesc_size, hid_feature, &sc->sc_fid);

	if (sc->sc_isize > UHID_BSIZE) {
		DPRINTF("input size is too large, "
		    "%d bytes (truncating)\n",
		    sc->sc_isize);
		sc->sc_isize = UHID_BSIZE;
	}
	if (sc->sc_osize > UHID_BSIZE) {
		DPRINTF("output size is too large, "
		    "%d bytes (truncating)\n",
		    sc->sc_osize);
		sc->sc_osize = UHID_BSIZE;
	}
	if (sc->sc_fsize > UHID_BSIZE) {
		DPRINTF("feature size is too large, "
		    "%d bytes (truncating)\n",
		    sc->sc_fsize);
		sc->sc_fsize = UHID_BSIZE;
	}

	error = usb_fifo_attach(uaa->device, sc, &sc->sc_mtx,
	    &uhid_fifo_methods, &sc->sc_fifo,
	    unit, -1, uaa->info.bIfaceIndex,
	    UID_ROOT, GID_OPERATOR, 0644);
	if (error) {
		goto detach;
	}

	sc->input_dev = (InputDevice*)zalloc(sizeof(InputDevice));
	if (sc->input_dev) {
		if (uaa->info.bInterfaceProtocol == UIPROTO_MOUSE) {
			sc->input_dev->devType = INDEV_TYPE_MOUSE;
			sc->input_dev->devName = "mouse";
		} else {
			sc->input_dev->devType = INDEV_TYPE_UNKNOWN;
			sc->input_dev->devName = "other";
		}

		ret = RegisterInputDevice(sc->input_dev);
		if (ret != HDF_SUCCESS) {
			DPRINTF("%s register failed, ret = %d!\n", sc->input_dev->devName, ret);
			free(sc->input_dev);
			sc->input_dev = NULL;
		} else if (sc->input_dev->devType == INDEV_TYPE_MOUSE) {
			DPRINTF("mouse register success!\n");
			mtx_lock(&sc->sc_mtx);
			sc->sc_flags &= ~UHID_FLAG_IMMED;
			usbd_transfer_start(sc->sc_xfer[UHID_INTR_DT_RD]);
			mtx_unlock(&sc->sc_mtx);
		}
	}

	return (0);			/* success */

detach:
	uhid_detach(dev);
	return (ENOMEM);
}

static int
uhid_detach(device_t dev)
{
	struct uhid_softc *sc = device_get_softc(dev);

	DPRINTF("enter\n");

	if (sc->input_dev) {
		if (sc->input_dev->devType == INDEV_TYPE_MOUSE) {
			usbd_transfer_stop(sc->sc_xfer[UHID_INTR_DT_RD]);
		}

		UnregisterInputDevice(sc->input_dev);
		free(sc->input_dev);
		sc->input_dev = NULL;
	}

	usb_fifo_detach(&sc->sc_fifo);

	usbd_transfer_unsetup(sc->sc_xfer, UHID_N_TRANSFER);

	if (sc->sc_repdesc_ptr) {
		if (!(sc->sc_flags & UHID_FLAG_STATIC_DESC)) {
			free(sc->sc_repdesc_ptr);
		}
	}
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static devclass_t uhid_devclass;

static device_method_t uhid_methods[] = {
	DEVMETHOD(device_probe, uhid_probe),
	DEVMETHOD(device_attach, uhid_attach),
	DEVMETHOD(device_detach, uhid_detach),

	DEVMETHOD_END
};

static driver_t uhid_driver = {
	.name = "uhid",
	.methods = uhid_methods,
	.size = sizeof(struct uhid_softc),
};

DRIVER_MODULE(uhid, uhub, uhid_driver, uhid_devclass, NULL, 0);
