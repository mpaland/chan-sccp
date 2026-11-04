/*!
 * \file	sccp_featureParkingLot.c
 * \brief	SCCP ParkingLot Class
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2015-Sept-16
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 */
#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "");

#include "sccp_featureParkingLot.h"

#ifdef CS_SCCP_PARK

#include "sccp_utils.h"
#include "sccp_vector.h"
#include "sccp_device.h"
#include "sccp_line.h"
#	include "sccp_linedevice.h"
#	include "sccp_channel.h"
#	include "sccp_feature.h"
#	include "sccp_labels.h"

static const uint32_t appID = APPID_VISUALPARKINGLOT;

#  include <asterisk/app.h>

/* asterisk-11 */
/*
Event: ParkedCall
Privilege: call,all
Timestamp: 1460205775.670404
Exten: 701
Channel: SCCP/10041-0000000e
Parkinglot: default
From: SCCP/10011-0000000f
Timeout: 45
CallerIDNum: 10041
CallerIDName: PHONE4
ConnectedLineNum: <unknown>
ConnectedLineName: <unknown>
Uniqueid: 1460205775.48

Event: UnParkedCall
Privilege: call,all
Timestamp: 1460205785.934545
Exten: 701
Channel: SCCP/10041-0000000e
Parkinglot: default
From: SCCP/10031-00000010
CallerIDNum: 10041
CallerIDName: PHONE4
ConnectedLineNum: <unknown>
ConnectedLineName: <unknown>
Uniqueid: 1460205775.48

Event: ParkedCallGiveUp
Privilege: call,all
Timestamp: 1460819185.922496
Exten: 701
Channel: SCCP/10041-00000001
Parkinglot: default
CallerIDNum: 10041
CallerIDName: PHONE4
ConnectedLineNum: 10011
ConnectedLineName: Diederik-Phone1
UniqueID: 1460819174.54

Event: ParkedCallTimeOut
Privilege: call,all
Timestamp: 1460974082.683646
Exten: 701
Channel: SCCP/10011-00000003
Parkinglot: default
CallerIDNum: 10011
CallerIDName: Diederik-Phone1
ConnectedLineNum: 10031
ConnectedLineName: Diederik-Phone3
UniqueID: 1460974037.17
*/

/* asterisk-13 */
/*
Event: ParkedCall
Privilege: call,all
SequenceNumber: 118
File: parking/parking_manager.c
Line: 676
Func: parked_call_message_response
ParkeeChannel: SCCP/10011-00000001
ParkeeChannelState: 6
ParkeeChannelStateDesc: Up
ParkeeCallerIDNum: 10011
ParkeeCallerIDName: Diederik-Phone1
ParkeeConnectedLineNum: <unknown>
ParkeeConnectedLineName: <unknown>
ParkeeLanguage: en
ParkeeAccountCode: 10011
ParkeeContext: internal
ParkeeExten: 10031
ParkeePriority: 3
ParkeeUniqueid: 1461160476.0
ParkeeLinkedid: 1461160476.0
ParkerDialString: SCCP/10031
Parkinglot: default
ParkingSpace: 701
ParkingTimeout: 45
ParkingDuration: 0

UnParkedCall
Privilege: call,all
SequenceNumber: 1091
File: parking/parking_manager.c
Line: 676
Func: parked_call_message_response
ParkeeChannel: SCCP/10011-00000001
ParkeeChannelState: 6
ParkeeChannelStateDesc: Up
ParkeeCallerIDNum: 10011
ParkeeCallerIDName: Diederik-Phone1
ParkeeConnectedLineNum: <unknown>
ParkeeConnectedLineName: <unknown>
ParkeeLanguage: en
ParkeeAccountCode: 10011
ParkeeContext: internal
ParkeeExten: 10031
ParkeePriority: 3
ParkeeUniqueid: 1461161791.29
ParkeeLinkedid: 1461161791.29
RetrieverChannel: SCCP/10041-00000003
RetrieverChannelState: 6
RetrieverChannelStateDesc: Up
RetrieverCallerIDNum: 10041
RetrieverCallerIDName: PHONE4
RetrieverConnectedLineNum: <unknown>
RetrieverConnectedLineName: <unknown>
RetrieverLanguage: en
RetrieverAccountCode: 79005
RetrieverContext: internal
RetrieverExten: 701
RetrieverPriority: 1
RetrieverUniqueid: 1461161803.31
RetrieverLinkedid: 1461161803.31
ParkerDialString: SCCP/10031
Parkinglot: default
ParkingSpace: 701
ParkingTimeout: 35
ParkingDuration: 10

Event: ParkedCallGiveUp
Privilege: call,all
SequenceNumber: 142
File: parking/parking_manager.c
Line: 676
Func: parked_call_message_response
ParkeeChannel: SCCP/10011-00000001
ParkeeChannelState: 6
ParkeeChannelStateDesc: Up
ParkeeCallerIDNum: 10011
ParkeeCallerIDName: Diederik-Phone1
ParkeeConnectedLineNum: <unknown>
ParkeeConnectedLineName: <unknown>
ParkeeLanguage: en
ParkeeAccountCode: 10011
ParkeeContext: internal
ParkeeExten: 10031
ParkeePriority: 3
ParkeeUniqueid: 1461160476.0
ParkeeLinkedid: 1461160476.0
ParkerDialString: SCCP/10031
Parkinglot: default
ParkingSpace: 701
ParkingTimeout: 36
ParkingDuration: 9

Event: ParkedCallTimeOut
Privilege: call,all
SequenceNumber: 427
File: parking/parking_manager.c
Line: 676
Func: parked_call_message_response
ParkeeChannel: SCCP/10011-00000007
ParkeeChannelState: 6
ParkeeChannelStateDesc: Up
ParkeeCallerIDNum: 10011
ParkeeCallerIDName: Diederik-Phone1
ParkeeConnectedLineNum: <unknown>
ParkeeConnectedLineName: <unknown>
ParkeeLanguage: en
ParkeeAccountCode: 10011
ParkeeContext: park-dial
ParkeeExten: SCCP_10031
ParkeePriority: 1
ParkeeUniqueid: 1461160740.12
ParkeeLinkedid: 1461160740.12
ParkerDialString: SCCP/10031
Parkinglot: default
ParkingSpace: 701
ParkingTimeout: 0
ParkingDuration: 45
*/

