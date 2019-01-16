/*!
 * \file	sccp_featureParkingLot.c
 * \brief	SCCP ParkingLot Class
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2015-Sept-16
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $date$
 * $revision$
 */
#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "");

#include "sccp_mwi.h"
#include "sccp_vector.h"
#include "sccp_line.h"		// only used in NotifyLine at the moment. remove when moved to sccp_line.c. A forward declaration would do
#include "sccp_device.h"	// only used in NotifyLine at the moment. remove when moved to sccp_line.c
#include "sccp_utils.h"

/* Temporariry until moved to astxxx.c */
#if CS_HAS_EVENT_H
#include <asterisk/event.h>
#elif CS_HAS_STASIS_H
#include <asterisk/stasis.h>
#endif
#ifdef HAVE_PBX_APP_H				// ast_mwi_state_type
#  include <asterisk/app.h>
#endif

pbx_mutex_t subscriptions_lock;
#define subscription_lock()		({pbx_mutex_lock(&subscriptions_lock);})		// discard const
#define subscription_unlock()		({pbx_mutex_unlock(&subscriptions_lock);})		// discard const

typedef struct pbx_event_sub pbx_event_subscription_t;
typedef struct subscription {
	sccp_mailbox_t *mailbox;
	sccp_line_t *line;
#if MWI_USE_EVENT
	pbx_event_subscription_t *pbx_subscription;
#else
	int sched;
#endif
} mwi_subscription_t;
SCCP_VECTOR(, mwi_subscription_t *) subscriptions;

/* Forward Declarations */
void NotifyLine(sccp_mailbox_t *mailbox, sccp_line_t *line, int newmsgs, int oldmsgs);

/* =======================
 * Event/Polling CallBacks
 * ======================= */
#if defined(CS_AST_HAS_EVENT)
static void pbxMailboxGetCached(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) mailbox:%p, uniqueid:%s\n", 
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox), (subscription->mailbox)->uniqueid);
	// call split uniqueid function
	RAII(struct ast_event *, event, NULL, ast_event_destroy(event));
	event = ast_event_get_cached(	AST_EVENT_MWI,
					AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, (subscription->mailbox)->mailbox,
					AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, (subscription->mailbox)->context,
					AST_EVENT_IE_END);

	if (event) {
		int newmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_NEWMSGS);
		int oldmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_OLDMSGS);
		NotifyLine(subscription->mailbox, subscription->line, newmsgs, oldmsgs);
	}
}
static void pbx_mwi_event(const struct ast_event *event, void *data)
{
	sccp_mwi_subscription_t *subscription = data;
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) mailbox:%p, uniqueid:%s\n", 
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox), (subscription->mailbox)->uniqueid);
	if (!subscription || !event || !GLOB(module_running)) {
		pbx_log(LOG_ERROR, "SCCP: MWI Event received but not all requirements are fullfilled (%p, %p, %d)\n", subscription, event, GLOB(module_running));
		return;
	}
	int newmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_NEWMSGS);
	int oldmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_OLDMSGS);
	NotifyLine(subscription->mailbox, subscription->line, newmsgs, oldmsgs);
}
static pbx_event_subscription_t *pbxMailboxSubscribe(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) mailbox:%p, uniqueid:%s\n", 
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox), (subscription->mailbox)->uniqueid);

	pbx_event_subscription_t *pbx_subscription = NULL;
	// call split uniqueid function

#if ASTERISK_VERSION_NUMBER >= 10800
	pbx_subscription = pbx_event_subscribe(AST_EVENT_MWI, pbx_mwi_event, "mailbox subscription", subscription, AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, subscription->mailbox, AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, subscription->context, AST_EVENT_IE_NEWMSGS, AST_EVENT_IE_PLTYPE_EXISTS, AST_EVENT_IE_END);
#else
	pbx_subscription = pbx_event_subscribe(AST_EVENT_MWI, pbx_mwi_event, subscription, AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, subscription->mailbox, AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, subscription->context, AST_EVENT_IE_END);
