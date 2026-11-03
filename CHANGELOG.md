# Changelog

## [4.4.0](https://github.com/mpaland/chan-sccp/compare/v4.3.5...v4.4.0) (2026-11-03)


### Bug Fixes

* (work around) FreePBX sometimes goes from DIALING-&gt;AST_CONTROL_PROGRESS skipping RINGING (for EarlyRTP=off) ([51dc094](https://github.com/mpaland/chan-sccp/commit/51dc094d0ab537033d49b4d2e24f2a6c435cfa73))
* accept_thread cancellation during module reload/unload ([eae0015](https://github.com/mpaland/chan-sccp/commit/eae0015252286d8c22c7efb4f5074b76493cf6e6))
* Add dtmf_passthrough feature to conference ([e2f37a7](https://github.com/mpaland/chan-sccp/commit/e2f37a772fcee1a6c516e3d30e948ae701ccfe93))
* Add Explicit Switch Fallthrough ([b6cc292](https://github.com/mpaland/chan-sccp/commit/b6cc2923262e40425075cecad46f20ff43f73b7d))
* ami config metadata ([dd45429](https://github.com/mpaland/chan-sccp/commit/dd45429cd19b6f02363f36efc2e14df905a29dd9))
* AMI table output when ActionID is requested ([f637066](https://github.com/mpaland/chan-sccp/commit/f637066ae84c0498d32925f5c6ac92555ddecc10))
* Apply netsock_cmp_port patch from Salvador ([5f91177](https://github.com/mpaland/chan-sccp/commit/5f91177c233f1dacdbd3014271038376ca8148c9))
* asterisk-13/asterisk-14 video calls ([162a57c](https://github.com/mpaland/chan-sccp/commit/162a57ce39dca18e186f9e803143342bc8e4cbc5)), closes [#431](https://github.com/mpaland/chan-sccp/issues/431) [#328](https://github.com/mpaland/chan-sccp/issues/328) [#244](https://github.com/mpaland/chan-sccp/issues/244) [#92](https://github.com/mpaland/chan-sccp/issues/92) [#402](https://github.com/mpaland/chan-sccp/issues/402)
* CallInfo:Presentation to CallInfo:partyPIRestrictionBits for ProtocolLevel:3 messages ([87900f7](https://github.com/mpaland/chan-sccp/commit/87900f741a9770ace7e66e6d0082074804ceac29)), closes [#423](https://github.com/mpaland/chan-sccp/issues/423)
* Calling function process_buffer taints argument msg.header. (Coverity) ([31c25a8](https://github.com/mpaland/chan-sccp/commit/31c25a89d4a288696aee78edced32125e2190450))
* callstate offhook before connected during answer (for 7911) ([7d2ea9f](https://github.com/mpaland/chan-sccp/commit/7d2ea9f118db6a98d41175e9d17da61de9fba708))
* channel unref in pbx_retrieve_remote_capabilities when breaking out of the iterator ([3377cec](https://github.com/mpaland/chan-sccp/commit/3377cec53f3941e144b69116f8fa62e151b92e77))
* **channel:** reference leaks, double frees and NULL dereferences in call setup ([b3a83ec](https://github.com/mpaland/chan-sccp/commit/b3a83ec4321e15f22f7714cf667188503730d07e))
* Check NULL Pointer after return from REQ()/build_packet ([4a23494](https://github.com/mpaland/chan-sccp/commit/4a23494e5d15d53e064a212a0cfeb8745ead66db))
* Check NULL Pointer after return from REQ()/build_packet ([7c2629f](https://github.com/mpaland/chan-sccp/commit/7c2629fc90cf11b9cc28287e07c2194784ba2006))
* clang-tidy 'google-readability-casting' ([00ac54f](https://github.com/mpaland/chan-sccp/commit/00ac54f8d8223df6d97671ed7092e9dfee14a079))
* clang-tidy bugprone-suspicious-string-compare ([4d6af02](https://github.com/mpaland/chan-sccp/commit/4d6af02fbee94bf3662fa5787ca6b0151e21e057))
* cli device ringtone completion ([483971e](https://github.com/mpaland/chan-sccp/commit/483971e895979a44ce6a5bb4b5a045312c166d1f))
* CLI:`module reload` causing corruption of GLOB(config_file_name) ([5f5362d](https://github.com/mpaland/chan-sccp/commit/5f5362ddac054f3a5974ea9b2653b934d2e9e041))
* compilation on FreeBSD ([a61c6d2](https://github.com/mpaland/chan-sccp/commit/a61c6d2cecdd149db767307cd71b736e77913251))
* compilation with --disable-park ([10b0628](https://github.com/mpaland/chan-sccp/commit/10b0628696793a3b0b26ca45ffb7b6812f9528e7)), closes [#534](https://github.com/mpaland/chan-sccp/issues/534) [#535](https://github.com/mpaland/chan-sccp/issues/535)
* compile warning clang-13 ([fb6ef15](https://github.com/mpaland/chan-sccp/commit/fb6ef15c8ac2f8bdadbc37f067252c06c1dca395))
* compiler error-warning about a duplicate typedef of servercontext declaration, which should only have been a forward declaration. ([91d4f25](https://github.com/mpaland/chan-sccp/commit/91d4f25f5cc5c0d5d5a0c3fdf819fa7535286398)), closes [#563](https://github.com/mpaland/chan-sccp/issues/563)
* compiler warning: "cast increases required alignment of target type" ([9bd1fbe](https://github.com/mpaland/chan-sccp/commit/9bd1fbe363dfbd08b49f502cd57ced03420267a7))
* conf/sqlite3.sql ([5d23411](https://github.com/mpaland/chan-sccp/commit/5d23411a22b4e787f57fcfb3039458d9a713bf15))
* config generate close(fd) issue reported by coverity ([ea1c695](https://github.com/mpaland/chan-sccp/commit/ea1c695690b7caf3fb4cb86447e86dc899c3d658))
* **config:** memory errors and a self-deadlock in the reload path ([fc5b200](https://github.com/mpaland/chan-sccp/commit/fc5b2002d14955fa95461126ed301264e86c958c))
* configure asterisk version detection ([64f2e39](https://github.com/mpaland/chan-sccp/commit/64f2e39d8aa3c0e906a620717cb8fa2a3cb1326c))
* continue if iconv/gettext not supported on the local platform ([066a14a](https://github.com/mpaland/chan-sccp/commit/066a14a1d081fdba52cb30f87a9c760d336e8ae9))
* Correct cXML when no callerid_name is available on an outbound parked call ([27225fa](https://github.com/mpaland/chan-sccp/commit/27225fa730167f527d6a43bbb4aae9f14eeb25b2))
* defaultLineInstance getting set by accident, if parkingLotFeature button is used, with a "default" option parameter ([cdd1fd1](https://github.com/mpaland/chan-sccp/commit/cdd1fd134ee99da6a60ff5ec633627af5b91ff55))
* devstate_cb checking hint channel states ([d02d1d5](https://github.com/mpaland/chan-sccp/commit/d02d1d50d18caefcc2505d779984d277d97cf2a3))
* Dialing parked extension shows 701 &lt;=&gt; Alice in a speed dial when Alice called parked extension. ([d02d1d5](https://github.com/mpaland/chan-sccp/commit/d02d1d50d18caefcc2505d779984d277d97cf2a3))
* Do not update the featureLabel when the hint is on a 7914 addon ([5cb59cf](https://github.com/mpaland/chan-sccp/commit/5cb59cf95ce18a23a46e0231aa7970964c1f3844))
* Don't exit configure if we currently cannot find openssl-devel libraries ([76ca45d](https://github.com/mpaland/chan-sccp/commit/76ca45d3fefe34815c639e4a2edc93adc7707464))
* dynamic phonebook update. Converting ALERTING -&gt; INUSE ([c5c56f4](https://github.com/mpaland/chan-sccp/commit/c5c56f4688b391868aa317e72566bdfd02bfc8ae))
* empty line::mailbox causes device restart on sccp reload ([d7a7ae6](https://github.com/mpaland/chan-sccp/commit/d7a7ae6a5cd67831e4746a63949d1f67b4e4fe77))
* Extend aclocal.m4 iconv detection ([066a14a](https://github.com/mpaland/chan-sccp/commit/066a14a1d081fdba52cb30f87a9c760d336e8ae9)), closes [#551](https://github.com/mpaland/chan-sccp/issues/551)
* Getting the btnlist to include the devicetype, so we can use this information in make_button_template ([5cb59cf](https://github.com/mpaland/chan-sccp/commit/5cb59cf95ce18a23a46e0231aa7970964c1f3844)), closes [#373](https://github.com/mpaland/chan-sccp/issues/373)
* hint shortcut making it update the hint-&gt;previousState and hint-&gt;currentState ([d02d1d5](https://github.com/mpaland/chan-sccp/commit/d02d1d50d18caefcc2505d779984d277d97cf2a3)), closes [#533](https://github.com/mpaland/chan-sccp/issues/533)
* hint when handling lineStatusChanged event with multiple channels ringing ([2007813](https://github.com/mpaland/chan-sccp/commit/200781339d8e0653c388e66ad5386a441796875f)), closes [#533](https://github.com/mpaland/chan-sccp/issues/533)
* **hint,mwi,devstate:** double free, skipped unsubscribes and a stack overrun ([934889d](https://github.com/mpaland/chan-sccp/commit/934889dce22bf51ea1c3c724340b1d5f2e9673cc))
* Incoming Video on asterisk-13 and lower ([49f306c](https://github.com/mpaland/chan-sccp/commit/49f306cad87233bda8c930c345a34c8a7fd74f3a)), closes [#431](https://github.com/mpaland/chan-sccp/issues/431) [#328](https://github.com/mpaland/chan-sccp/issues/328) [#244](https://github.com/mpaland/chan-sccp/issues/244) [#92](https://github.com/mpaland/chan-sccp/issues/92) [#402](https://github.com/mpaland/chan-sccp/issues/402)
* Make sure d-&gt;protocol is always set ([9b8357b](https://github.com/mpaland/chan-sccp/commit/9b8357b6150bdfc16064ec9f3125a3ea428c217e)), closes [#550](https://github.com/mpaland/chan-sccp/issues/550)
* minor compiler warning ([4d4927a](https://github.com/mpaland/chan-sccp/commit/4d4927a29feb7ad5f4c095ee1b84bce7fc96b88a))
* missing lock in ast-13's astwrap_hangup. ([7d2ea9f](https://github.com/mpaland/chan-sccp/commit/7d2ea9f118db6a98d41175e9d17da61de9fba708))
* module reload chan_sccp potentially corrupting config filename to be loaded ([81f140a](https://github.com/mpaland/chan-sccp/commit/81f140a736044eba26102c55f11509b5a881582f))
* **module:** safe unload ordering and failed-load cleanup ([c822984](https://github.com/mpaland/chan-sccp/commit/c822984243ee840c60fab00c2b68faa9ef413c42))
* Move AST_STATE_UP to sccp_pbx_answer ([7d2ea9f](https://github.com/mpaland/chan-sccp/commit/7d2ea9f118db6a98d41175e9d17da61de9fba708)), closes [#503](https://github.com/mpaland/chan-sccp/issues/503)
* null pointer dereference in sccp_astwrap_rtp_write ([7356648](https://github.com/mpaland/chan-sccp/commit/73566485deb131ff7d5837afa1dfcfedf5641347))
* partial revert of cdd1fd134 ([5f4259e](https://github.com/mpaland/chan-sccp/commit/5f4259e862837db1a5e9a9e462b7a16abb7cb8fe))
* Passing tainted expression recv_buffer to process_buffer, which uses it as a loop boundary. (Coverity) ([bbeed39](https://github.com/mpaland/chan-sccp/commit/bbeed39736124dd5d002fed0dc55cf0d76f3a358))
* **pbx:** Asterisk object reference leaks and a locked-channel exit ([13ab344](https://github.com/mpaland/chan-sccp/commit/13ab34481ea78c330288837488b8244362c3c0da))
* potential null pointer dereference (coverity) ([80236f6](https://github.com/mpaland/chan-sccp/commit/80236f66f775041bbcaf326e18b7ca093af41c45))
* Potential Null Pointer Dereferences ([7c2629f](https://github.com/mpaland/chan-sccp/commit/7c2629fc90cf11b9cc28287e07c2194784ba2006))
* Prevent a call we are trying to answer to be put on hold. ([3cd2c2a](https://github.com/mpaland/chan-sccp/commit/3cd2c2a4d368d8f8ac486ed90935a60fbed088dc))
* prevent segfault on checkForDND processing ([9642a64](https://github.com/mpaland/chan-sccp/commit/9642a6490d8bffcada56dc2edf7b89c46436b3f0))
* put 'transfer' softkey back into the 'CONNECTED' softkey set ([bdd2590](https://github.com/mpaland/chan-sccp/commit/bdd25901b6f2dcc85111e8d04700d7f4373c831d))
* Put preexisting call on hold, when answering ([7d2ea9f](https://github.com/mpaland/chan-sccp/commit/7d2ea9f118db6a98d41175e9d17da61de9fba708))
* race condition when shared lines answer at the some time ([7d2ea9f](https://github.com/mpaland/chan-sccp/commit/7d2ea9f118db6a98d41175e9d17da61de9fba708))
* refcount order issue during line releasing ([14be0a3](https://github.com/mpaland/chan-sccp/commit/14be0a3ef770b1db0425caca7238d39fcf2800cc))
* remark out unused variable ([781865e](https://github.com/mpaland/chan-sccp/commit/781865e2abf67a7e4757341e0eed3f0c8500f606))
* Remove extreneous logging regarding receipt of empty frames ([e0cb339](https://github.com/mpaland/chan-sccp/commit/e0cb339a121e98d44564bf70776742d2cc4699aa)), closes [#558](https://github.com/mpaland/chan-sccp/issues/558)
* rename transport accept/close callbacks, fix sccp_dev_starttone prototype ([e4b4712](https://github.com/mpaland/chan-sccp/commit/e4b4712985a31e8dca96a04cea8aea8d2f2588cf))
* reset dialtone to l-&gt;initial_dialtone_tone when processing backspace ([8f16e01](https://github.com/mpaland/chan-sccp/commit/8f16e019c7b813354ffb9bcd9a338dafda817c61))
* Resource Leak ([4a23494](https://github.com/mpaland/chan-sccp/commit/4a23494e5d15d53e064a212a0cfeb8745ead66db))
* Revert accidental checkin ([d927bd5](https://github.com/mpaland/chan-sccp/commit/d927bd5ef22f979aa4735e3272205175f4ce35ae))
* Reverted accidental merge, causing trouble with the 'CONNECTED' softkeyset ([ad4e0de](https://github.com/mpaland/chan-sccp/commit/ad4e0deae676ef97525eada867a8ba6a7dc2220a))
* **rtp:** register tx/rx payload mappings for all negotiable codecs ([b8127be](https://github.com/mpaland/chan-sccp/commit/b8127beeca8bf161616d7f8332890782e62c6e38))
* sccp reload causing reload of all devices using feature buttons after devstate change ([8f087d6](https://github.com/mpaland/chan-sccp/commit/8f087d6b6a7d60cc0af1c68df0152627e4acd531))
* sccp_channel_stop_schedule_cfwd_noanswer ([ea2e254](https://github.com/mpaland/chan-sccp/commit/ea2e25474854ec724b080fb19d0acf146afd1551))
* sccp_channel_stop_schedule_digittimout ([ea2e254](https://github.com/mpaland/chan-sccp/commit/ea2e25474854ec724b080fb19d0acf146afd1551)), closes [#526](https://github.com/mpaland/chan-sccp/issues/526)
* session lock initialization ([6de386c](https://github.com/mpaland/chan-sccp/commit/6de386c9797b51315f1efd5b0ff840cbf187a059))
* **session:** message, context and job leaks on error paths ([0abb159](https://github.com/mpaland/chan-sccp/commit/0abb1590221f54e29cd8763b77ca91326404475e))
* Setting using c-&gt;pbx_callid when calling pbx_channel_callid_set ([ff3962b](https://github.com/mpaland/chan-sccp/commit/ff3962ba5b0b2cda5cb17c05430ab49b64e9e615))
* shared line hold/resume ([c8af12d](https://github.com/mpaland/chan-sccp/commit/c8af12d4fc5866a9521b35d8c9ee37024b2cbffe))
* Simple solution to the second issue on [#533](https://github.com/mpaland/chan-sccp/issues/533) ([26b79e6](https://github.com/mpaland/chan-sccp/commit/26b79e6af2fe7384ac1bb2123920c6a70c8118b6))
* stack overflow issue in AMI Table generation ([16fd7da](https://github.com/mpaland/chan-sccp/commit/16fd7da3623e33797d205dcde9331b4b728d5d04)), closes [#559](https://github.com/mpaland/chan-sccp/issues/559) [#560](https://github.com/mpaland/chan-sccp/issues/560)
* Switch travis.org to travis.com, because travis.org is shutting down ([ec5436b](https://github.com/mpaland/chan-sccp/commit/ec5436bbf64a5f20076731f4f56809eb8253554a))
* Taking phone top status bar description from default line description if/when device description has not been provided ([c9cd0c3](https://github.com/mpaland/chan-sccp/commit/c9cd0c357cf48088c4543d3e9c0a68c78006c569))
* typo ([a07da4b](https://github.com/mpaland/chan-sccp/commit/a07da4b87d32a816b27d35f80a0086ecf3c89161))
* typo 'sccp show softkeyssets' -&gt; 'sccp show softkeysets' ([d6411af](https://github.com/mpaland/chan-sccp/commit/d6411af30e6ebf4557b5f27e88875ddbfa2c28d7))
* Update gitter webhook for travis.com ([cdb31a2](https://github.com/mpaland/chan-sccp/commit/cdb31a232b05d0ceb7f951a320e2b82422a34e1a))
* use-after-free of config_file_name on 'sccp reload line' ([c030f3e](https://github.com/mpaland/chan-sccp/commit/c030f3e40c46270068010628011d03be40cf864e))
* Visual ParkingLot Feature, issue reported by [@darkponay](https://github.com/darkponay). ([1123fce](https://github.com/mpaland/chan-sccp/commit/1123fce9e0376cae561e82e357a8e7c4335356cc))
* Wait until the first non-null packets are getting through to rtp_read before stopping holePunching. ([3a50480](https://github.com/mpaland/chan-sccp/commit/3a50480633d9b50bee73c3557b319fbcc0c9c6e2))
* When cleaning/destroying up a channel/line or device, we should force events to be handled synchronously, so action destruction cannot surpas the event being handled ([76000de](https://github.com/mpaland/chan-sccp/commit/76000de2f3c9e61c1ceefbd3e8fee4772ddb6267))


### Code Refactoring

* **lifetime:** guaranteed break points for device/channel/line reference cycles ([a89c1b7](https://github.com/mpaland/chan-sccp/commit/a89c1b78625bb350a8dc4f1c2b13b12610e9f738))
* **locking:** never call out of a list lock, fix the lock order inversions ([6fafe40](https://github.com/mpaland/chan-sccp/commit/6fafe40856527a8e63bf9837611fe44517cf626f))
* **pbx:** masqueradeHelper never consumes caller references ([c3e4051](https://github.com/mpaland/chan-sccp/commit/c3e405125576e4d1cdc87cbbfe0d76610e38071d))
* **refcount:** lock-free core on compiler atomics, no resurrection, static buckets ([42a880f](https://github.com/mpaland/chan-sccp/commit/42a880f92e7932691333c7a70b053727f526b9c7))
* **rtp:** finish the NAT hole punch state machine ([325af16](https://github.com/mpaland/chan-sccp/commit/325af1616305e9ea04b2ca8828721d56ddc397e5))
* **session:** reference counted sessions, cooperative thread shutdown ([299df4c](https://github.com/mpaland/chan-sccp/commit/299df4ca5e173f718d3f44a7fd185e7ee9362981))
* **threadpool:** joinable workers, no cancellation, jobs released on shutdown ([419737a](https://github.com/mpaland/chan-sccp/commit/419737a4a515d98e4b2c3f0e26aaa8ec88f59232))
* **xml:** applyStyleSheet returns the transformed document, no per-request libxml global cleanup ([e1e279a](https://github.com/mpaland/chan-sccp/commit/e1e279aa626d3988394cb47d2d19f16f0a13d8e0))


### Build System

* drop configure probes that are constant on Asterisk &gt;= 20 ([4441682](https://github.com/mpaland/chan-sccp/commit/44416825ef9fc5aeaf64649c145ebdf583755761))
* fixes from the first Asterisk 22 build of the new layout ([60af31c](https://github.com/mpaland/chan-sccp/commit/60af31c5c5c8d69850869c14691cd74716e6c256))
* regenerate autotools artifacts ([83676a7](https://github.com/mpaland/chan-sccp/commit/83676a7de952533eac254ddc2687868e99ab4b3f))
* require Asterisk &gt;= 20, detect the major version from AMI_VERSION ([6fb6540](https://github.com/mpaland/chan-sccp/commit/6fb65405aa57f4faa028ea198544799b39a2fb4b))
* single Asterisk glue implementation in src/pbx_impl/ast ([f1e1c4f](https://github.com/mpaland/chan-sccp/commit/f1e1c4f21a6dfb89f82b7704fd072437d668f835))


### Continuous Integration

* add release-please workflow ([73f5804](https://github.com/mpaland/chan-sccp/commit/73f5804aa3dd1d3079eb55ba008849ac16bf6d94))
* update GitHub workflows to current action versions ([9302f0f](https://github.com/mpaland/chan-sccp/commit/9302f0f4ff7bcf265bb50832ff5d508a78537692))


### Documentation

* Asterisk &gt;= 20 in README, packaging and NEWS, add doc/ANALYSIS.md ([ecd8fc5](https://github.com/mpaland/chan-sccp/commit/ecd8fc57a29f2d89066cee24ba71e207da806265))
* modernize README badges and links for the fork ([84c70bd](https://github.com/mpaland/chan-sccp/commit/84c70bde94d9fc8f9d4024e4dcf51876f573ba79))
* record the Asterisk 24.0.0-rc4 compatibility check ([b368534](https://github.com/mpaland/chan-sccp/commit/b368534b10ccd33900cc8a733d5d2918eb93cea2))


### Cleanup

* drop the compatibility shims left over from the version cleanup ([1076e98](https://github.com/mpaland/chan-sccp/commit/1076e983368b5c813fc97a566695c00e0f9eec72))
* Lower some of the debug messages during module reloading ([42e0556](https://github.com/mpaland/chan-sccp/commit/42e0556cf488825bf75fd66f1bf9488193f9b6b3))
* reformat sccp_config ([e81d3dc](https://github.com/mpaland/chan-sccp/commit/e81d3dc95efb80597508e75a556845990297d17f))
* remove ast_event based hint/devstate/MWI paths ([a5df17b](https://github.com/mpaland/chan-sccp/commit/a5df17b11ec087776d7acce31a3943833e0336c5))
* remove pre-Asterisk-20 conditionals from channel, pbx and feature code ([1d444ae](https://github.com/mpaland/chan-sccp/commit/1d444ae08a9161f792b5dc11a3605ff0c729666f))
* remove pre-Asterisk-20 conditionals from sccp_conference.c ([ebc408f](https://github.com/mpaland/chan-sccp/commit/ebc408f9fd9af95a06039e79df0bac2e5f77549c))
* remove pre-Asterisk-20 conditionals from the pbx glue layer ([da36fcd](https://github.com/mpaland/chan-sccp/commit/da36fcdc801e1216081ce17eaca57fe3f2f6d51e))
* remove pre-Asterisk-20 conditionals from the remaining sources ([93cc767](https://github.com/mpaland/chan-sccp/commit/93cc767a756734fe2053028a65e9c4be1ea92fe4))


### Miscellaneous Chores

* release 4.4.0 ([237c0b0](https://github.com/mpaland/chan-sccp/commit/237c0b0eeafe7989af17c0ea0fd8ecbae118b808))
