/*!
 * \file 	sccp_channel.c
 * \brief 	SCCP Channel Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \date
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * \date        $Date$
 * \version     $Revision$
 */
#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#include "asterisk/abstract_jb.h"
#endif

#include "chan_sccp.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#include "sccp_lock.h"
#include "sccp_dllists.h"
#include "sccp_actions.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include "sccp_pbx.h"
#include "sccp_line.h"
#include "sccp_channel.h"
#include "sccp_indicate.h"

#include <asterisk/pbx.h>
#include <asterisk/utils.h>
#include <asterisk/causes.h>
#include <asterisk/callerid.h>
#include <asterisk/musiconhold.h>
#include <asterisk/rtp.h>

#ifndef CS_AST_HAS_TECH_PVT
#include <asterisk/channel_pvt.h>
#endif

#ifdef CS_SCCP_PARK
#include <asterisk/features.h>
#endif

#ifdef CS_MANAGER_EVENTS
#include <asterisk/manager.h>
#endif


#ifndef ast_free
#define ast_free free
#endif


static uint32_t callCount = 1;
AST_MUTEX_DEFINE_STATIC(callCountLock);

/*!
 * \brief Allocate SCCP Channel on Device
 * \param l SCCP Line
 * \param device SCCP Device
 * \return SCCP Channel
 */
sccp_channel_t * sccp_channel_allocate(sccp_line_t * l, sccp_device_t *device)
{
	/* this just allocate a sccp channel (not the asterisk channel, for that look at sccp_pbx_channel_allocate) */
	sccp_channel_t * c;

	/* If there is no current line, then we can't make a call in, or out. */
	if (!l) {
		ast_log(LOG_ERROR, "SCCP: Tried to open channel on a device with no lines\n");
		return NULL;
	}

	if (device && !device->session) {
		ast_log(LOG_ERROR, "SCCP: Tried to open channel on device %s without a session\n", device->id);
		return NULL;
	}

	c = ast_malloc(sizeof(sccp_channel_t));
	if (!c) {
		/* error allocating memory */
		ast_log(LOG_ERROR, "%s: No memory to allocate channel on line %s\n",l->id, l->name);
		return NULL;
	}
	memset(c, 0, sizeof(sccp_channel_t));

	sccp_mutex_init(&c->lock);

	/* this is for dialing scheduler */
	c->digittimeout = -1;
	/* maybe usefull -FS*/
	c->owner = NULL;
	/* default ringermode SKINNY_STATION_OUTSIDERING. Change it with SCCPRingerMode app */
	c->ringermode = SKINNY_STATION_OUTSIDERING;
	/* inbound for now. It will be changed later on outgoing calls */
	c->calltype = SKINNY_CALLTYPE_INBOUND;
	c->answered_elsewhere = FALSE;

	/* callcount limit should be reset at his upper limit :) */
	if(callCount == 0xFFFFFFFF)
		callCount = 1;

	sccp_mutex_lock(&callCountLock);
	c->callid = callCount++;
	c->passthrupartyid = c->callid ^ 0xFFFFFFFF;
	sccp_mutex_unlock(&callCountLock);

	c->line = l;
	c->isCodecFix = FALSE;
	c->device = device;
	sccp_channel_updateChannelCapability(c);

	sccp_line_addChannel(l, c);

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: New channel number: %d on line %s\n", l->id, c->callid, l->name);

	return c;
}

/*!
 * \brief Update Channel Capability
 * \param channel SCCP Channel
 */
void sccp_channel_updateChannelCapability(sccp_channel_t *channel){
	if(!channel)
		return;

	
	char s1[512], s2[512];
	if(!channel->device){
		channel->capability = AST_FORMAT_ALAW|AST_FORMAT_ULAW|AST_FORMAT_G729A;
		memcpy(&channel->codecs, &GLOB(global_codecs), sizeof(channel->codecs));
	}else{
		channel->capability = channel->device->capability;
		memcpy(&channel->codecs, &channel->device->codecs, sizeof(channel->codecs));
	}

	if(channel->isCodecFix == FALSE){
		/* we does not have set a preferred format before */
		channel->format = ast_codec_choose(&channel->codecs, channel->capability, 1);
	}

	if(channel->owner){
		ast_channel_lock(channel->owner);
		channel->owner->nativeformats = channel->format; /* if we set nativeformats to a single format, we force asterisk to translate stream */
		channel->owner->rawreadformat = channel->format;
		channel->owner->rawwriteformat = channel->format;



		channel->owner->writeformat	= channel->format; /*|AST_FORMAT_H263|AST_FORMAT_H264|AST_FORMAT_H263_PLUS;*/
		channel->owner->readformat 	= channel->format; /*|AST_FORMAT_H263|AST_FORMAT_H264|AST_FORMAT_H263_PLUS;*/


		ast_set_read_format(channel->owner, channel->format);
		ast_set_write_format(channel->owner, channel->format);
		ast_channel_unlock(channel->owner);
	}




	sccp_log(2)(VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, capabilities: %s(%d) USED: %s(%d) \n",
	channel->line->name,
	channel->callid,
#ifndef ASTERISK_CONF_1_2
	ast_getformatname_multiple(s1, sizeof(s1) -1, channel->capability & AST_FORMAT_AUDIO_MASK),
#else
	ast_getformatname_multiple(s1, sizeof(s1) -1, channel->capability),
#endif
	channel->capability,
#ifndef ASTERISK_CONF_1_2
	ast_getformatname_multiple(s2, sizeof(s2) -1, channel->format & AST_FORMAT_AUDIO_MASK),
#else
	ast_getformatname_multiple(s2, sizeof(s2) -1, channel->format),
#endif
	channel->format);
	
}


/*!
 * \brief Get Active Channel
 * \param d SCCP Device
 * \return SCCP Channel
 */
sccp_channel_t * sccp_channel_get_active(sccp_device_t * d) {
	sccp_channel_t * c;

	if (!d)
		return NULL;

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Getting the active channel on device.\n",d->id);

	sccp_device_lock(d);
	c = d->active_channel;
	sccp_device_unlock(d);

	if(c && c->state == SCCP_CHANNELSTATE_DOWN)
		return NULL;

	return c;
}

/*!
 * \brief Set SCCP Channel to Active
 * \param d SCCP Device
 * \param c SCCP Channel
 */
void sccp_channel_set_active(sccp_device_t * d, sccp_channel_t * c)
{
	if(!d)
		return;

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Set the active channel %d on device\n", DEV_ID_LOG(d), (c) ? c->callid : 0);
	sccp_device_lock(d);
	d->active_channel = c;
	d->currentLine = c->line;
	sccp_device_unlock(d);
}

/*!
 * \brief Send Call Information to Device/Channel
 * \param device SCCP Device
 * \param c SCCP Channel
 */
void sccp_channel_send_callinfo(sccp_device_t *device, sccp_channel_t * c)
{
	sccp_moo_t * r;
	uint8_t			instance =0;

	if(!device || !c)
		return;

	sccp_device_lock(device);
	instance = sccp_device_find_index_for_line(device, c->line->name);
	sccp_device_unlock(device);


	REQ(r, CallInfoMessage);

	if(c->device == device){
		if (c->callingPartyName)
			sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, c->callingPartyName, sizeof(r->msg.CallInfoMessage.callingPartyName));
		if (c->callingPartyNumber)
			sccp_copy_string(r->msg.CallInfoMessage.callingParty, c->callingPartyNumber, sizeof(r->msg.CallInfoMessage.callingParty));

		if (c->calledPartyName)
			sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, c->calledPartyName, sizeof(r->msg.CallInfoMessage.calledPartyName));
		if (c->calledPartyNumber)
			sccp_copy_string(r->msg.CallInfoMessage.calledParty, c->calledPartyNumber, sizeof(r->msg.CallInfoMessage.calledParty));

		r->msg.CallInfoMessage.lel_lineId   = htolel(instance);
		r->msg.CallInfoMessage.lel_callRef  = htolel(c->callid);
		r->msg.CallInfoMessage.lel_callType = htolel(c->calltype);
		r->msg.CallInfoMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_UNKNOWN);
	}else{
		/* remote device notification */
		if (c->callingPartyName)
			sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, c->callingPartyName, sizeof(r->msg.CallInfoMessage.callingPartyName));
		if (c->callingPartyNumber)
			sccp_copy_string(r->msg.CallInfoMessage.callingParty, c->callingPartyNumber, sizeof(r->msg.CallInfoMessage.callingParty));

		if (c->calledPartyName)
			sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, c->calledPartyName, sizeof(r->msg.CallInfoMessage.calledPartyName));
		if (c->calledPartyNumber)
			sccp_copy_string(r->msg.CallInfoMessage.calledParty, c->calledPartyNumber, sizeof(r->msg.CallInfoMessage.calledParty));

		r->msg.CallInfoMessage.lel_lineId   = htolel(instance);
		r->msg.CallInfoMessage.lel_callRef  = htolel(c->callid);
		r->msg.CallInfoMessage.lel_callType = htolel(c->calltype);
		r->msg.CallInfoMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_UNKNOWN);
	}

	sccp_dev_send(device, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send callinfo for %s channel %d on line instance %d"
			"\n\tcallerid: %s"
			"\n\tcallerName: %s\n", (device)?device->id:"(null)", calltype2str(c->calltype), c->callid, instance, c->callingPartyNumber, c->callingPartyName);

}

/*!
 * \brief Send Dialed Number to SCCP Channel device
 * \param c SCCP Channel
 */