#endif
	if (!pbx_subscription) {
		pbx_log(LOG_ERROR, "SCCP: PBX MWI event could not be subscribed to for mailbox %s@%s\n", subscription->mailbox, subscription->context);
	}
	pbxMailboxGetCached(subscription);
	return pbx_subscription;
}
static void pbxMailboxUnsubscribe(mwi_subscription_t *subscription)
{
	pbx_event_unsubscribe(subscription->pbx_subscription);
}
#elif defined(CS_AST_HAS_STASIS)
static void pbxMailboxGetCached(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) mailbox:%p, uniqueid:%s\n", 
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox), (subscription->mailbox)->uniqueid);
	RAII(struct stasis_message *, mwi_message, NULL, ao2_cleanup);
	mwi_message = stasis_cache_get(ast_mwi_state_cache(), ast_mwi_state_type(), (subscription->mailbox)->uniqueid);
	if (mwi_message) {
		struct ast_mwi_state *mwi_state = stasis_message_data(mwi_message);
		int newmsgs = mwi_state->new_msgs;
		int oldmsgs = mwi_state->old_msgs;
		NotifyLine(subscription->mailbox, subscription->line, newmsgs, oldmsgs);
	}
}
static void pbx_mwi_event(void *data, struct stasis_subscription *sub, struct stasis_message *msg)
{
	mwi_subscription_t *subscription = data;
	if (!subscription || !GLOB(module_running) || stasis_subscription_final_message(sub, msg)) {
		pbx_log(LOG_ERROR, "SCCP: MWI Event received but not all requirements are fullfilled (%p, %d)\n", subscription, GLOB(module_running));
		return;
	}
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) mailbox:%p, uniqueid:%s\n", 
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox), (subscription->mailbox)->uniqueid);
	if (ast_mwi_state_type() == stasis_message_type(msg)) {
		struct ast_mwi_state *mwi_state = stasis_message_data(msg);
		if (mwi_state) {
			int newmsgs = mwi_state->new_msgs;
			int oldmsgs = mwi_state->old_msgs;
			NotifyLine(subscription->mailbox, subscription->line, newmsgs, oldmsgs);
		}
	}
}
static pbx_event_subscription_t * pbxMailboxSubscribe(mwi_subscription_t *subscription)
{
	pbx_event_subscription_t *pbx_subscription = NULL;
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) mailbox:%p, uniqueid:%s\n", 
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox), (subscription->mailbox)->uniqueid);

	struct stasis_topic *mailbox_specific_topic = ast_mwi_topic((subscription->mailbox)->uniqueid);
	if (mailbox_specific_topic) {
		pbx_subscription = stasis_subscribe_pool(mailbox_specific_topic, pbx_mwi_event, subscription);
#  if CS_AST_HAS_STASIS_SUBSCRIPTION_SET_FILTER
		stasis_subscription_accept_message_type(pbx_subscription, ast_mwi_state_type());
		//stasis_subscription_accept_message_type(subscription->pbx_subscription, stasis_subscription_change_type());
		stasis_subscription_set_filter(pbx_subscription, STASIS_SUBSCRIPTION_FILTER_SELECTIVE);
#  endif
	}
	pbxMailboxGetCached(subscription);
	return pbx_subscription;
}
static void pbxMailboxUnsubscribe(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) mailbox:%p, uniqueid:%s\n", 
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox), (subscription->mailbox)->uniqueid);
	stasis_unsubscribe(subscription->pbx_subscription);
}
#else
// scheduled polling (asterisk-1.6)
static void pbxMailboxGetCached(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) mailbox:%p, uniqueid:%s\n", 
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox), (subscription->mailbox)->uniqueid);
	if (pbx_app_inboxcount(subscription->uniqueid, &(mailbox->newmsgs), &(mailbox->oldmsgs))) {
		pbx_log(LOG_ERROR, "Failed to retrieve messages from mailbox:%s\n", mailbox->uniqueid);
	}
	NotifyLine(mailbox, line);
}
static void pbxMailboxReschedule(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) mailbox:%p, uniqueid:%s\n", 
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox), (subscription->mailbox)->uniqueid);
	if ((subscription->schedUpdate = iPbx.sched_add(interval * 1000, pbxMailboxGetCached, subscription)) < 0) {
		pbx_log(LOG_ERROR, "Error creating mailbox subscription.\n");
	}
}
static int pbx_mwi_event(const void *data)
{
	mwi_subscription_t *subscription = data;
	if (!subscription) {
		// error
	}
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) mailbox:%p, uniqueid:%s\n", 
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox), (subscription->mailbox)->uniqueid);
	pbxMailboxGetCached(subscription);
	pbxMailboxReschedule(subscription);
}

static void pbxMailboxSubscribe(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) mailbox:%p, uniqueid:%s\n", 
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox), (subscription->mailbox)->uniqueid);
	pbx_mwi_event(subscription);
}
static void pbxMailboxUnsubscribe(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) mailbox:%p, uniqueid:%s\n", 
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox), (subscription->mailbox)->uniqueid);
	subscription->sched = SCCP_SCHED_DEL(subscription->schedUpdate);
}
#endif

