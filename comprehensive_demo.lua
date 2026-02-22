-- Comprehensive Demo Plugin for Telegram Desktop
-- This plugin demonstrates many of the available hooks in the plugin system

local messageCount = 0
local editCount = 0
local deleteCount = 0
local sentCount = 0
local reactionCount = 0
local peerUpdateCount = 0
local onlineStatusChanges = {}
local callCount = 0
local uploadCount = 0
local downloadCount = 0
local packetCount = 0
local connectionState = "unknown"

function onLoad()
    log("========================================")
    log("Comprehensive Demo Plugin Loaded!")
    log("This plugin demonstrates various hooks")
    log("========================================")
end

function onUnload()
    log("========================================")
    log("Comprehensive Demo Plugin Unloaded!")
    log("Statistics:")
    log(string.format("  Messages received: %d", messageCount))
    log(string.format("  Messages edited: %d", editCount))
    log(string.format("  Messages deleted: %d", deleteCount))
    log(string.format("  Messages sent: %d", sentCount))
    log(string.format("  Reactions: %d", reactionCount))
    log(string.format("  Peer updates: %d", peerUpdateCount))
    log(string.format("  Calls: %d", callCount))
    log(string.format("  File uploads: %d", uploadCount))
    log(string.format("  File downloads: %d", downloadCount))
    log(string.format("  Network packets: %d", packetCount))
    log("========================================")
end

-- Message Hooks
function onNewMessage(chatName, senderName, text, isOutgoing)
    messageCount = messageCount + 1
    local direction = isOutgoing and "→" or "←"
    local prefix = string.format("[MSG #%d]", messageCount)
    log(string.format("%s %s [%s] %s: %s", 
        prefix, direction, chatName, senderName, text))
end

function onMessageEdited(chatName, senderName, oldText, newText, isOutgoing)
    editCount = editCount + 1
    local direction = isOutgoing and "→" or "←"
    log(string.format("[EDIT #%d] %s [%s] %s", 
        editCount, direction, chatName, senderName))
    log(string.format("  Old: %s", oldText))
    log(string.format("  New: %s", newText))
end

function onMessageDeleted(chatName, senderName, text, isOutgoing)
    deleteCount = deleteCount + 1
    local direction = isOutgoing and "→" or "←"
    log(string.format("[DELETE #%d] %s [%s] %s deleted: %s", 
        deleteCount, direction, chatName, senderName, text))
end

function onMessageSent(chatName, text, isScheduled)
    sentCount = sentCount + 1
    local type = isScheduled and "SCHEDULED" or "SENT"
    log(string.format("[%s #%d] To [%s]: %s", 
        type, sentCount, chatName, text))
end

function onMessageReaction(chatName, senderName, text, reaction, isOutgoing)
    reactionCount = reactionCount + 1
    local direction = isOutgoing and "→" or "←"
    log(string.format("[REACTION #%d] %s [%s] %s reacted %s to: %s", 
        reactionCount, direction, chatName, senderName, reaction, text))
end

-- Peer/User Hooks
function onPeerUpdated(peerName, updateType)
    peerUpdateCount = peerUpdateCount + 1
    log(string.format("[PEER UPDATE #%d] %s: %s", 
        peerUpdateCount, peerName, updateType))
    
    -- Log specific update types
    if updateType == "name" then
        log(string.format("  → Name changed for %s", peerName))
    elseif updateType == "photo" then
        log(string.format("  → Photo changed for %s", peerName))
    elseif updateType == "username" then
        log(string.format("  → Username changed for %s", peerName))
    end
end

function onUserOnlineStatusChanged(userName, isOnline)
    local previousStatus = onlineStatusChanges[userName]
    onlineStatusChanges[userName] = isOnline
    
    local status = isOnline and "ONLINE" or "OFFLINE"
    if previousStatus == nil then
        log(string.format("[ONLINE STATUS] %s is now %s", userName, status))
    elseif previousStatus ~= isOnline then
        log(string.format("[ONLINE STATUS] %s changed: %s → %s", 
            userName, previousStatus and "ONLINE" or "OFFLINE", status))
    end
end

function onChatOpened(chatName)
    log(string.format("[CHAT] Opened: %s", chatName))
end

function onChatClosed(chatName)
    log(string.format("[CHAT] Closed: %s", chatName))
end

-- Call Hooks
function onCallStarted(userName, isVideo, isOutgoing)
    callCount = callCount + 1
    local callType = isVideo and "VIDEO" or "VOICE"
    local direction = isOutgoing and "OUTGOING" or "INCOMING"
    log(string.format("[CALL #%d] %s %s call with %s", 
        callCount, direction, callType, userName))
end

function onCallEnded(userName, reason)
    log(string.format("[CALL ENDED] Call with %s ended. Reason: %s", 
        userName, reason))
end

-- File Operation Hooks
local uploadProgress = {}
local downloadProgress = {}

function onFileUploadStarted(fileName, fileSize)
    uploadCount = uploadCount + 1
    uploadProgress[fileName] = {
        uploaded = 0,
        total = tonumber(fileSize) or 0
    }
    log(string.format("[UPLOAD #%d STARTED] %s (%.2f MB)", 
        uploadCount, fileName, (tonumber(fileSize) or 0) / (1024 * 1024)))
end

