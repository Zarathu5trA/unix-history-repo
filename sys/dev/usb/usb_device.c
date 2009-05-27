/* $FreeBSD$ */
/*-
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

#include <dev/usb/usb_mfunc.h>
#include <dev/usb/usb_error.h>
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#include "usbdevs.h"

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_transfer.h>
#include <dev/usb/usb_parse.h>
#include <dev/usb/usb_request.h>
#include <dev/usb/usb_dynamic.h>
#include <dev/usb/usb_hub.h>
#include <dev/usb/usb_util.h>
#include <dev/usb/usb_mbuf.h>
#include <dev/usb/usb_msctest.h>
#if USB_HAVE_UGEN
#include <dev/usb/usb_dev.h>
#include <dev/usb/usb_generic.h>
#endif

#include <dev/usb/quirk/usb_quirk.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>

/* function prototypes  */

static void	usb2_init_pipe(struct usb2_device *, uint8_t,
		    struct usb2_endpoint_descriptor *, struct usb2_pipe *);
static void	usb2_unconfigure(struct usb2_device *, uint8_t);
static void	usb2_detach_device(struct usb2_device *, uint8_t, uint8_t);
static void	usb2_detach_device_sub(struct usb2_device *, device_t *,
		    uint8_t);
static uint8_t	usb2_probe_and_attach_sub(struct usb2_device *,
		    struct usb2_attach_arg *);
static void	usb2_init_attach_arg(struct usb2_device *,
		    struct usb2_attach_arg *);
static void	usb2_suspend_resume_sub(struct usb2_device *, device_t,
		    uint8_t);
static void	usb2_clear_stall_proc(struct usb2_proc_msg *_pm);
usb2_error_t	usb2_config_parse(struct usb2_device *, uint8_t, uint8_t);
static void	usb2_set_device_strings(struct usb2_device *);
#if USB_HAVE_UGEN
static void	usb2_notify_addq(const char *type, struct usb2_device *);
static void	usb2_fifo_free_wrap(struct usb2_device *, uint8_t, uint8_t);
static struct cdev *usb2_make_dev(struct usb2_device *, int, int);
static void	usb2_cdev_create(struct usb2_device *);
static void	usb2_cdev_free(struct usb2_device *);
static void	usb2_cdev_cleanup(void *);
#endif

/* This variable is global to allow easy access to it: */

int	usb2_template = 0;

SYSCTL_INT(_hw_usb, OID_AUTO, template, CTLFLAG_RW,
    &usb2_template, 0, "Selected USB device side template");

static const char* statestr[USB_STATE_MAX] = {
	[USB_STATE_DETACHED]	= "DETACHED",
	[USB_STATE_ATTACHED]	= "ATTACHED",
	[USB_STATE_POWERED]	= "POWERED",
	[USB_STATE_ADDRESSED]	= "ADDRESSED",
	[USB_STATE_CONFIGURED]	= "CONFIGURED",
};

const char *
usb2_statestr(enum usb_dev_state state)
{
	return ((state < USB_STATE_MAX) ? statestr[state] : "UNKNOWN");
}

/*------------------------------------------------------------------------*
 *	usb2_get_pipe_by_addr
 *
 * This function searches for an USB pipe by endpoint address and
 * direction.
 *
 * Returns:
 * NULL: Failure
 * Else: Success
 *------------------------------------------------------------------------*/
struct usb2_pipe *
usb2_get_pipe_by_addr(struct usb2_device *udev, uint8_t ea_val)
{
	struct usb2_pipe *pipe = udev->pipes;
	struct usb2_pipe *pipe_end = udev->pipes + udev->pipes_max;
	enum {
		EA_MASK = (UE_DIR_IN | UE_DIR_OUT | UE_ADDR),
	};

	/*
	 * According to the USB specification not all bits are used
	 * for the endpoint address. Keep defined bits only:
	 */
	ea_val &= EA_MASK;

	/*
	 * Iterate accross all the USB pipes searching for a match
	 * based on the endpoint address:
	 */
	for (; pipe != pipe_end; pipe++) {

		if (pipe->edesc == NULL) {
			continue;
		}
		/* do the mask and check the value */
		if ((pipe->edesc->bEndpointAddress & EA_MASK) == ea_val) {
			goto found;
		}
	}

	/*
	 * The default pipe is always present and is checked separately:
	 */
	if ((udev->default_pipe.edesc) &&
	    ((udev->default_pipe.edesc->bEndpointAddress & EA_MASK) == ea_val)) {
		pipe = &udev->default_pipe;
		goto found;
	}
	return (NULL);

found:
	return (pipe);
}

/*------------------------------------------------------------------------*
 *	usb2_get_pipe
 *
 * This function searches for an USB pipe based on the information
 * given by the passed "struct usb2_config" pointer.
 *
 * Return values:
 * NULL: No match.
 * Else: Pointer to "struct usb2_pipe".
 *------------------------------------------------------------------------*/
struct usb2_pipe *
usb2_get_pipe(struct usb2_device *udev, uint8_t iface_index,
    const struct usb2_config *setup)
{
	struct usb2_pipe *pipe = udev->pipes;
	struct usb2_pipe *pipe_end = udev->pipes + udev->pipes_max;
	uint8_t index = setup->ep_index;
	uint8_t ea_mask;
	uint8_t ea_val;
	uint8_t type_mask;
	uint8_t type_val;

	DPRINTFN(10, "udev=%p iface_index=%d address=0x%x "
	    "type=0x%x dir=0x%x index=%d\n",
	    udev, iface_index, setup->endpoint,
	    setup->type, setup->direction, setup->ep_index);

	/* check USB mode */

	if (setup->usb_mode != USB_MODE_DUAL &&
	    udev->flags.usb_mode != setup->usb_mode) {
		/* wrong mode - no pipe */
		return (NULL);
	}

	/* setup expected endpoint direction mask and value */

	if (setup->direction == UE_DIR_RX) {
		ea_mask = (UE_DIR_IN | UE_DIR_OUT);
		ea_val = (udev->flags.usb_mode == USB_MODE_DEVICE) ?
		    UE_DIR_OUT : UE_DIR_IN;
	} else if (setup->direction == UE_DIR_TX) {
		ea_mask = (UE_DIR_IN | UE_DIR_OUT);
		ea_val = (udev->flags.usb_mode == USB_MODE_DEVICE) ?
		    UE_DIR_IN : UE_DIR_OUT;
	} else if (setup->direction == UE_DIR_ANY) {
		/* match any endpoint direction */
		ea_mask = 0;
		ea_val = 0;
	} else {
		/* match the given endpoint direction */
		ea_mask = (UE_DIR_IN | UE_DIR_OUT);
		ea_val = (setup->direction & (UE_DIR_IN | UE_DIR_OUT));
	}

	/* setup expected endpoint address */

	if (setup->endpoint == UE_ADDR_ANY) {
		/* match any endpoint address */
	} else {
		/* match the given endpoint address */
		ea_mask |= UE_ADDR;
		ea_val |= (setup->endpoint & UE_ADDR);
	}

	/* setup expected endpoint type */

	if (setup->type == UE_BULK_INTR) {
		/* this will match BULK and INTERRUPT endpoints */
		type_mask = 2;
		type_val = 2;
	} else if (setup->type == UE_TYPE_ANY) {
		/* match any endpoint type */
		type_mask = 0;
		type_val = 0;
	} else {
		/* match the given endpoint type */
		type_mask = UE_XFERTYPE;
		type_val = (setup->type & UE_XFERTYPE);
	}

	/*
	 * Iterate accross all the USB pipes searching for a match
	 * based on the endpoint address. Note that we are searching
	 * the pipes from the beginning of the "udev->pipes" array.
	 */
	for (; pipe != pipe_end; pipe++) {

		if ((pipe->edesc == NULL) ||
		    (pipe->iface_index != iface_index)) {
			continue;
		}
		/* do the masks and check the values */

		if (((pipe->edesc->bEndpointAddress & ea_mask) == ea_val) &&
		    ((pipe->edesc->bmAttributes & type_mask) == type_val)) {
			if (!index--) {
				goto found;
			}
		}
	}

	/*
	 * Match against default pipe last, so that "any pipe", "any
	 * address" and "any direction" returns the first pipe of the
	 * interface. "iface_index" and "direction" is ignored:
	 */
	if ((udev->default_pipe.edesc) &&
	    ((udev->default_pipe.edesc->bEndpointAddress & ea_mask) == ea_val) &&
	    ((udev->default_pipe.edesc->bmAttributes & type_mask) == type_val) &&
	    (!index)) {
		pipe = &udev->default_pipe;
		goto found;
	}
	return (NULL);

found:
	return (pipe);
}

/*------------------------------------------------------------------------*
 *	usb2_interface_count
 *
 * This function stores the number of USB interfaces excluding
 * alternate settings, which the USB config descriptor reports into
 * the unsigned 8-bit integer pointed to by "count".
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_interface_count(struct usb2_device *udev, uint8_t *count)
{
	if (udev->cdesc == NULL) {
		*count = 0;
		return (USB_ERR_NOT_CONFIGURED);
	}
	*count = udev->ifaces_max;
	return (USB_ERR_NORMAL_COMPLETION);
}


/*------------------------------------------------------------------------*
 *	usb2_init_pipe
 *
 * This function will initialise the USB pipe structure pointed to by
 * the "pipe" argument. The structure pointed to by "pipe" must be
 * zeroed before calling this function.
 *------------------------------------------------------------------------*/