void sccp_channel_send_dialednumber(sccp_channel_t * c)
{
	sccp_moo_t 		*r;
	sccp_device_t	*device;
	uint8_t instance;

	if (ast_strlen_zero(c->calledPartyNumber))
		return;

	if(!c->device)
		return;
	device = c->device;

	REQ(r, DialedNumberMessage);

	instance = sccp_device_find_index_for_line(device, c->line->name);
	sccp_copy_string(r->msg.DialedNumberMessage.calledParty, c->calledPartyNumber, sizeof(r->msg.DialedNumberMessage.calledParty));

	r->msg.DialedNumberMessage.lel_lineId   = htolel(instance);
	r->msg.DialedNumberMessage.lel_callRef  = htolel(c->callid);
	sccp_dev_send(device, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send the dialed number %s for %s channel %d\n", device->id, c->calledPartyNumber, calltype2str(c->calltype), c->callid);
}

/*!
 * \brief Set Call State for SCCP Channel c, and Send this State to SCCP Device d.
 * \param d device to send call state
 * \param c SCCP Channel
 * \param state channel state
 */
void sccp_channel_setSkinnyCallstate(sccp_channel_t * c, skinny_callstate_t state)
{
	//uint8_t instance;
	
	c->previousChannelState =c->state;
	c->state = state;
	
	//c->callstate = state;

	return;

	//instance = sccp_device_find_index_for_line(d, c->line->name);
	//sccp_device_sendcallstate(d, instance, c->callid, state, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
}


/*!
 * \brief Set CallingParty on SCCP Channel c
 * \param c SCCP Channel
 * \param name Name as char
 * \param number Number as char
 */
void sccp_channel_set_callingparty(sccp_channel_t * c, char *name, char *number)
{
	if (!c)
		return;


	if (name && strncmp(name, c->callingPartyName, StationMaxNameSize - 1)) {
		sccp_copy_string(c->callingPartyName, name, sizeof(c->callingPartyName));
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Set callingParty Name %s on channel %d\n", DEV_ID_LOG(c->device), c->callingPartyName, c->callid);
	}

	if (number && strncmp(number, c->callingPartyNumber, StationMaxDirnumSize - 1)) {
		sccp_copy_string(c->callingPartyNumber, number, sizeof(c->callingPartyNumber));
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Set callingParty Number %s on channel %d\n", DEV_ID_LOG(c->device), c->callingPartyNumber, c->callid);
	}
	return;
}


/*!
 * \brief Set CalledParty on SCCP Channel c
 * \param c SCCP Channel
 * \param name Called Party Name
 * \param number Called Party Number
 */
void sccp_channel_set_calledparty(sccp_channel_t * c, char *name, char *number)
{
        if (!c)
		return;

	if (name && strncmp(name, c->calledPartyName, StationMaxNameSize - 1)) {
		sccp_copy_string(c->calledPartyName, name, sizeof(c->calledPartyNumber));
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Set calledParty Name %s on channel %d\n", DEV_ID_LOG(c->device), c->calledPartyName, c->callid);
	}

	if (number && strncmp(number, c->calledPartyNumber, StationMaxDirnumSize - 1)) {
		sccp_copy_string(c->calledPartyNumber, number, sizeof(c->callingPartyNumber));
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Set calledParty Number %s on channel %d\n", DEV_ID_LOG(c->device), c->calledPartyNumber, c->callid);
	}
}

/*!
 * \brief Request Call Statistics for SCCP Channel
 * \param c SCCP Channel
 */
void sccp_channel_StatisticsRequest(sccp_channel_t * c)
{
	sccp_moo_t * r;
	sccp_device_t * d = c->device;

	if (!c || !d)
		return;

	REQ(r, ConnectionStatisticsReq);

	/* XXX need to test what we have to copy in the DirectoryNumber */
	if (c->calltype == SKINNY_CALLTYPE_OUTBOUND)
		sccp_copy_string(r->msg.ConnectionStatisticsReq.DirectoryNumber, c->calledPartyNumber, sizeof(r->msg.ConnectionStatisticsReq.DirectoryNumber));
	else
		sccp_copy_string(r->msg.ConnectionStatisticsReq.DirectoryNumber, c->callingPartyNumber, sizeof(r->msg.ConnectionStatisticsReq.DirectoryNumber));

	r->msg.ConnectionStatisticsReq.lel_callReference = htolel((c) ? c->callid : 0);
	r->msg.ConnectionStatisticsReq.lel_StatsProcessing = htolel(SKINNY_STATSPROCESSING_CLEAR);
	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Requesting CallStatisticsAndClear from Phone\n", (d && d->id)?d->id:"SCCP");
}

#ifndef ASTERISK_CONF_1_2
/*!
 * \brief Get RTP Peer from Channel
 * \param ast Asterisk Channel
 * \param rtp Asterisk RTP
 * \return ENUM of RTP Result
 */
enum ast_rtp_get_result sccp_channel_get_rtp_peer(struct ast_channel *ast, struct ast_rtp **rtp)
{
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;
	enum ast_rtp_get_result res = AST_RTP_TRY_NATIVE;

	sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) Asterisk requested RTP peer for channel %s\n", ast->name);

	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) NO PVT\n");
		return AST_RTP_GET_FAILED;
	}

	if (!c->rtp.audio) {
		sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) NO RTP\n");
		return AST_RTP_GET_FAILED;
	}

	*rtp = c->rtp.audio;
	if(!(d = c->device)) {
		sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) NO DEVICE\n");
		return AST_RTP_GET_FAILED;
	}

	if (ast_test_flag(&GLOB(global_jbconf), AST_JB_FORCED)) {
		sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) JitterBuffer is Forced. AST_RTP_GET_FAILED\n");
		return AST_RTP_GET_FAILED;
	}

 	if (!d->directrtp) {
 		sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) Direct RTP disabled\n");
 		return AST_RTP_GET_FAILED;
 	}

	if (d->nat) {
		res = AST_RTP_TRY_PARTIAL;
		sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) Using AST_RTP_TRY_PARTIAL for channel %s\n", ast->name);
	} else {
		sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) Using AST_RTP_TRY_NATIVE for channel %s\n", ast->name);
	}

	return res;
}



#ifndef CS_AST_HAS_RTP_ENGINE
enum ast_rtp_get_result sccp_channel_get_vrtp_peer(struct ast_channel *ast, struct ast_rtp **rtp){
#else
enum ast_rtp_glue_result sccp_channel_get_vrtp_peer(struct ast_channel *ast, struct ast_rtp_instance **rtp){
#endif

	sccp_channel_t *c = NULL;
	if (!(c = CS_AST_CHANNEL_PVT(ast)) || !(c->rtp.video)) {
#ifndef CS_AST_HAS_RTP_ENGINE
		return AST_RTP_GET_FAILED;
#else
		return AST_RTP_GLUE_RESULT_FORBID;
#endif
	}
#ifdef CS_AST_HAS_RTP_ENGINE
	ao2_ref(c->rtp.video, +1);
#endif
	*rtp = c->rtp.video;
	return AST_RTP_TRY_PARTIAL;
}



#ifdef ASTERISK_CONF_1_4
/*!
 * \brief Set RTP Peer from Channel
 * \param ast Asterisk Channel
 * \param rtp Asterisk RTP
 * \param vrtp Asterisk RTP
 * \param codecs Codecs as int
 * \param nat_active Is NAT Active as int
 * \return Result as int
 */
int sccp_channel_set_rtp_peer(struct ast_channel *ast, struct ast_rtp *rtp, struct ast_rtp *vrtp, int codecs, int nat_active)
#else
/*!
 * \brief Set RTP Peer from Channel
 * \param ast Asterisk Channel
 * \param rtp Asterisk RTP
 * \param vrtp Asterisk RTP
 * \param trtp Asterisk RTP
 * \param codecs Codecs as int
 * \param nat_active Is NAT Active as int
 * \return Result as int
 */
int sccp_channel_set_rtp_peer(struct ast_channel *ast, struct ast_rtp *rtp, struct ast_rtp *vrtp, struct ast_rtp *trtp, int codecs, int nat_active)
#endif
{
	sccp_channel_t *c = NULL;
	sccp_line_t *l = NULL;
	sccp_device_t *d = NULL;
	sccp_moo_t *r;

	struct ast_format_list fmt;
	struct sockaddr_in us;
	struct sockaddr_in them;

	sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer)\n");

	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO PVT\n");
		return -1;
	}
	if (!(l = c->line)) {
		sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO LINE\n");
		return -1;
	}
	if(!(d = c->device)) {
		sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO DEVICE\n");
		return -1;
	}

	if(!d->directrtp) {
		sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) Direct RTP disabled\n");
		return -1;
	}

	if (rtp) {
		ast_rtp_get_peer(rtp, &them);

		sccp_log(1)(VERBOSE_PREFIX_1 "%s: (sccp_channel_set_rtp_peer) Stop media transmission on channel %d\n", DEV_ID_LOG(d), c->callid);

		/* Shutdown any early-media or previous media on re-invite */
		REQ(r, StopMediaTransmission);
		r->msg.StopMediaTransmission.lel_conferenceId = htolel(c->callid);
		r->msg.StopMediaTransmission.lel_passThruPartyId = htolel(c->passthrupartyid);
		r->msg.StopMediaTransmission.lel_conferenceId1 = htolel(c->callid);
		sccp_dev_send(d, r);

		sccp_log(1)(VERBOSE_PREFIX_1 "%s: (sccp_channel_set_rtp_peer) Asterisk request to set peer ip to '%s:%d'\n", DEV_ID_LOG(d), ast_inet_ntoa(them.sin_addr), ntohs(them.sin_port));

		sccp_log(1)(VERBOSE_PREFIX_1 "%s: (sccp_channel_set_rtp_peer) Asterisk request codec '%d'\n", DEV_ID_LOG(d), codecs);

		//fmt = ast_codec_pref_getsize(&d->codecs, ast_best_codec(d->capability));
		int codec = ast_codec_choose(&c->codecs, codecs, 1);
		sccp_log(1)(VERBOSE_PREFIX_1 "%s: (sccp_channel_set_rtp_peer) Asterisk request codec '%d'\n", DEV_ID_LOG(d), codec);
		fmt = ast_codec_pref_getsize(&d->codecs, codec);

		c->format = fmt.bits; /* updating channel format */
		sccp_log(1)(VERBOSE_PREFIX_1 "%s: (sccp_channel_set_rtp_peer) Setting payloadType to '%d' (%d ms)\n", DEV_ID_LOG(d), fmt.bits, fmt.cur_ms);

		r->msg.StartMediaTransmission.lel_conferenceId = htolel(c->callid);
		r->msg.StartMediaTransmission.lel_passThruPartyId = htolel(c->callid);

		if(d->inuseprotocolversion < 17) {
			REQ(r, StartMediaTransmission);
			r->msg.StartMediaTransmission.lel_conferenceId = htolel(c->callid);
			r->msg.StartMediaTransmission.lel_passThruPartyId = htolel(c->callid);
			r->msg.StartMediaTransmission.lel_conferenceId1 = htolel(c->callid);
		} else {
			r = sccp_build_packet(StartMediaTransmission, sizeof(r->msg.StartMediaTransmission_v17));
			r->msg.StartMediaTransmission_v17.lel_conferenceId = htolel(c->callid);
			r->msg.StartMediaTransmission_v17.lel_passThruPartyId = htolel(c->callid);
			r->msg.StartMediaTransmission_v17.lel_conferenceId1 = htolel(c->callid);
		}
		if(d->inuseprotocolversion < 17) {
			if (!d->directrtp || d->nat) {
				ast_rtp_get_us(rtp, &us);
				sccp_log(1)(VERBOSE_PREFIX_1 "%s: (sccp_channel_set_rtp_peer) Set RTP peer ip to '%s:%d'\n", DEV_ID_LOG(d), ast_inet_ntoa(us.sin_addr), ntohs(us.sin_port));
				memcpy(&r->msg.StartMediaTransmission.bel_remoteIpAddr, &us.sin_addr, 4);
				r->msg.StartMediaTransmission.lel_remotePortNumber = htolel(ntohs(us.sin_port));
			} else {
				sccp_log(1)(VERBOSE_PREFIX_1 "%s: (sccp_channel_set_rtp_peer) Set RTP peer ip to '%s:%d'\n", DEV_ID_LOG(d), ast_inet_ntoa(them.sin_addr), ntohs(them.sin_port));
				memcpy(&r->msg.StartMediaTransmission.bel_remoteIpAddr, &them.sin_addr, 4);
				r->msg.StartMediaTransmission.lel_remotePortNumber = htolel(ntohs(them.sin_port));
			}
			r->msg.StartMediaTransmission.lel_millisecondPacketSize = htolel(fmt.cur_ms);
			r->msg.StartMediaTransmission.lel_payloadType = htolel(sccp_codec_ast2skinny(fmt.bits));
			r->msg.StartMediaTransmission.lel_precedenceValue = htolel(l->rtptos);
			r->msg.StartMediaTransmission.lel_ssValue = htolel(l->silencesuppression); // Silence supression
			r->msg.StartMediaTransmission.lel_maxFramesPerPacket = htolel(0);
			r->msg.StartMediaTransmission.lel_rtptimeout = htolel(10);
		} else {
			if (!d->directrtp || d->nat) {
				ast_rtp_get_us(rtp, &us);
				sccp_log(1)(VERBOSE_PREFIX_1 "%s: (sccp_channel_set_rtp_peer) Set RTP peer ip to '%s:%d'\n", DEV_ID_LOG(d), ast_inet_ntoa(us.sin_addr), ntohs(us.sin_port));
				memcpy(&r->msg.StartMediaTransmission_v17.bel_remoteIpAddr, &us.sin_addr, 4);
				r->msg.StartMediaTransmission_v17.lel_remotePortNumber = htolel(ntohs(us.sin_port));
			} else {
				sccp_log(1)(VERBOSE_PREFIX_1 "%s: (sccp_channel_set_rtp_peer) Set RTP peer ip to '%s:%d'\n", DEV_ID_LOG(d), ast_inet_ntoa(them.sin_addr), ntohs(them.sin_port));
				memcpy(&r->msg.StartMediaTransmission_v17.bel_remoteIpAddr, &them.sin_addr, 4);
				r->msg.StartMediaTransmission_v17.lel_remotePortNumber = htolel(ntohs(them.sin_port));
			}
			r->msg.StartMediaTransmission_v17.lel_millisecondPacketSize = htolel(fmt.cur_ms);
			r->msg.StartMediaTransmission_v17.lel_payloadType = htolel(sccp_codec_ast2skinny(fmt.bits));
			r->msg.StartMediaTransmission_v17.lel_precedenceValue = htolel(l->rtptos);
			r->msg.StartMediaTransmission_v17.lel_ssValue = htolel(l->silencesuppression); // Silence supression
			r->msg.StartMediaTransmission_v17.lel_maxFramesPerPacket = htolel(0);
			r->msg.StartMediaTransmission_v17.lel_rtptimeout = htolel(10);
		}
		sccp_dev_send(d, r);
		c->mediaStatus.transmit = TRUE;

		return 0;
	} else {
		if(ast->_state != AST_STATE_UP) {
			sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) Early RTP stage, codecs=%d, nat=%d\n", codecs, d->nat);
		} else {
			sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) Native Bridge Break, codecs=%d, nat=%d\n", codecs, d->nat);
		}
		return 0;
	}

	/* Need a return here to break the bridge */
	return 0;
}
#endif

