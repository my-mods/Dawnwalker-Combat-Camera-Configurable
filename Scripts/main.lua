-- Combat Camera - Configurable. MIT. Settings change only at startup / Apply.
local Settings=require("Settings")
local source=debug.getinfo(1,"S").source:gsub("^@","")
local root=assert(source:match("^(.*[/\\])Scripts[/\\][^/\\]+$"),"Cannot locate mod directory")
ModDiagnosticLevel=require("ModLogLevels").readLevel(root.."settings.ini")
local loaded,values=pcall(Settings.load,root.."settings.ini")
if not loaded then
    if ModDiagnosticLevel>=1 then print("[ERROR] Settings preparation failed: "..tostring(values).."\n") end
    return
end
ModDiagnosticLevel=values.logLevel
local function apply(newValues)
    values=Settings.normalize(newValues)
    ModDiagnosticLevel=values.logLevel
    local args={};for _,id in ipairs(Settings.order)do args[#args+1]=values[id] end
    _CCSetLogV2(table.unpack(args))
end
if type(_CCSetLogV2)~="function" or type(_CCStart)~="function" then
    if values.logLevel>=1 then print("[CombatCamera][ERROR] Native component unavailable; verify the complete archive is enabled.\n") end
    return
end
ExecuteInGameThread(function()
    apply(values)
    if _CCStart() and values.logLevel>=3 then print("[CombatCamera] Initialized; saved settings applied.\n") end
end)
local ok,err=pcall(function()
    require("ModDmmApi").subscribe("CombatCamera",function(committed)
        local requested=Settings.normalize(committed)
        values=requested
        ModDiagnosticLevel=values.logLevel
        local applied,issue=pcall(apply,committed)
        if not applied and values.logLevel>=1 then print("[ERROR] Settings Apply failed: "..tostring(issue).."\n") end
        if values.logLevel>=4 then print("[CombatCamera] Mod Setting Menu Apply received.\n") end
    end)
end)
if not ok and values.logLevel>=2 then print("[CombatCamera] Settings Apply subscription failed: "..tostring(err).."\n") end
