# chan-sccp: analysis and modernisation for Asterisk 20+

Status: all items found during the analysis are fixed in the commit series `e4b47129..HEAD` on `develop`
(based on upstream `chan-sccp/chan-sccp@90dc24f2`, 2023-12-19). What remains is runtime verification on
real phones, listed in section 5.

## 1. Scope and method

The driver had not been maintained for three years and did not work on current Asterisk releases: no
outbound audio on Asterisk >= 20.10 / 21 / 22, and no module load (or no compile) on 21/22. The goals were

1. support Asterisk 20, 21, 22 and later only, with a single glue implementation;
2. integrate the meaningful open upstream pull requests;
3. find and fix memory leaks, reference count errors, locking defects and lifetime bugs.

Method: the whole `src/` tree was read against the Asterisk 22.11.0 source (kept locally under
`doc/asterisk-22-current/`, ignored by git) for API semantics (`rtp_engine.c`, `channel.c`, `stasis.c`,
`astobj2`). Every finding was verified by reading the call paths, fixed, and each step was compiled on
Ubuntu 26.04 / Asterisk 22 (`asterisk-dev`) with the CI configure flags
(`--enable-conference --enable-video --enable-devdoc --enable-experimental-xml --with-hash-size=3001`).
Pre-Asterisk-20 code was removed mechanically with `unifdef -k -m` and the fixed feature set of Asterisk 20+,
then reviewed hunk by hunk.

Size of the change: 130 files, roughly 18k lines added and 64k removed (the ten dead per-version
implementations alone were ~35k lines).

## 2. Asterisk 20+ compatibility findings

