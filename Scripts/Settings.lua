-- Combat Camera - Configurable. MIT.
local M = {}
M.order = {"enabled", "freeCamera", "targeting", "crosshair", "delayMs", "coneDegrees", "aimAssist", "assistStrength", "logLevel", "cameraMode", "trackingSpeed", "trackingResumeMs", "lockLastAttacker", "autoLockOnHit", "afterTargetDeath", "trackingCatchup"}
M.rules = {
    enabled={1,0,1}, freeCamera={1,0,1}, targeting={1,0,1}, crosshair={0,0,2},
    delayMs={65,0,1000}, coneDegrees={45,0,90}, aimAssist={1,0,1},
    assistStrength={35,0,80}, logLevel={2,0,4},
    cameraMode={0,0,2}, trackingSpeed={50,10,100}, trackingResumeMs={750,0,3000},
    lockLastAttacker={0,0,1},
    autoLockOnHit={1,0,1}, afterTargetDeath={0,0,1},
    trackingCatchup={300,100,500},
}
function M.normalize(values)
    local out={}
    for _,key in ipairs(M.order) do
        local rule=M.rules[key]; local value=tonumber(values[key])
        if not value or value~=value or value==math.huge or value==-math.huge then value=rule[1] end
        out[key]=math.max(rule[2],math.min(rule[3],math.floor(value+0.5)))
    end
    return out
end
function M.load(path)
    local file,readError,readCode=io.open(path,"rb");assert(file or readCode==2,readError)
    local raw=file and assert(file:read("*a")) or ""
    if file then file:close() end
    local originalText=raw
    raw=require("ModLogLevels").normalizeIniHeaders(raw)
    assert(#raw<1024*1024,"settings.ini exceeds 1 MiB")
    local section,seen,values="",{},{}
    local offset,insertAt,sectionCount=1,nil,0
    for line in (raw.."\n"):gmatch("([^\n]*)\n") do
        local parsed=offset==1 and line:gsub("^\239\187\191","") or line
        local head=parsed:match("^%s*%[([^%]]+)%]%s*\r?$")
        if head then
            section=head
            if head=="Settings" then
                sectionCount=sectionCount+1;assert(sectionCount==1,"Duplicate [Settings] section")
                insertAt=math.min(#raw,offset+#line)
            end
        end
        if section=="Settings" then
            local key,value=parsed:match("^%s*([%w_]+)%s*=%s*(.*)")
            if key and M.rules[key] then
                assert(not seen[key],"Duplicate settings key: "..key)
                local number=tonumber(value:match("^([^;#]*)"));local rule=M.rules[key]
                assert(number and number==number and number>=rule[2] and number<=rule[3] and number%1==0,"Invalid settings value: "..key)
                seen[key]=true; values[key]=number
            end
        end
        offset=offset+#line+1
    end
    -- An explicit current level wins over the retired toggle, including Off=0.
    if not seen.logLevel then
        local legacy, count, scope=nil,0,""
        for line in (raw.."\n"):gmatch("([^\n]*)\n") do
            scope=line:match("^%s*%[([^%]]+)%]") or scope
            if scope=="Settings" then
                local v=line:match("^%s*debugLogging%s*=%s*([^;#]*)")
                if v then count=count+1;legacy=tonumber(v) end
            end
        end
        assert(count<=1 and (count==0 or legacy==0 or legacy==1),"Invalid legacy logging setting")
        values.logLevel=legacy==1 and 4 or 2
    end
    local missing={}
    for _,key in ipairs(M.order) do
        if not seen[key] then missing[#missing+1]=key.."="..(values[key] or M.rules[key][1]) end
    end
    if #missing>0 or raw~=originalText then
        -- Preserve every existing byte; add defaults to the existing section.
        local addition=table.concat(missing,"\r\n").."\r\n"
        if #missing==0 then
            -- Header normalization alone still uses the checked transaction.
        elseif insertAt then
            local prefix=raw:sub(1,insertAt)
            if prefix:sub(-1)~="\n" then prefix=prefix.."\r\n" end
            raw=prefix..addition..raw:sub(insertAt+1)
        else raw=raw..(#raw>0 and "\r\n" or "").."[Settings]\r\n"..addition end
        local tmp,backup=path..".log-levels.tmp",path..".before-log-levels"
        local probe=io.open(tmp,"rb");if probe then probe:close();error("Recover settings temporary: "..tmp) end
        local original=io.open(path,"rb")
        if original then
            local old=original:read("*a");original:close()
            local saved=io.open(backup,"rb")
            if saved then saved:close() else
                local copy=assert(io.open(backup,"wb"));assert(copy:write(old));assert(copy:close())
            end
        end
        local writer=assert(io.open(tmp,"wb"));local wrote=writer:write(raw);local closed=writer:close()
        if not wrote or not closed then os.remove(tmp);error("Settings write failed") end
        local verify=assert(io.open(tmp,"rb"));local check=verify:read("*a");verify:close()
        assert(check==raw,"Settings temporary verification failed")
        local old=path..".log-levels.old"
        if original then
            local occupied=io.open(old,"rb");if occupied then occupied:close();os.remove(tmp);error("Recover "..old) end
            local ok,why=os.rename(path,old);if not ok then os.remove(tmp);error(why) end
        end
        local moved,why=os.rename(tmp,path)
        if not moved then
            if original then
                local restored,issue=os.rename(old,path)
                if not restored then error(tostring(why).."; rollback failed: "..tostring(issue).."; recover "..old.." and "..tmp) end
            end
            os.remove(tmp);error(why)
        end
        if original then os.remove(old) end
    end
    return M.normalize(values)
end
return M
