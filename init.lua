local server = nil
local data, ip, port
local wait_timer = 0

local queue = {}
local nametags = {}
local talk_form = "formspec_version[5]"..
    "size[1.5,1]"..
    "button[0.125,0.125;1.25,0.75;play;Talk]"

local modname = minetest.get_current_modname()
local modpath = minetest.get_modpath(modname)

local function sanitizeNick(data_received)
    return string.gsub(data_received, "%c", ""):sub( 1, 20 )
end

local function make_unique_id(player_name)
	local id=player_name .. "_"
	for i=0,5 do
		id=id..(math.random(0,9))
	end
	return id
end

-- DS-minetest wrapper for require
-- the same as `IE.require(...)`, but sets the env to IE
local function require_with_IE_env(...)
	local old_thread_env = insecure_environment.getfenv(0)

	-- set env of thread
	-- (the loader used by IE.require will probably use the thread env for
	-- the loaded functions)
	insecure_environment.setfenv(0, insecure_environment)

	-- (IE.require's env is neither _G, nor IE. we need to leave it like this,
	-- otherwise it won't find the loaders (it uses the global `loaders`, not
	-- `package.loaders` btw. (see luajit/src/lib_package.c)))

	-- we might be pcall()ed, so we need to pcall to make sure that we reset
	-- the thread env afterwards
	local ok, ret = insecure_environment.pcall(insecure_environment.require, ...)

	-- reset env of thread
	insecure_environment.setfenv(0, old_thread_env)

	if not ok then
		insecure_environment.error(ret)
	end
	return ret
end

-- require the socket and gets udp
if minetest.request_insecure_environment then
     insecure_environment = minetest.request_insecure_environment()
     if insecure_environment then
        local old_path = insecure_environment.package.path
        local old_cpath = insecure_environment.package.cpath
        
        insecure_environment.package.path = insecure_environment.package.path.. ";" .. "external/?.lua"
        insecure_environment.package.cpath = insecure_environment.package.cpath.. ";" .. "external/?.so"
        --overriding require to insecure require to allow modules to load dependencies
        --local old_require = require
        --require = insecure_environment.require

        -- load namespace
        local socket = require_with_IE_env("socket")
        --reset changes
        --require = old_require
        insecure_environment.package.path = old_path
        insecure_environment.package.cpath = old_cpath

        -- create a TCP socket and bind it to the local host, at any port
        server = insecure_environment.assert(socket.bind("*", 41000))
        server:settimeout(0)
        -- find out which port the OS chose for us
        ip, port = server:getsockname()
        --my_io = insecure_environment.io
    end
end

-- loop forever waiting for clients
minetest.register_globalstep(function(dtime)
    if server then
        --minetest.chat_send_all(dump("we have server"))
        wait_timer = wait_timer + dtime
        if wait_timer > 0.5 then wait_timer = 0.5 end

        if wait_timer >= 0.5 then
            wait_timer = 0
            local uid = nil

            -- wait for a connection from any client
            local client = server:accept()
            if client then
                local peer = client:getpeername()
                --minetest.chat_send_all(dump(peer));
                -- make sure we don't block waiting for this client's line
                --client:settimeout(10)
                -- receive the line
                local line, err = client:receive('*l')
                -- if there was no error, send it back to the client
                if not err then
                    --minetest.chat_send_all("recebeu: " .. line)
                    local nick = sanitizeNick(line)
                    local player = minetest.get_player_by_name(nick)
                    if player then
                        --minetest.chat_send_all("peer: " .. peer .. " - player ip: " .. minetest.get_player_ip(nick) )
                        if peer == minetest.get_player_ip(nick) and minetest.check_player_privs(player, {can_talk=true}) then
                            nametags[nick] = player:get_nametag_attributes()
                            --sets the nametag of the player to red
                            player:set_nametag_attributes({bgcolor = "red"})
                            --and sets to normal color again after 10 seconds
                            --[[minetest.after(10, function()
                                player:set_nametag_attributes(nametags[nick])
                            end)]]--


                            _G.pcall(client:send("file" .. "\n"))
                            --receive file
                            local uid = make_unique_id(nick)
                            local filename = "ptt_" .. uid .. ".ogg"
                            --local file_path = modpath .. DIR_DELIM .. "sounds" .. DIR_DELIM .. filename
                            local file_path = minetest.get_worldpath() .. DIR_DELIM .. filename


                            local data = ""
                            local rec_lenght = 4096
                            local count = 0
                            while true do
                                count = count + 1
                                local recv, e, part = client:receive(rec_lenght)
                                if recv ~= nil then
                                    data = data..recv
                                    if count > 25 then break end --lets limmit the size to 100k
                                else
                                    data = data..part
                                    break
                                end
                            end

                            --local data, e, part = client:receive('*a')
                            client:close()
                            local file = insecure_environment.io.open(file_path, "wb")
                            --minetest.chat_send_all(dump(file))
                            if file then
                                file:write(data)
                                file:close()

                                if minetest.dynamic_add_media then
                                    local media_options = {filepath = file_path, ephemeral = true}
                                    local media_to_play = filename:gsub("%.ogg", "")
                                    minetest.dynamic_add_media(media_options, function(name)
                                        --minetest.chat_send_all(media_to_play)
                                        queue[nick] = media_to_play
                                        minetest.show_formspec(nick, "ptt_talk:talk", talk_form)
                                        insecure_environment.os.remove (file_path)
                                    end)
                                end
                            end
                        else
                            _G.pcall(client:send("Error! You aren't allowed to talk." .. "\n"))
                        end
                    end
                else
                    --minetest.chat_send_all("deu erro: " .. err)
                end
                -- done with client, close the object
                client:close()

            end
        end


    end
end)

minetest.register_on_player_receive_fields(function(player, formname, fields)
    local form_name = "ptt_talk:talk"
    local name = player:get_player_name()
    if nametags[name] then
        player:set_nametag_attributes(nametags[name])
    end
	if formname == form_name then
        if fields.play then
            minetest.log("action", name .. " used voice chat")
            minetest.sound_play(queue[name], {
                --to_player = nick,
                object = player,
                max_hear_distance = 30,
                gain = 1.0,
                fade = 0.0,
                pitch = 1.0,
                --exclude_player = nick,
            }, true)
        end
        minetest.close_formspec(name, form_name)
    end
end)

minetest.register_privilege("can_talk", {
    description = "Gives the player the permission to use ptt_talk",
    give_to_singleplayer = true
})