| Finding | Effect before | Resolution |
|---|---|---|
| `ast_rtp_write()` resolves the outgoing payload type exclusively from `codecs->payload_mapping_tx` since Asterisk commit 055031d (18.x, 20.10+, all 21/22). chan-sccp never populated the tx table (no SDP). | `res_rtp_asterisk: Don't know how to send format ulaw packets with RTP`, one-way audio (upstream #615, #617). | `sccp_astwrap_rtp_registerPayload()` registers every negotiable codec for tx and rx when the rtp instance is created and whenever the codec is (re)selected. Static payload types from `skinny_codecs[]`, dynamic ones assigned by Asterisk with tx forced to the same value. Commit `b8127bee`. |
| `ast_channel_macroexten()/macrocontext()` removed with app_macro in Asterisk 21. | Module did not load on 21, did not compile on 22 (upstream #609). | Accessors and `iPbx` members removed (no callers). `f1e1c4f2`. |
| configure mapped Asterisk 20/21/22 onto "group 118" (a symlink to `ast116`); version detection had three paths and 360 lines. | Fragile detection, wrong feature set. | Major version from `AMI_VERSION` in `asterisk/manager.h` (AMI major = Asterisk major - 11), minor from pkg-config, `< 20` rejected. `6fb65405`, `60af31c5`. |
| `ast_enable_distributed_devstate()` no longer exists. | Compile error with `--enable-distributed-devicestate`. | Option kept, implemented via the stasis device state topic. `da36fcdc`. |
| `ast_unreal_new_channels()` takes an `ast_callid` value since 13. | Type error on 22. | Fixed call. `60af31c5`. |
| The `accept`/`close` members of `sccp_transport` collide with the `DEBUG_FD_LEAKS` macros in `asterisk.h`. | Compile error in DEBUG_FD_LEAKS builds (upstream #625). | Renamed `accept_connection`/`close_socket` (PR #626). `e4b47129`. |
| `sccp_dev_starttone()` declaration/definition mismatch. | Hard error with `-Werror=enum-int-mismatch` (AST_DEVMODE). | Fixed (PR #626). `e4b47129`. |
| ~60 configure probes for API details that varied between Asterisk 1.6 and 19; ~100 `#if` on version macros. | Dead code, probes that could never fire (e.g. `CS_AST_HAS_EVENT`), a broken `LOW_MEMORY` probe. | Probes replaced by one header sanity check plus the menuselect dependent ones; all conditionals removed. `44416825`, `da36fcdc`..`1076e983`. |

Build layout now: `src/pbx_impl/ast/{ast.c, astwrap.c/.h, ast_announce.c/.h, define.h}` built into `libast.la`.
`astwrap.h` carries `#error` for `ASTERISK_VERSION_MAJOR < 20`. `ASTERISK_VERSION_GROUP` (100+major) and
`ASTERISK_VERSION_NUMBER` (10000+major*100+minor) are still defined for the few remaining comparisons.

## 3. Fixed defects

Severity: **crash** (NULL/UAF/double free/overflow), **leak**, **deadlock/race**, **logic**.
Line numbers refer to the file state before the fix (upstream `90dc24f2`, or the state at the start of the
respective commit for the glue layer after the move).

### 3.1 Upstream pull requests

| PR | Content | Disposition |
|---|---|---|
| #629 | Seed tx payload map with `set_m_type` for pt 0/8/9/18 | Superseded by the full fix in `b8127bee` (dynamic codecs, late codec changes, video pt); credited in the commit. |
| #628 | ast120-122 symlink dirs, macro stubs, `acinclude.m4 $ac_dir` fix, linedevice buttons fix | `acinclude.m4` fix taken (`6fb65405`), linedevice intent taken in a simpler form (`b3a83ec4`); the rest superseded by the single-implementation layout. |
| #626 | Transport callback rename, starttone prototype | Applied as-is. `e4b47129`. |
| #611 | Delete the macroexten/macrocontext wrappers | Applied and extended (interface members removed). `f1e1c4f2`. |
| #602 | UAF of `config_file_name` on `sccp reload line` | Applied. `c030f3e4`. |
| #627 | FreePBX module dump (97 PHP files) | Not part of the driver; skipped. Can live in its own repository. |
| #417 | 2017 DirectRTP work in progress | Skipped (unfinished, predates the current RTP code). |

### 3.2 RTP / glue layer (`src/pbx_impl/ast/astwrap.c`, commit `b8127bee`, `13ab3448`)

| Site | Severity | Defect |
|---|---|---|
| createRtpInstance :2456-2541 | logic | tx payload table never populated (see section 2). |
| createRtpInstance :2532 | crash | `ast_rtp_codecs_set_framing(... c->owner ...)` with NULL owner when the instance is created before the pbx channel. |
| setReadFormat :2699 | logic | called `ast_set_write_format` instead of `ast_set_read_format`. |
| recalculateVideoCodecFormat (sccp_channel.c:561) | logic | pushed the video codec into the audio read/write format and the audio rtp instance. |
| __find_joint_capabilities :537-549 | leak | `ast_format_cap_append()` given the media type as framing; extra `ao2_ref` on `best_fmt_native`; `best_fmt_cap` never released. |
| requester :1767/:1773 | leak | `ast_format_cap_get_best_by_type()` reference leaked per call. |
| allocPBXChannel :1181-1216 | crash | failed joint caps: channel dropped with `ao2_cleanup`/`ast_hangup` while locked and snapshot-staged, tech_pvt still holding an sccp reference. |
| allocPBXChannel :1265/:1269 | leak | `ast_get_namedgroups()` reference leaked (setter takes its own). |
| allocPBXChannel :1278 | leak | `ast_get_indication_zone()` called twice, first reference leaked. |
| hangup :1442 | crash | unref of a channel reference nobody took ("unknown channel" branch). |
| ast.c :1330-1360 | leak | `pbx_strdup()` passed to `pbx_builtin_setvar_helper()` which copies. |
| ast.c :642-648 | deadlock | `requestHangup` exit with the ast_channel still locked. |
| getChannelByName :2576 | leak | returned two references (`ast_channel_get_by_name` + explicit ref), callers released one. `c3e40512`. |
| sccp_webservice.c :453 | leak | `ast_http_get_post_vars()` result never destroyed. |

### 3.3 Configuration and reload (`sccp_config.c`, `chan_sccp.c`, `sccp_session.c`, commit `fc5b2002`)

| Site | Severity | Defect |
|---|---|---|
| :2978-2992 | leak | previous `ast_config` destroyed only on forced load: every `sccp reload` leaked the parsed configuration. |
| :466-469, :504-507 | crash | variable-set builders returned the already destroyed list on allocation failure (UAF/double free at :776). |
| :1955/:1963 | leak | `break` skipped `sccp_free(var_name)`. |
| :2282-2286 | deadlock | return with `buttonconfigList` locked. |
| :3041-3044 | crash | `config_file_name` UAF (PR #602). |
| :3206 | crash | softkeyset name copied with `sizeof(struct)` (heap overflow); calloc result unchecked. |
| :3225-3232 | crash | `strsep()` advanced pointer passed to `free()`. |
| :1017-1025 | crash | freed STRINGPTR left dangling. |
| chan_sccp.c:362 / sccp_session.c:1043 | deadlock | `sccp_reload` holds `GLOB(lock)` write-locked, `stop_accept_thread` took it again when bindaddr/port changed. |

### 3.4 Sessions, transport, threads (commit `0abb1590`)

| Site | Severity | Defect |
|---|---|---|
| sccp_session.c :1208/:1234 | leak | `send2` returned -2/-4 without freeing the owned message. |
| :109 | leak | `servercontext_create` leaked context and transport on bind failure. |
| :1019 | logic | accept loop closed the fd of the last accepted session on exit. |
| sccp_transport_tls.c :209-219 | crash | `if (sc->fd)` true for -1; `ssl` not NULLed after `SSL_free`. |
| sccp_threadpool.c :123 | logic | cancel state/type set on the creating thread. |
| :343-348 | leak | queued jobs never freed on destroy. |
| :273 | crash | `exit(1)` on OOM in `add_work`. |
| sccp_pbx.c :383-389 | leak | failed `add_work` leaked the conveyor and a linedevice reference. |
| sccp_utils.c :1601 | leak | `sccp_append_variable` with NULL list leaked the new variable. |

### 3.5 Channel / pbx reference counting (commit `b3a83ec4`)

| Site | Severity | Defect |
|---|---|---|
| sccp_channel.c :210-217 | leak | three early returns leaked the retained line. |
| :244-260, :340 | crash | `privateData` NULL on callinfo failure, destructor dereferenced it. |
| :1606-1617, :1641-1649 | leak | retain released only inside the "scheduled" branch (channel never destroyed). |
| :1725-1733 | leak | `getEmptyChannel` leaked the channel when the pbx channel could not be created. |
| sccp_pbx.c :449-480 | crash | `cfwdnoanswer_cb` returned non-zero (= reschedule) after `AUTO_RELEASE`. |
| :654-666 | leak | forwarder/tmp_channel references leaked on `return -2`. |
| sccp_feature.c :995 | leak | `if (sccp_channel_retain(c))` discarded the reference (every MeetMe channel). |
| :141-147 | leak | pickup callerid strings never freed. |
| sccp_actions.c :1430 | leak | button template freed without releasing its line references. |
| sccp_linedevice.c :393-433 | crash | buttons array: NULL template, array freed inside the loop (UAF, double free), dangling pointer. |
| sccp_actions.c :4669 | crash | DeviceToUserData length from the wire copied unchecked into a stack buffer. |
| :4683, :4614 | leak/deadlock | dtu softkey action overwritten without free; return with `addons` locked. |
| sccp_line.c :173 | crash | NULL dereference when the line was not in the list. |
| sccp_cli.c :1956-2011, :3830 | leak/deadlock/crash | device reference never released, returns with `GLOB(lines)`/buttonconfig locked, empty device list dereferenced. |

### 3.6 Hint / MWI / devstate (commit `934889dc`)

| Site | Severity | Defect |
|---|---|---|
| sccp_mwi.c :278-296 | crash | `removed` never reset: second match double-unsubscribed and double-freed. |
| :306-314 | leak | `REMOVE_UNORDERED` + `idx++` skipped every second subscription. |
| sccp_devstate.c :100, :174 | leak | subscriber records never freed. |
| :138-143 | crash | `states[]` written past `AST_DEVICE_TOTAL` for digit 9. |
| devstate :95, hint :239/:258, mwi :126 | race | `stasis_unsubscribe()` then free while a callback may run; now `stasis_unsubscribe_and_join()`. |

### 3.7 Module lifecycle and refcount framework (commit `c8229842`)

| Site | Severity | Defect |
|---|---|---|
| astwrap.c unload_module | crash | `sccp_preUnload()` ran before tech/CLI/AMI/glue were unregistered; NULL globals dereferenced after a failed load; unmatched module unref. |
| chan_sccp.c :306-318 | race | event vectors freed before the threadpool drained. |
| sccp_refcount.c :252-278 | leak | destroy stopped at the first empty bucket and freed heads during the first type pass. |
| :523-528, :345, :492 | race | bucket destroyed while locked; bucket pointer read unlocked. |
| sccp_device.c :647 | crash | `permithosts` list destroyed on every device destruction, never initialised. |

### 3.8 Structural changes (commits `42a880f9`..`325af161`)

These replace designs that could not be patched locally. Each introduces an invariant that later changes
must keep.

**Reference counting** (`42a880f9`, `sccp_refcount.c`, `sccp_atomic.h`)
- Counters use the gcc/clang `__atomic` builtins. The mutex fallback, which modified the counter unlocked
  after 60 failed `trylock()`s and whose `CAS32` unlocked a mutex it never took, is gone.
- Invariant: `sccp_refcount_retain()` refuses objects whose count is already 0 (CAS loop). A pointer read
  from a shared field must therefore be retained *before* the holder's own reference can be released, and a
  failed retain means "object is dying, treat as NULL".
- Hash buckets are static list heads living from init to destroy; destructors run outside the bucket lock.
- `sccp_refcount_destroy()` reports every object still alive at unload at WARNING level (type + identifier).

**Sessions** (`299df4ca`, `sccp_session.c`)
- A session is reference counted: the session thread, `GLOB(sessions)` and `device->session` hold one each.
  Set `device->session` only through `sccp_device_setSession()`.
- Stopping from another thread: `session_stop` + `shutdown()` of the socket, then wait on the `exited`
  condition (5 s timeout with warning). No `pthread_cancel`, no `pthread_join` on the detached thread.
- Teardown (device cleanup, leaving the list, closing the socket) is separate from destruction (locks,
  condition, memory), so a session whose thread never started is released cleanly.

**Threadpool** (`419737a4`, `sccp_threadpool.c`)
- Workers are joinable, never cancelled; finished workers are reaped via a `finished` list.
- Jobs carry an optional `arg_cleanup`; jobs still queued at shutdown run it. Channel cleanup jobs register
  their job function as the cleanup because they only release resources.
- If a worker is stuck in a job at destroy, the pool is leaked with an error rather than freed under it.

**Lock ordering** (`6fafe408`)
- Rule: list locks (`line->channels`, `line->devices`, `device->buttonconfig`, `GLOB(devices|lines|sessions)`,
  `parkinglots`) are leaf locks. Collect (and retain) under the lock, unlock, then act. Never call into
  Asterisk, fire events, change the dialplan or take another sccp lock while holding one.
- Canonical orders: `GLOB(sessions)` -> session lock; `parkinglots` -> `pl->lock`.
- Applied to `sccp_line_kill_channels`, `sccp_dev_clean`, `sccp_linedevice_remove`, session add/remove
  device, parking lot observer handling, and the pre/post reload and pre-unload traversals.

**Reference cycles** (`a89c1b78`)
- `sccp_pbx_hangup()` always runs `sccp_channel_clean()` and returns the channel, so the tech_pvt reference
  is released on every path (device-less channels used to leak channel, line and device).
- Both destructors release anything a skipped clean path left behind and log it at WARNING: a warning
  `(device_destroy) ... still referenced` or `(channel_destroy) ...` in the log points at a broken clean path.

**masqueradeHelper contract** (`c3e40512`)
- The helper never consumes caller references and hangs up nothing on failure. Callers release at a single
  exit and decide the failure disposition. `getChannelByName()` returns exactly one reference.

**XML** (`e1e279aa`)
- `iXML.applyStyleSheet(xmlDoc **doc, ...)` hands the transformed document back. libxml/libxslt global
  cleanup runs only in the module destructor, never per request.

**NAT hole punch** (`325af161`, `sccp_rtp.h`, `sccp_channel.c`, `sccp_indicate.c`)
- State `sccp_rtp_holepunch_t {NONE, ACTIVE, DONE}` lives in `sccp_rtp_t`, guarded by `rtp->lock`, which is
  now initialised in `sccp_channel_allocate()` and destroyed with the channel (it was used uninitialised).
- One-shot per rtp port lifetime: `startHolePunch` acts on NONE->ACTIVE only; `finishHolePunch` acts on
  ACTIVE->DONE (first inbound packet or call connected), keeping media open when the call is up; reset to
  NONE when the receive channel is closed without keeping the port, on port re-request and on rtp destroy.
- `sccp_indicate` CONNECTED completes a pending punch instead of registering the startMediaTransmission
  callback (no double open).
- The config default changes from the same upstream series (`dateformat` M/D/YY, `firstdigittimeout` 10,
  `digittimeout` 5) are kept and documented in `NEWS` and `conf/sccp.conf`.

## 4. Verification done

- Full build on Ubuntu 26.04 with Asterisk 22 headers after every commit; final state builds with 0 errors
  and 15 warnings, all pre-existing (old-style definitions, libxml deprecations, one unused label).
- Build from the committed generated `configure` (no bootstrap) with the CI flags.
- `./configure --enable-debug` build (strict warning set, `-Werror=implicit`, `-Wfatal-errors`): 0 errors,
  the same 15 pre-existing warnings.
- `./configure --with-asterisk-version=18.9` exits 1 with "requires Asterisk 20 or newer".
- `grep -rn 'ASTERISK_VERSION_\|ASTERISK_CONF_1_' src/` shows only the `astwrap.h` guard and the
  configure-provided defines.

## 5. Needs runtime verification on real phones

Compile-time verification cannot cover these; they are the items to test on an Asterisk 20/21/22 box with
SCCP phones before a release:

1. **Audio both ways** SCCP <-> PJSIP for ulaw, alaw, g722, g729 and one dynamic codec (iLBC or opus):
   confirms the payload registration (section 2) and that the phone accepts Asterisk-assigned dynamic types.
2. **Video** (`--enable-video`): OpenMultiMediaChannel and StartMultiMediaTransmission must carry the same
   payload type as the rtp instance (h264 103 by default).
3. **NAT / hole punch**: outgoing call from a phone behind NAT with early media (progress), fast answer right
   after progress, hold/resume, and a second call on the same phone. Expect exactly one punch per port
   lifetime in the `rtp` debug log.
4. **Reload**: `sccp reload`, `sccp reload line`, changing `bindaddr`/`port`, repeated 10x; watch memory with
   `memory show summary` (MALLOC_DEBUG) or RSS.
5. **Unload / load** `chan_sccp.so` 10x with registered phones: no `still referenced` warnings from the
   destructors and no leftover objects reported by `sccp_refcount_destroy()`; sessions must stop within the
   5 s timeout (a warning is logged otherwise).
6. **Call forward answer** (masquerade path) and **conference** (`--enable-conference`): both exercise the
   new masqueradeHelper contract.
7. **Thread sanitizer** build (`./configure --enable-debug CFLAGS="-fsanitize=thread"`) for the refcount,
   session and threadpool changes under call load.

## 6. Build notes

- Requirements: Asterisk >= 20 development headers (`asterisk-dev` / `asterisk-devel`), autoconf, automake,
  libtool, pkg-config, gawk, libxml2/libxslt headers, libssl headers.
- `./tools/bootstrap.sh` regenerates the autotools files (autoconf 2.72 / automake 1.18.1 were used for the
  committed ones) and runs `configure`; the committed `configure` can be used directly.
- Detection reads `AMI_VERSION` from the `asterisk/manager.h` found by the compiler include trace; gcc 15
  prints empty expansions for these macros through `-E`, which is why the header text is read instead.
- The tree is checked out with CRLF on Windows; build on Linux from a clone or with `core.autocrlf` unset,
  since autotools scripts do not tolerate CRLF.
- `doc/asterisk-22-current/` is an untracked local copy of the Asterisk source used as API reference and is
  listed in `.gitignore`.