/* forward declarations */
struct parkinglot;
typedef struct parkinglot sccp_parkinglot_t;
static void notifyLocked(sccp_parkinglot_t *pl);

typedef struct plslot plslot_t;
typedef struct plobserver plobserver_t;

/* private variables */
struct plslot {
	int slot;
	const char *exten;
	const char *from;
	const char *channel;
	const char *callerid_num;
	const char *callerid_name;
	const char *connectedline_num;
	const char *connectedline_name;
};

struct plobserver {
	sccp_device_t * device;
	uint8_t instance;
	uint32_t transactionId;											/* the id handed to the phone is 32 bit: truncated to 8 bit it could read back as 0 = "no window" */
};

struct parkinglot {
	pbx_mutex_t lock;
	char *context;
	unsigned int inflight;											/* notifications that dropped ->lock to talk to phones and will take it again: removal waits for 0 */
	SCCP_VECTOR(, plobserver_t) observers;
	SCCP_VECTOR(, plslot_t) slots;
	SCCP_RWLIST_ENTRY(sccp_parkinglot_t) list;
};

#define ICONSTATE_NEW_ON 0x020303												// option:closed, color=yellow, flashspeed=slow
#define ICONSTATE_NEW_OFF 0x010000												// option:open, color=off, flashspeed=None
#define ICONSTATE_OLD_ON 1													// option:closed
#define ICONSTATE_OLD_OFF 0													// option:open

/* private functions */
//#define sccp_parkinglot_lock(x)	({sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_2 "%s:%d:requestinglock:%p\n",__PRETTY_FUNCTION__,__LINE__,x);pbx_mutex_lock(&((sccp_parkinglot_t * const)(x))->lock);sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_2 "%s:%d:locked:%p\n",__PRETTY_FUNCTION__,__LINE__,x);})				// discard const
//#define sccp_parkinglot_unlock(x)	({sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_2 "%s:%d:unlock:%p\n",__PRETTY_FUNCTION__,__LINE__,x);pbx_mutex_unlock(&((sccp_parkinglot_t * const)(x))->lock);})			// discard const
#define sccp_parkinglot_lock(x)		({pbx_mutex_lock(&((sccp_parkinglot_t * const)(x))->lock);})				// discard const
#define sccp_parkinglot_unlock(x)	({if (x) {pbx_mutex_unlock(&((sccp_parkinglot_t * const)(x))->lock);}})		// discard const; NULL safe for the RAII cleanups

SCCP_RWLIST_HEAD(sccp_parkinglot_vector, sccp_parkinglot_t) parkinglots;
/*
 * Locking:
 * - lock order: parkinglots (list) first, then pl->lock
 * - pl->lock is never held while talking to a device (lamp, button state, feature event, visual parkinglot):
 *   devices hold their buttonconfig lock when they call in here (registration), and the slot changes arrive on
 *   the manager hook thread. The device side runs from a snapshot with pl->lock dropped (pl->inflight keeps the
 *   parkinglot alive meanwhile) and observers are looked up again by (device, instance) afterwards, never kept
 *   as pointers into pl->observers across an unlock.
 */
