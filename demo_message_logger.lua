-- Demo Lua Plugin: Message Logger
-- This plugin logs all new messages received in Telegram

function onLoad()
    log("Message Logger plugin loaded!")
    log("This plugin will log all new messages you receive.")
end

function onUnload()
    log("Message Logger plugin unloaded.")
end

function onNewMessage(chatName, senderName, text, isOutgoing)
    local direction = isOutgoing and "OUT" or "IN"
    local logMessage = string.format(
        "[%s] %s -> %s: %s",
        direction,
        senderName,
        chatName,
        text
    )
    log(logMessage)
end