function onFileUploadProgress(fileName, uploaded, total)
    if not uploadProgress[fileName] then
        uploadProgress[fileName] = { uploaded = 0, total = 0 }
    end
    
    local prev = uploadProgress[fileName].uploaded
    uploadProgress[fileName].uploaded = tonumber(uploaded) or 0
    uploadProgress[fileName].total = tonumber(total) or 0
    
    -- Only log every 10% progress to avoid spam
    local prevPercent = (prev / uploadProgress[fileName].total) * 100
    local currPercent = (uploadProgress[fileName].uploaded / uploadProgress[fileName].total) * 100
    
    if math.floor(currPercent / 10) > math.floor(prevPercent / 10) then
        log(string.format("[UPLOAD PROGRESS] %s: %.1f%% (%.2f MB / %.2f MB)", 
            fileName, currPercent,
            uploadProgress[fileName].uploaded / (1024 * 1024),
            uploadProgress[fileName].total / (1024 * 1024)))
    end
end

function onFileUploadCompleted(fileName, success)
    if success then
        log(string.format("[UPLOAD COMPLETED] %s: SUCCESS", fileName))
    else
        log(string.format("[UPLOAD FAILED] %s: FAILED", fileName))
    end
    uploadProgress[fileName] = nil
end

function onFileDownloadStarted(fileName, fileSize)
    downloadCount = downloadCount + 1
    downloadProgress[fileName] = {
        downloaded = 0,
        total = tonumber(fileSize) or 0
    }
    log(string.format("[DOWNLOAD #%d STARTED] %s (%.2f MB)", 
        downloadCount, fileName, (tonumber(fileSize) or 0) / (1024 * 1024)))
end

function onFileDownloadProgress(fileName, downloaded, total)
    if not downloadProgress[fileName] then
        downloadProgress[fileName] = { downloaded = 0, total = 0 }
    end
    
    local prev = downloadProgress[fileName].downloaded
    downloadProgress[fileName].downloaded = tonumber(downloaded) or 0
    downloadProgress[fileName].total = tonumber(total) or 0
    
    -- Only log every 10% progress to avoid spam
    local prevPercent = (prev / downloadProgress[fileName].total) * 100
    local currPercent = (downloadProgress[fileName].downloaded / downloadProgress[fileName].total) * 100
    
    if math.floor(currPercent / 10) > math.floor(prevPercent / 10) then
        log(string.format("[DOWNLOAD PROGRESS] %s: %.1f%% (%.2f MB / %.2f MB)", 
            fileName, currPercent,
            downloadProgress[fileName].downloaded / (1024 * 1024),
            downloadProgress[fileName].total / (1024 * 1024)))
    end
end

function onFileDownloadCompleted(fileName, success)
    if success then
        log(string.format("[DOWNLOAD COMPLETED] %s: SUCCESS", fileName))
    else
        log(string.format("[DOWNLOAD FAILED] %s: FAILED", fileName))
    end
    downloadProgress[fileName] = nil
end

-- Network Hooks (Low-level)
local packetStats = {
    received = 0,
    sent = 0,
    receivedBytes = 0,
    sentBytes = 0
}

function onPacketReceived(packetType, packetSize)
    packetCount = packetCount + 1
    packetStats.received = packetStats.received + 1
    packetStats.receivedBytes = packetStats.receivedBytes + (tonumber(packetSize) or 0)
    
    -- Only log non-nop packets to avoid spam
    if packetType ~= "nop" then
        log(string.format("[PACKET RX] Type: %s, Size: %d bytes (Total RX: %d packets, %.2f KB)", 
            packetType, tonumber(packetSize) or 0, 
            packetStats.received, packetStats.receivedBytes / 1024))
    end
    
    -- Log summary every 100 packets
    if packetCount % 100 == 0 then
        log(string.format("[NETWORK STATS] Total: %d packets | RX: %d (%.2f KB) | TX: %d (%.2f KB)", 
            packetCount,
            packetStats.received, packetStats.receivedBytes / 1024,
            packetStats.sent, packetStats.sentBytes / 1024))
    end
end

function onPacketSent(packetType, packetSize)
    packetCount = packetCount + 1
    packetStats.sent = packetStats.sent + 1
    packetStats.sentBytes = packetStats.sentBytes + (tonumber(packetSize) or 0)
    
    -- Only log non-nop packets to avoid spam
    if packetType ~= "nop" then
        log(string.format("[PACKET TX] Type: %s, Size: %d bytes (Total TX: %d packets, %.2f KB)", 
            packetType, tonumber(packetSize) or 0,
            packetStats.sent, packetStats.sentBytes / 1024))
    end
end

function onConnectionStateChanged(state)
    if connectionState ~= state then
        log(string.format("[CONNECTION] State changed: %s → %s", 
            connectionState, state))
        connectionState = state
        
        if state == "connected" then
            log("  → Successfully connected to Telegram servers")
        elseif state == "disconnected" then
            log("  → Disconnected from Telegram servers")
        end
    end
end

-- History Hooks
function onHistoryUpdated(chatName, updateType)
    log(string.format("[HISTORY UPDATE] [%s]: %s", chatName, updateType))
end

function onUnreadCountChanged(chatName, unreadCount)
    if tonumber(unreadCount) and tonumber(unreadCount) > 0 then
        log(string.format("[UNREAD] [%s]: %d unread message(s)", 
            chatName, tonumber(unreadCount)))
    end
end