static void __attribute__((constructor)) sccp_parkinglots_init(void)
{
	SCCP_RWLIST_HEAD_INIT(&parkinglots);
}
static void __attribute__((destructor)) sccp_parkinglots_destroy(void)
{
	SCCP_RWLIST_HEAD_DESTROY(&parkinglots);
}
#define OBSERVER_CB_CMP(elem, value) ((elem).device == (value).device && (elem).instance == (value).instance)
#define SLOT_CB_CMP(elem, value) ((elem).slot == (value))

#define SLOT_CLEANUP(elem) 							\
	if ((elem).exten) {sccp_free((elem).exten);}				\
	if ((elem).from) {sccp_free((elem).from);}				\
	if ((elem).channel) {sccp_free((elem).channel);}			\
	if ((elem).callerid_num) {sccp_free((elem).callerid_num);}		\
	if ((elem).callerid_name) {sccp_free((elem).callerid_name);}		\
	if ((elem).connectedline_num) {sccp_free((elem).connectedline_num);}	\
	if ((elem).connectedline_name) {sccp_free((elem).connectedline_name);}

static void destroyParkinglot(sccp_parkinglot_t *pl)
{
	if (pl->context) {
		sccp_free(pl->context);
	}
	SCCP_VECTOR_RESET(&pl->observers, SCCP_VECTOR_ELEM_CLEANUP_NOOP);
	SCCP_VECTOR_FREE(&pl->observers);
	SCCP_VECTOR_RESET(&pl->slots, SLOT_CLEANUP);
	SCCP_VECTOR_FREE(&pl->slots);
	pbx_mutex_destroy(&pl->lock);
	sccp_free(pl);
}

/* called with parkinglots write locked */
static sccp_parkinglot_t * findParkinglotByContextListLocked(const char *parkinglot)
{
	sccp_parkinglot_t *pl = NULL;
	SCCP_RWLIST_TRAVERSE(&parkinglots, pl, list) {
		if (sccp_strcaseequals(pl->context, parkinglot)) {
			break;
		}
	}
	return pl;
}

/* exported functions */
/* returns locked pl: it is locked while the list is still write locked, so nobody can remove it in between */
static sccp_parkinglot_t * addParkinglot(const char *parkinglot)
{
	pbx_assert(parkinglot != NULL);

	sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "SCCP: (addParkinglot) %s\n", parkinglot);
	sccp_parkinglot_t *pl = (sccp_parkinglot_t *) sccp_calloc(sizeof(sccp_parkinglot_t), 1);
	if (!pl) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return NULL;
	}
	pl->context = pbx_strdup(parkinglot);
	pbx_mutex_init(&pl->lock);
	SCCP_VECTOR_INIT(&pl->observers,1);
	SCCP_VECTOR_INIT(&pl->slots,1);

	SCCP_RWLIST_WRLOCK(&parkinglots);
	sccp_parkinglot_t *existing = findParkinglotByContextListLocked(parkinglot);		/* a concurrent lookup may have created it meanwhile */
	if (existing) {
		sccp_parkinglot_lock(existing);
		SCCP_RWLIST_UNLOCK(&parkinglots);
		destroyParkinglot(pl);
		return existing;
	}
	SCCP_RWLIST_INSERT_HEAD(&parkinglots, pl, list);
	sccp_parkinglot_lock(pl);
	SCCP_RWLIST_UNLOCK(&parkinglots);
	return pl;
}

/* called with no parkinglot locked; removes and destroys the parkinglot when it is unobserved and no notification
 * is in flight. Looked up by context: a pointer kept across an unlock may already have been removed by someone else. */
static int removeParkinglot(const char *context)
{
	pbx_assert(context != NULL);

	sccp_parkinglot_t *removed = NULL;
	sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "SCCP: (removeParkinglot) %s\n", context);

	SCCP_RWLIST_WRLOCK(&parkinglots);
	sccp_parkinglot_t *pl = findParkinglotByContextListLocked(context);
	if (pl) {
		sccp_parkinglot_lock(pl);
		if (SCCP_VECTOR_SIZE(&pl->observers) == 0 && pl->inflight == 0) {				/* re-check under both locks */
			removed = SCCP_RWLIST_REMOVE(&parkinglots, pl, list);
		}
		sccp_parkinglot_unlock(pl);
	}
	SCCP_RWLIST_UNLOCK(&parkinglots);

	if (removed) {
		destroyParkinglot(removed);
	}
	sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "SCCP: (removeParkinglot) %s\n", removed ? "done" : "still in use");
	return removed ? TRUE : FALSE;
}