/*!
 * \brief Tell Device to Open a RTP Receive Channel
 *
 * At this point we choose the codec for receive channel and tell them to device.
 * We will get a OpenReceiveChannelAck message that includes all information.
 *
 * \param c SCCP Channel
 */
void sccp_channel_openreceivechannel(sccp_channel_t * c)
{
	sccp_moo_t * r;
	sccp_device_t * d = NULL;
	int payloadType;
	int packetSize;
	struct sockaddr_in them;
	uint8_t	instance;




	if(!c || !c->device)
		return;

	sccp_channel_lock(c);
	d = c->device;
	
	sccp_channel_updateChannelCapability(c);
	c->isCodecFix = TRUE;
#ifndef ASTERISK_CONF_1_2
	struct ast_format_list fmt = ast_codec_pref_getsize(&c->codecs, c->format);
	payloadType = sccp_codec_ast2skinny(fmt.bits);
	packetSize = fmt.cur_ms;
#else
	payloadType = sccp_codec_ast2skinny(c->format); // was c->format
	packetSize = 20;
#endif



	if(!payloadType){
		c->isCodecFix = FALSE;
		sccp_channel_updateChannelCapability(c);
		c->isCodecFix = TRUE;
	}



	sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: channel %s payloadType %d\n", c->device->id, (c->owner)?c->owner->name:"NULL", payloadType);

	/* create the rtp stuff. It must be create before setting the channel AST_STATE_UP. otherwise no audio will be played */
	sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: Ask the device to open a RTP port on channel %d. Codec: %s, echocancel: %s\n", c->device->id, c->callid, codec2str(payloadType), c->line->echocancel ? "ON" : "OFF");
	if (!c->rtp.audio) {
		sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: Starting RTP on channel %s-%08X\n", DEV_ID_LOG(c->device), c->line->name, c->callid);
		sccp_channel_start_rtp(c);
	}
	if (!c->rtp.audio) {
		ast_log(LOG_WARNING, "%s: Error opening RTP for channel %s-%08X\n", DEV_ID_LOG(c->device), c->line->name, c->callid);

		instance = sccp_device_find_index_for_line(c->device, c->line->name);
		sccp_dev_starttone(c->device, SKINNY_TONE_REORDERTONE, instance, c->callid, 0);
		sccp_channel_unlock(c);
		return;
	}

	sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: Open receive channel with format %s[%d] (%d ms), payload %d, echocancel: %d\n", c->device->id, codec2str(payloadType), c->format, packetSize, payloadType, c->line->echocancel);

	if(d->inuseprotocolversion >= 17) {
		r = sccp_build_packet(OpenReceiveChannel, sizeof(r->msg.OpenReceiveChannel_v17));
		ast_rtp_get_peer(c->rtp.audio, &them);
		memcpy(&r->msg.OpenReceiveChannel_v17.bel_remoteIpAddr, &them.sin_addr, 4);
		r->msg.OpenReceiveChannel_v17.lel_conferenceId = htolel(c->callid);
		r->msg.OpenReceiveChannel_v17.lel_passThruPartyId = htolel(c->passthrupartyid) ;
		r->msg.OpenReceiveChannel_v17.lel_millisecondPacketSize = htolel(packetSize);
		/* if something goes wrong the default codec is ulaw */
		r->msg.OpenReceiveChannel_v17.lel_payloadType = htolel((payloadType) ? payloadType : 4);
		r->msg.OpenReceiveChannel_v17.lel_vadValue = htolel(c->line->echocancel);
		r->msg.OpenReceiveChannel_v17.lel_conferenceId1 = htolel(c->callid);
		r->msg.OpenReceiveChannel_v17.lel_rtptimeout = htolel(10);
		r->msg.OpenReceiveChannel_v17.lel_unknown20 = htolel(4000);
	}else {
		REQ(r, OpenReceiveChannel);
		r->msg.OpenReceiveChannel.lel_conferenceId = htolel(c->callid);
		r->msg.OpenReceiveChannel.lel_passThruPartyId = htolel(c->passthrupartyid) ;
		r->msg.OpenReceiveChannel.lel_millisecondPacketSize = htolel(packetSize);
		/* if something goes wrong the default codec is ulaw */
		r->msg.OpenReceiveChannel.lel_payloadType = htolel((payloadType) ? payloadType : 4);
		r->msg.OpenReceiveChannel.lel_vadValue = htolel(c->line->echocancel);
		r->msg.OpenReceiveChannel.lel_conferenceId1 = htolel(c->callid);
		r->msg.OpenReceiveChannel.lel_rtptimeout = htolel(10);
	}
	sccp_dev_send(c->device, r);
	c->mediaStatus.receive = TRUE;
	sccp_channel_unlock(c);

}

void sccp_channel_openMultiMediaChannel(sccp_channel_t *channel){

}

void sccp_channel_startMultiMediaTransmission(sccp_channel_t *channel){

}


/*!
 * \brief Tell a Device to Start Media Transmission.
 *
 * We choose codec according to c->format.
 *
 * \param c SCCP Channel
 * \note rtp should be started before, otherwise we do not start transmission
 */