/* ===========================
 * Create/Destroy Subscription
 * =========================== */
static void createSubscription(sccp_mailbox_t *mailbox, sccp_line_t *line)
{
	// \todo remove next line, when converted to using uniqueid
	snprintf(mailbox->uniqueid, sizeof(mailbox->uniqueid), "%s@%s", mailbox->mailbox, mailbox->context);

	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) mailbox:%p, uniqueid:%s\n", 
		line->name, __PRETTY_FUNCTION__, mailbox, mailbox->uniqueid);

	mwi_subscription_t *subscription = sccp_calloc(1, sizeof(mwi_subscription_t));
	if (!subscription) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return;
	}
	subscription->mailbox = mailbox;
	subscription->line = sccp_line_retain(line);

	subscription_lock();
	SCCP_VECTOR_APPEND(&subscriptions, subscription);
	subscription_unlock();

	subscription->pbx_subscription = pbxMailboxSubscribe(subscription);
}

static void removeSubscription(sccp_mailbox_t *mailbox, sccp_line_t *line)
{
	mwi_subscription_t *removed = NULL;
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) mailbox:%p, uniqueid:%s\n", 
		line->name, __PRETTY_FUNCTION__, mailbox, mailbox->uniqueid);
	subscription_lock();
	for (uint32_t idx = 0; idx < SCCP_VECTOR_SIZE(&subscriptions); idx++) {
		mwi_subscription_t *subscription = SCCP_VECTOR_GET(&subscriptions, idx);
		if (subscription->mailbox == mailbox && subscription->line == line) {
			SCCP_VECTOR_REMOVE_UNORDERED(&subscriptions, idx);
			removed = subscription;
			break;
		}
	}
	subscription_unlock();
	if (removed) {
		pbxMailboxUnsubscribe(removed);
		sccp_line_release(&removed->line);
		sccp_free(removed);
	}
}

/*!
 * \note this does not lock the subscriptions_lock, to prevent potential (future) deadlock in pbxMailboxUnsubscribe
 * Only call this function, when made sure there are no further incoming event possible
 */
static void removeAllSubscriptions(void)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "SCCP: (mwi::removeAllSubscriptions)\n");
	for (uint32_t idx = 0; idx < SCCP_VECTOR_SIZE(&subscriptions); idx++) {
		mwi_subscription_t *subscription = SCCP_VECTOR_GET(&subscriptions, idx);
		if (subscription) {
			SCCP_VECTOR_REMOVE_UNORDERED(&subscriptions, idx);
			pbxMailboxUnsubscribe(subscription);
			sccp_line_release(&subscription->line);
			sccp_free(subscription);
		}
	}
	SCCP_VECTOR_RESET(&subscriptions, SCCP_VECTOR_ELEM_CLEANUP_NOOP);
}

/* ===========================
 * Handle SCCP Events
 * =========================== */
static void handleLineCreationEvent(const sccp_event_t * event)
{
	if (!event || !event->lineInstance.line) {
		pbx_log(LOG_ERROR, "Event or line not provided\n");
		return;
	}
	sccp_mailbox_t *mailbox = NULL;
	sccp_line_t *line = event->lineInstance.line;

	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::handleLineCreationEvent)\n", line->name);
	if (line && (&line->mailboxes) != NULL) {
		SCCP_LIST_TRAVERSE(&line->mailboxes, mailbox, list) {
			createSubscription(mailbox, line);
		}
	}
}

static void handleLineDestructionEvent(const sccp_event_t * event)
{
	if (!event || !event->lineInstance.line) {
		pbx_log(LOG_ERROR, "Eevent or line not provided\n");
		return;
	}
	sccp_line_t *line = event->lineInstance.line;

	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::handleLineDestructionEvent)\n", line->name);

	sccp_mailbox_t *mailbox = NULL;
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&line->mailboxes, mailbox, list) {
		removeSubscription(mailbox, line);
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
}

/* ==================================
 * Inform the line of any MWI changes
 * ================================== */