/* returns locked pl */
static sccp_parkinglot_t * const findParkinglotByContext(const char *parkinglot)
{
	pbx_assert(parkinglot != NULL);

	sccp_parkinglot_t *pl = NULL;
	SCCP_RWLIST_RDLOCK(&parkinglots);
	SCCP_RWLIST_TRAVERSE(&parkinglots, pl, list) {
		sccp_parkinglot_lock(pl);
		if (sccp_strcaseequals(pl->context, parkinglot)) {
			//sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "SCCP: (findParkinglotByContext) found match:%s\n", pl->context);
			// returning parkinglot locked
			break;
		}
		sccp_parkinglot_unlock(pl);
	}
	SCCP_RWLIST_UNLOCK(&parkinglots);
	return pl;
}

/* returns locked pl */
static sccp_parkinglot_t * const findCreateParkinglot(const char *parkinglot, boolean_t create)
{
	pbx_assert(parkinglot != NULL);

	//sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "SCCP: (findCreateParkinglot) %s (create:%s)\n", parkinglot, create ? "TRUE" : "FALSE");
	sccp_parkinglot_t *pl = findParkinglotByContext(parkinglot);
	if (!pl && create) {
		pl = addParkinglot(parkinglot);									/* returns locked */
	}
	sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "SCCP: (findCreateParkinglot) Found:%s \n", pl ? "TRUE" : "FALSE");
	return pl;
}

/* called with pl locked */
static plobserver_t * findObserverLocked(sccp_parkinglot_t *pl, constDevicePtr d, uint8_t instance)
{
	for (size_t idx = 0; idx < SCCP_VECTOR_SIZE(&pl->observers); idx++) {
		plobserver_t *observer = SCCP_VECTOR_GET_ADDR(&pl->observers, idx);
		if (observer && observer->device == d && observer->instance == instance) {
			return observer;
		}
	}
	return NULL;
}

// observer
static int attachObserver(sccp_device_t * device, const sccp_buttonconfig_t * const buttonConfig)
{
	pbx_assert(device != NULL && buttonConfig != NULL);
	int res = FALSE;

	if(!sccp_strlen_zero(buttonConfig->button.feature.options)) {
		sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (attachObserver) device:%s at instance:%d\n", buttonConfig->button.feature.options, device->id, buttonConfig->instance);
		RAII(sccp_parkinglot_t *, pl, findCreateParkinglot(buttonConfig->button.feature.options, TRUE), sccp_parkinglot_unlock);
		if (pl) {
			plobserver_t observer = {
				.device = device,
				.instance = buttonConfig->instance,
				.transactionId = 0,
			};

			if (findObserverLocked(pl, device, buttonConfig->instance)) {				/* already observing (re-registration) */
				res = TRUE;
			} else if (SCCP_VECTOR_APPEND(&pl->observers, observer) == 0) {
				res = TRUE;
			}
		}
	}
	return res;
}

static int detachObserver(sccp_device_t * device, const sccp_buttonconfig_t * const buttonConfig)
{
	pbx_assert(device != NULL && buttonConfig != NULL);
	int res = FALSE;

	if(!sccp_strlen_zero(buttonConfig->button.feature.options)) {
		sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (detachObserver) device:%s at instance:%d\n", buttonConfig->button.feature.options, device->id, buttonConfig->instance);
		sccp_parkinglot_t * pl = findCreateParkinglot(buttonConfig->button.feature.options, FALSE); /* don't use RAII, removeParkinglot may destroy it */
		if (pl) {
			plobserver_t cmp = {
				.device = device,
				.instance = buttonConfig->instance,
			};
			if (SCCP_VECTOR_REMOVE_CMP_UNORDERED(&pl->observers, cmp, OBSERVER_CB_CMP, SCCP_VECTOR_ELEM_CLEANUP_NOOP) == 0) {
				res = TRUE;
			}
			boolean_t unused = (SCCP_VECTOR_SIZE(&pl->observers) == 0);
			sccp_parkinglot_unlock(pl);
			if (unused) {
				removeParkinglot(buttonConfig->button.feature.options);			/* by context, pl is not to be touched any more */
			}
		}
	}
	return res;
}