void sccp_channel_startmediatransmission(sccp_channel_t * c)
{
	sccp_moo_t * r;
	sccp_device_t * d = NULL;
	struct sockaddr_in sin;
	struct ast_hostent ahp;
	struct hostent *hp;
	int payloadType;
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif
	int packetSize;
#ifndef ASTERISK_CONF_1_2
	struct ast_format_list fmt;
#endif

	if (!c->rtp.audio) {
		sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: can't start rtp media transmission, maybe channel is down %s-%08X\n", c->device->id, c->line->name, c->callid);
		return;
	}

	if(!(d = c->device))
		return;


	ast_rtp_get_us(c->rtp.audio, &sin);

	if(d->inuseprotocolversion < 17) {
		REQ(r, StartMediaTransmission);
		r->msg.StartMediaTransmission.lel_conferenceId = htolel(c->callid);
		r->msg.StartMediaTransmission.lel_passThruPartyId = htolel(c->passthrupartyid);
		r->msg.StartMediaTransmission.lel_conferenceId1 = htolel(c->callid);
	} else {
		r = sccp_build_packet(StartMediaTransmission, sizeof(r->msg.StartMediaTransmission_v17));
		r->msg.StartMediaTransmission_v17.lel_conferenceId = htolel(c->callid);
		r->msg.StartMediaTransmission_v17.lel_passThruPartyId = htolel(c->passthrupartyid);
		r->msg.StartMediaTransmission_v17.lel_conferenceId1 = htolel(c->callid);
	}

	if (c->device->nat) {
		if (GLOB(externip.sin_addr.s_addr)) {
			if (GLOB(externexpire) && (time(NULL) >= GLOB(externexpire))) {
				time(&GLOB(externexpire));
				GLOB(externexpire) += GLOB(externrefresh);
				if ((hp = ast_gethostbyname(GLOB(externhost), &ahp))) {
					memcpy(&GLOB(externip.sin_addr), hp->h_addr, sizeof(GLOB(externip.sin_addr)));
				} else
					ast_log(LOG_NOTICE, "Warning: Re-lookup of '%s' failed!\n", GLOB(externhost));
			}
			memcpy(&sin.sin_addr, &GLOB(externip.sin_addr), 4);
		}
	}

#ifndef ASTERISK_CONF_1_2
	fmt = ast_codec_pref_getsize(&c->device->codecs, c->format);
	payloadType = sccp_codec_ast2skinny(fmt.bits);
	packetSize = fmt.cur_ms;
#else
	payloadType = sccp_codec_ast2skinny(c->format); // was c->format
	packetSize = 20;
#endif

	if(d->inuseprotocolversion < 17) {
		memcpy(&r->msg.StartMediaTransmission.bel_remoteIpAddr, &sin.sin_addr, 4);
		r->msg.StartMediaTransmission.lel_remotePortNumber = htolel(ntohs(sin.sin_port));
		r->msg.StartMediaTransmission.lel_millisecondPacketSize = htolel(packetSize);
		r->msg.StartMediaTransmission.lel_payloadType = htolel((payloadType) ? payloadType : 4);
		r->msg.StartMediaTransmission.lel_precedenceValue = htolel(c->line->rtptos);
		r->msg.StartMediaTransmission.lel_ssValue = htolel(c->line->silencesuppression); // Silence supression
		r->msg.StartMediaTransmission.lel_maxFramesPerPacket = htolel(0);
		r->msg.StartMediaTransmission.lel_rtptimeout = htolel(10);
	} else {
		memcpy(&r->msg.StartMediaTransmission_v17.bel_remoteIpAddr, &sin.sin_addr, 4);
		r->msg.StartMediaTransmission_v17.lel_remotePortNumber = htolel(ntohs(sin.sin_port));
		r->msg.StartMediaTransmission_v17.lel_millisecondPacketSize = htolel(packetSize);
		r->msg.StartMediaTransmission_v17.lel_payloadType = htolel((payloadType) ? payloadType : 4);
		r->msg.StartMediaTransmission_v17.lel_precedenceValue = htolel(c->line->rtptos);
		r->msg.StartMediaTransmission_v17.lel_ssValue = htolel(c->line->silencesuppression); // Silence supression
		r->msg.StartMediaTransmission_v17.lel_maxFramesPerPacket = htolel(0);
		r->msg.StartMediaTransmission_v17.lel_rtptimeout = htolel(10);
	}
	sccp_dev_send(c->device, r);
#ifdef ASTERISK_CONF_1_2
	sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: Tell device to send RTP media to %s:%d with codec: %s (%d ms), tos %d, silencesuppression: %s\n",c->device->id, ast_inet_ntoa(iabuf, sizeof(iabuf), sin.sin_addr), ntohs(sin.sin_port), codec2str(payloadType), packetSize, c->line->rtptos, c->line->silencesuppression ? "ON" : "OFF");
#else
	sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: Tell device to send RTP media to %s:%d with codec: %s(%d) (%d ms), tos %d, silencesuppression: %s\n",c->device->id, ast_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), codec2str(payloadType),payloadType, packetSize, c->line->rtptos, c->line->silencesuppression ? "ON" : "OFF");
#endif
}

/*!
 * \brief Tell Device to Close an RTP Receive Channel and Stop Media Transmission
 * \param c SCCP Channel
 * \note sccp_channel_stopmediatransmission is explicit call within this function!
 */
void sccp_channel_closereceivechannel(sccp_channel_t * c)
{
	sccp_moo_t * r;
	sccp_device_t * d = c->device;

	if(d) {
		REQ(r, CloseReceiveChannel);
		r->msg.CloseReceiveChannel.lel_conferenceId = htolel(c->callid);
		r->msg.CloseReceiveChannel.lel_passThruPartyId = htolel(c->passthrupartyid);
		r->msg.CloseReceiveChannel.lel_conferenceId1 = htolel(c->callid);
		sccp_dev_send(d, r);
		sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: Close openreceivechannel on channel %d\n", DEV_ID_LOG(d), c->callid);
	}
	c->isCodecFix = FALSE;
	c->mediaStatus.receive = FALSE;
	sccp_channel_stopmediatransmission(c);
}


/*!
 * \brief Tell device to Stop Media Transmission.
 *
 * Also RTP will be Stopped/Destroyed and Call Statistic is requested.
 * \param c SCCP Channel
 */
void sccp_channel_stopmediatransmission(sccp_channel_t * c)
{
	sccp_moo_t * r;
	sccp_device_t * d = NULL;

	if(!c)
		return;

	d = c->device;

	REQ(r, StopMediaTransmission);
	if(d) {
		r->msg.StopMediaTransmission.lel_conferenceId = htolel(c->callid);
		r->msg.StopMediaTransmission.lel_passThruPartyId = htolel(c->passthrupartyid);
		r->msg.StopMediaTransmission.lel_conferenceId1 = htolel(c->callid);
		sccp_dev_send(d, r);
		sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: Stop media transmission on channel %d\n",(d && d->id)?d->id:"(none)", c->callid);
	}
	c->mediaStatus.transmit = FALSE;
	// stopping rtp
	if(c->rtp.audio) {
		sccp_channel_stop_rtp(c);
	}

	/* requesting statistics */
	sccp_channel_StatisticsRequest(c);
}

/*!
 * \brief Update Channel Media Type / Native Bridged Format Match
 * \param c SCCP Channel
 * \note Copied function from v2 (FS)
 */
void sccp_channel_updatemediatype(sccp_channel_t * c) {
        struct ast_channel * bridged = NULL;

        /* checking for ast_channel owner */
        if(!c->owner) {
                return;
        }
        /* is owner ast_channel a zombie ? */
        if(ast_test_flag(c->owner, AST_FLAG_ZOMBIE)) {
                return; /* c->owner is zombie, leaving */
        }
        /* channel is hanging up */
        if(c->owner->_softhangup != 0) {
                return;
        }
        /* channel is not running */
        if(!c->owner->pbx) {
                return;
        }
        /* check for briged ast_channel */
        if(!(bridged = CS_AST_BRIDGED_CHANNEL(c->owner))) {
                return; /* no bridged channel, leaving */
        }
        /* is bridged ast_channel a zombie ? */
        if(ast_test_flag(bridged, AST_FLAG_ZOMBIE)) {
                return; /* bridged channel is zombie, leaving */
        }
        /* channel is hanging up */
        if(bridged->_softhangup != 0) {
                return;
        }
        /* channel is not running */
        if(!bridged->pbx) {
                return;
        }
        if(c->state != SCCP_CHANNELSTATE_ZOMBIE) {
                ast_log(LOG_NOTICE, "%s: Channel %s -> nativeformats:%d - r:%d/w:%d - rr:%d/rw:%d\n",
                                        DEV_ID_LOG(c->device),
                                        bridged->name,
                                        bridged->nativeformats,
                                        bridged->writeformat,
                                        bridged->readformat,
                                #ifdef CS_AST_HAS_TECH_PVT
                                        bridged->rawreadformat,
                                        bridged->rawwriteformat
                                #else
                                        bridged->pvt->rawwriteformat,
                                        bridged->pvt->rawreadformat
                                #endif
                                        );
                if(!(bridged->nativeformats & c->owner->nativeformats) && (bridged->nativeformats & c->device->capability)) {
                        c->owner->nativeformats = c->format = bridged->nativeformats;
                        sccp_channel_closereceivechannel(c);
                        usleep(100);
                        sccp_channel_openreceivechannel(c);
                        ast_set_read_format(c->owner, c->format);
                        ast_set_write_format(c->owner, c->format);
                }
        }
}

/*!
 * \brief Hangup this channel.
 * \param c SCCP Channel
 */
void sccp_channel_endcall(sccp_channel_t * c)
{
	uint8_t res = 0;

	if (!c || !c->line){
		ast_log(LOG_WARNING, "No channel or line or device to hangup\n");
		return;
	}

	/* this is a station active endcall or onhook */
    	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Ending call %d on line %s (%s)\n", DEV_ID_LOG(c->device), c->callid, c->line->name, sccp_indicate2str(c->state));
	/* end callforwards */
	sccp_channel_t	*channel;
	SCCP_LIST_LOCK(&c->line->channels);
	SCCP_LIST_TRAVERSE(&c->line->channels, channel, list) {
		if(channel->parentChannel == c)
			 sccp_channel_endcall(channel);
	}
	SCCP_LIST_UNLOCK(&c->line->channels);
	/* */

	if (c->owner) {
		/* Is there a blocker ? */
		res = (c->owner->pbx || c->owner->blocker);

		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Sending %s hangup request to %s\n", DEV_ID_LOG(c->device), res ? "(queue)" : "(force)", c->owner->name);
		

		
		c->owner->hangupcause = AST_CAUSE_NORMAL_CLEARING;

		/* force hanguo for invalid dials */
		if(c->state == SCCP_CHANNELSTATE_INVALIDNUMBER || c->state == SCCP_CHANNELSTATE_OFFHOOK){
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: Sending force hangup request to %s\n", DEV_ID_LOG(c->device), c->owner->name);
			ast_hangup(c->owner);
		}else{
			if (res) {
				c->owner->_softhangup |= AST_SOFTHANGUP_DEV;
				ast_queue_hangup(c->owner);
			} else {
				ast_hangup(c->owner);
			}
		}
	} else {
		sccp_log(10)(VERBOSE_PREFIX_1 "%s: No Asterisk channel to hangup for sccp channel %d on line %s\n", DEV_ID_LOG(c->device), c->callid, c->line->name);
	}
}

/*!
 * \brief Allocate a new Outgoing Channel.
 *
 * \param l SCCP Line that owns this channel
 * \param device SCCP Device that owns this channel
 * \param dial Dialed Number as char
 * \param calltype Calltype as int
 * \return SCCP Channel or NULL if something is wrong
 */
