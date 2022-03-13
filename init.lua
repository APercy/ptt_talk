local server = nil
local data, ip, port
local wait_timer = 0
--local my_io = nil

modname = minetest.get_current_modname()
modpath = minetest.get_modpath(modname)

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
                        _G.pcall(client:send("file" .. "\n"))
                        --receive file
                        local uid = make_unique_id(nick)
                        local filename = "ptt_" .. uid .. ".ogg"
                        local file_path = modpath .. DIR_DELIM .. "sounds" .. DIR_DELIM .. filename
                        --local file_path = minetest.get_worldpath() .. DIR_DELIM .. filename
                        local data, e = client:receive('*a')

                        local file = insecure_environment.io.open(file_path, "wb")
                        --local file = my_io.open(file_path, 'wb') -- write binary
                        if file then
                            file:write(data)
                            file:close()
                            --[[insecure_environment.io.output(file)
                            insecure_environment.io.write(data)
                            insecure_environment.io.close(file)]]--

                            if minetest.dynamic_add_media then
                                local media_options = {filepath = file_path, ephemeral = true}
                                local media_to_play = filename:gsub("%.ogg", "")
                                minetest.dynamic_add_media(media_options, function(name)
                                        --minetest.chat_send_all(media_to_play)
                                        minetest.sound_play(media_to_play, {
                                            --to_player = nick,
                                            object = player,
                                            max_hear_distance = 30,
                                            gain = 1.0,
                                            fade = 0.0,
                                            pitch = 1.0,
                                        }, true)
                                        insecure_environment.os.remove (file_path)
                                    end)
                            end
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