static char * const getParkingLotCXML(sccp_parkinglot_t *pl, int protocolversion, uint8_t instance, uint32_t transactionId, char **const outbuf)
{
	pbx_assert(pl != NULL && outbuf != NULL);

	sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (getParkingLotCXML) with version:%d\n", pl->context, protocolversion);
	*outbuf = NULL;
	if (SCCP_VECTOR_SIZE(&pl->slots)) {
		pbx_str_t *buf = ast_str_create(DEFAULT_PBX_STR_BUFFERSIZE);
		pbx_str_append(&buf, 0, "<?xml version=\"1.0\"?>");
		if (protocolversion < 15) {
			pbx_str_append(&buf, 0, "<CiscoIPPhoneMenu>");
		} else {
			pbx_str_append(&buf, 0, "<CiscoIPPhoneMenu appId='%d' onAppClosed='%d'>", appID, appID);
		}
		pbx_str_append(&buf, 0, "<Title>Parked Calls</Title>");
		pbx_str_append(&buf, 0, "<Prompt>Choose a ParkingLot Slot</Prompt>");
		for (size_t idx = 0; idx < SCCP_VECTOR_SIZE(&pl->slots); idx++) {
			plslot_t *slot = SCCP_VECTOR_GET_ADDR(&pl->slots, idx);
			pbx_str_append(&buf, 0, "<MenuItem>");
			const char *connected_line = !sccp_strcaseequals(slot->connectedline_name, "<unknown>") ? slot->connectedline_name : slot->from;
			if (!sccp_strcaseequals(slot->callerid_name, "<unknown>")) {
				pbx_str_append(&buf, 0, "<Name>%s (%s) by %s</Name>", slot->callerid_name, slot->callerid_num, connected_line);
			} else {
				pbx_str_append(&buf, 0, "<Name>%s by %s</Name>", slot->callerid_num, connected_line);
			}
			pbx_str_append(&buf, 0, "<URL>UserCallData:%d:%d:%d:%d:%s/%s</URL>", appID, instance, 0, transactionId, pl->context, slot->exten);
			pbx_str_append(&buf, 0, "</MenuItem>");
		}
		pbx_str_append(&buf, 0, "<SoftKeyItem>");
		pbx_str_append(&buf, 0, "<Name>Dial</Name>");
		pbx_str_append(&buf, 0, "<Position>1</Position>");
		pbx_str_append(&buf, 0, "<URL>UserDataSoftKey:Select:%d:DIAL/%d</URL>", appID, transactionId);
		pbx_str_append(&buf, 0, "</SoftKeyItem>\n");
		pbx_str_append(&buf, 0, "<SoftKeyItem>");
		pbx_str_append(&buf, 0, "<Name>Exit</Name>");
		pbx_str_append(&buf, 0, "<Position>3</Position>");
		pbx_str_append(&buf, 0, "<URL>UserDataSoftKey:Select:%d:EXIT/%d</URL>", appID, transactionId);
		pbx_str_append(&buf, 0, "</SoftKeyItem>\n");

		pbx_str_append(&buf, 0, "</CiscoIPPhoneMenu>");
		*outbuf = pbx_strdup(pbx_str_buffer(buf));
		sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (getParkingLotCXML) with version:%d, result:\n[%s]\n", pl->context, protocolversion, *outbuf);
		sccp_free(buf);
	}
	return *outbuf;
}

/* --- notifications: prepared under pl->lock, executed with pl->lock dropped --- */
typedef enum {
	PL_WINDOW_FOLLOW = 0,											/* refresh an open window (show while there are slots, else hide) */
	PL_WINDOW_SHOW,												/* open (button press) */
	PL_WINDOW_HIDE,												/* close (exit) */
} plwindow_t;

typedef struct {
	sccp_device_t *device;											/* retained */
	uint8_t instance;
	boolean_t updateButton;											/* lamp, button state and feature event */
	uint32_t oldTransactionId;										/* window shown now, 0 = none */
	uint32_t newTransactionId;										/* window shown after this, 0 = none */
	boolean_t windowChanged;
	boolean_t cannotRetrieve;										/* asked to open the window, but there is nothing to show */
	char *xml;												/* window content to send */
} plwork_t;

/* called with pl locked */
static boolean_t prepareWorkLocked(sccp_parkinglot_t *pl, const plobserver_t *observer, boolean_t updateButton, plwindow_t window, plwork_t *work)
{
	memset(work, 0, sizeof(*work));
	if (!(work->device = sccp_device_retain(observer->device))) {
		return FALSE;
	}
	work->instance = observer->instance;
	work->updateButton = updateButton;
	work->oldTransactionId = observer->transactionId;
	work->newTransactionId = observer->transactionId;

	boolean_t show = FALSE;
	boolean_t hide = FALSE;
	switch (window) {
		case PL_WINDOW_FOLLOW:
			if (observer->transactionId) {
				if (SCCP_VECTOR_SIZE(&pl->slots) > 0 && !work->device->active_channel) {
					show = TRUE;
				} else {
					hide = TRUE;
				}
			}
			break;
		case PL_WINDOW_SHOW:
			show = TRUE;
			break;
		case PL_WINDOW_HIDE:
			hide = TRUE;
			break;
	}
	if (show) {
		uint32_t transactionId = sccp_random();
		if (getParkingLotCXML(pl, work->device->protocolversion, observer->instance, transactionId, &work->xml)) {
			work->newTransactionId = transactionId;
		} else {
			work->cannotRetrieve = (window == PL_WINDOW_SHOW);
			work->newTransactionId = 0;
		}
		work->windowChanged = TRUE;
	} else if (hide) {
		work->newTransactionId = 0;
		work->windowChanged = TRUE;
	}
	return TRUE;
}