sccp_channel_t * sccp_channel_newcall(sccp_line_t * l, sccp_device_t *device, char * dial, uint8_t calltype)
{
	/* handle outgoing calls */
	sccp_channel_t * c;

	if (!l) {
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if a line is not defined!\n");
		return NULL;
	}

	if (!device || ast_strlen_zero(device->id)) {
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if a device is not defined!\n");
		return NULL;
	}


	/* look if we have a call to put on hold */
	if ( (c = sccp_channel_get_active(device)) && (NULL == c->conference) ) {
		/* there is an active call, let's put it on hold first */
		if (!sccp_channel_hold(c))
			return NULL;
	}

	c = sccp_channel_allocate(l, device);

	if (!c) {
		ast_log(LOG_ERROR, "%s: Can't allocate SCCP channel for line %s\n", device->id, l->name);
		return NULL;
	}

	sccp_channel_lock(c);

	c->ss_action = SCCP_SS_DIAL; /* softswitch will catch the number to be dialed */
	c->ss_data = 0; // nothing to pass to action

	c->calltype = calltype;

	sccp_channel_set_active(device, c);

	/* copy the number to dial in the ast->exten */
	if (dial) {
		sccp_copy_string(c->dialedNumber, dial, sizeof(c->dialedNumber));
		sccp_indicate_nolock(device, c, SCCP_CHANNELSTATE_SPEEDDIAL);
	}
	else {
		sccp_indicate_nolock(device, c, SCCP_CHANNELSTATE_OFFHOOK);
	}
	sccp_channel_unlock(c);

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c)) {
		ast_log(LOG_WARNING, "%s: Unable to allocate a new channel for line %s\n", device->id, l->name);
		sccp_indicate_lock(device, c, SCCP_CHANNELSTATE_CONGESTION);
		return c;
	}

	sccp_ast_setstate(c, AST_STATE_OFFHOOK);


	if (device->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !c->rtp.audio) {
		sccp_channel_openreceivechannel(c);
	}

	if(dial) {
		sccp_pbx_softswitch(c);
		return c;
	}

	if( (c->digittimeout = sccp_sched_add(sched, GLOB(firstdigittimeout) * 1000, sccp_pbx_sched_dial, c)) < 0 ) {
		sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: Unable to schedule dialing in '%d' ms\n", GLOB(firstdigittimeout));
	}

	return c;
}


/*!
 * \brief Answer an Incoming Call.
 * \param device SCCP Device who answers
 * \param c incoming SCCP channel
 * \todo handle codec choose
 */
void sccp_channel_answer(sccp_device_t *device, sccp_channel_t * c)
{
	sccp_line_t * l;
	sccp_device_t 	*d;
	sccp_channel_t * c1;
#ifdef CS_AST_HAS_FLAG_MOH
	struct ast_channel * bridged;
#endif

	if (!c || !c->line ) {
		ast_log(LOG_ERROR, "SCCP: Channel %d has no line\n", (c ? c->callid : 0));
		return;
	}

	if (!c->owner) {
		ast_log(LOG_ERROR, "SCCP: Channel %d has no owner\n", c->callid);
		return;
	}

	l = c->line;
	d = (c->state == SCCP_CHANNELSTATE_HOLD)?device:c->device;

	/* channel was on hold, restore active -> inc. channelcount*/
	if(c->state == SCCP_CHANNELSTATE_HOLD){
		sccp_line_lock(c->line);
		//c->line->channelCount++;
		c->line->statistic.numberOfActiveChannels--;
		sccp_line_unlock(c->line);
	}

	if(!d){
		if(!device){
			ast_log(LOG_ERROR, "SCCP: Channel %d has no device\n", (c ? c->callid : 0));
			return;
		}
		d = device;
	}
	c->device = d;

	sccp_channel_updateChannelCapability(c);

	/*
	c->owner->nativeformats = device->capability;
	c->format = ast_codec_choose(&device->codecs, device->capability, 1);
	c->owner->rawreadformat = device->capability;
	c->owner->rawwriteformat = device->capability;
	*/

	/* answering an incoming call */
	/* look if we have a call to put on hold */
	if ((c1 = sccp_channel_get_active(d))) {
		if(NULL != c1) {
			/* If there is a ringout or offhook call, we end it so that we can answer the call. */
			if(c1 && (c1->state == SCCP_CHANNELSTATE_OFFHOOK || c1->state == SCCP_CHANNELSTATE_RINGOUT)) {
				sccp_channel_endcall(c1);
			} else if (!sccp_channel_hold(c1)) {
				/* there is an active call, let's put it on hold first */
				return;
			}
		}
	}


	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Answer the channel %s-%08X\n", DEV_ID_LOG(d), l->name, c->callid);

	/* end callforwards */
	sccp_channel_t	*channel;
	SCCP_LIST_LOCK(&c->line->channels);
	SCCP_LIST_TRAVERSE(&c->line->channels, channel, list) {
		if(channel->parentChannel == c){
			 sccp_log(1)(VERBOSE_PREFIX_3 "%s: Hangup cfwd channel %s-%08X\n", DEV_ID_LOG(d), l->name, channel->callid);
			 sccp_channel_endcall(channel);
		}
	}
	SCCP_LIST_UNLOCK(&c->line->channels);
	/* */

	sccp_channel_set_active(d, c);
	sccp_dev_set_activeline(d, c->line);

	/* the old channel state could be CALLTRANSFER, so the bridged channel is on hold */
	/* do we need this ? -FS */
#ifdef CS_AST_HAS_FLAG_MOH
	bridged = CS_AST_BRIDGED_CHANNEL(c->owner);
	if (bridged && ast_test_flag(bridged, AST_FLAG_MOH)) {
			 ast_moh_stop(bridged);
			 ast_clear_flag(bridged, AST_FLAG_MOH);
	}
#endif

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Answering channel with state '%s' (%d)\n", DEV_ID_LOG(c->device), ast_state2str(c->owner->_state), c->owner->_state);
	ast_queue_control(c->owner, AST_CONTROL_ANSWER);

	/* @Marcello: Here you assume that it is not neccessary to tell the phone
	   something it already knows ;-) But I am not sure if this would be needed
	   nevertheless to log all incoming answered calls properly. We will have to
	   investigate this further. (-DD) */
	if (c->state != SCCP_CHANNELSTATE_OFFHOOK)
		sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_OFFHOOK);

	sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_CONNECTED);
}


/*!
 * \brief Put channel on Hold.
 *
 * \param c SCCP Channel
 * \return Status as in (0 if something was wrong, otherwise 1)
 */
int sccp_channel_hold(sccp_channel_t * c)
{
	sccp_line_t * l;
	sccp_device_t * d;
	int instance;

	if (!c)
		return 0;

	if (!c->line || !c->device) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", c->callid);
		return 0;
	}

	l = c->line;
	d = c->device;

	if (c->state == SCCP_CHANNELSTATE_HOLD) {
		ast_log(LOG_WARNING, "SCCP: Channel already on hold\n");
		return 0;
	}

	instance = sccp_device_find_index_for_line(d, l->name);
	/* put on hold an active call */
	if (c->state != SCCP_CHANNELSTATE_CONNECTED && c->state != SCCP_CHANNELSTATE_PROCEED) { // TOLL FREE NUMBERS STAYS ALWAYS IN CALL PROGRESS STATE
		/* something wrong on the code let's notify it for a fix */
		ast_log(LOG_ERROR, "%s can't put on hold an inactive channel %s-%08X (%s)\n", d->id, l->name, c->callid, sccp_indicate2str(c->state));
		/* hard button phones need it */
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
		return 0;
	}

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Hold the channel %s-%08X\n", d->id, l->name, c->callid);

#ifndef CS_AST_CONTROL_HOLD
	struct ast_channel * peer;
	peer = CS_AST_BRIDGED_CHANNEL(c->owner);

	if (peer) {
#ifdef ASTERISK_CONF_1_2
		ast_moh_start(peer, NULL);
#else
#ifdef CS_AST_RTP_NEW_SOURCE
		ast_rtp_new_source(c->rtp.audio);
#endif
		ast_moh_start(peer, NULL, l->musicclass);
#endif
#ifndef CS_AST_HAS_FLAG_MOH
		ast_set_flag(peer, AST_FLAG_MOH);
#endif
	}
	else
	{
		ast_log(LOG_ERROR, "SCCP: Cannot find bridged channel on '%s'\n", c->owner->name);
		return 0;
	}
#else
	sccp_device_lock(d);

	if (!c->owner) {
		sccp_log(1)(VERBOSE_PREFIX_3 "C owner disappeared! Can't free ressources\n");
		sccp_device_unlock(d);
		return 0;
	}
	sccp_ast_queue_control(c, AST_CONTROL_HOLD);
	sccp_device_unlock(d);
#endif

	sccp_device_lock(d);
	d->active_channel = NULL;
	sccp_device_unlock(d);
	sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_HOLD); // this will also close (but not destroy) the RTP stream

#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents))
		manager_event(EVENT_FLAG_CALL, "Hold",
			      "Status: On\r\n"
			      "Channel: %s\r\n"
			      "Uniqueid: %s\r\n",
			      c->owner->name,
			      c->owner->uniqueid);
#endif

	if(l){
		//l->channelCount--; /* channel is not active, so dec. count */
		l->statistic.numberOfActiveChannels--;
	}


	sccp_log(64)(VERBOSE_PREFIX_3 "C partyID: %u state: %d\n",c->passthrupartyid, c->state);

	return 1;
}


/*!
 * \brief Resume a channel that is on hold.
 * \param device device who resumes the channel
 * \param c channel
 * \return 0 if something was wrong, otherwise 1
 */
int sccp_channel_resume(sccp_device_t *device, sccp_channel_t * c)
{
	sccp_line_t * l;
	sccp_device_t 	*d;
	sccp_channel_t * hold;
	int instance;

	if (!c || !c->owner)
		return 0;

	if (!c->line || !c->device) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", c->callid);
		return 0;
	}

	l = c->line;
	d = c->device;


	/* on remote device pickups the call */
	if(d != device)
		d = device;

	instance = sccp_device_find_index_for_line(d, l->name);


	/* look if we have a call to put on hold */
 	if ( (hold = sccp_channel_get_active(d)) ) {
 		/* there is an active call, let's put it on hold first */
 		if (!sccp_channel_hold(hold))
 			return 0;
 	}

	/* resume an active call */
	if (c->state != SCCP_CHANNELSTATE_HOLD && c->state != SCCP_CHANNELSTATE_CALLTRANSFER && c->state != SCCP_CHANNELSTATE_CALLCONFERENCE) {
		/* something wrong on the code let's notify it for a fix */
		ast_log(LOG_ERROR, "%s can't resume the channel %s-%08X. Not on hold\n", d->id, l->name, c->callid);
		sccp_dev_displayprompt(d, instance, c->callid, "No active call to put on hold",5);
		return 0;
	}

	/* check if we are in the middle of a transfer */
	sccp_device_lock(d);
	if (d->transfer_channel == c) {
		d->transfer_channel = NULL;
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Transfer on the channel %s-%08X\n", d->id, l->name, c->callid);
	}

	if (d->conference_channel == c) {
		d->conference_channel = NULL;
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Conference on the channel %s-%08X\n", d->id, l->name, c->callid);
	}

	sccp_device_unlock(d);

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Resume the channel %s-%08X\n", d->id, l->name, c->callid);

