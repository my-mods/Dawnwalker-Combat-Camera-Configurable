-- ModLogLevels.lua
-- MIT License. Single source of truth for diagnostic verbosity.
--
-- Levels are cumulative: choosing a level prints that level's messages plus
-- everything more severe. Debug therefore prints everything, and Off prints
-- nothing at all.
--
--   Off      Print nothing. The deliberate "I know it is broken" choice.
--   Error    The mod gave up on something: it disabled itself or a feature.
--   Warning  One capability degraded; the rest of the mod still works.
--   Info     Notable lifecycle moments during normal play.
--   Debug    Everything, including per-event tracing and timing summaries.
--
-- Warning is the default because it reproduces exactly what this mod printed
-- before levels existed, back when logging was a plain on/off toggle: failures
-- and "feature unavailable" notices were printed unconditionally, and only the
-- tracing output obeyed the toggle.
local M = {}

M.OFF = 0
M.ERROR = 1
M.WARNING = 2
M.INFO = 3
M.DEBUG = 4

M.DEFAULT = M.WARNING

-- Ordered quietest to loudest. This is the order the settings picker lists.
M.ordered = {M.OFF, M.ERROR, M.WARNING, M.INFO, M.DEBUG}

-- Written into each log line so its severity is visible at a glance. Off has
-- no tag because nothing is ever printed at that level.
M.tags = {
    [M.ERROR] = 'ERROR',
    [M.WARNING] = 'WARN',
    [M.INFO] = 'INFO',
    [M.DEBUG] = 'DEBUG',
}

-- Shown in Mod Settings, in `M.ordered` order.
M.labels = {
    [M.OFF] = 'Off',
    [M.ERROR] = 'Error',
    [M.WARNING] = 'Warning',
    [M.INFO] = 'Info',
    [M.DEBUG] = 'Debug',
}

function M.valid(level)
    -- The current menu requires plain section headers. Keep inline comments as
-- separate comment lines; original bytes remain in the migration backup.
function M.normalizeIniHeaders(text)
    text=text:gsub('^\239\187\191','')
    local newline=text:find('\r\n',1,true) and '\r\n' or '\n'
    return (text:gsub('[^\r\n]+',function(line)
        local heading,comment=line:match('^(%s*%[[^%]]+%])%s*([;#].*)$')
        return heading and (heading..newline..comment) or line
    end))
end
-- Read-only bootstrap so even subscription/startup failures respect saved Off.
function M.readLevel(path)
    local f=io.open(path,'rb');if not f then return M.DEFAULT end
    local text=f:read(1048577);f:close();if not text or #text>1048576 then return M.DEFAULT end
    local section,level,legacy,count='',nil,nil,0
    for line in (M.normalizeIniHeaders(text)..'\n'):gmatch('([^\n]*)\n') do
        local clean=line:gsub('[;#].*$','')
        section=clean:match('^%s*%[([^%]]+)%]%s*$') or section
        if section=='Settings' or section=='LessWildlife' then
            local key,value=clean:match('^%s*([%w_]+)%s*=%s*(.-)%s*$')
            if key=='logLevel' then count=count+1;level=tonumber(value)
            elseif key=='debugLogging' then legacy=tonumber(value) end
        end
    end
    if count==1 and M.valid(level) then return level end
    if count>0 then return M.DEFAULT end
    return M.fromLegacyToggle(legacy)
end
return M.tags[level] ~= nil or level == M.OFF
end

-- Translates the `debugLogging` on/off toggle that levels replaced.
--
-- On meant "print the tracing output too", so it becomes Debug. Off still
-- printed every failure and degradation notice, which is what Warning covers,
-- so Off becomes Warning rather than Off. Mapping it to Off instead would
-- silently take away diagnostics that players already rely on.
function M.fromLegacyToggle(enabled)
    return enabled == 1 and M.DEBUG or M.WARNING
end

-- The current menu requires plain section headers. Keep inline comments as
-- separate comment lines; original bytes remain in the migration backup.
function M.normalizeIniHeaders(text)
    text=text:gsub('^\239\187\191','')
    local newline=text:find('\r\n',1,true) and '\r\n' or '\n'
    return (text:gsub('[^\r\n]+',function(line)
        local heading,comment=line:match('^(%s*%[[^%]]+%])%s*([;#].*)$')
        return heading and (heading..newline..comment) or line
    end))
end
-- Read-only bootstrap so even subscription/startup failures respect saved Off.
function M.readLevel(path)
    local f=io.open(path,'rb');if not f then return M.DEFAULT end
    local text=f:read(1048577);f:close();if not text or #text>1048576 then return M.DEFAULT end
    local section,level,legacy,count='',nil,nil,0
    for line in (M.normalizeIniHeaders(text)..'\n'):gmatch('([^\n]*)\n') do
        local clean=line:gsub('[;#].*$','')
        section=clean:match('^%s*%[([^%]]+)%]%s*$') or section
        if section=='Settings' or section=='LessWildlife' then
            local key,value=clean:match('^%s*([%w_]+)%s*=%s*(.-)%s*$')
            if key=='logLevel' then count=count+1;level=tonumber(value)
            elseif key=='debugLogging' then legacy=tonumber(value) end
        end
    end
    if count==1 and M.valid(level) then return level end
    if count>0 then return M.DEFAULT end
    return M.fromLegacyToggle(legacy)
end
return M