/* called with pl locked, returns with pl locked; drops the lock while the devices are talked to */
static void executeWorkLocked(sccp_parkinglot_t *pl, plwork_t *work, size_t numwork)
{
	if (!numwork) {
		return;
	}
	size_t numslots = SCCP_VECTOR_SIZE(&pl->slots);
	pl->inflight++;
	sccp_parkinglot_unlock(pl);

	for (size_t w = 0; w < numwork; w++) {
		constDevicePtr device = work[w].device;
		if (work[w].updateButton) {
			uint32_t iconstate = 0;
			sccp_buttonconfig_t *config = NULL;
			if (device->protocolversion < 15) {
				sccp_device_setLamp(device, SKINNY_STIMULUS_PARKINGLOT, 0, numslots ? SKINNY_LAMP_ON : SKINNY_LAMP_OFF);
				iconstate = numslots ? ICONSTATE_OLD_ON : ICONSTATE_OLD_OFF;
			} else {
				iconstate = numslots ? ICONSTATE_NEW_ON : ICONSTATE_NEW_OFF;
			}
			SCCP_LIST_LOCK(&((devicePtr)device)->buttonconfig);
			SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
				if (config->type == FEATURE && config->instance == work[w].instance) {
					config->button.feature.status = iconstate;
				}
			}
			SCCP_LIST_UNLOCK(&((devicePtr)device)->buttonconfig);
		}
		if (work[w].xml) {
			sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (showVisualParkingLot) showing on device:%s, instance:%d\n", pl->context, device->id, work[w].instance);
			device->protocol->sendUserToDeviceDataVersionMessage(device, appID, 0, 0, work[w].newTransactionId, work[w].xml, 0);
		} else if (work[w].cannotRetrieve) {
			sccp_dev_displayprinotify(device, SKINNY_DISP_CANNOT_RETRIEVE_PARKED_CALL, SCCP_MESSAGE_PRIORITY_TIMEOUT, 5);
		} else if (work[w].windowChanged && work[w].oldTransactionId) {
			char xmlStr[DEFAULT_PBX_STR_BUFFERSIZE];
			sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (hideVisualParkingLot) device:%s, instance:%d\n", pl->context, device->id, work[w].instance);
			if (device->protocolversion < 15) {
				snprintf(xmlStr, DEFAULT_PBX_STR_BUFFERSIZE, "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"Init:Services\"/></CiscoIPPhoneExecute>");
			} else {
				snprintf(xmlStr, DEFAULT_PBX_STR_BUFFERSIZE, "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"App:Close:%d\"/></CiscoIPPhoneExecute>", appID);
			}
			device->protocol->sendUserToDeviceDataVersionMessage(device, appID, 0, 0, work[w].oldTransactionId, xmlStr, 0);
		}
		if (work[w].updateButton) {
			sccp_feat_changed(device, NULL, SCCP_FEATURE_PARKINGLOT);
		}
	}

	sccp_parkinglot_lock(pl);
	pl->inflight--;
	for (size_t w = 0; w < numwork; w++) {
		if (work[w].windowChanged) {
			plobserver_t *observer = findObserverLocked(pl, work[w].device, work[w].instance);	/* looked up again: the vector may have changed */
			if (observer) {
				observer->transactionId = work[w].newTransactionId;
			}
		}
		if (work[w].xml) {
			sccp_free(work[w].xml);
		}
		sccp_device_release(&work[w].device);								/* explicit release */
	}
}