#ifndef CS_AST_CONTROL_HOLD
	struct ast_channel * peer;
	peer = CS_AST_BRIDGED_CHANNEL(c->owner);
	if (peer) {
		ast_moh_stop(peer);
#ifdef CS_AST_RTP_NEW_SOURCE
		if(c->rtp.audio)
			ast_rtp_new_source(c->rtp.audio);
#endif

		// this is for STABLE version
#ifndef CS_AST_HAS_FLAG_MOH
		ast_clear_flag(peer, AST_FLAG_MOH);
#endif
	}
#else
#ifdef CS_AST_RTP_NEW_SOURCE
	if(c->rtp.audio)
		ast_rtp_new_source(c->rtp.audio);
#endif
	sccp_ast_queue_control(c, AST_CONTROL_UNHOLD);
#endif

	sccp_channel_stop_rtp(c);
	sccp_channel_lock(c);

	c->device = d;
	
	/* force codec update */
	c->isCodecFix = FALSE;
	sccp_channel_updateChannelCapability(c);
	c->isCodecFix = TRUE;
	/* */
	
	
	c->state = SCCP_CHANNELSTATE_HOLD;
	sccp_channel_unlock(c);
	sccp_channel_start_rtp(c);
	sccp_channel_set_active(d, c);
#ifdef CS_AST_CONTROL_SRCUPDATE
	sccp_ast_queue_control(c, AST_CONTROL_SRCUPDATE); 	// notify changes e.g codec
#endif
	sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_CONNECTED); 	// this will also reopen the RTP stream


#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents))
		manager_event(EVENT_FLAG_CALL, "Hold",
				  "Status: Off\r\n"
				  "Channel: %s\r\n"
				  "Uniqueid: %s\r\n",
				  c->owner->name,
				  c->owner->uniqueid);
#endif


	/* state of channel is set down from the remoteDevices, so correct channel state */
	sccp_channel_lock(c);
	c->state = SCCP_CHANNELSTATE_CONNECTED;
	sccp_channel_unlock(c);
	if(l){
		//l->channelCount++; /* channel becomes active */
		l->statistic.numberOfHoldChannels--;
	}


	sccp_log(64)(VERBOSE_PREFIX_3 "C partyID: %u state: %d\n",c->passthrupartyid, c->state);
	return 1;
}

/*!
 * \brief Cleanup Channel before Free.
 * \param c SCCP Channel
 * \note we assume channel is locked
 */
void sccp_channel_cleanbeforedelete(sccp_channel_t *c)   // we assume channel is locked
{

	sccp_line_t *l;
	sccp_device_t *d;
	sccp_selectedchannel_t *x;

	if(!c)
		return;

	d = c->device;
	l = c->line;


	/* mark the channel DOWN so any pending thread will terminate */
	if (c->owner) {
		ast_setstate(c->owner, AST_STATE_DOWN);
		c->owner = NULL;
	}

	/* this is in case we are destroying the session */
	if (c->state != SCCP_CHANNELSTATE_DOWN)
		sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_ONHOOK);

	if (c->rtp.audio || c->rtp.video)
		sccp_channel_stop_rtp(c);


	sccp_line_removeChannel(l, c);


	if(d) {
		/* deactive the active call if needed */
		sccp_device_lock(d);
		d->channelCount--;

		if (d->active_channel == c)
			d->active_channel = NULL;
		if (d->transfer_channel == c)
			d->transfer_channel = NULL;
		if (d->conference_channel == c)
			d->conference_channel = NULL;


		if((x = sccp_device_find_selectedchannel(d, c))) {
			SCCP_LIST_LOCK(&d->selectedChannels);
			SCCP_LIST_REMOVE(&d->selectedChannels, x, list);
			SCCP_LIST_UNLOCK(&d->selectedChannels);
			ast_free(x);
		}
		sccp_device_unlock(d);
	}
}

/*!
 * \brief Delete Channel (WO)
 * \param c SCCP Channel
 * \param list_lock List Lock as int
 * \param channel_lock Channel Lock as int
 * \note We assume channel is locked
 */
void sccp_channel_delete_wo(sccp_channel_t * c, uint8_t list_lock, uint8_t channel_lock)   // We assume channel is locked
{
	sccp_line_t * l = NULL;
	sccp_device_t * d = NULL;


	if (!c)
		return;

	while(channel_lock && sccp_channel_trylock(c)) {
		sccp_log(99)(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
		usleep(200);
	}

	l = c->line;

	if(l) {
		d = c->device;
		AST_LIST_REMOVE(&l->channels, c, list);
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Channel %d deleted from line %s\n", DEV_ID_LOG(d), c->callid, l ? l->name : "(null)");
	}

	if(channel_lock)
		sccp_channel_unlock(c);
	
	sccp_mutex_destroy(&c->lock);
	ast_free(c);

	return;
}


/*!
 * \brief Create a new RTP Source.
 * \param c SCCP Channel
 * \todo Add Video Capability
 */
void sccp_channel_start_rtp(sccp_channel_t * c)
{
	sccp_session_t * s;
	sccp_line_t * l = NULL;
	sccp_device_t * d = NULL;
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif
	boolean_t isVideoSupported = FALSE;
	char pref_buf[128];

	if (!c)
		return;

	if (c->line)
		l = c->line;

	if(l)
		d = c->device;

	if(d)
		s = d->session;
	else
		return;

	sccp_device_lock(d);
	isVideoSupported = sccp_device_isVideoSupported(d);
	sccp_device_unlock(d);

	sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: do we have video support? %s\n", d->id, isVideoSupported?"yes":"no");

/* No need to lock, because already locked in the sccp_indicate.c */
/*	sccp_channel_lock(c); */
#ifdef ASTERISK_CONF_1_2
	sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: Creating rtp server connection at %s\n", d->id, ast_inet_ntoa(iabuf, sizeof(iabuf), s->ourip));
#else
	sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: Creating rtp server connection at %s\n", d->id, ast_inet_ntoa(s->ourip));
#endif

	/* finally we deal with this -FS SVN 423*/
	c->rtp.audio = ast_rtp_new_with_bindaddr(sched, io, 1, 0, s->ourip);

	/* can we handle video */
	if(isVideoSupported)
		c->rtp.video = ast_rtp_new_with_bindaddr(sched, io, 1, 0, s->ourip);


#ifdef ASTERISK_CONF_1_2
	if (c->rtp.audio && c->owner)
		c->owner->fds[0] = ast_rtp_fd(c->rtp.audio);

	if(isVideoSupported && c->rtp.video && c->owner)
		c->owner->fds[0] = ast_rtp_fd(c->rtp.video);
#endif

#if ASTERISK_VERSION_NUM >= 10400
#if ASTERISK_VERSION_NUM < 10600
//#ifdef ASTERISK_CONF_1_4
	if (c->rtp.audio && c->owner) {
		c->owner->fds[0] = ast_rtp_fd(c->rtp.audio);
		c->owner->fds[1] = ast_rtcp_fd(c->rtp.audio);
	}

	if (isVideoSupported && c->rtp.video && c->owner) {
		c->owner->fds[2] = ast_rtp_fd(c->rtp.video);
		c->owner->fds[3] = ast_rtcp_fd(c->rtp.video);
	}
#else


	if (c->rtp.audio && c->owner) {
		ast_channel_set_fd(c->owner, 0, ast_rtp_fd(c->rtp.audio));
		ast_channel_set_fd(c->owner, 1, ast_rtcp_fd(c->rtp.audio));
	}

	if (isVideoSupported && c->rtp.video && c->owner) {
		ast_channel_set_fd(c->owner, 2, ast_rtp_fd(c->rtp.video));
		ast_channel_set_fd(c->owner, 3, ast_rtcp_fd(c->rtp.video));


		//sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: Creating video rtp server connection at %s:%d\n", d->id, ast_inet_ntoa(s->ourip), ntohs(.sin_port));
	}
#endif

	/* tell changes to asterisk */
	if((c->rtp.audio || c->rtp.video) && c->owner) {
		if(c->owner)
			ast_queue_frame(c->owner, &sccp_null_frame);
	}

	if(c->rtp.audio){
		ast_rtp_codec_setpref(c->rtp.audio, &c->codecs);
		ast_codec_pref_string(&c->codecs, pref_buf, sizeof(pref_buf) - 1);
		sccp_log(2)(VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, set pef: %s\n",
			    c->line->name,
			    c->callid,
			    pref_buf
		);
	}

#endif



	if (c->rtp.audio) {
#ifdef ASTERISK_CONF_1_6
//		ast_rtp_setqos(c->rtp.audio, c->line->rtptos, 5, "SCCP RTP");
		ast_rtp_setqos(c->rtp.audio, c->line->rtptos, c->line->rtpcos, "SCCP RTP");
#else
		ast_rtp_settos(c->rtp.audio, c->line->rtptos);
#if defined(linux)                                                              
                if (setsockopt(c->rtp.audio, SOL_SOCKET, SO_PRIORITY, c->line->rtpcos, sizeof(c->line->rtpcos))) < 0)  
                	ast_log(LOG_WARNING, "Failed to set SCCP socket COS to %d: %s\n", c->line->rtpcos, strerror(errno));
#endif
		
#endif
		ast_rtp_setnat(c->rtp.audio, d->nat);
#ifdef ASTERISK_CONF_1_6
		ast_rtp_codec_setpref(c->rtp.audio, &c->codecs);
#endif
	}

	if (c->rtp.video) {
#ifdef ASTERISK_CONF_1_6
		ast_rtp_setqos(c->rtp.video, 0, 6, "SCCP VRTP");
#else
		ast_rtp_settos(c->rtp.video, c->line->rtptos);
#endif
		ast_rtp_setnat(c->rtp.video, d->nat);

		ast_rtp_setdtmf(c->rtp.video, 0);
		ast_rtp_setdtmfcompensate(c->rtp.video, 0);
		ast_rtp_set_rtptimeout(c->rtp.video, 10);
		ast_rtp_set_rtpholdtimeout(c->rtp.video, 0);
		ast_rtp_set_rtpkeepalive(c->rtp.video, 0);
		//ast_rtp_set_constantssrc(c->rtp.video);
		//ast_rtp_set_constantssrc(c->rtp.video);
		ast_rtp_set_m_type(c->rtp.video, AST_FORMAT_H263);

	}

/*	sccp_channel_unlock(c); */
}


