-- Combat Camera - Configurable. MIT.
local M = {}
M.order = {"enabled", "freeCamera", "targeting", "crosshair", "delayMs", "coneDegrees", "aimAssist", "assistStrength", "debugLogging"}
M.rules = {
    enabled={1,0,1}, freeCamera={1,0,1}, targeting={1,0,1}, crosshair={0,0,2},
    delayMs={65,0,1000}, coneDegrees={45,0,90}, aimAssist={1,0,1},
    assistStrength={35,0,80}, debugLogging={0,0,1},
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
    local file=io.open(path,"rb");local raw=file and file:read("*a") or ""
    if file then file:close() end
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
    local missing={}
    for _,key in ipairs(M.order) do
        if not seen[key] then missing[#missing+1]=key.."="..M.rules[key][1] end
    end
    if #missing>0 then
        -- Preserve every existing byte; add defaults to the existing section.
        local addition=table.concat(missing,"\r\n").."\r\n"
        if insertAt then
            local prefix=raw:sub(1,insertAt)
            if prefix:sub(-1)~="\n" then prefix=prefix.."\r\n" end
            raw=prefix..addition..raw:sub(insertAt+1)
        else raw=raw..(#raw>0 and "\r\n" or "").."[Settings]\r\n"..addition end
        local writer=assert(io.open(path,"wb"));assert(writer:write(raw));assert(writer:close())
    end
    return M.normalize(values)
end
return M