/* called with pl locked, returns with pl locked: notify the observers (all, or those of one device / one instance) */
static void notifyObserversLocked(sccp_parkinglot_t *pl, constDevicePtr onlyDevice, int onlyInstance, boolean_t updateButton, plwindow_t window)
{
	size_t numobservers = SCCP_VECTOR_SIZE(&pl->observers);
	if (!numobservers) {
		return;
	}
	plwork_t *work = (plwork_t *)sccp_calloc(numobservers, sizeof(plwork_t));
	if (!work) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return;
	}
	size_t numwork = 0;
	for (size_t idx = 0; idx < numobservers; idx++) {
		plobserver_t *observer = SCCP_VECTOR_GET_ADDR(&pl->observers, idx);
		if (!observer || (onlyDevice && observer->device != onlyDevice) || (onlyInstance >= 0 && observer->instance != onlyInstance)) {
			continue;
		}
		if (prepareWorkLocked(pl, observer, updateButton, window, &work[numwork])) {
			numwork++;
		}
	}
	executeWorkLocked(pl, work, numwork);
	sccp_free(work);
}

static void hideVisualParkingLot(const char *parkinglot, constDevicePtr d, uint8_t instance)
{
	pbx_assert(parkinglot != NULL &&  d != NULL);

	RAII(sccp_parkinglot_t *, pl, findCreateParkinglot(parkinglot, TRUE), sccp_parkinglot_unlock);
	if (pl) {
		sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (hideVisualParkingLot) device:%s, instance:%d, size:%d\n", parkinglot, d->id, instance, (int)SCCP_VECTOR_SIZE(&pl->observers));
		notifyObserversLocked(pl, d, instance, FALSE, PL_WINDOW_HIDE);
	}
}

static void notifyDevice(constDevicePtr device, const sccp_buttonconfig_t * const buttonConfig)
{
	pbx_assert(device != NULL && buttonConfig != NULL);

	if(!sccp_strlen_zero(buttonConfig->button.feature.options)) {
		sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (notifyDevice) notifyDevice:%s\n", buttonConfig->button.feature.options, device->id);
		RAII(sccp_parkinglot_t *, pl, findCreateParkinglot(buttonConfig->button.feature.options, TRUE), sccp_parkinglot_unlock);
		if (pl) {
			notifyObserversLocked(pl, device, -1, TRUE, PL_WINDOW_FOLLOW);
		}
	}
}

static void notifyLocked(sccp_parkinglot_t *pl)
{
	pbx_assert(pl != NULL);

	sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (notify)\n", pl->context);
	notifyObserversLocked(pl, NULL, -1, TRUE, PL_WINDOW_FOLLOW);
}

// slot
static int addSlot(const char *parkinglot, int slot, struct message *m)
{
	pbx_assert(parkinglot != NULL && m != NULL);

	int res = FALSE;

	sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (addSlot) adding to slot:%d\n", parkinglot, slot);

	RAII(sccp_parkinglot_t *, pl, findCreateParkinglot(parkinglot, TRUE), sccp_parkinglot_unlock);
	if (pl) {
		if (SCCP_VECTOR_GET_CMP(&pl->slots, slot, SLOT_CB_CMP) == NULL) {
			plslot_t new_slot = {
				.slot = slot,
				.exten = pbx_strdup(astman_get_header(m, PARKING_SLOT)),
				.from = pbx_strdup(astman_get_header(m, PARKING_FROM)),
				.channel = pbx_strdup(astman_get_header(m, PARKING_PREFIX "Channel")),
				.callerid_num = pbx_strdup(astman_get_header(m, PARKING_PREFIX "CallerIDNum")),
				.callerid_name = pbx_strdup(astman_get_header(m, PARKING_PREFIX "CallerIDName")),
				.connectedline_num = pbx_strdup(astman_get_header(m, PARKING_PREFIX "ConnectedLineNum")),
				.connectedline_name = pbx_strdup(astman_get_header(m, PARKING_PREFIX "ConnectedLineName")),
			};
			if (SCCP_VECTOR_APPEND(&pl->slots, new_slot) == 0)  {
				notifyLocked(pl);
				res = TRUE;
			}
		} else {
			notifyLocked(pl);
		}
	} else {
		sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "SCCP: (addSlot) ParkingLot:%s is not being observed\n", parkinglot);
	}
	return res;
}

static int removeSlot(const char *parkinglot, int slot)
{
	pbx_assert(parkinglot != NULL);

	sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (removeSlot) removing slot:%d\n", parkinglot, slot);
	int res = FALSE;

	RAII(sccp_parkinglot_t *, pl, findCreateParkinglot(parkinglot, TRUE), sccp_parkinglot_unlock);
	if (pl) {
		if (SCCP_VECTOR_REMOVE_CMP_UNORDERED(&pl->slots, slot, SLOT_CB_CMP, SLOT_CLEANUP) == 0) {
			notifyLocked(pl);
			res = TRUE;
		}
	} else {
		sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "SCCP: (removeSlot) ParkingLot:%s is not being observed\n", parkinglot);
	}
	return !res;
}