/*!
 * \brief Stop an RTP Source.
 * \param c SCCP Channel
 */
void sccp_channel_stop_rtp(sccp_channel_t * c)
{
	sccp_device_t * d = NULL;
	sccp_line_t * l = NULL;
	if(c && c->line) {
		l = c->line;
		d = c->device;
	}

	if (c->rtp.audio) {
		sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: Stopping phone media transmission on channel %s-%08X\n", (d && d->id)?d->id:"SCCP", l?l->name:"(null)", c->callid);
		ast_rtp_stop(c->rtp.audio);
	}

	if (c->rtp.video) {
		sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: Stopping video media transmission on channel %s-%08X\n", (d && d->id)?d->id:"SCCP", l?l->name:"(null)", c->callid);
		ast_rtp_stop(c->rtp.video);
	}
}

/*!
 * \brief Destroy RTP Source.
 * \param c SCCP Channel
 */
void sccp_channel_destroy_rtp(sccp_channel_t * c)
{
	sccp_device_t * d = NULL;
	sccp_line_t * l = NULL;
	if(c && c->line) {
		l = c->line;
		d = c->device;
	}

	if (c->rtp.audio) {
		sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: destroying phone media transmission on channel %s-%08X\n", (d && d->id)?d->id:"SCCP", l?l->name:"(null)", c->callid);
		ast_rtp_destroy(c->rtp.audio);
		c->rtp.audio = NULL;
	}

	if (c->rtp.video) {
		sccp_log(SCCP_VERBOSE_LEVEL_RTP)(VERBOSE_PREFIX_3 "%s: destroying video media transmission on channel %s-%08X\n", (d && d->id)?d->id:"SCCP", l?l->name:"(null)", c->callid);
		ast_rtp_destroy(c->rtp.video);
		c->rtp.video = NULL;
	}
}


/*!
 * \brief Handle Transfer Request (Pressing the Transfer Softkey)
 * \param c SCCP Channel
 */
void sccp_channel_transfer(sccp_channel_t * c)
{
	sccp_device_t * d;
	sccp_channel_t * newcall = NULL;
	uint32_t	blindTransfer = 0;
	uint8_t		instance;

	if (!c)
		return;

	if (!c->line || !c->device) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", c->callid);
		return;
	}

	d = c->device;

	if (!d->transfer || !c->line->transfer) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Transfer disabled on device or line\n", (d && d->id)?d->id:"SCCP");
		return;
	}

	sccp_device_lock(d);
	/* are we in the middle of a transfer? */
	if (d->transfer_channel && (d->transfer_channel != c)) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: In the middle of a Transfer. Going to transfer completion\n", (d && d->id)?d->id:"SCCP");
		sccp_device_unlock(d);
		sccp_channel_transfer_complete(c);
		return;
	}

	d->transfer_channel = c;
	sccp_device_unlock(d);
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Transfer request from line channel %s-%08X\n", (d && d->id)?d->id:"SCCP", (c->line && c->line->name)?c->line->name:"(null)", c->callid);

	if (!c->owner) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: No bridged channel to transfer on %s-%08X\n", (d && d->id)?d->id:"SCCP", (c->line && c->line->name)?c->line->name:"(null)", c->callid);
		instance = sccp_device_find_index_for_line(d, c->line->name);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}
	if ( (c->state != SCCP_CHANNELSTATE_HOLD && c->state != SCCP_CHANNELSTATE_CALLTRANSFER) && !sccp_channel_hold(c) )
		return;
	if (c->state != SCCP_CHANNELSTATE_CALLTRANSFER)
		sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_CALLTRANSFER);
	newcall = sccp_channel_newcall(c->line, d, NULL, SKINNY_CALLTYPE_OUTBOUND);
	/* set a var for BLINDTRANSFER. It will be removed if the user manually answer the call Otherwise it is a real BLINDTRANSFER*/
 	if ( blindTransfer || (newcall && newcall->owner && c->owner && CS_AST_BRIDGED_CHANNEL(c->owner)) ) {
		pbx_builtin_setvar_helper(newcall->owner, "BLINDTRANSFER", CS_AST_BRIDGED_CHANNEL(c->owner)->name);
		pbx_builtin_setvar_helper(CS_AST_BRIDGED_CHANNEL(c->owner), "BLINDTRANSFER", newcall->owner->name);
	}
}

/*!
 * \brief Handle Transfer Ringing Thread
 */
static void * sccp_channel_transfer_ringing_thread(void *data)
{
	char * name = data;
	struct ast_channel * ast;

	if (!name)
		return NULL;

	sleep(1);
	ast = ast_get_channel_by_name_locked(name);
	ast_free(name);

	if (!ast)
		return NULL;

        sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: (Ringing within Transfer %s(%p)\n", ast->name, ast);
	if (GLOB(blindtransferindication) == SCCP_BLINDTRANSFER_RING) {
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: (sccp_channel_transfer_ringing_thread) Send ringing indication to %s(%p)\n", ast->name, ast);
		ast_indicate(ast, AST_CONTROL_RINGING);
	}
	else if (GLOB(blindtransferindication) == SCCP_BLINDTRANSFER_MOH) {
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: (sccp_channel_transfer_ringing_thread) Started music on hold for channel %s(%p)\n", ast->name, ast);
#ifdef ASTERISK_CONF_1_2
		ast_moh_start(ast, NULL);
#else
		ast_moh_start(ast, NULL, NULL);
#endif
	}
	sccp_ast_channel_unlock(ast);
	return NULL;
}

/*!
 * \brief Bridge Two Channels
 * \param cDestinationLocal Local Destination SCCP Channel
 * \todo Find a way solve the chan->state problem
 */
void sccp_channel_transfer_complete(sccp_channel_t * cDestinationLocal) {
#ifndef CS_AST_CHANNEL_HAS_CID
	char *name, *number, *cidtmp;
#endif
	struct ast_channel	*astcSourceRemote = NULL, *astcDestinationLocal = NULL, *astcDestinationRemote = NULL;
	sccp_channel_t * cSourceLocal;
	sccp_device_t * d = NULL;
	pthread_attr_t attr;
	pthread_t t;
	uint8_t			instance;

	if (!cDestinationLocal)
		return;

	if (!cDestinationLocal->line || !cDestinationLocal->device) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", cDestinationLocal->callid);
		return;
	}

	// Obtain the device from which the transfer was initiated
	d = cDestinationLocal->device;
	sccp_device_lock(d);
	// Obtain the source channel on that device
	cSourceLocal = d->transfer_channel;
	sccp_device_unlock(d);

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Complete transfer from %s-%08X\n", d->id, cDestinationLocal->line->name, cDestinationLocal->callid);
	instance = sccp_device_find_index_for_line(d, cDestinationLocal->line->name);

	if (cDestinationLocal->state != SCCP_CHANNELSTATE_RINGOUT && cDestinationLocal->state != SCCP_CHANNELSTATE_CONNECTED) {
		ast_log(LOG_WARNING, "SCCP: Failed to complete transfer. The channel is not ringing or connected\n");
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, instance, cDestinationLocal->callid, 0);
		sccp_dev_displayprompt(d, instance, cDestinationLocal->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}

	if (!cDestinationLocal->owner || !cSourceLocal->owner) {
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Transfer error, asterisk channel error %s-%08X and %s-%08X\n", d->id, cDestinationLocal->line->name, cDestinationLocal->callid, cSourceLocal->line->name, cSourceLocal->callid);
		return;
	}

	astcSourceRemote = CS_AST_BRIDGED_CHANNEL(cSourceLocal->owner);
	astcDestinationRemote = CS_AST_BRIDGED_CHANNEL(cDestinationLocal->owner);
	astcDestinationLocal = cDestinationLocal->owner;
/*
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: transferred: %s(%p)\npeer->owner: %s(%p)\ndestination: %s(%p)\nc->owner:%s(%p)\n", d->id,
								(transferred&&transferred_name)?transferred->name:"", transferred?transferred:0x0,
								(peer && peer->owner && peer->owner->name)?peer->owner->name:"", (peer && peer->owner)?peer->owner:0x0,
								(destination && destination->name)?destination->name:"", destination?destination:0x0,
								(c && c->owner && c->owner->name)?c->owner->name:"", (c && c->owner)?c->owner:0x0);
*/
	if (!astcSourceRemote || !astcDestinationLocal) {
		ast_log(LOG_WARNING, "SCCP: Failed to complete transfer. Missing asterisk transferred or transferee channel\n");

		sccp_dev_displayprompt(d, instance, cDestinationLocal->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}

	if (cDestinationLocal->state == SCCP_CHANNELSTATE_RINGOUT) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Blind transfer. Signalling ringing state to %s\n", d->id, astcSourceRemote->name);
		ast_indicate(astcSourceRemote, AST_CONTROL_RINGING); // Shouldn't this be ALERTING?
		/* starting the ringing thread */
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		if (ast_pthread_create(&t, &attr, sccp_channel_transfer_ringing_thread, strdup(astcSourceRemote->name))) {
			ast_log(LOG_WARNING, "%s: Unable to create thread for the blind transfer ring indication. %s\n", d->id, strerror(errno));
		}
	}
	if (ast_channel_masquerade(astcDestinationLocal, astcSourceRemote)) {
		ast_log(LOG_WARNING, "SCCP: Failed to masquerade %s into %s\n", astcDestinationLocal->name, astcSourceRemote->name);

		sccp_dev_displayprompt(d, instance, cDestinationLocal->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}

	if (cDestinationLocal->state == SCCP_CHANNELSTATE_RINGOUT) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Blind transfer. Signalling ringing state to %s\n", d->id, astcSourceRemote->name);
	}

	if (!cSourceLocal->owner){
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Peer owner disappeared! Can't free ressources\n");
		return;
	}

	sccp_device_lock(d);
	d->transfer_channel = NULL;
	sccp_device_unlock(d);

	if (!astcDestinationRemote) {
	    /* the channel was ringing not answered yet. BLIND TRANSFER */
// TEST
//		if(cDestinationLocal->rtp)
//			sccp_channel_destroy_rtp(cDestinationLocal);
        return;
    }

#ifndef CS_AST_HAS_TECH_PVT
	if (strncasecmp(astcDestinationRemote->type,"SCCP",4)) {
#else
	if (strncasecmp(astcDestinationRemote->tech->type,"SCCP",4)) {
#endif
		return;
	}

	/* it's a SCCP channel destination on transfer */
	cDestinationLocal = CS_AST_CHANNEL_PVT(astcDestinationRemote);

	if (cDestinationLocal) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Transfer confirmation destination on channel %s\n", d->id, astcDestinationRemote->name);
		/* display the transferred CID info to destination */
#ifdef CS_AST_CHANNEL_HAS_CID
		sccp_channel_set_callingparty(cDestinationLocal, astcSourceRemote->cid.cid_name, astcSourceRemote->cid.cid_num);
#else
		if (astcSourceRemote->callerid && (cidtmp = strdup(astcSourceRemote->callerid))) {
			ast_callerid_parse(cidtmp, &name, &number);
			sccp_channel_set_callingparty(cDestinationLocal, name, number);
			if(cidtmp)
				ast_free(cidtmp);
			if(name)
				ast_free(name);
			if(number)
				ast_free(number);
		}
#endif
		sccp_channel_send_callinfo(d, cDestinationLocal);
		if (GLOB(transfer_tone) && cDestinationLocal->state == SCCP_CHANNELSTATE_CONNECTED)
			/* while connected not all the tones can be played */

			sccp_dev_starttone(cDestinationLocal->device, GLOB(autoanswer_tone), instance, cDestinationLocal->callid, 0);
	}
}