static void
usb2_init_pipe(struct usb2_device *udev, uint8_t iface_index,
    struct usb2_endpoint_descriptor *edesc, struct usb2_pipe *pipe)
{
	struct usb2_bus_methods *methods;

	methods = udev->bus->methods;

	(methods->pipe_init) (udev, edesc, pipe);

	/* initialise USB pipe structure */
	pipe->edesc = edesc;
	pipe->iface_index = iface_index;
	TAILQ_INIT(&pipe->pipe_q.head);
	pipe->pipe_q.command = &usb2_pipe_start;

	/* the pipe is not supported by the hardware */
 	if (pipe->methods == NULL)
		return;

	/* clear stall, if any */
	if (methods->clear_stall != NULL) {
		USB_BUS_LOCK(udev->bus);
		(methods->clear_stall) (udev, pipe);
		USB_BUS_UNLOCK(udev->bus);
	}
}

/*-----------------------------------------------------------------------*
 *	usb2_pipe_foreach
 *
 * This function will iterate all the USB endpoints except the control
 * endpoint. This function is NULL safe.
 *
 * Return values:
 * NULL: End of USB pipes
 * Else: Pointer to next USB pipe
 *------------------------------------------------------------------------*/
struct usb2_pipe *
usb2_pipe_foreach(struct usb2_device *udev, struct usb2_pipe *pipe)
{
	struct usb2_pipe *pipe_end = udev->pipes + udev->pipes_max;

	/* be NULL safe */
	if (udev == NULL)
		return (NULL);

	/* get next pipe */
	if (pipe == NULL)
		pipe = udev->pipes;
	else
		pipe++;

	/* find next allocated pipe */
	while (pipe != pipe_end) {
		if (pipe->edesc != NULL)
			return (pipe);
		pipe++;
	}
	return (NULL);
}

/*------------------------------------------------------------------------*
 *	usb2_unconfigure
 *
 * This function will free all USB interfaces and USB pipes belonging
 * to an USB device.
 *
 * Flag values, see "USB_UNCFG_FLAG_XXX".
 *------------------------------------------------------------------------*/
static void
usb2_unconfigure(struct usb2_device *udev, uint8_t flag)
{
	uint8_t do_unlock;

	/* automatic locking */
	if (sx_xlocked(udev->default_sx + 1)) {
		do_unlock = 0;
	} else {
		do_unlock = 1;
		sx_xlock(udev->default_sx + 1);
	}

	/* detach all interface drivers */
	usb2_detach_device(udev, USB_IFACE_INDEX_ANY, flag);

#if USB_HAVE_UGEN
	/* free all FIFOs except control endpoint FIFOs */
	usb2_fifo_free_wrap(udev, USB_IFACE_INDEX_ANY, flag);

	/*
	 * Free all cdev's, if any.
	 */
	usb2_cdev_free(udev);
#endif

#if USB_HAVE_COMPAT_LINUX
	/* free Linux compat device, if any */
	if (udev->linux_dev) {
		usb_linux_free_device(udev->linux_dev);
		udev->linux_dev = NULL;
	}
#endif

	usb2_config_parse(udev, USB_IFACE_INDEX_ANY, USB_CFG_FREE);

	/* free "cdesc" after "ifaces" and "pipes", if any */
	if (udev->cdesc != NULL) {
		if (udev->flags.usb_mode != USB_MODE_DEVICE)
			free(udev->cdesc, M_USB);
		udev->cdesc = NULL;
	}
	/* set unconfigured state */
	udev->curr_config_no = USB_UNCONFIG_NO;
	udev->curr_config_index = USB_UNCONFIG_INDEX;

	if (do_unlock) {
		sx_unlock(udev->default_sx + 1);
	}
}

/*------------------------------------------------------------------------*
 *	usb2_set_config_index
 *
 * This function selects configuration by index, independent of the
 * actual configuration number. This function should not be used by
 * USB drivers.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_set_config_index(struct usb2_device *udev, uint8_t index)
{
	struct usb2_status ds;
	struct usb2_config_descriptor *cdp;
	uint16_t power;
	uint16_t max_power;
	uint8_t selfpowered;
	uint8_t do_unlock;
	usb2_error_t err;

	DPRINTFN(6, "udev=%p index=%d\n", udev, index);

	/* automatic locking */
	if (sx_xlocked(udev->default_sx + 1)) {
		do_unlock = 0;
	} else {
		do_unlock = 1;
		sx_xlock(udev->default_sx + 1);
	}

	usb2_unconfigure(udev, USB_UNCFG_FLAG_FREE_SUBDEV);

	if (index == USB_UNCONFIG_INDEX) {
		/*
		 * Leave unallocated when unconfiguring the
		 * device. "usb2_unconfigure()" will also reset
		 * the current config number and index.
		 */
		err = usb2_req_set_config(udev, NULL, USB_UNCONFIG_NO);
		if (udev->state == USB_STATE_CONFIGURED)
			usb2_set_device_state(udev, USB_STATE_ADDRESSED);
		goto done;
	}
	/* get the full config descriptor */
	if (udev->flags.usb_mode == USB_MODE_DEVICE) {
		/* save some memory */
		err = usb2_req_get_descriptor_ptr(udev, &cdp, 
		    (UDESC_CONFIG << 8) | index);
	} else {
		/* normal request */
		err = usb2_req_get_config_desc_full(udev,
		    NULL, &cdp, M_USB, index);
	}
	if (err) {
		goto done;
	}
	/* set the new config descriptor */

	udev->cdesc = cdp;

	/* Figure out if the device is self or bus powered. */
	selfpowered = 0;
	if ((!udev->flags.uq_bus_powered) &&
	    (cdp->bmAttributes & UC_SELF_POWERED) &&
	    (udev->flags.usb_mode == USB_MODE_HOST)) {
		/* May be self powered. */
		if (cdp->bmAttributes & UC_BUS_POWERED) {
			/* Must ask device. */
			err = usb2_req_get_device_status(udev, NULL, &ds);
			if (err) {
				DPRINTFN(0, "could not read "
				    "device status: %s\n",
				    usb2_errstr(err));
			} else if (UGETW(ds.wStatus) & UDS_SELF_POWERED) {
				selfpowered = 1;
			}
			DPRINTF("status=0x%04x \n",
				UGETW(ds.wStatus));
		} else
			selfpowered = 1;
	}
	DPRINTF("udev=%p cdesc=%p (addr %d) cno=%d attr=0x%02x, "
	    "selfpowered=%d, power=%d\n",
	    udev, cdp,
	    udev->address, cdp->bConfigurationValue, cdp->bmAttributes,
	    selfpowered, cdp->bMaxPower * 2);

	/* Check if we have enough power. */
	power = cdp->bMaxPower * 2;

	if (udev->parent_hub) {
		max_power = udev->parent_hub->hub->portpower;
	} else {
		max_power = USB_MAX_POWER;
	}

	if (power > max_power) {
		DPRINTFN(0, "power exceeded %d > %d\n", power, max_power);
		err = USB_ERR_NO_POWER;
		goto done;
	}
	/* Only update "self_powered" in USB Host Mode */
	if (udev->flags.usb_mode == USB_MODE_HOST) {
		udev->flags.self_powered = selfpowered;
	}
	udev->power = power;
	udev->curr_config_no = cdp->bConfigurationValue;
	udev->curr_config_index = index;
	usb2_set_device_state(udev, USB_STATE_CONFIGURED);

	/* Set the actual configuration value. */
	err = usb2_req_set_config(udev, NULL, cdp->bConfigurationValue);
	if (err) {
		goto done;
	}

	err = usb2_config_parse(udev, USB_IFACE_INDEX_ANY, USB_CFG_ALLOC);
	if (err) {
		goto done;
	}

	err = usb2_config_parse(udev, USB_IFACE_INDEX_ANY, USB_CFG_INIT);
	if (err) {
		goto done;
	}

#if USB_HAVE_UGEN
	/* create device nodes for each endpoint */
	usb2_cdev_create(udev);
#endif