#include "sccp_labels.h"
void NotifyLine(sccp_mailbox_t *mailbox, sccp_line_t *l, int newmsgs, int oldmsgs)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::NotifyLine) Notify newmsgs:%d oldmsgs:%d\n", l->name, newmsgs, oldmsgs);

	int prevLineNewMsgs = l->voicemailStatistic.newmsgs;
	int prevLineOldMsgs = l->voicemailStatistic.oldmsgs;
	l->voicemailStatistic.newmsgs = newmsgs;
	l->voicemailStatistic.oldmsgs = oldmsgs;

	sccp_linedevices_t *linedevice = NULL;
	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
		AUTO_RELEASE(sccp_device_t, d, sccp_device_retain(linedevice->device));
		if (d && sccp_device_getRegistrationState(d) == SKINNY_DEVICE_RS_OK) {
			// toggle line iconStatus
			// toggle line messageStatus
			// --> sccp_line_update_mwistate(l);
			if (prevLineNewMsgs != l->voicemailStatistic.newmsgs || prevLineOldMsgs != l->voicemailStatistic.oldmsgs) {
				sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_check) Set voicemail icon:%s for Line%s on Button:%d\n", d->id, 
					l->voicemailStatistic.newmsgs ? "on" : "off", 
					l->name,
					linedevice->lineInstance);
				sccp_msg_t *msg;
				REQ(msg, SetLampMessage);
				msg->data.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
				msg->data.SetLampMessage.lel_stimulusInstance = htolel(linedevice->lineInstance);
				msg->data.SetLampMessage.lel_lampMode = l->voicemailStatistic.newmsgs ? htolel(SKINNY_LAMP_ON) : htolel(SKINNY_LAMP_OFF);
				sccp_dev_send(d, msg);
			}

			// --> sccp_device_update_mwistate(d);
				//boolean_t lightStatus = d->voicemailStatistic.newmsgs;
				//boolean_t messageStatus = (d->voicemailStatistic.newmsgs || d->voicemailStatistic.oldmsgs)
				// toggle device lightStatus;
				// except if !mwioncall and deviceStatus is !onhook
				// toggle device messageStatus;
			int prevDeviceNewMsgs = d->voicemailStatistic.newmsgs;
			int prevDeviceOldMsgs = d->voicemailStatistic.oldmsgs;
			d->voicemailStatistic.newmsgs = l->voicemailStatistic.newmsgs - prevLineNewMsgs;
			d->voicemailStatistic.oldmsgs = l->voicemailStatistic.oldmsgs - prevLineOldMsgs;
			
			if (prevDeviceNewMsgs != d->voicemailStatistic.newmsgs || prevDeviceOldMsgs != d->voicemailStatistic.oldmsgs) {
				sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_check) Set voicemail lamp:%s\n", d->id, 
					d->voicemailStatistic.newmsgs ? "on" : "off");
				sccp_msg_t *msg;
				REQ(msg, SetLampMessage);
				msg->data.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
				msg->data.SetLampMessage.lel_stimulusInstance = 0;
				msg->data.SetLampMessage.lel_lampMode = (d->voicemailStatistic.newmsgs) ? htolel(d->mwilamp) : htolel(SKINNY_LAMP_OFF);
				sccp_dev_send(d, msg);
			
			}
			if (d->voicemailStatistic.newmsgs || d->voicemailStatistic.oldmsgs) {
				sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_check) Set Have Voicemail on Display\n", d->id);
				char buffer[StationMaxDisplayTextSize];
				snprintf(buffer, StationMaxDisplayTextSize, "%s: (%u/%u)", SKINNY_DISP_YOU_HAVE_VOICEMAIL, d->voicemailStatistic.newmsgs, d->voicemailStatistic.oldmsgs);
				sccp_device_addMessageToStack(d, SCCP_MESSAGE_PRIORITY_VOICEMAIL, buffer);
			} else {
				sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_check) Remove Have Voicemail from Display\n", d->id);
				sccp_device_clearMessageFromStack(d, SCCP_MESSAGE_PRIORITY_VOICEMAIL);
			}
		}
	}
	SCCP_LIST_UNLOCK(&l->devices);
}

/*!
 * \brief Show MWI Subscriptions
 * \param fd Fd as int
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 *
 * \called_from_asterisk
 */
#include <asterisk/cli.h>
/* ==================================
 * CLI / MIW Output
 * ================================== */
