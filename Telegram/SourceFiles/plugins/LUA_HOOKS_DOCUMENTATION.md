# Lua Plugin System Hooks Documentation

This document describes all available hooks in the Telegram Desktop Lua plugin system. Each hook can be implemented in your Lua script to react to specific events.

## Table of Contents

1. [Message Hooks](#message-hooks)
2. [Peer/User Hooks](#peeruser-hooks)
3. [Call Hooks](#call-hooks)
4. [File Operation Hooks](#file-operation-hooks)
5. [Network Hooks](#network-hooks)
6. [History Hooks](#history-hooks)
7. [Lifecycle Hooks](#lifecycle-hooks)

---

## Message Hooks

### `onNewMessage(chatName, senderName, text, isOutgoing)`

Called when a new message is received or sent.

**Parameters:**
- `chatName` (string): Name of the chat/channel
- `senderName` (string): Name of the message sender
- `text` (string): Message text content
- `isOutgoing` (boolean): `true` if message was sent by you, `false` if received

**Implementation:**
```12:14:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireNewMessage(
	const QString &chatName,
	const QString &senderName,
	const QString &text,
	bool isOutgoing)
```

**Event Source:**
```425:443:Telegram/SourceFiles/plugins/plugin_manager.cpp
	session->changes().messageUpdates(
		Data::MessageUpdate::Flag::NewAdded
	) | rpl::start_with_next([=](const Data::MessageUpdate &update) {
		const auto item = update.item;
		const auto history = item->history();
		const auto peer = history->peer;
		const QString chatName = peer->name();
		const QString senderName = item->from()
			? item->from()->name()
			: chatName;
		const QString text = item->originalText().text;
		const bool isOutgoing = item->out();

		fireToAll([=](PluginState *plugin) {
			plugin->fireNewMessage(chatName, senderName, text, isOutgoing);
		});
	}, _sessionLifetime);
```

---

### `onMessageEdited(chatName, senderName, oldText, newText, isOutgoing)`

Called when a message is edited.

**Parameters:**
- `chatName` (string): Name of the chat/channel
- `senderName` (string): Name of the message sender
- `oldText` (string): Previous message text (currently same as newText)
- `newText` (string): New message text after edit
- `isOutgoing` (boolean): `true` if message was sent by you

**Implementation:**
```212:221:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireMessageEdited(
	const QString &chatName,
	const QString &senderName,
	const QString &oldText,
	const QString &newText,
	bool isOutgoing)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onMessageEdited", {chatName, senderName, oldText, newText}, {isOutgoing});
}
```

**Event Source:**
```477:495:Telegram/SourceFiles/plugins/plugin_manager.cpp
	// Message Updates: Edited messages
	session->changes().messageUpdates(
		Data::MessageUpdate::Flag::Edited
	) | rpl::start_with_next([=](const Data::MessageUpdate &update) {
		const auto item = update.item;
		const auto history = item->history();
		const auto peer = history->peer;
		const QString chatName = peer->name();
		const QString senderName = item->from()
			? item->from()->name()
			: chatName;
		const QString newText = item->originalText().text;
		// For edited messages, we don't have the old text easily accessible
		// So we'll pass the new text as both old and new for now
		const QString oldText = newText; // TODO: Store previous text if needed
		const bool isOutgoing = item->out();

		fireToAll([=](PluginState *plugin) {
			plugin->fireMessageEdited(chatName, senderName, oldText, newText, isOutgoing);
		});
	}, _sessionLifetime);
```

**Related Code:**
```2561:2581:Telegram/SourceFiles/data/data_session.cpp
void Session::updateEditedMessage(const MTPMessage &data) {
	const auto existing = data.match([](const MTPDmessageEmpty &)
			-> HistoryItem* {
		return nullptr;
	}, [&](const auto &data) {
		return message(peerFromMTP(data.vpeer_id()), data.vid().v);
	});
	if (!existing) {
		Reactions::CheckUnknownForUnread(this, data);
		return;
	}
	if (existing->isLocalUpdateMedia() && data.type() == mtpc_message) {
		updateExistingMessage(data.c_message());
	}
	data.match([](const MTPDmessageEmpty &) {
	}, [&](const MTPDmessageService &data) {
		existing->applyEdition(data);
	}, [&](const auto &data) {
		existing->applyEdition(HistoryMessageEdition(_session, data));
	});
}
```

---

### `onMessageDeleted(chatName, senderName, text, isOutgoing)`

Called when a message is deleted.

**Parameters:**
- `chatName` (string): Name of the chat/channel
- `senderName` (string): Name of the message sender
- `text` (string): Message text content
- `isOutgoing` (boolean): `true` if message was sent by you

**Implementation:**
```223:231:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireMessageDeleted(
	const QString &chatName,
	const QString &senderName,
	const QString &text,
	bool isOutgoing)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onMessageDeleted", {chatName, senderName, text}, {isOutgoing});
}
```

**Event Source:**
```497:515:Telegram/SourceFiles/plugins/plugin_manager.cpp
	// Message Updates: Deleted messages
	session->changes().messageUpdates(
		Data::MessageUpdate::Flag::Destroyed
	) | rpl::start_with_next([=](const Data::MessageUpdate &update) {
		const auto item = update.item;
		const auto history = item->history();
		const auto peer = history->peer;
		const QString chatName = peer->name();
		const QString senderName = item->from()
			? item->from()->name()
			: chatName;
		const QString text = item->originalText().text;
		const bool isOutgoing = item->out();

		fireToAll([=](PluginState *plugin) {
			plugin->fireMessageDeleted(chatName, senderName, text, isOutgoing);
		});
	}, _sessionLifetime);
```

---

### `onMessageSent(chatName, text, isScheduled)`

Called when you send a message.

**Parameters:**
- `chatName` (string): Name of the chat/channel
- `text` (string): Message text content
- `isScheduled` (boolean): `true` if message was scheduled

**Implementation:**
```233:240:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireMessageSent(
	const QString &chatName,
	const QString &text,
	bool isScheduled)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onMessageSent", {chatName, text}, {isScheduled});
}
```

**Event Source:**
```517:535:Telegram/SourceFiles/plugins/plugin_manager.cpp
	// History Updates: Message sent
	session->changes().historyUpdates(
		Data::HistoryUpdate::Flag::MessageSent
	) | rpl::start_with_next([=](const Data::HistoryUpdate &update) {
		const auto history = update.history;
		const auto peer = history->peer;
		const QString chatName = peer->name();
		// Get the last sent message
		const QString text = "Message sent"; // TODO: Get actual message text
		const bool isScheduled = false;

		fireToAll([=](PluginState *plugin) {
			plugin->fireMessageSent(chatName, text, isScheduled);
		});
	}, _sessionLifetime);
```

**Related Code:**
```3844:3887:Telegram/SourceFiles/apiwrap.cpp
void ApiWrap::sendMessage(MessageToSend &&message) {
	const auto history = message.action.history;
	const auto peer = history->peer;
	auto &textWithTags = message.textWithTags;

	auto action = message.action;
	action.generateLocal = true;
	sendAction(action);

	const auto clearCloudDraft = action.clearDraft;
	const auto draftTopicRootId = action.replyTo.topicRootId;
	const auto draftMonoforumPeerId = action.replyTo.monoforumPeerId;
	const auto replyTo = action.replyTo.messageId
		? peer->owner().message(action.replyTo.messageId)
		: nullptr;
	const auto topicRootId = draftTopicRootId
		? draftTopicRootId
		: replyTo
		? replyTo->topicRootId()
		: Data::ForumTopic::kGeneralId;
	const auto topic = peer->forumTopicFor(topicRootId);
	if (!(topic ? Data::CanSendTexts(topic) : Data::CanSendTexts(peer))
		|| Api::SendDice(message)) {
		return;
	}
	local().saveRecentSentHashtags(textWithTags.text);

	auto sending = TextWithEntities();
	auto left = TextWithEntities {
		textWithTags.text,
		TextUtilities::ConvertTextTagsToEntities(textWithTags.tags)
	};
	auto prepareFlags = Ui::ItemTextOptions(
		history,
		_session->user()).flags;
	TextUtilities::PrepareForSending(left, prepareFlags);

	HistoryItem *lastMessage = nullptr;

	auto &histories = history->owner().histories();
```

---

### `onMessageReaction(chatName, senderName, text, reaction, isOutgoing)`

Called when a reaction is added to a message.

**Parameters:**
- `chatName` (string): Name of the chat/channel
- `senderName` (string): Name of the message sender
- `text` (string): Message text content
- `reaction` (string): Reaction emoji/text
- `isOutgoing` (boolean): `true` if message was sent by you

**Implementation:**
```242:251:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireMessageReaction(
	const QString &chatName,
	const QString &senderName,
	const QString &text,
	const QString &reaction,
	bool isOutgoing)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onMessageReaction", {chatName, senderName, text, reaction}, {isOutgoing});
}
```

**Event Source:**
```537:555:Telegram/SourceFiles/plugins/plugin_manager.cpp
	// Message Updates: Reactions
	session->changes().messageUpdates(
		Data::MessageUpdate::Flag::NewUnreadReaction
	) | rpl::start_with_next([=](const Data::MessageUpdate &update) {
		const auto item = update.item;
		const auto history = item->history();
		const auto peer = history->peer;
		const QString chatName = peer->name();
		const QString senderName = item->from()
			? item->from()->name()
			: chatName;
		const QString text = item->originalText().text;
		const bool isOutgoing = item->out();
		// Extract reaction info if available
		const QString reaction = "unknown"; // TODO: Extract actual reaction

		fireToAll([=](PluginState *plugin) {
			plugin->fireMessageReaction(chatName, senderName, text, reaction, isOutgoing);
		});
	}, _sessionLifetime);
```

---

## Peer/User Hooks

### `onPeerUpdated(peerName, updateType)`

Called when peer information is updated (name, photo, etc.).

**Parameters:**
- `peerName` (string): Name of the peer (user/chat/channel)
- `updateType` (string): Type of update ("name", "photo", "online_status", "about", "username")

**Implementation:**
```253:259:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::firePeerUpdated(
	const QString &peerName,
	const QString &updateType)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onPeerUpdated", {peerName, updateType});
}
```

**Event Source:**
```562:596:Telegram/SourceFiles/plugins/plugin_manager.cpp
	// Peer Updates: Various peer changes
	session->changes().peerUpdates(
		Data::PeerUpdate::Flag::Name
		| Data::PeerUpdate::Flag::Photo
		| Data::PeerUpdate::Flag::OnlineStatus
		| Data::PeerUpdate::Flag::About
		| Data::PeerUpdate::Flag::Username
	) | rpl::start_with_next([=](const Data::PeerUpdate &update) {
		const auto peer = update.peer;
		const QString peerName = peer->name();
		QString updateType = "unknown";
		
		if (update.flags & Data::PeerUpdate::Flag::Name) {
			updateType = "name";
		} else if (update.flags & Data::PeerUpdate::Flag::Photo) {
			updateType = "photo";
		} else if (update.flags & Data::PeerUpdate::Flag::OnlineStatus) {
			updateType = "online_status";
			// Also fire online status change for users
			if (const auto user = peer->asUser()) {
				const auto lastseen = user->lastseen();
				const bool isOnline = lastseen.isOnline(::base::unixtime::now());
				fireToAll([=](PluginState *plugin) {
					plugin->fireUserOnlineStatusChanged(peerName, isOnline);
				});
			}
		} else if (update.flags & Data::PeerUpdate::Flag::About) {
			updateType = "about";
		} else if (update.flags & Data::PeerUpdate::Flag::Username) {
			updateType = "username";
		}

		fireToAll([=](PluginState *plugin) {
			plugin->firePeerUpdated(peerName, updateType);
		});
	}, _sessionLifetime);
```

**Related Code:**
```57:131:Telegram/SourceFiles/data/data_changes.h
struct PeerUpdate {
	enum class Flag : uint64 {
		None = 0,

		// Common flags
		Name                = (1ULL << 0),
		Username            = (1ULL << 1),
		Photo               = (1ULL << 2),
		About               = (1ULL << 3),
		Notifications       = (1ULL << 4),
		Migration           = (1ULL << 5),
		UnavailableReason   = (1ULL << 6),
		ChatThemeEmoji      = (1ULL << 7),
		ChatWallPaper       = (1ULL << 8),
		IsBlocked          = (1ULL << 9),
		MessagesTTL         = (1ULL << 10),
		FullInfo            = (1ULL << 11),
		Usernames           = (1ULL << 12),
		TranslationDisabled = (1ULL << 13),
		Color               = (1ULL << 14),
		BackgroundEmoji     = (1ULL << 15),
		StoriesState        = (1ULL << 16),
		VerifyInfo          = (1ULL << 17),
		StarsPerMessage     = (1ULL << 18),

		// For users
		CanShareContact     = (1ULL << 19),
		IsContact           = (1ULL << 20),
		PhoneNumber         = (1ULL << 21),
		OnlineStatus        = (1ULL << 22),
		BotCommands         = (1ULL << 23),
		BotCanBeInvited     = (1ULL << 24),
		BotStartToken       = (1ULL << 25),
		CommonChats         = (1ULL << 26),
		PeerGifts           = (1ULL << 27),
		HasCalls            = (1ULL << 28),
		SupportInfo         = (1ULL << 29),
		IsBot               = (1ULL << 30),
		EmojiStatus         = (1ULL << 31),
		BusinessDetails     = (1ULL << 32),
		Birthday            = (1ULL << 33),
		PersonalChannel     = (1ULL << 34),
		StarRefProgram      = (1ULL << 35),
		PaysPerMessage      = (1ULL << 36),
		GiftSettings        = (1ULL << 37),

		// For chats and channels
		InviteLinks         = (1ULL << 38),
		Members             = (1ULL << 39),
		Admins              = (1ULL << 40),
		BannedUsers         = (1ULL << 41),
		Rights              = (1ULL << 42),
		PendingRequests     = (1ULL << 43),
		Reactions           = (1ULL << 44),

		// For channels
		ChannelAmIn         = (1ULL << 45),
		StickersSet         = (1ULL << 46),
		EmojiSet            = (1ULL << 47),
		DiscussionLink      = (1ULL << 48),
		MonoforumLink       = (1ULL << 49),
		ChannelLocation     = (1ULL << 50),
		Slowmode            = (1ULL << 51),
		GroupCall           = (1ULL << 52),

		// For iteration
		LastUsedBit         = (1ULL << 52),
	};
	using Flags = base::flags<Flag>;
	friend inline constexpr auto is_flag_type(Flag) { return true; }

	not_null<PeerData*> peer;
	Flags flags = 0;

};
```

---

### `onUserOnlineStatusChanged(userName, isOnline)`

Called when a user's online status changes.

**Parameters:**
- `userName` (string): Name of the user
- `isOnline` (boolean): `true` if user is now online, `false` if offline

**Implementation:**
```261:267:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireUserOnlineStatusChanged(
	const QString &userName,
	bool isOnline)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onUserOnlineStatusChanged", {userName}, {isOnline});
}
```

**Event Source:** See `onPeerUpdated` above - this hook is fired as part of peer updates when the update type is "online_status".

---

### `onChatOpened(chatName)`

Called when a chat is opened.

**Parameters:**
- `chatName` (string): Name of the chat/channel

**Implementation:**
```269:273:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireChatOpened(const QString &chatName)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onChatOpened", {chatName});
}
```

**Note:** This hook is currently not connected to any event source. It can be implemented by hooking into the chat opening logic.

---

### `onChatClosed(chatName)`

Called when a chat is closed.

**Parameters:**
- `chatName` (string): Name of the chat/channel

**Implementation:**
```275:279:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireChatClosed(const QString &chatName)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onChatClosed", {chatName});
}
```

**Note:** This hook is currently not connected to any event source. It can be implemented by hooking into the chat closing logic.

---

## Call Hooks

### `onCallStarted(userName, isVideo, isOutgoing)`

Called when a call is started.

**Parameters:**
- `userName` (string): Name of the user in the call
- `isVideo` (boolean): `true` if it's a video call
- `isOutgoing` (boolean): `true` if you initiated the call

**Implementation:**
```281:288:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireCallStarted(
	const QString &userName,
	bool isVideo,
	bool isOutgoing)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onCallStarted", {userName}, {isVideo, isOutgoing});
}
```

**Event Source:**
```598:612:Telegram/SourceFiles/plugins/plugin_manager.cpp
	// Call events
	Core::App().calls().currentCallValue(
	) | rpl::start_with_next([=](Calls::Call *call) {
		if (!call) return;
		const auto user = call->user();
		const QString userName = user->name();
		// Check if video call by checking remote video state (simplified)
		const bool isVideo = (call->remoteVideoState() != Webrtc::VideoState::Inactive);
		const bool isOutgoing = call->type() == Calls::Call::Type::Outgoing;
		
		fireToAll([=](PluginState *plugin) {
			plugin->fireCallStarted(userName, isVideo, isOutgoing);
		});
	}, _sessionLifetime);
```

**Related Code:**
```197:213:Telegram/SourceFiles/calls/calls_instance.cpp
void Instance::startOutgoingCall(not_null<UserData*> user, bool video) {
	if (activateCurrentCall()) {
		return;
	}
	if (user->callsStatus() == UserData::CallsStatus::Private) {
		// Request full user once more to refresh the setting in case it was changed.
		user->session().api().requestFullPeer(user);
		Ui::show(Ui::MakeInformBox(tr::lng_call_error_not_available(
			tr::now,
			lt_user,
			user->name())));
		return;
	}
	requestPermissionsOrFail(crl::guard(this, [=] {
		createCall(user, Call::Type::Outgoing, video);
	}), video);
}
```

---

### `onCallEnded(userName, reason)`

Called when a call ends.

**Parameters:**
- `userName` (string): Name of the user in the call
- `reason` (string): Reason for call ending (e.g., "ended", "failed")

**Implementation:**
```290:296:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireCallEnded(
	const QString &userName,
	const QString &reason)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onCallEnded", {userName, reason});
}
```

**Event Source:**
```614:625:Telegram/SourceFiles/plugins/plugin_manager.cpp
	// Hook into call state changes - we'll monitor when calls end
	// Note: This is a simplified approach; full implementation would track call lifecycle
	Core::App().calls().currentCallValue(
	) | rpl::filter([=](Calls::Call *call) {
		return call == nullptr; // Call ended
	}) | rpl::start_with_next([=] {
		// When call becomes null, it means call ended
		// We'll use a simple approach here
		fireToAll([=](PluginState *plugin) {
			plugin->fireCallEnded("Unknown", "ended");
		});
	}, _sessionLifetime);
```

---

## File Operation Hooks

### `onFileUploadStarted(fileName, fileSize)`

Called when a file upload starts.

**Parameters:**
- `fileName` (string): Name of the file being uploaded
- `fileSize` (number): Size of the file in bytes

**Implementation:**
```303:310:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireFileUploadStarted(
	const QString &fileName,
	int64 fileSize)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onFileUploadStarted", {fileName}, {}, {fileSize});
}
```

**Event Source:**
```627:650:Telegram/SourceFiles/plugins/plugin_manager.cpp
	// File upload events - Photo uploads
	session->uploader().photoProgress(
	) | rpl::start_with_next([=](const FullMsgId &fullId) {
		fireToAll([=](PluginState *plugin) {
			plugin->fireFileUploadProgress("photo", 0, 0);
		});
	}, _sessionLifetime);

	session->uploader().photoReady(
	) | rpl::start_with_next([=](const Storage::UploadedMedia &media) {
		fireToAll([=](PluginState *plugin) {
			plugin->fireFileUploadCompleted("photo", true);
		});
	}, _sessionLifetime);

	session->uploader().photoFailed(
	) | rpl::start_with_next([=](const FullMsgId &fullId) {
		fireToAll([=](PluginState *plugin) {
			plugin->fireFileUploadCompleted("photo", false);
		});
	}, _sessionLifetime);
```

**Related Code:**
```67:94:Telegram/SourceFiles/storage/file_upload.h
	[[nodiscard]] rpl::producer<UploadedMedia> photoReady() const {
		return _photoReady.events();
	}
	[[nodiscard]] rpl::producer<UploadedMedia> documentReady() const {
		return _documentReady.events();
	}
	[[nodiscard]] rpl::producer<UploadSecureDone> secureReady() const {
		return _secureReady.events();
	}
	[[nodiscard]] rpl::producer<FullMsgId> photoProgress() const {
		return _photoProgress.events();
	}
	[[nodiscard]] rpl::producer<FullMsgId> documentProgress() const {
		return _documentProgress.events();
	}
	[[nodiscard]] auto secureProgress() const
	-> rpl::producer<UploadSecureProgress> {
		return _secureProgress.events();
	}
	[[nodiscard]] rpl::producer<FullMsgId> photoFailed() const {
		return _photoFailed.events();
	}
	[[nodiscard]] rpl::producer<FullMsgId> documentFailed() const {
		return _documentFailed.events();
	}
	[[nodiscard]] rpl::producer<FullMsgId> secureFailed() const {
		return _secureFailed.events();
	}
```

---

### `onFileUploadProgress(fileName, uploaded, total)`

Called during file upload to report progress.

**Parameters:**
- `fileName` (string): Name of the file being uploaded
- `uploaded` (number): Number of bytes uploaded so far
- `total` (number): Total size of the file in bytes

**Implementation:**
```312:320:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireFileUploadProgress(
	const QString &fileName,
	int64 uploaded,
	int64 total)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onFileUploadProgress", {fileName}, {}, {uploaded, total});
}
```

---

### `onFileUploadCompleted(fileName, success)`

Called when a file upload completes.

**Parameters:**
- `fileName` (string): Name of the file that was uploaded
- `success` (boolean): `true` if upload succeeded, `false` if it failed

**Implementation:**
```322:330:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireFileUploadCompleted(
	const QString &fileName,
	bool success)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onFileUploadCompleted", {fileName}, {success});
}
```

---

### `onFileDownloadStarted(fileName, fileSize)`

Called when a file download starts.

**Parameters:**
- `fileName` (string): Name of the file being downloaded
- `fileSize` (number): Size of the file in bytes

**Implementation:**
```332:340:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireFileDownloadStarted(
	const QString &fileName,
	int64 fileSize)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onFileDownloadStarted", {fileName}, {}, {fileSize});
}
```

**Note:** Download hooks are currently not fully implemented. The download manager API is more complex and would need additional integration.

---

### `onFileDownloadProgress(fileName, downloaded, total)`

Called during file download to report progress.

**Parameters:**
- `fileName` (string): Name of the file being downloaded
- `downloaded` (number): Number of bytes downloaded so far
- `total` (number): Total size of the file in bytes

**Implementation:**
```342:350:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireFileDownloadProgress(
	const QString &fileName,
	int64 downloaded,
	int64 total)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onFileDownloadProgress", {fileName}, {}, {downloaded, total});
}
```

---

### `onFileDownloadCompleted(fileName, success)`

Called when a file download completes.

**Parameters:**
- `fileName` (string): Name of the file that was downloaded
- `success` (boolean): `true` if download succeeded, `false` if it failed

**Implementation:**
```352:360:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireFileDownloadCompleted(
	const QString &fileName,
	bool success)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onFileDownloadCompleted", {fileName}, {success});
}
```

---

## Network Hooks

### `onPacketReceived(packetType, packetSize)`

Called when a network packet is received from the Telegram server.

**Parameters:**
- `packetType` (string): Type of packet (e.g., "mtp_123", "nop")
- `packetSize` (number): Size of the packet in bytes

**Implementation:**
```362:370:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::firePacketReceived(
	const QString &packetType,
	int packetSize)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onPacketReceived", {packetType}, {}, {packetSize});
}
```

**Event Source:**
```391:430:Telegram/SourceFiles/mtproto/connection_tcp.cpp
mtpBuffer TcpConnection::parsePacket(bytes::const_span bytes) {
	const auto packet = _protocol->readPacket(bytes);
	CONNECTION_LOG_INFO(u"Packet received, size = %1."_q.arg(packet.size()));
	
	// Notify plugins about received packet
	{
		const auto ints = gsl::make_span(
			reinterpret_cast<const mtpPrime*>(packet.data()),
			packet.size() / sizeof(mtpPrime));
		if (!ints.empty() && ints.size() >= 3) {
			// Try to determine packet type from first int (MTP constructor)
			QString packetType = "unknown";
			if (ints[0] == 0) {
				packetType = "nop";
			} else {
				// MTP constructor number - simplified type detection
				packetType = QString("mtp_%1").arg(ints[0]);
			}
			const int packetSize = int(packet.size());
			Plugins::Manager::instance().firePacketReceived(packetType, packetSize);
		}
	}
	
	const auto ints = gsl::make_span(
		reinterpret_cast<const mtpPrime*>(packet.data()),
		packet.size() / sizeof(mtpPrime));
	Assert(!ints.empty());
	if (ints.size() < 3) {
		// nop or error or new quickack, latter is not yet supported.
		if (ints[0] != 0) {
			CONNECTION_LOG_ERROR(u"Error packet received, code = %1"_q
				.arg(ints[0]));
		}
		return mtpBuffer(1, ints[0]);
	}
	auto result = mtpBuffer(ints.size());
	memcpy(result.data(), ints.data(), ints.size() * sizeof(mtpPrime));
	return result;
}
```

---

### `onPacketSent(packetType, packetSize)`

Called when a network packet is sent to the Telegram server.

**Parameters:**
- `packetType` (string): Type of packet (e.g., "mtp_123", "nop")
- `packetSize` (number): Size of the packet in bytes

**Implementation:**
```372:380:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::firePacketSent(
	const QString &packetType,
	int packetSize)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onPacketSent", {packetType}, {}, {packetSize});
}
```

**Event Source:**
```450:476:Telegram/SourceFiles/mtproto/connection_tcp.cpp
void TcpConnection::sendData(mtpBuffer &&buffer) {
	Expects(buffer.size() > 2);

	if (!_socket) {
		return;
	}
	char connectionStartPrefixBytes[kConnectionStartPrefixSize];
	const auto connectionStartPrefix = prepareConnectionStartPrefix(
		bytes::make_span(connectionStartPrefixBytes));

	// buffer: 2 available int-s + data + available int.
	const auto bytes = _protocol->finalizePacket(buffer);
	CONNECTION_LOG_INFO(u"TCP Info: write packet %1 bytes."_q
		.arg(bytes.size()));
	
	// Notify plugins about sent packet
	if (buffer.size() >= 3) {
		QString packetType = "unknown";
		if (buffer[0] == 0) {
			packetType = "nop";
		} else {
			packetType = QString("mtp_%1").arg(buffer[0]);
		}
		const int packetSize = int(bytes.size());
		Plugins::Manager::instance().firePacketSent(packetType, packetSize);
	}
	
	aesCtrEncrypt(bytes, _sendKey, &_sendState);
	_socket->write(connectionStartPrefix, bytes);
}
```

---

### `onConnectionStateChanged(state)`

Called when the connection state changes.

**Parameters:**
- `state` (string): Connection state ("connected", "disconnected")

**Implementation:**
```382:390:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireConnectionStateChanged(
	const QString &state)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onConnectionStateChanged", {state});
}
```

**Event Source:**
```432:441:Telegram/SourceFiles/mtproto/connection_tcp.cpp
void TcpConnection::socketConnected() {
	Expects(_status == Status::Waiting);

	auto buffer = preparePQFake(_checkNonce);

	CONNECTION_LOG_INFO("Sending fake req_pq.");

	_pingTime = crl::now();
	sendData(std::move(buffer));
	
	Plugins::Manager::instance().fireConnectionStateChanged("connected");
}
```

```443:448:Telegram/SourceFiles/mtproto/connection_tcp.cpp
void TcpConnection::socketDisconnected() {
	if (_status == Status::Waiting || _status == Status::Ready) {
		Plugins::Manager::instance().fireConnectionStateChanged("disconnected");
		disconnected();
	}
}
```

---

## History Hooks

### `onHistoryUpdated(chatName, updateType)`

Called when history information is updated.

**Parameters:**
- `chatName` (string): Name of the chat/channel
- `updateType` (string): Type of update

**Implementation:**
```392:400:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireHistoryUpdated(
	const QString &chatName,
	const QString &updateType)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onHistoryUpdated", {chatName, updateType});
}
```

**Note:** This hook is currently not connected to any event source. It can be implemented by hooking into history update events.

---

### `onUnreadCountChanged(chatName, unreadCount)`

Called when the unread message count changes for a chat.

**Parameters:**
- `chatName` (string): Name of the chat/channel
- `unreadCount` (number): New unread message count

**Implementation:**
```402:410:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::fireUnreadCountChanged(
	const QString &chatName,
	int unreadCount)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onUnreadCountChanged", {chatName}, {}, {unreadCount});
}
```

**Event Source:**
```550:560:Telegram/SourceFiles/plugins/plugin_manager.cpp
	// History Updates: Unread count changed
	session->changes().historyUpdates(
		Data::HistoryUpdate::Flag::UnreadView
	) | rpl::start_with_next([=](const Data::HistoryUpdate &update) {
		const auto history = update.history;
		const auto peer = history->peer;
		const QString chatName = peer->name();
		const int unreadCount = history->unreadCount();

		fireToAll([=](PluginState *plugin) {
			plugin->fireUnreadCountChanged(chatName, unreadCount);
		});
	}, _sessionLifetime);
```

**Related Code:**
```133:161:Telegram/SourceFiles/data/data_changes.h
struct HistoryUpdate {
	enum class Flag : uint32 {
		None = 0,

		IsPinned           = (1U << 0),
		UnreadView         = (1U << 1),
		TopPromoted        = (1U << 2),
		Folder             = (1U << 3),
		UnreadMentions     = (1U << 4),
		UnreadReactions    = (1U << 5),
		ClientSideMessages = (1U << 6),
		ChatOccupied       = (1U << 7),
		MessageSent        = (1U << 8),
		ScheduledSent      = (1U << 9),
		OutboxRead         = (1U << 10),
		BotKeyboard        = (1U << 11),
		CloudDraft         = (1U << 12),
		TranslateFrom      = (1U << 13),
		TranslatedTo       = (1U << 14),

		LastUsedBit        = (1U << 14),
	};
	using Flags = base::flags<Flag>;
	friend inline constexpr auto is_flag_type(Flag) { return true; }

	not_null<History*> history;
	Flags flags = 0;

};
```

---

## Lifecycle Hooks

### `onLoad()`

Called when the plugin is loaded and enabled.

**Implementation:**
```107:109:Telegram/SourceFiles/plugins/plugin_manager.cpp
	// Call onLoad() if defined
	callHook("onLoad");
	return true;
```

**Example:**
```lua
function onLoad()
    log("Plugin loaded successfully!")
end
```

---

### `onUnload()`

Called when the plugin is unloaded or disabled.

**Implementation:**
```112:118:Telegram/SourceFiles/plugins/plugin_manager.cpp
void PluginState::unload() {
	if (!_L) return;
	callHook("onUnload");
	lua_close(_L);
	_L = nullptr;
	addLog("[INFO] Plugin unloaded: " + _name);
}
```

**Example:**
```lua
function onUnload()
    log("Plugin unloaded. Cleaning up...")
end
```

---

## Utility Functions

### `log(message)`

Log a message to the plugin's log buffer. This is available as both `log()` and `print()`.

**Implementation:**
```75:83:Telegram/SourceFiles/plugins/plugin_manager.cpp
	// Register custom print/log functions that write to our log buffer
	lua_pushlightuserdata(_L, this);
	lua_pushcclosure(_L, &PluginState::luaLog, 1);
	lua_setglobal(_L, "log");

	// Override print() to also write to our buffer
	lua_pushlightuserdata(_L, this);
	lua_pushcclosure(_L, &PluginState::luaPrint, 1);
	lua_setglobal(_L, "print");
```

**Example:**
```lua
log("This is a log message")
print("This also goes to the log")
```

---

## Example Plugin

Here's a complete example plugin that uses multiple hooks:

```lua
-- example_plugin.lua
-- A comprehensive example plugin demonstrating various hooks

function onLoad()
    log("Example plugin loaded!")
end

function onUnload()
    log("Example plugin unloaded!")
end

function onNewMessage(chatName, senderName, text, isOutgoing)
    local direction = isOutgoing and "Outgoing" or "Incoming"
    log(string.format("[%s] New message in '%s' from '%s': %s", 
        direction, chatName, senderName, text))
end

function onMessageEdited(chatName, senderName, oldText, newText, isOutgoing)
    log(string.format("Message edited in '%s': '%s' -> '%s'", 
        chatName, oldText, newText))
end

function onMessageSent(chatName, text, isScheduled)
    local type = isScheduled and "Scheduled" or "Sent"
    log(string.format("[%s] Message to '%s': %s", type, chatName, text))
end

function onPeerUpdated(peerName, updateType)
    log(string.format("Peer '%s' updated: %s", peerName, updateType))
end

function onUserOnlineStatusChanged(userName, isOnline)
    local status = isOnline and "online" or "offline"
    log(string.format("User '%s' is now %s", userName, status))
end

function onCallStarted(userName, isVideo, isOutgoing)
    local callType = isVideo and "Video" or "Voice"
    local direction = isOutgoing and "Outgoing" or "Incoming"
    log(string.format("[%s %s Call] Started with %s", 
        direction, callType, userName))
end

function onFileUploadProgress(fileName, uploaded, total)
    if total > 0 then
        local percent = (uploaded / total) * 100
        log(string.format("Upload progress: %s - %.1f%%", fileName, percent))
    end
end

function onPacketReceived(packetType, packetSize)
    -- Only log non-nop packets to avoid spam
    if packetType ~= "nop" then
        log(string.format("Received packet: %s (%d bytes)", packetType, packetSize))
    end
end

function onUnreadCountChanged(chatName, unreadCount)
    log(string.format("Unread count in '%s': %d", chatName, unreadCount))
end
```

---

## Notes

- All hooks are optional - you only need to implement the ones you care about
- Hooks are called synchronously, so keep them fast to avoid blocking the UI
- Errors in hooks are caught and logged automatically
- The `log()` and `print()` functions are available in all Lua scripts
- Plugin logs can be viewed in the Plugins UI

---

## Implementation Details

The plugin system is implemented in:
- `Telegram/SourceFiles/plugins/plugin_manager.h` - Header file with class definitions
- `Telegram/SourceFiles/plugins/plugin_manager.cpp` - Implementation of plugin management and event dispatching
- `Telegram/SourceFiles/plugins/plugin_box.h` - UI header for plugin management
- `Telegram/SourceFiles/plugins/plugin_box.cpp` - UI implementation for plugin management
- `Telegram/SourceFiles/mtproto/connection_tcp.cpp` - Network packet hooks

The plugin manager is initialized in:
```132:132:Telegram/SourceFiles/main/main_session.cpp
	Plugins::Manager::instance().setSession(this);
```