done:
	DPRINTF("error=%s\n", usb2_errstr(err));
	if (err) {
		usb2_unconfigure(udev, USB_UNCFG_FLAG_FREE_SUBDEV);
	}
	if (do_unlock) {
		sx_unlock(udev->default_sx + 1);
	}
	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_config_parse
 *
 * This function will allocate and free USB interfaces and USB pipes,
 * parse the USB configuration structure and initialise the USB pipes
 * and interfaces. If "iface_index" is not equal to
 * "USB_IFACE_INDEX_ANY" then the "cmd" parameter is the
 * alternate_setting to be selected for the given interface. Else the
 * "cmd" parameter is defined by "USB_CFG_XXX". "iface_index" can be
 * "USB_IFACE_INDEX_ANY" or a valid USB interface index. This function
 * is typically called when setting the configuration or when setting
 * an alternate interface.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_config_parse(struct usb2_device *udev, uint8_t iface_index, uint8_t cmd)
{
	struct usb2_idesc_parse_state ips;
	struct usb2_interface_descriptor *id;
	struct usb2_endpoint_descriptor *ed;
	struct usb2_interface *iface;
	struct usb2_pipe *pipe;
	usb2_error_t err;
	uint8_t ep_curr;
	uint8_t ep_max;
	uint8_t temp;
	uint8_t do_init;
	uint8_t alt_index;

	if (iface_index != USB_IFACE_INDEX_ANY) {
		/* parameter overload */
		alt_index = cmd;
		cmd = USB_CFG_INIT;
	} else {
		/* not used */
		alt_index = 0;
	}

	err = 0;

	DPRINTFN(5, "iface_index=%d cmd=%d\n",
	    iface_index, cmd);

	if (cmd == USB_CFG_FREE)
		goto cleanup;

	if (cmd == USB_CFG_INIT) {
		sx_assert(udev->default_sx + 1, SA_LOCKED);

		/* check for in-use pipes */

		pipe = udev->pipes;
		ep_max = udev->pipes_max;
		while (ep_max--) {
			/* look for matching pipes */
			if ((iface_index == USB_IFACE_INDEX_ANY) ||
			    (iface_index == pipe->iface_index)) {
				if (pipe->refcount != 0) {
					/*
					 * This typically indicates a
					 * more serious error.
					 */
					err = USB_ERR_IN_USE;
				} else {
					/* reset pipe */
					memset(pipe, 0, sizeof(*pipe));
					/* make sure we don't zero the pipe again */
					pipe->iface_index = USB_IFACE_INDEX_ANY;
				}
			}
			pipe++;
		}

		if (err)
			return (err);
	}

	memset(&ips, 0, sizeof(ips));

	ep_curr = 0;
	ep_max = 0;

	while ((id = usb2_idesc_foreach(udev->cdesc, &ips))) {

		/* check for interface overflow */
		if (ips.iface_index == USB_IFACE_MAX)
			break;			/* crazy */

		iface = udev->ifaces + ips.iface_index;

		/* check for specific interface match */

		if (cmd == USB_CFG_INIT) {
			if ((iface_index != USB_IFACE_INDEX_ANY) && 
			    (iface_index != ips.iface_index)) {
				/* wrong interface */
				do_init = 0;
			} else if (alt_index != ips.iface_index_alt) {
				/* wrong alternate setting */
				do_init = 0;
			} else {
				/* initialise interface */
				do_init = 1;
			}
		} else
			do_init = 0;

		/* check for new interface */
		if (ips.iface_index_alt == 0) {
			/* update current number of endpoints */
			ep_curr = ep_max;
		}
		/* check for init */
		if (do_init) {
			/* setup the USB interface structure */
			iface->idesc = id;
			/* default setting */
			iface->parent_iface_index = USB_IFACE_INDEX_ANY;
			/* set alternate index */
			iface->alt_index = alt_index;
		}

		DPRINTFN(5, "found idesc nendpt=%d\n", id->bNumEndpoints);

		ed = (struct usb2_endpoint_descriptor *)id;

		temp = ep_curr;

		/* iterate all the endpoint descriptors */
		while ((ed = usb2_edesc_foreach(udev->cdesc, ed))) {

			if (temp == USB_EP_MAX)
				break;			/* crazy */

			pipe = udev->pipes + temp;

			if (do_init) {
				usb2_init_pipe(udev, 
				    ips.iface_index, ed, pipe);
			}

			temp ++;

			/* find maximum number of endpoints */
			if (ep_max < temp)
				ep_max = temp;

			/* optimalisation */
			id = (struct usb2_interface_descriptor *)ed;
		}
	}

	/* NOTE: It is valid to have no interfaces and no endpoints! */

	if (cmd == USB_CFG_ALLOC) {
		udev->ifaces_max = ips.iface_index;
		udev->ifaces = NULL;
		if (udev->ifaces_max != 0) {
			udev->ifaces = malloc(sizeof(*iface) * udev->ifaces_max,
			        M_USB, M_WAITOK | M_ZERO);
			if (udev->ifaces == NULL) {
				err = USB_ERR_NOMEM;
				goto done;
			}
		}
		if (ep_max != 0) {
			udev->pipes = malloc(sizeof(*pipe) * ep_max,
			        M_USB, M_WAITOK | M_ZERO);
			if (udev->pipes == NULL) {
				err = USB_ERR_NOMEM;
				goto done;
			}
		} else {
			udev->pipes = NULL;
		}
		USB_BUS_LOCK(udev->bus);
		udev->pipes_max = ep_max;
		/* reset any ongoing clear-stall */
		udev->pipe_curr = NULL;
		USB_BUS_UNLOCK(udev->bus);
	}

done:
	if (err) {
		if (cmd == USB_CFG_ALLOC) {
cleanup:
			USB_BUS_LOCK(udev->bus);
			udev->pipes_max = 0;
			/* reset any ongoing clear-stall */
			udev->pipe_curr = NULL;
			USB_BUS_UNLOCK(udev->bus);

			/* cleanup */
			if (udev->ifaces != NULL)
				free(udev->ifaces, M_USB);
			if (udev->pipes != NULL)
				free(udev->pipes, M_USB);

			udev->ifaces = NULL;
			udev->pipes = NULL;
			udev->ifaces_max = 0;
		}
	}
	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_set_alt_interface_index
 *
 * This function will select an alternate interface index for the
 * given interface index. The interface should not be in use when this
 * function is called. That means there should not be any open USB
 * transfers. Else an error is returned. If the alternate setting is
 * already set this function will simply return success. This function
 * is called in Host mode and Device mode!
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_set_alt_interface_index(struct usb2_device *udev,
    uint8_t iface_index, uint8_t alt_index)
{
	struct usb2_interface *iface = usb2_get_iface(udev, iface_index);
	usb2_error_t err;
	uint8_t do_unlock;

	/* automatic locking */
	if (sx_xlocked(udev->default_sx + 1)) {
		do_unlock = 0;
	} else {
		do_unlock = 1;
		sx_xlock(udev->default_sx + 1);
	}
	if (iface == NULL) {
		err = USB_ERR_INVAL;
		goto done;
	}
	if (udev->flags.usb_mode == USB_MODE_DEVICE) {
		usb2_detach_device(udev, iface_index,
		    USB_UNCFG_FLAG_FREE_SUBDEV);
	} else {
		if (iface->alt_index == alt_index) {
			/* 
			 * Optimise away duplicate setting of
			 * alternate setting in USB Host Mode!
			 */
			err = 0;
			goto done;
		}
	}
#if USB_HAVE_UGEN
	/*
	 * Free all generic FIFOs for this interface, except control
	 * endpoint FIFOs:
	 */
	usb2_fifo_free_wrap(udev, iface_index, 0);
#endif

	err = usb2_config_parse(udev, iface_index, alt_index);
	if (err) {
		goto done;
	}
	err = usb2_req_set_alt_interface_no(udev, NULL, iface_index,
	    iface->idesc->bAlternateSetting);

done:
	if (do_unlock) {
		sx_unlock(udev->default_sx + 1);
	}
	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_set_endpoint_stall
 *
 * This function is used to make a BULK or INTERRUPT endpoint
 * send STALL tokens.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_set_endpoint_stall(struct usb2_device *udev, struct usb2_pipe *pipe,
    uint8_t do_stall)
{
	struct usb2_xfer *xfer;
	uint8_t et;
	uint8_t was_stalled;

	if (pipe == NULL) {
		/* nothing to do */
		DPRINTF("Cannot find endpoint\n");
		/*
		 * Pretend that the clear or set stall request is
		 * successful else some USB host stacks can do
		 * strange things, especially when a control endpoint
		 * stalls.
		 */
		return (0);
	}
	et = (pipe->edesc->bmAttributes & UE_XFERTYPE);

	if ((et != UE_BULK) &&
	    (et != UE_INTERRUPT)) {
		/*
	         * Should not stall control
	         * nor isochronous endpoints.
	         */
		DPRINTF("Invalid endpoint\n");
		return (0);
	}
	USB_BUS_LOCK(udev->bus);

	/* store current stall state */
	was_stalled = pipe->is_stalled;

	/* check for no change */
	if (was_stalled && do_stall) {
		/* if the pipe is already stalled do nothing */
		USB_BUS_UNLOCK(udev->bus);
		DPRINTF("No change\n");
		return (0);
	}
	/* set stalled state */
	pipe->is_stalled = 1;

	if (do_stall || (!was_stalled)) {
		if (!was_stalled) {
			/* lookup the current USB transfer, if any */
			xfer = pipe->pipe_q.curr;
		} else {
			xfer = NULL;
		}

		/*
		 * If "xfer" is non-NULL the "set_stall" method will
		 * complete the USB transfer like in case of a timeout
		 * setting the error code "USB_ERR_STALLED".
		 */
		(udev->bus->methods->set_stall) (udev, xfer, pipe);
	}
	if (!do_stall) {
		pipe->toggle_next = 0;	/* reset data toggle */
		pipe->is_stalled = 0;	/* clear stalled state */

		(udev->bus->methods->clear_stall) (udev, pipe);

		/* start up the current or next transfer, if any */
		usb2_command_wrapper(&pipe->pipe_q, pipe->pipe_q.curr);
	}
	USB_BUS_UNLOCK(udev->bus);
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_reset_iface_endpoints - used in USB device side mode
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_reset_iface_endpoints(struct usb2_device *udev, uint8_t iface_index)
{
	struct usb2_pipe *pipe;
	struct usb2_pipe *pipe_end;
	usb2_error_t err;

	pipe = udev->pipes;
	pipe_end = udev->pipes + udev->pipes_max;

	for (; pipe != pipe_end; pipe++) {

		if ((pipe->edesc == NULL) ||
		    (pipe->iface_index != iface_index)) {
			continue;
		}
		/* simulate a clear stall from the peer */
		err = usb2_set_endpoint_stall(udev, pipe, 0);
		if (err) {
			/* just ignore */
		}
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_detach_device_sub
 *
 * This function will try to detach an USB device. If it fails a panic
 * will result.
 *
 * Flag values, see "USB_UNCFG_FLAG_XXX".
 *------------------------------------------------------------------------*/
static void
usb2_detach_device_sub(struct usb2_device *udev, device_t *ppdev,
    uint8_t flag)
{
	device_t dev;
	int err;

	if (!(flag & USB_UNCFG_FLAG_FREE_SUBDEV)) {

		*ppdev = NULL;

	} else if (*ppdev) {

		/*
		 * NOTE: It is important to clear "*ppdev" before deleting
		 * the child due to some device methods being called late
		 * during the delete process !
		 */
		dev = *ppdev;
		*ppdev = NULL;

		device_printf(dev, "at %s, port %d, addr %d "
		    "(disconnected)\n",
		    device_get_nameunit(udev->parent_dev),
		    udev->port_no, udev->address);

		if (device_is_attached(dev)) {
			if (udev->flags.peer_suspended) {
				err = DEVICE_RESUME(dev);
				if (err) {
					device_printf(dev, "Resume failed!\n");
				}
			}
			if (device_detach(dev)) {
				goto error;
			}
		}
		if (device_delete_child(udev->parent_dev, dev)) {
			goto error;
		}
	}
	return;

error:
	/* Detach is not allowed to fail in the USB world */
	panic("An USB driver would not detach!\n");
}

/*------------------------------------------------------------------------*
 *	usb2_detach_device
 *
 * The following function will detach the matching interfaces.
 * This function is NULL safe.
 *
 * Flag values, see "USB_UNCFG_FLAG_XXX".
 *------------------------------------------------------------------------*/
void
usb2_detach_device(struct usb2_device *udev, uint8_t iface_index,
    uint8_t flag)
{
	struct usb2_interface *iface;
	uint8_t i;

	if (udev == NULL) {
		/* nothing to do */
		return;
	}
	DPRINTFN(4, "udev=%p\n", udev);

	sx_assert(udev->default_sx + 1, SA_LOCKED);

	/*
	 * First detach the child to give the child's detach routine a
	 * chance to detach the sub-devices in the correct order.
	 * Then delete the child using "device_delete_child()" which
	 * will detach all sub-devices from the bottom and upwards!
	 */
	if (iface_index != USB_IFACE_INDEX_ANY) {
		i = iface_index;
		iface_index = i + 1;
	} else {
		i = 0;
		iface_index = USB_IFACE_MAX;
	}

	/* do the detach */

	for (; i != iface_index; i++) {

		iface = usb2_get_iface(udev, i);
		if (iface == NULL) {
			/* looks like the end of the USB interfaces */
			break;
		}
		usb2_detach_device_sub(udev, &iface->subdev, flag);
	}
}

/*------------------------------------------------------------------------*
 *	usb2_probe_and_attach_sub
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
usb2_probe_and_attach_sub(struct usb2_device *udev,
    struct usb2_attach_arg *uaa)
{
	struct usb2_interface *iface;
	device_t dev;
	int err;

	iface = uaa->iface;
	if (iface->parent_iface_index != USB_IFACE_INDEX_ANY) {
		/* leave interface alone */
		return (0);
	}
	dev = iface->subdev;
	if (dev) {

		/* clean up after module unload */

		if (device_is_attached(dev)) {
			/* already a device there */
			return (0);
		}
		/* clear "iface->subdev" as early as possible */

		iface->subdev = NULL;

		if (device_delete_child(udev->parent_dev, dev)) {

			/*
			 * Panic here, else one can get a double call
			 * to device_detach().  USB devices should
			 * never fail on detach!
			 */
			panic("device_delete_child() failed!\n");
		}
	}
	if (uaa->temp_dev == NULL) {

		/* create a new child */
		uaa->temp_dev = device_add_child(udev->parent_dev, NULL, -1);
		if (uaa->temp_dev == NULL) {
			device_printf(udev->parent_dev,
			    "Device creation failed!\n");
			return (1);	/* failure */
		}
		device_set_ivars(uaa->temp_dev, uaa);
		device_quiet(uaa->temp_dev);
	}
	/*
	 * Set "subdev" before probe and attach so that "devd" gets
	 * the information it needs.
	 */
	iface->subdev = uaa->temp_dev;

	if (device_probe_and_attach(iface->subdev) == 0) {
		/*
		 * The USB attach arguments are only available during probe
		 * and attach !
		 */
		uaa->temp_dev = NULL;
		device_set_ivars(iface->subdev, NULL);

		if (udev->flags.peer_suspended) {
			err = DEVICE_SUSPEND(iface->subdev);
			if (err)
				device_printf(iface->subdev, "Suspend failed\n");
		}
		return (0);		/* success */
	} else {
		/* No USB driver found */
		iface->subdev = NULL;
	}
	return (1);			/* failure */
}

/*------------------------------------------------------------------------*
 *	usb2_set_parent_iface
 *
 * Using this function will lock the alternate interface setting on an
 * interface. It is typically used for multi interface drivers. In USB
 * device side mode it is assumed that the alternate interfaces all
 * have the same endpoint descriptors. The default parent index value
 * is "USB_IFACE_INDEX_ANY". Then the alternate setting value is not
 * locked.
 *------------------------------------------------------------------------*/
void
usb2_set_parent_iface(struct usb2_device *udev, uint8_t iface_index,
    uint8_t parent_index)
{
	struct usb2_interface *iface;

	iface = usb2_get_iface(udev, iface_index);
	if (iface) {
		iface->parent_iface_index = parent_index;
	}
}

static void
usb2_init_attach_arg(struct usb2_device *udev,
    struct usb2_attach_arg *uaa)
{
	bzero(uaa, sizeof(*uaa));

	uaa->device = udev;
	uaa->usb_mode = udev->flags.usb_mode;
	uaa->port = udev->port_no;

	uaa->info.idVendor = UGETW(udev->ddesc.idVendor);
	uaa->info.idProduct = UGETW(udev->ddesc.idProduct);
	uaa->info.bcdDevice = UGETW(udev->ddesc.bcdDevice);
	uaa->info.bDeviceClass = udev->ddesc.bDeviceClass;
	uaa->info.bDeviceSubClass = udev->ddesc.bDeviceSubClass;
	uaa->info.bDeviceProtocol = udev->ddesc.bDeviceProtocol;
	uaa->info.bConfigIndex = udev->curr_config_index;
	uaa->info.bConfigNum = udev->curr_config_no;
}

/*------------------------------------------------------------------------*
 *	usb2_probe_and_attach
 *
 * This function is called from "uhub_explore_sub()",
 * "usb2_handle_set_config()" and "usb2_handle_request()".
 *
 * Returns:
 *    0: Success
 * Else: A control transfer failed
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_probe_and_attach(struct usb2_device *udev, uint8_t iface_index)
{
	struct usb2_attach_arg uaa;
	struct usb2_interface *iface;
	uint8_t i;
	uint8_t j;
	uint8_t do_unlock;

	if (udev == NULL) {
		DPRINTF("udev == NULL\n");
		return (USB_ERR_INVAL);
	}
	/* automatic locking */
	if (sx_xlocked(udev->default_sx + 1)) {
		do_unlock = 0;
	} else {
		do_unlock = 1;
		sx_xlock(udev->default_sx + 1);
	}

	if (udev->curr_config_index == USB_UNCONFIG_INDEX) {
		/* do nothing - no configuration has been set */
		goto done;
	}
	/* setup USB attach arguments */

	usb2_init_attach_arg(udev, &uaa);

	/* Check if only one interface should be probed: */
	if (iface_index != USB_IFACE_INDEX_ANY) {
		i = iface_index;
		j = i + 1;
	} else {
		i = 0;
		j = USB_IFACE_MAX;
	}

	/* Do the probe and attach */
	for (; i != j; i++) {

		iface = usb2_get_iface(udev, i);
		if (iface == NULL) {
			/*
			 * Looks like the end of the USB
			 * interfaces !
			 */
			DPRINTFN(2, "end of interfaces "
			    "at %u\n", i);
			break;
		}
		if (iface->idesc == NULL) {
			/* no interface descriptor */
			continue;
		}
		uaa.iface = iface;

		uaa.info.bInterfaceClass =
		    iface->idesc->bInterfaceClass;
		uaa.info.bInterfaceSubClass =
		    iface->idesc->bInterfaceSubClass;
		uaa.info.bInterfaceProtocol =
		    iface->idesc->bInterfaceProtocol;
		uaa.info.bIfaceIndex = i;
		uaa.info.bIfaceNum =
		    iface->idesc->bInterfaceNumber;
		uaa.use_generic = 0;

		DPRINTFN(2, "iclass=%u/%u/%u iindex=%u/%u\n",
		    uaa.info.bInterfaceClass,
		    uaa.info.bInterfaceSubClass,
		    uaa.info.bInterfaceProtocol,
		    uaa.info.bIfaceIndex,
		    uaa.info.bIfaceNum);

		/* try specific interface drivers first */

		if (usb2_probe_and_attach_sub(udev, &uaa)) {
			/* ignore */
		}
		/* try generic interface drivers last */

		uaa.use_generic = 1;

		if (usb2_probe_and_attach_sub(udev, &uaa)) {
			/* ignore */
		}
	}

	if (uaa.temp_dev) {
		/* remove the last created child; it is unused */

		if (device_delete_child(udev->parent_dev, uaa.temp_dev)) {
			DPRINTFN(0, "device delete child failed!\n");
		}
	}
done:
	if (do_unlock) {
		sx_unlock(udev->default_sx + 1);
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_suspend_resume_sub
 *
 * This function is called when the suspend or resume methods should
 * be executed on an USB device.
 *------------------------------------------------------------------------*/
static void
usb2_suspend_resume_sub(struct usb2_device *udev, device_t dev, uint8_t do_suspend)
{
	int err;

	if (dev == NULL) {
		return;
	}
	if (!device_is_attached(dev)) {
		return;
	}
	if (do_suspend) {
		err = DEVICE_SUSPEND(dev);
	} else {
		err = DEVICE_RESUME(dev);
	}
	if (err) {
		device_printf(dev, "%s failed!\n",
		    do_suspend ? "Suspend" : "Resume");
	}
}

/*------------------------------------------------------------------------*
 *	usb2_suspend_resume
 *
 * The following function will suspend or resume the USB device.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_suspend_resume(struct usb2_device *udev, uint8_t do_suspend)
{
	struct usb2_interface *iface;
	uint8_t i;

	if (udev == NULL) {
		/* nothing to do */
		return (0);
	}
	DPRINTFN(4, "udev=%p do_suspend=%d\n", udev, do_suspend);

	sx_assert(udev->default_sx + 1, SA_LOCKED);

	USB_BUS_LOCK(udev->bus);
	/* filter the suspend events */
	if (udev->flags.peer_suspended == do_suspend) {
		USB_BUS_UNLOCK(udev->bus);
		/* nothing to do */
		return (0);
	}
	udev->flags.peer_suspended = do_suspend;
	USB_BUS_UNLOCK(udev->bus);

	/* do the suspend or resume */

	for (i = 0; i != USB_IFACE_MAX; i++) {

		iface = usb2_get_iface(udev, i);
		if (iface == NULL) {
			/* looks like the end of the USB interfaces */
			break;
		}
		usb2_suspend_resume_sub(udev, iface->subdev, do_suspend);
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *      usb2_clear_stall_proc
 *
 * This function performs generic USB clear stall operations.
 *------------------------------------------------------------------------*/
static void
usb2_clear_stall_proc(struct usb2_proc_msg *_pm)
{
	struct usb2_clear_stall_msg *pm = (void *)_pm;
	struct usb2_device *udev = pm->udev;

	/* Change lock */
	USB_BUS_UNLOCK(udev->bus);
	mtx_lock(udev->default_mtx);

	/* Start clear stall callback */
	usb2_transfer_start(udev->default_xfer[1]);

	/* Change lock */
	mtx_unlock(udev->default_mtx);
	USB_BUS_LOCK(udev->bus);
}

/*------------------------------------------------------------------------*
 *	usb2_alloc_device
 *
 * This function allocates a new USB device. This function is called
 * when a new device has been put in the powered state, but not yet in
 * the addressed state. Get initial descriptor, set the address, get
 * full descriptor and get strings.
 *
 * Return values:
 *    0: Failure
 * Else: Success
 *------------------------------------------------------------------------*/
struct usb2_device *
usb2_alloc_device(device_t parent_dev, struct usb2_bus *bus,
    struct usb2_device *parent_hub, uint8_t depth, uint8_t port_index,
    uint8_t port_no, enum usb_dev_speed speed, enum usb_hc_mode mode)
{
	struct usb2_attach_arg uaa;
	struct usb2_device *udev;
	struct usb2_device *adev;
	struct usb2_device *hub;
	uint8_t *scratch_ptr;
	uint32_t scratch_size;
	usb2_error_t err;
	uint8_t device_index;

	DPRINTF("parent_dev=%p, bus=%p, parent_hub=%p, depth=%u, "
	    "port_index=%u, port_no=%u, speed=%u, usb_mode=%u\n",
	    parent_dev, bus, parent_hub, depth, port_index, port_no,
	    speed, mode);

	/*
	 * Find an unused device index. In USB Host mode this is the
	 * same as the device address.
	 *
	 * Device index zero is not used and device index 1 should
	 * always be the root hub.
	 */
	for (device_index = USB_ROOT_HUB_ADDR;
	    (device_index != bus->devices_max) &&
	    (bus->devices[device_index] != NULL);
	    device_index++) /* nop */;

	if (device_index == bus->devices_max) {
		device_printf(bus->bdev,
		    "No free USB device index for new device!\n");
		return (NULL);
	}

	if (depth > 0x10) {
		device_printf(bus->bdev,
		    "Invalid device depth!\n");
		return (NULL);
	}
	udev = malloc(sizeof(*udev), M_USB, M_WAITOK | M_ZERO);
	if (udev == NULL) {
		return (NULL);
	}
	/* initialise our SX-lock */
	sx_init(udev->default_sx, "0123456789ABCDEF - USB device SX lock" + depth);

	/* initialise our SX-lock */
	sx_init(udev->default_sx + 1, "0123456789ABCDEF - USB config SX lock" + depth);

	usb2_cv_init(udev->default_cv, "WCTRL");
	usb2_cv_init(udev->default_cv + 1, "UGONE");

	/* initialise our mutex */
	mtx_init(udev->default_mtx, "USB device mutex", NULL, MTX_DEF);

	/* initialise generic clear stall */
	udev->cs_msg[0].hdr.pm_callback = &usb2_clear_stall_proc;
	udev->cs_msg[0].udev = udev;
	udev->cs_msg[1].hdr.pm_callback = &usb2_clear_stall_proc;
	udev->cs_msg[1].udev = udev;

	/* initialise some USB device fields */
	udev->parent_hub = parent_hub;
	udev->parent_dev = parent_dev;
	udev->port_index = port_index;
	udev->port_no = port_no;
	udev->depth = depth;
	udev->bus = bus;
	udev->address = USB_START_ADDR;	/* default value */
	udev->plugtime = (usb2_ticks_t)ticks;
	usb2_set_device_state(udev, USB_STATE_POWERED);
	/*
	 * We need to force the power mode to "on" because there are plenty
	 * of USB devices out there that do not work very well with
	 * automatic suspend and resume!
	 */
	udev->power_mode = USB_POWER_MODE_ON;
	udev->pwr_save.last_xfer_time = ticks;
	/* we are not ready yet */
	udev->refcount = 1;

	/* set up default endpoint descriptor */
	udev->default_ep_desc.bLength = sizeof(udev->default_ep_desc);
	udev->default_ep_desc.bDescriptorType = UDESC_ENDPOINT;
	udev->default_ep_desc.bEndpointAddress = USB_CONTROL_ENDPOINT;
	udev->default_ep_desc.bmAttributes = UE_CONTROL;
	udev->default_ep_desc.wMaxPacketSize[0] = USB_MAX_IPACKET;
	udev->default_ep_desc.wMaxPacketSize[1] = 0;
	udev->default_ep_desc.bInterval = 0;
	udev->ddesc.bMaxPacketSize = USB_MAX_IPACKET;

	udev->speed = speed;
	udev->flags.usb_mode = mode;

	/* search for our High Speed USB HUB, if any */

	adev = udev;
	hub = udev->parent_hub;

	while (hub) {
		if (hub->speed == USB_SPEED_HIGH) {
			udev->hs_hub_addr = hub->address;
			udev->parent_hs_hub = hub;
			udev->hs_port_no = adev->port_no;
			break;
		}
		adev = hub;
		hub = hub->parent_hub;
	}

	/* init the default pipe */
	usb2_init_pipe(udev, 0,
	    &udev->default_ep_desc,
	    &udev->default_pipe);

	/* set device index */
	udev->device_index = device_index;

#if USB_HAVE_UGEN
	/* Create ugen name */
	snprintf(udev->ugen_name, sizeof(udev->ugen_name),
	    USB_GENERIC_NAME "%u.%u", device_get_unit(bus->bdev),
	    device_index);
	LIST_INIT(&udev->pd_list);

	/* Create the control endpoint device */
	udev->default_dev = usb2_make_dev(udev, 0, FREAD|FWRITE);

	/* Create a link from /dev/ugenX.X to the default endpoint */
	make_dev_alias(udev->default_dev, udev->ugen_name);
#endif
	if (udev->flags.usb_mode == USB_MODE_HOST) {

		err = usb2_req_set_address(udev, NULL, device_index);

		/* This is the new USB device address from now on */

		udev->address = device_index;

		/*
		 * We ignore any set-address errors, hence there are
		 * buggy USB devices out there that actually receive
		 * the SETUP PID, but manage to set the address before
		 * the STATUS stage is ACK'ed. If the device responds
		 * to the subsequent get-descriptor at the new
		 * address, then we know that the set-address command
		 * was successful.
		 */
		if (err) {
			DPRINTFN(0, "set address %d failed "
			    "(%s, ignored)\n", udev->address, 
			    usb2_errstr(err));
		}
		/* allow device time to set new address */
		usb2_pause_mtx(NULL, 
		    USB_MS_TO_TICKS(USB_SET_ADDRESS_SETTLE));
	} else {
		/* We are not self powered */
		udev->flags.self_powered = 0;

		/* Set unconfigured state */
		udev->curr_config_no = USB_UNCONFIG_NO;
		udev->curr_config_index = USB_UNCONFIG_INDEX;

		/* Setup USB descriptors */
		err = (usb2_temp_setup_by_index_p) (udev, usb2_template);
		if (err) {
			DPRINTFN(0, "setting up USB template failed maybe the USB "
			    "template module has not been loaded\n");
			goto done;
		}
	}
	usb2_set_device_state(udev, USB_STATE_ADDRESSED);

	/*
	 * Get the first 8 bytes of the device descriptor !
	 *
	 * NOTE: "usb2_do_request" will check the device descriptor
	 * next time we do a request to see if the maximum packet size
	 * changed! The 8 first bytes of the device descriptor
	 * contains the maximum packet size to use on control endpoint
	 * 0. If this value is different from "USB_MAX_IPACKET" a new
	 * USB control request will be setup!
	 */
	err = usb2_req_get_desc(udev, NULL, NULL, &udev->ddesc,
	    USB_MAX_IPACKET, USB_MAX_IPACKET, 0, UDESC_DEVICE, 0, 0);
	if (err) {
		DPRINTFN(0, "getting device descriptor "
		    "at addr %d failed, %s!\n", udev->address,
		    usb2_errstr(err));
		/* XXX try to re-enumerate the device */
		err = usb2_req_re_enumerate(udev, NULL);
		if (err) {
			goto done;
		}
	}
	DPRINTF("adding unit addr=%d, rev=%02x, class=%d, "
	    "subclass=%d, protocol=%d, maxpacket=%d, len=%d, speed=%d\n",
	    udev->address, UGETW(udev->ddesc.bcdUSB),
	    udev->ddesc.bDeviceClass,
	    udev->ddesc.bDeviceSubClass,
	    udev->ddesc.bDeviceProtocol,
	    udev->ddesc.bMaxPacketSize,
	    udev->ddesc.bLength,
	    udev->speed);

	/* get the full device descriptor */
	err = usb2_req_get_device_desc(udev, NULL, &udev->ddesc);
	if (err) {
		DPRINTF("addr=%d, getting full desc failed\n",
		    udev->address);
		goto done;
	}
	/*
	 * Setup temporary USB attach args so that we can figure out some
	 * basic quirks for this device.
	 */
	usb2_init_attach_arg(udev, &uaa);

	if (usb2_test_quirk(&uaa, UQ_BUS_POWERED)) {
		udev->flags.uq_bus_powered = 1;
	}
	if (usb2_test_quirk(&uaa, UQ_NO_STRINGS)) {
		udev->flags.no_strings = 1;
	}
	/*
	 * Workaround for buggy USB devices.
	 *
	 * It appears that some string-less USB chips will crash and
	 * disappear if any attempts are made to read any string
	 * descriptors.
	 *
	 * Try to detect such chips by checking the strings in the USB
	 * device descriptor. If no strings are present there we
	 * simply disable all USB strings.
	 */
	scratch_ptr = udev->bus->scratch[0].data;
	scratch_size = sizeof(udev->bus->scratch[0].data);

	if (udev->ddesc.iManufacturer ||
	    udev->ddesc.iProduct ||
	    udev->ddesc.iSerialNumber) {
		/* read out the language ID string */
		err = usb2_req_get_string_desc(udev, NULL,
		    (char *)scratch_ptr, 4, scratch_size,
		    USB_LANGUAGE_TABLE);
	} else {
		err = USB_ERR_INVAL;
	}

	if (err || (scratch_ptr[0] < 4)) {
		udev->flags.no_strings = 1;
	} else {
		/* pick the first language as the default */
		udev->langid = UGETW(scratch_ptr + 2);
	}

	/* assume 100mA bus powered for now. Changed when configured. */
	udev->power = USB_MIN_POWER;
	/* fetch the vendor and product strings from the device */
	usb2_set_device_strings(udev);

	if (udev->flags.usb_mode == USB_MODE_HOST) {
		uint8_t config_index;
		uint8_t config_quirk;
		uint8_t set_config_failed = 0;

		/*
		 * Most USB devices should attach to config index 0 by
		 * default
		 */
		if (usb2_test_quirk(&uaa, UQ_CFG_INDEX_0)) {
			config_index = 0;
			config_quirk = 1;
		} else if (usb2_test_quirk(&uaa, UQ_CFG_INDEX_1)) {
			config_index = 1;
			config_quirk = 1;
		} else if (usb2_test_quirk(&uaa, UQ_CFG_INDEX_2)) {
			config_index = 2;
			config_quirk = 1;
		} else if (usb2_test_quirk(&uaa, UQ_CFG_INDEX_3)) {
			config_index = 3;
			config_quirk = 1;
		} else if (usb2_test_quirk(&uaa, UQ_CFG_INDEX_4)) {
			config_index = 4;
			config_quirk = 1;
		} else {
			config_index = 0;
			config_quirk = 0;
		}

repeat_set_config:

		DPRINTF("setting config %u\n", config_index);

		/* get the USB device configured */
		err = usb2_set_config_index(udev, config_index);
		if (err) {
			if (udev->ddesc.bNumConfigurations != 0) {
				if (!set_config_failed) {
					set_config_failed = 1;
					/* XXX try to re-enumerate the device */
					err = usb2_req_re_enumerate(
					    udev, NULL);
					if (err == 0)
					    goto repeat_set_config;
				}
				DPRINTFN(0, "Failure selecting "
				    "configuration index %u: %s, port %u, "
				    "addr %u (ignored)\n",
				    config_index, usb2_errstr(err), udev->port_no,
				    udev->address);
			}
			/*
			 * Some USB devices do not have any
			 * configurations. Ignore any set config
			 * failures!
			 */
			err = 0;
		} else if (config_quirk) {
			/* user quirk selects configuration index */
		} else if ((config_index + 1) < udev->ddesc.bNumConfigurations) {

			if ((udev->cdesc->bNumInterface < 2) &&
			    (usb2_get_no_descriptors(udev->cdesc,
			    UDESC_ENDPOINT) == 0)) {
				DPRINTFN(0, "Found no endpoints "
				    "(trying next config)!\n");
				config_index++;
				goto repeat_set_config;
			}
			if (config_index == 0) {
				/*
				 * Try to figure out if we have an
				 * auto-install disk there:
				 */
				if (usb2_test_autoinstall(udev, 0, 0) == 0) {
					DPRINTFN(0, "Found possible auto-install "
					    "disk (trying next config)\n");
					config_index++;
					goto repeat_set_config;
				}
			}
		} else if (usb2_test_huawei_autoinst_p(udev, &uaa) == 0) {
			DPRINTFN(0, "Found Huawei auto-install disk!\n");
			err = USB_ERR_STALLED;	/* fake an error */
		}
	} else {
		err = 0;		/* set success */
	}

	DPRINTF("new dev (addr %d), udev=%p, parent_hub=%p\n",
	    udev->address, udev, udev->parent_hub);

	/* register our device - we are ready */
	usb2_bus_port_set_device(bus, parent_hub ?
	    parent_hub->hub->ports + port_index : NULL, udev, device_index);

#if USB_HAVE_UGEN
	/* Symlink the ugen device name */
	udev->ugen_symlink = usb2_alloc_symlink(udev->ugen_name);

	/* Announce device */
	printf("%s: <%s> at %s\n", udev->ugen_name, udev->manufacturer,
	    device_get_nameunit(udev->bus->bdev));

	usb2_notify_addq("+", udev);
#endif
done:
	if (err) {
		/* free device  */
		usb2_free_device(udev,
		    USB_UNCFG_FLAG_FREE_SUBDEV |
		    USB_UNCFG_FLAG_FREE_EP0);
		udev = NULL;
	}
	return (udev);
}

#if USB_HAVE_UGEN
static struct cdev *
usb2_make_dev(struct usb2_device *udev, int ep, int mode)
{
	struct usb2_fs_privdata* pd;
	char devname[20];

	/* Store information to locate ourselves again later */
	pd = malloc(sizeof(struct usb2_fs_privdata), M_USBDEV,
	    M_WAITOK | M_ZERO);
	pd->bus_index = device_get_unit(udev->bus->bdev);
	pd->dev_index = udev->device_index;
	pd->ep_addr = ep;
	pd->mode = mode;

	/* Now, create the device itself */
	snprintf(devname, sizeof(devname), "%u.%u.%u",
	    pd->bus_index, pd->dev_index, pd->ep_addr);
	pd->cdev = make_dev(&usb2_devsw, 0, UID_ROOT,
	    GID_OPERATOR, 0600, USB_DEVICE_DIR "/%s", devname);
	pd->cdev->si_drv1 = pd;

	return (pd->cdev);
}

static void
usb2_cdev_create(struct usb2_device *udev)
{
	struct usb2_config_descriptor *cd;
	struct usb2_endpoint_descriptor *ed;
	struct usb2_descriptor *desc;
	struct usb2_fs_privdata* pd;
	struct cdev *dev;
	int inmode, outmode, inmask, outmask, mode;
	uint8_t ep;

	KASSERT(LIST_FIRST(&udev->pd_list) == NULL, ("stale cdev entries"));

	DPRINTFN(2, "Creating device nodes\n");

	if (usb2_get_mode(udev) == USB_MODE_DEVICE) {
		inmode = FWRITE;
		outmode = FREAD;
	} else {		 /* USB_MODE_HOST */
		inmode = FREAD;
		outmode = FWRITE;
	}

	inmask = 0;
	outmask = 0;
	desc = NULL;

	/*
	 * Collect all used endpoint numbers instead of just
	 * generating 16 static endpoints.
	 */
	cd = usb2_get_config_descriptor(udev);
	while ((desc = usb2_desc_foreach(cd, desc))) {
		/* filter out all endpoint descriptors */
		if ((desc->bDescriptorType == UDESC_ENDPOINT) &&
		    (desc->bLength >= sizeof(*ed))) {
			ed = (struct usb2_endpoint_descriptor *)desc;

			/* update masks */
			ep = ed->bEndpointAddress;
			if (UE_GET_DIR(ep)  == UE_DIR_OUT)
				outmask |= 1 << UE_GET_ADDR(ep);
			else
				inmask |= 1 << UE_GET_ADDR(ep);
		}
	}

	/* Create all available endpoints except EP0 */
	for (ep = 1; ep < 16; ep++) {
		mode = inmask & (1 << ep) ? inmode : 0;
		mode |= outmask & (1 << ep) ? outmode : 0;
		if (mode == 0)
			continue;	/* no IN or OUT endpoint */

		dev = usb2_make_dev(udev, ep, mode);
		pd = dev->si_drv1;
		LIST_INSERT_HEAD(&udev->pd_list, pd, pd_next);
	}
}

static void
usb2_cdev_free(struct usb2_device *udev)
{
	struct usb2_fs_privdata* pd;

	DPRINTFN(2, "Freeing device nodes\n");

	while ((pd = LIST_FIRST(&udev->pd_list)) != NULL) {
		KASSERT(pd->cdev->si_drv1 == pd, ("privdata corrupt"));

		destroy_dev_sched_cb(pd->cdev, usb2_cdev_cleanup, pd);
		pd->cdev = NULL;
		LIST_REMOVE(pd, pd_next);
	}
}

static void
usb2_cdev_cleanup(void* arg)
{
	free(arg, M_USBDEV);
}
#endif

/*------------------------------------------------------------------------*
 *	usb2_free_device
 *
 * This function is NULL safe and will free an USB device.
 *
 * Flag values, see "USB_UNCFG_FLAG_XXX".
 *------------------------------------------------------------------------*/
void
usb2_free_device(struct usb2_device *udev, uint8_t flag)
{
	struct usb2_bus *bus;

	if (udev == NULL)
		return;		/* already freed */

	DPRINTFN(4, "udev=%p port=%d\n", udev, udev->port_no);

	bus = udev->bus;
	usb2_set_device_state(udev, USB_STATE_DETACHED);

#if USB_HAVE_UGEN
	usb2_notify_addq("-", udev);

	printf("%s: <%s> at %s (disconnected)\n", udev->ugen_name,
	    udev->manufacturer, device_get_nameunit(bus->bdev));

	/* Destroy UGEN symlink, if any */
	if (udev->ugen_symlink) {
		usb2_free_symlink(udev->ugen_symlink);
		udev->ugen_symlink = NULL;
	}
#endif
	/*
	 * Unregister our device first which will prevent any further
	 * references:
	 */
	usb2_bus_port_set_device(bus, udev->parent_hub ?
	    udev->parent_hub->hub->ports + udev->port_index : NULL,
	    NULL, USB_ROOT_HUB_ADDR);

#if USB_HAVE_UGEN
	/* wait for all pending references to go away: */
	mtx_lock(&usb2_ref_lock);
	udev->refcount--;
	while (udev->refcount != 0) {
		usb2_cv_wait(udev->default_cv + 1, &usb2_ref_lock);
	}
	mtx_unlock(&usb2_ref_lock);

	destroy_dev_sched_cb(udev->default_dev, usb2_cdev_cleanup,
	    udev->default_dev->si_drv1);
#endif

	if (udev->flags.usb_mode == USB_MODE_DEVICE) {
		/* stop receiving any control transfers (Device Side Mode) */
		usb2_transfer_unsetup(udev->default_xfer, USB_DEFAULT_XFER_MAX);
	}

	/* the following will get the device unconfigured in software */
	usb2_unconfigure(udev, flag);

	/* unsetup any leftover default USB transfers */
	usb2_transfer_unsetup(udev->default_xfer, USB_DEFAULT_XFER_MAX);

	/* template unsetup, if any */
	(usb2_temp_unsetup_p) (udev);

	/* 
	 * Make sure that our clear-stall messages are not queued
	 * anywhere:
	 */
	USB_BUS_LOCK(udev->bus);
	usb2_proc_mwait(&udev->bus->non_giant_callback_proc,
	    &udev->cs_msg[0], &udev->cs_msg[1]);
	USB_BUS_UNLOCK(udev->bus);

	sx_destroy(udev->default_sx);
	sx_destroy(udev->default_sx + 1);

	usb2_cv_destroy(udev->default_cv);
	usb2_cv_destroy(udev->default_cv + 1);

	mtx_destroy(udev->default_mtx);
#if USB_HAVE_UGEN
	KASSERT(LIST_FIRST(&udev->pd_list) == NULL, ("leaked cdev entries"));
#endif

	/* free device */
	free(udev, M_USB);
}

/*------------------------------------------------------------------------*
 *	usb2_get_iface
 *
 * This function is the safe way to get the USB interface structure
 * pointer by interface index.
 *
 * Return values:
 *   NULL: Interface not present.
 *   Else: Pointer to USB interface structure.
 *------------------------------------------------------------------------*/
struct usb2_interface *
usb2_get_iface(struct usb2_device *udev, uint8_t iface_index)
{
	struct usb2_interface *iface = udev->ifaces + iface_index;

	if (iface_index >= udev->ifaces_max)
		return (NULL);
	return (iface);
}

/*------------------------------------------------------------------------*
 *	usb2_find_descriptor
 *
 * This function will lookup the first descriptor that matches the
 * criteria given by the arguments "type" and "subtype". Descriptors
 * will only be searched within the interface having the index
 * "iface_index".  If the "id" argument points to an USB descriptor,
 * it will be skipped before the search is started. This allows
 * searching for multiple descriptors using the same criteria. Else
 * the search is started after the interface descriptor.
 *
 * Return values:
 *   NULL: End of descriptors
 *   Else: A descriptor matching the criteria
 *------------------------------------------------------------------------*/
void   *
usb2_find_descriptor(struct usb2_device *udev, void *id, uint8_t iface_index,
    uint8_t type, uint8_t type_mask,
    uint8_t subtype, uint8_t subtype_mask)
{
	struct usb2_descriptor *desc;
	struct usb2_config_descriptor *cd;
	struct usb2_interface *iface;

	cd = usb2_get_config_descriptor(udev);
	if (cd == NULL) {
		return (NULL);
	}
	if (id == NULL) {
		iface = usb2_get_iface(udev, iface_index);
		if (iface == NULL) {
			return (NULL);
		}
		id = usb2_get_interface_descriptor(iface);
		if (id == NULL) {
			return (NULL);
		}
	}
	desc = (void *)id;

	while ((desc = usb2_desc_foreach(cd, desc))) {

		if (desc->bDescriptorType == UDESC_INTERFACE) {
			break;
		}
		if (((desc->bDescriptorType & type_mask) == type) &&
		    ((desc->bDescriptorSubtype & subtype_mask) == subtype)) {
			return (desc);
		}
	}
	return (NULL);
}

/*------------------------------------------------------------------------*
 *	usb2_devinfo
 *
 * This function will dump information from the device descriptor
 * belonging to the USB device pointed to by "udev", to the string
 * pointed to by "dst_ptr" having a maximum length of "dst_len" bytes
 * including the terminating zero.
 *------------------------------------------------------------------------*/
void
usb2_devinfo(struct usb2_device *udev, char *dst_ptr, uint16_t dst_len)
{
	struct usb2_device_descriptor *udd = &udev->ddesc;
	uint16_t bcdDevice;
	uint16_t bcdUSB;

	bcdUSB = UGETW(udd->bcdUSB);
	bcdDevice = UGETW(udd->bcdDevice);

	if (udd->bDeviceClass != 0xFF) {
		snprintf(dst_ptr, dst_len, "%s %s, class %d/%d, rev %x.%02x/"
		    "%x.%02x, addr %d",
		    udev->manufacturer, udev->product,
		    udd->bDeviceClass, udd->bDeviceSubClass,
		    (bcdUSB >> 8), bcdUSB & 0xFF,
		    (bcdDevice >> 8), bcdDevice & 0xFF,
		    udev->address);
	} else {
		snprintf(dst_ptr, dst_len, "%s %s, rev %x.%02x/"
		    "%x.%02x, addr %d",
		    udev->manufacturer, udev->product,
		    (bcdUSB >> 8), bcdUSB & 0xFF,
		    (bcdDevice >> 8), bcdDevice & 0xFF,
		    udev->address);
	}
}

#if USB_VERBOSE
/*
 * Descriptions of of known vendors and devices ("products").
 */
struct usb_knowndev {
	uint16_t vendor;
	uint16_t product;
	uint32_t flags;
	const char *vendorname;
	const char *productname;
};

#define	USB_KNOWNDEV_NOPROD	0x01	/* match on vendor only */

#include "usbdevs.h"
#include "usbdevs_data.h"
#endif					/* USB_VERBOSE */

static void
usb2_set_device_strings(struct usb2_device *udev)
{
	struct usb2_device_descriptor *udd = &udev->ddesc;
#if USB_VERBOSE
	const struct usb_knowndev *kdp;
#endif
	char temp[64];
	uint16_t vendor_id;
	uint16_t product_id;

	vendor_id = UGETW(udd->idVendor);
	product_id = UGETW(udd->idProduct);

	/* get serial number string */
	bzero(temp, sizeof(temp));
	usb2_req_get_string_any(udev, NULL, temp, sizeof(temp),
	    udev->ddesc.iSerialNumber);
	udev->serial = strdup(temp, M_USB);

	/* get manufacturer string */
	bzero(temp, sizeof(temp));
	usb2_req_get_string_any(udev, NULL, temp, sizeof(temp),
	    udev->ddesc.iManufacturer);
	usb2_trim_spaces(temp);
	if (temp[0] != '\0')
		udev->manufacturer = strdup(temp, M_USB);

	/* get product string */
	bzero(temp, sizeof(temp));
	usb2_req_get_string_any(udev, NULL, temp, sizeof(temp),
	    udev->ddesc.iProduct);
	usb2_trim_spaces(temp);
	if (temp[0] != '\0')
		udev->product = strdup(temp, M_USB);

#if USB_VERBOSE
	if (udev->manufacturer == NULL || udev->product == NULL) {
		for (kdp = usb_knowndevs; kdp->vendorname != NULL; kdp++) {
			if (kdp->vendor == vendor_id &&
			    (kdp->product == product_id ||
			    (kdp->flags & USB_KNOWNDEV_NOPROD) != 0))
				break;
		}
		if (kdp->vendorname != NULL) {
			/* XXX should use pointer to knowndevs string */
			if (udev->manufacturer == NULL) {
				udev->manufacturer = strdup(kdp->vendorname,
				    M_USB);
			}
			if (udev->product == NULL &&
			    (kdp->flags & USB_KNOWNDEV_NOPROD) == 0) {
				udev->product = strdup(kdp->productname,
				    M_USB);
			}
		}
	}
#endif
	/* Provide default strings if none were found */
	if (udev->manufacturer == NULL) {
		snprintf(temp, sizeof(temp), "vendor 0x%04x", vendor_id);
		udev->manufacturer = strdup(temp, M_USB);
	}
	if (udev->product == NULL) {
		snprintf(temp, sizeof(temp), "product 0x%04x", product_id);
		udev->product = strdup(temp, M_USB);
	}
}

/*
 * Returns:
 * See: USB_MODE_XXX
 */
enum usb_hc_mode
usb2_get_mode(struct usb2_device *udev)
{
	return (udev->flags.usb_mode);
}

/*
 * Returns:
 * See: USB_SPEED_XXX
 */
enum usb_dev_speed
usb2_get_speed(struct usb2_device *udev)
{
	return (udev->speed);
}

uint32_t
usb2_get_isoc_fps(struct usb2_device *udev)
{
	;				/* indent fix */
	switch (udev->speed) {
	case USB_SPEED_LOW:
	case USB_SPEED_FULL:
		return (1000);
	default:
		return (8000);
	}
}

struct usb2_device_descriptor *
usb2_get_device_descriptor(struct usb2_device *udev)
{
	if (udev == NULL)
		return (NULL);		/* be NULL safe */
	return (&udev->ddesc);
}

struct usb2_config_descriptor *
usb2_get_config_descriptor(struct usb2_device *udev)
{
	if (udev == NULL)
		return (NULL);		/* be NULL safe */
	return (udev->cdesc);
}

/*------------------------------------------------------------------------*
 *	usb2_test_quirk - test a device for a given quirk
 *
 * Return values:
 * 0: The USB device does not have the given quirk.
 * Else: The USB device has the given quirk.
 *------------------------------------------------------------------------*/
uint8_t
usb2_test_quirk(const struct usb2_attach_arg *uaa, uint16_t quirk)
{
	uint8_t found;

	found = (usb2_test_quirk_p) (&uaa->info, quirk);
	return (found);
}

struct usb2_interface_descriptor *
usb2_get_interface_descriptor(struct usb2_interface *iface)
{
	if (iface == NULL)
		return (NULL);		/* be NULL safe */
	return (iface->idesc);
}

uint8_t
usb2_get_interface_altindex(struct usb2_interface *iface)
{
	return (iface->alt_index);
}

uint8_t
usb2_get_bus_index(struct usb2_device *udev)
{
	return ((uint8_t)device_get_unit(udev->bus->bdev));
}

uint8_t
usb2_get_device_index(struct usb2_device *udev)
{
	return (udev->device_index);
}

#if USB_HAVE_UGEN
/*------------------------------------------------------------------------*
 *	usb2_notify_addq
 *
 * This function will generate events for dev.
 *------------------------------------------------------------------------*/
static void
usb2_notify_addq(const char *type, struct usb2_device *udev)
{
	char *data = NULL;
	struct malloc_type *mt;

	mtx_lock(&malloc_mtx);
	mt = malloc_desc2type("bus");	/* XXX M_BUS */
	mtx_unlock(&malloc_mtx);
	if (mt == NULL)
		return;

	data = malloc(512, mt, M_NOWAIT);
	if (data == NULL)
		return;

	/* String it all together. */
	snprintf(data, 1024,
	    "%s"
	    "%s "
	    "vendor=0x%04x "
	    "product=0x%04x "
	    "devclass=0x%02x "
	    "devsubclass=0x%02x "
	    "sernum=\"%s\" "
	    "at "
	    "port=%u "
	    "on "
	    "%s\n",
	    type,
	    udev->ugen_name,
	    UGETW(udev->ddesc.idVendor),
	    UGETW(udev->ddesc.idProduct),
	    udev->ddesc.bDeviceClass,
	    udev->ddesc.bDeviceSubClass,
	    udev->serial,
	    udev->port_no,
	    udev->parent_hub != NULL ?
		udev->parent_hub->ugen_name :
		device_get_nameunit(device_get_parent(udev->bus->bdev)));

	devctl_queue_data(data);
}

/*------------------------------------------------------------------------*
 *	usb2_fifo_free_wrap
 *
 * This function will free the FIFOs.
 *
 * Description of "flag" argument: If the USB_UNCFG_FLAG_FREE_EP0 flag
 * is set and "iface_index" is set to "USB_IFACE_INDEX_ANY", we free
 * all FIFOs. If the USB_UNCFG_FLAG_FREE_EP0 flag is not set and
 * "iface_index" is set to "USB_IFACE_INDEX_ANY", we free all non
 * control endpoint FIFOs. If "iface_index" is not set to
 * "USB_IFACE_INDEX_ANY" the flag has no effect.
 *------------------------------------------------------------------------*/
static void
usb2_fifo_free_wrap(struct usb2_device *udev,
    uint8_t iface_index, uint8_t flag)
{
	struct usb2_fifo *f;
	uint16_t i;

	/*
	 * Free any USB FIFOs on the given interface:
	 */
	for (i = 0; i != USB_FIFO_MAX; i++) {
		f = udev->fifo[i];
		if (f == NULL) {
			continue;
		}
		/* Check if the interface index matches */
		if (iface_index == f->iface_index) {
			if (f->methods != &usb2_ugen_methods) {
				/*
				 * Don't free any non-generic FIFOs in
				 * this case.
				 */
				continue;
			}
			if ((f->dev_ep_index == 0) &&
			    (f->fs_xfer == NULL)) {
				/* no need to free this FIFO */
				continue;
			}
		} else if (iface_index == USB_IFACE_INDEX_ANY) {
			if ((f->methods == &usb2_ugen_methods) &&
			    (f->dev_ep_index == 0) &&
			    (!(flag & USB_UNCFG_FLAG_FREE_EP0)) &&
			    (f->fs_xfer == NULL)) {
				/* no need to free this FIFO */
				continue;
			}
		} else {
			/* no need to free this FIFO */
			continue;
		}
		/* free this FIFO */
		usb2_fifo_free(f);
	}
}
#endif

/*------------------------------------------------------------------------*
 *	usb2_peer_can_wakeup
 *
 * Return values:
 * 0: Peer cannot do resume signalling.
 * Else: Peer can do resume signalling.
 *------------------------------------------------------------------------*/
uint8_t
usb2_peer_can_wakeup(struct usb2_device *udev)
{
	const struct usb2_config_descriptor *cdp;

	cdp = udev->cdesc;
	if ((cdp != NULL) && (udev->flags.usb_mode == USB_MODE_HOST)) {
		return (cdp->bmAttributes & UC_REMOTE_WAKEUP);
	}
	return (0);			/* not supported */
}

void
usb2_set_device_state(struct usb2_device *udev, enum usb_dev_state state)
{

	KASSERT(state < USB_STATE_MAX, ("invalid udev state"));

	DPRINTF("udev %p state %s -> %s\n", udev,
	    usb2_statestr(udev->state), usb2_statestr(state));
	udev->state = state;
}

uint8_t
usb2_device_attached(struct usb2_device *udev)
{
	return (udev->state > USB_STATE_DETACHED);
}