void sccp_channel_forward(sccp_channel_t *parent, sccp_linedevices_t *lineDevice, char *fwdNumber){
	sccp_channel_t 	*forwarder = NULL;
	char 		dialedNumber[256];

	if(!parent){
		ast_log(LOG_ERROR, "We can not forward a call without parent channel\n");
		return;
	}


	sccp_copy_string(dialedNumber, fwdNumber, sizeof(dialedNumber));


	forwarder = sccp_channel_allocate(parent->line, NULL);

	if (!forwarder) {
		ast_log(LOG_ERROR, "%s: Can't allocate SCCP channel\n", lineDevice->device->id);
		return;
	}

	sccp_channel_lock(forwarder);

	forwarder->parentChannel = parent;
	forwarder->ss_action = SCCP_SS_DIAL; /* softswitch will catch the number to be dialed */
	forwarder->ss_data = 0; // nothing to pass to action

	forwarder->calltype = SKINNY_CALLTYPE_OUTBOUND;


	/* copy the number to dial in the ast->exten */

	sccp_copy_string(forwarder->dialedNumber, dialedNumber, sizeof(forwarder->dialedNumber));
	sccp_channel_unlock(forwarder);

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(forwarder)) {
		ast_log(LOG_WARNING, "%s: Unable to allocate a new channel for line %s\n", lineDevice->device->id, forwarder->line->name);

		//TODO cleanup allocation
		sccp_channel_cleanbeforedelete(forwarder);
		ast_free(forwarder);
	}

	sccp_copy_string(forwarder->owner->exten, dialedNumber, sizeof(forwarder->owner->exten));
	sccp_ast_setstate(forwarder, AST_STATE_OFFHOOK);
	if (!ast_strlen_zero(dialedNumber) && !ast_check_hangup(forwarder->owner)
			&& ast_exists_extension(forwarder->owner, forwarder->line->context, dialedNumber, 1, forwarder->line->cid_num) ) {
		/* found an extension, let's dial it */
		ast_log(LOG_NOTICE, "%s: (sccp_channel_forward) channel %s-%08x is dialing number %s\n", "SCCP", forwarder->line->name, forwarder->callid, strdup(dialedNumber));
		/* Answer dialplan command works only when in RINGING OR RING ast_state */
		sccp_ast_setstate(forwarder, AST_STATE_RING);
		if (ast_pbx_start(forwarder->owner)) {
			ast_log(LOG_WARNING, "%s: invalide number\n", "SCCP");
		}
	}
}


#ifdef CS_SCCP_PARK

/*!
 * \brief Dual Structure
 */

struct sccp_dual {

        struct ast_channel *chan1;

        struct ast_channel *chan2;
};

/*!
 * \brief Channel Park Thread
 * \param stuff Stuff
 * \todo Some work to do i guess
 * \todo replace parameter stuff with something sensable
 */
static void * sccp_channel_park_thread(void *stuff) {
	struct ast_channel *chan1, *chan2;
	struct sccp_dual *dual;
	struct ast_frame *f;
	int ext;
	int res;
	char extstr[20];
	sccp_channel_t * c;
	memset(&extstr, 0 , sizeof(extstr));

	dual = stuff;
	chan1 = dual->chan1;
	chan2 = dual->chan2;
	ast_free(dual);
	f = ast_read(chan1);
	if (f)
		ast_frfree(f);
	res = ast_park_call(chan1, chan2, 0, &ext);
	if (!res) {
		extstr[0] = 128;
		extstr[1] = SKINNY_LBL_CALL_PARK_AT;
		sprintf(&extstr[2]," %d",ext);
		c = CS_AST_CHANNEL_PVT(chan2);
		sccp_dev_displaynotify(c->device, extstr, 10);
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Parked channel %s on %d\n", DEV_ID_LOG(c->device), chan1->name, ext);
	}
	ast_hangup(chan2);
	return NULL;
}


/*!
 * \brief Park an SCCP Channel
 * \param c SCCP Channel
 */
void sccp_channel_park(sccp_channel_t * c) {
	sccp_device_t * d;
	sccp_line_t      * l;
	struct sccp_dual *dual;
	struct ast_channel *chan1m, *chan2m, *bridged;
	pthread_t th;
	uint8_t		instance;

	if (!c)
		return;

	d = c->device;
	l = c->line;
	if (!d)
		return;

	if (!d->park) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Park disabled on device\n", d->id);
		return;
	}

	if (!c->owner) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Can't Park: no asterisk channel\n", d->id);
		return;
	}
	bridged = CS_AST_BRIDGED_CHANNEL(c->owner);
	if (!bridged) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Can't Park: no asterisk bridged channel\n", d->id);
		return;
	}
	sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_CALLPARK);

#ifdef ASTERISK_CONF_1_2
	chan1m = ast_channel_alloc(0);
#else
    /* This should definetly fix CDR */
    chan1m = ast_channel_alloc(0, AST_STATE_DOWN, l->cid_num, l->cid_name, l->accountcode, c->dialedNumber, l->context, l->amaflags, "SCCP/%s-%08X", l->name, c->callid);
#endif
       // chan1m = ast_channel_alloc(0); function changed in 1.4.0
       // Assuming AST_STATE_DOWN is suitable.. need to check
	if (!chan1m) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Park Failed: can't create asterisk channel\n", d->id);

		instance = sccp_device_find_index_for_line(c->device, c->line->name);
		sccp_dev_displayprompt(c->device, instance, c->callid, SKINNY_DISP_NO_PARK_NUMBER_AVAILABLE, 0);
		return;
	}
#ifdef ASTERISK_CONF_1_2
	chan2m = ast_channel_alloc(0);
#else
	// chan2m = ast_channel_alloc(0, AST_STATE_DOWN, l->cid_num, l->cid_name, "SCCP/%s", l->name,  NULL, 0, NULL);
    /* This should definetly fix CDR */
    chan2m = ast_channel_alloc(0, AST_STATE_DOWN, l->cid_num, l->cid_name, l->accountcode, c->dialedNumber, l->context, l->amaflags, "SCCP/%s-%08X", l->name, c->callid);
#endif
       // chan2m = ast_channel_alloc(0); function changed in 1.4.0
       // Assuming AST_STATE_DOWN is suitable.. need to check
	if (!chan2m) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Park Failed: can't create asterisk channel\n", d->id);

		instance = sccp_device_find_index_for_line(c->device, c->line->name);
		sccp_dev_displayprompt(c->device, instance, c->callid, SKINNY_DISP_NO_PARK_NUMBER_AVAILABLE, 0);
		ast_hangup(chan1m);
		return;
	}

#ifdef CS_AST_HAS_AST_STRING_FIELD
	ast_string_field_build(chan1m, name, "Parking/%s", bridged->name);
#else
	snprintf(chan1m->name, sizeof(chan1m->name), "Parking/%s", bridged->name);
#endif
	//snprintf(chan1m->name, sizeof(chan1m->name), "Parking/%s", bridged->name);/
	/* Make formats okay */
	chan1m->readformat = bridged->readformat;
	chan1m->writeformat = bridged->writeformat;
	ast_channel_masquerade(chan1m, bridged);
	/* Setup the extensions and such */
	sccp_copy_string(chan1m->context, bridged->context, sizeof(chan1m->context));
	sccp_copy_string(chan1m->exten, bridged->exten, sizeof(chan1m->exten));
	chan1m->priority = bridged->priority;

	/* We make a clone of the peer channel too, so we can play
	   back the announcement */
#ifdef CS_AST_HAS_AST_STRING_FIELD
	ast_string_field_build(chan2m, name, "SCCPParking/%s", c->owner->name);
#else
	snprintf(chan2m->name, sizeof(chan2m->name), "SCCPParking/%s", c->owner->name);
#endif
	//snprintf(chan2m->name, sizeof (chan2m->name), "SCCPParking/%s",c->owner->name);

	/* Make formats okay */
	chan2m->readformat = c->owner->readformat;
	chan2m->writeformat = c->owner->writeformat;
	ast_channel_masquerade(chan2m, c->owner);
	/* Setup the extensions and such */
	sccp_copy_string(chan2m->context, c->owner->context, sizeof(chan2m->context));
	sccp_copy_string(chan2m->exten, c->owner->exten, sizeof(chan2m->exten));
	chan2m->priority = c->owner->priority;
	if (ast_do_masquerade(chan2m)) {
		ast_log(LOG_WARNING, "SCCP: Masquerade failed :(\n");
		ast_hangup(chan2m);
		return;
	}

	dual = ast_malloc(sizeof(struct sccp_dual));
	if (dual) {
		memset(d, 0, sizeof(*dual));
		dual->chan1 = chan1m;
		dual->chan2 = chan2m;
		if (!ast_pthread_create(&th, NULL, sccp_channel_park_thread, dual))
			return;
		ast_free(dual);
	}
}




#endif