static int showSubscriptions(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_line_t *line = NULL;
	//sccp_mailboxLine_t *mailboxLine = NULL;
	//char linebuf[31] = "";
	int local_line_total = 0;

	subscription_lock();
#define CLI_AMI_TABLE_NAME MWISubscriptions
#define CLI_AMI_TABLE_PER_ENTRY_NAME MailboxSubscriber
#define CLI_AMI_TABLE_ITERATOR for (uint32_t idx = 0; idx < SCCP_VECTOR_SIZE(&subscriptions); idx++)
#define CLI_AMI_TABLE_BEFORE_ITERATION 															\
	mwi_subscription_t *subscription = SCCP_VECTOR_GET(&subscriptions, idx);									\
	line = subscription->line;

#if defined (CS_AST_HAS_EVENT)
#define CLI_AMI_TABLE_FIELDS 																\
 		CLI_AMI_TABLE_FIELD(Mailbox,		"-10.10",	s,	10,	(subscription->mailbox)->mailbox)				\
 		CLI_AMI_TABLE_FIELD(LineName,		"-30.30",	s,	30,	line->name)							\
 		CLI_AMI_TABLE_FIELD(Context,		"-15.15",	s,	15,	(subscription->mailbox)->context)				\
 		CLI_AMI_TABLE_FIELD(New,		"3.3",		d,	3,	line->voicemailStatistic.newmsgs)				\
 		CLI_AMI_TABLE_FIELD(Old,		"3.3",		d,	3,	line->voicemailStatistic.oldmsgs)				\
 		CLI_AMI_TABLE_FIELD(Sub,		"-3.3",		s,	3,	subscription->pbx_subscription ? "YES" : "NO")

#elif defined(CS_AST_HAS_STASIS)
#define CLI_AMI_TABLE_FIELDS 																\
 		CLI_AMI_TABLE_FIELD(Mailbox,		"-10.10",	s,	10,	(subscription->mailbox)->mailbox)				\
 		CLI_AMI_TABLE_FIELD(LineName,		"-30.30",	s,	30,	line->name)							\
 		CLI_AMI_TABLE_FIELD(Context,		"-15.15",	s,	15,	(subscription->mailbox)->context)				\
 		CLI_AMI_TABLE_FIELD(New,		"3.3",		d,	3,	line->voicemailStatistic.newmsgs)				\
 		CLI_AMI_TABLE_FIELD(Old,		"3.3",		d,	3,	line->voicemailStatistic.oldmsgs)				\
 		CLI_AMI_TABLE_FIELD(Sub,		"-3.3",		s,	3,	subscription->pbx_subscription ? "YES" : "NO")			\
		CLI_AMI_TABLE_FIELD(AstUniq,		"36.36",	s,	36,	subscription->pbx_subscription ? 				\
											stasis_subscription_uniqueid(subscription->pbx_subscription) : "")
#else
#define CLI_AMI_TABLE_FIELDS 																\
 		CLI_AMI_TABLE_FIELD(Mailbox,		"-10.10",	s,	10,	(subscription->mailbox)->mailbox)				\
 		CLI_AMI_TABLE_FIELD(LineName,		"-30.30",	s,	30,	line->name)							\
 		CLI_AMI_TABLE_FIELD(Context,		"-15.15",	s,	15,	(subscription->mailbox)->context)				\
 		CLI_AMI_TABLE_FIELD(New,		"3.3",		d,	3,	line->voicemailStatistic.newmsgs)				\
 		CLI_AMI_TABLE_FIELD(Old,		"3.3",		d,	3,	line->voicemailStatistic.oldmsgs)
#endif
#include "sccp_cli_table.h"
		local_line_total++;
	subscription_unlock();
	if (s) {
		totals->lines = local_line_total;
		totals->tables = 1;
	}
	return RESULT_SUCCESS;
}

/* ==================================
 * Module Init / Register SCCP Events
 * ================================== */
static void module_start(void)
{
	pbx_log(LOG_NOTICE, "SCCP: (mwi::module_start)\n");
	SCCP_VECTOR_INIT(&subscriptions,1);
	pbx_mutex_init(&subscriptions_lock);

	//sccp_event_subscribe(SCCP_EVENT_LINE_CREATED, handleLineCreationEvent, TRUE);
	//sccp_event_subscribe(SCCP_EVENT_LINE_DESTROYED, handleLineDestructionEvent, FALSE);

#if MWI_USE_POLLING
	// start polling for all subscriptions
#endif
}

static void module_stop(void)
{
	pbx_log(LOG_NOTICE, "SCCP: (mwi::module_stop)\n");
	//sccp_event_unsubscribe(SCCP_EVENT_LINE_DESTROYED, handleLineDestructionEvent);
	//sccp_event_unsubscribe(SCCP_EVENT_LINE_CREATED, handleLineCreationEvent);

	removeAllSubscriptions();
	SCCP_VECTOR_FREE(&subscriptions);
	pbx_mutex_destroy(&subscriptions_lock);
}

/* =====================================
 * Assign external function to interface
 * ===================================== */
const VoicemailInterface iVoicemail = {
	.startModule = module_start,
	.stopModule = module_stop,
	.showSubscriptions = showSubscriptions,
};
