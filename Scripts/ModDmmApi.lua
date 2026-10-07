-- MIT. Scope pinned API output to this mod without replacing global print.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])'))
local env=setmetatable({print=function(message)
    if (ModDiagnosticLevel or 2)>=1 then print('[ERROR] '..tostring(message)) end
end},{__index=_G})
return assert(loadfile(directory..'dmm_api.lua','t',env))()