/*
 * Handle Park Feature Button Press
 * -If we have an active call -> pressing the park feature key, will park that call
 * -If we are not on an active call:
 * 	- If there is 0 parked calls: Display Status Message, "No parked calls'
 * 	- If there is 1 parked call: Unpark that call immediatly
 *	- If there is more than 1 parked call: display the visual parking lot representation.
 */
static void handleButtonPress(constDevicePtr d, const sccp_buttonconfig_t * const buttonConfig)
{
	pbx_assert(d != NULL && buttonConfig != NULL);
	sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (handleButtonPress) options:%s, instance:%d\n", d->id, buttonConfig->button.feature.options, buttonConfig->instance);

	AUTO_RELEASE(sccp_channel_t, channel , sccp_device_getActiveChannel(d));
	if (channel && channel->state != SCCP_CHANNELSTATE_OFFHOOK && channel->state != SCCP_CHANNELSTATE_HOLD) {
		sccp_channel_park(channel);
	} else if(!sccp_strlen_zero(buttonConfig->button.feature.options)) {
		char exten[SCCP_MAX_EXTENSION] = "";
		boolean_t unparkSingle = FALSE;
		boolean_t cannotRetrieve = FALSE;
		{
			RAII(sccp_parkinglot_t *, pl, findCreateParkinglot(buttonConfig->button.feature.options, TRUE), sccp_parkinglot_unlock);
			if (!pl) {
				return;
			}
			if (SCCP_VECTOR_SIZE(&pl->slots) == 0) {
				sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (handleButtonPress) 0 slot occupied. Show statusBar message\n", buttonConfig->button.feature.options);
				cannotRetrieve = TRUE;									/* shown after unlocking: no device calls under pl->lock */
			} else if(sccp_strcaseequals(buttonConfig->button.feature.args, "RetrieveSingle") && SCCP_VECTOR_SIZE(&pl->slots) == 1) {
				sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (handleButtonPress) 1 slot occupied -> Unpark Call Immediately\n", buttonConfig->button.feature.options);
				plslot_t *slot = SCCP_VECTOR_GET_ADDR(&pl->slots, 0);
				if (slot && slot->exten) {
					sccp_copy_string(exten, slot->exten, sizeof(exten));				/* copied: the call is set up after unlocking */
					unparkSingle = TRUE;
				}
			} else {
				sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (handleButtonPress) multiple slots occupied -> Show Visual ParkingLot\n", buttonConfig->button.feature.options);
				notifyObserversLocked(pl, d, buttonConfig->instance, FALSE, PL_WINDOW_SHOW);
			}
		}
		if (cannotRetrieve) {
			sccp_dev_displayprinotify(d, SKINNY_DISP_CANNOT_RETRIEVE_PARKED_CALL, SCCP_MESSAGE_PRIORITY_TIMEOUT, 5);
		}
		if (unparkSingle) {
			AUTO_RELEASE(sccp_line_t, line , channel ? sccp_line_retain(channel->line) : d->currentLine ? sccp_dev_getActiveLine(d) : sccp_line_find_byid(d, d->defaultLineInstance));
			AUTO_RELEASE(sccp_channel_t, new_channel, sccp_channel_newcall(line, d, exten, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL));                                        // implicit release
		}
	}
}

static void handleDevice2User(const char *parkinglot, constDevicePtr d, const char *slot_exten, uint8_t instance, uint32_t transactionId)
{
	pbx_assert(d != NULL);
	sccp_log(DEBUGCAT_PARKINGLOT)(VERBOSE_PREFIX_1 "%s: (handleDevice2Usewr) instance:%d, transactionId:%d\n", d->id, instance, transactionId);

	if (d->dtu_softkey.action && d->dtu_softkey.transactionID == transactionId) {
		if (sccp_strequals(d->dtu_softkey.action, "DIAL")) {
			AUTO_RELEASE(sccp_line_t, line , d->currentLine ? sccp_dev_getActiveLine(d) : sccp_line_find_byid(d, d->defaultLineInstance));
			AUTO_RELEASE(sccp_channel_t, new_channel, sccp_channel_newcall(line, d, slot_exten, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL));                                        // implicit release
		} else if (sccp_strequals(d->dtu_softkey.action, "EXIT")) {
			hideVisualParkingLot(parkinglot, d, instance);
		}
	}
}
/* Assign to interface */
const ParkingLotInterface iParkingLot = {
	.attachObserver = attachObserver,
	.detachObserver = detachObserver,
	.addSlot = addSlot,
	.removeSlot = removeSlot,
	.handleButtonPress = handleButtonPress,
	.handleDevice2User = handleDevice2User,
	.notifyDevice = notifyDevice,
};
#else
const ParkingLotInterface iParkingLot = { 0 };
#endif
