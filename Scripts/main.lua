-- Combat Camera - Configurable. MIT. Settings change only at startup / Apply.
local Settings=require("Settings")
local source=debug.getinfo(1,"S").source:gsub("^@","")
local root=assert(source:match("^(.*[/\\])Scripts[/\\][^/\\]+$"),"Cannot locate mod directory")
local values=Settings.load(root.."settings.ini")
local function apply(newValues)
    values=Settings.normalize(newValues)
    local args={};for _,id in ipairs(Settings.order)do args[#args+1]=values[id] end
    _CCSet(table.unpack(args))
end
if type(_CCSet)~="function" or type(_CCStart)~="function" then
    print("[CombatCamera] Native component unavailable; verify the complete archive is enabled.\n")
    return
end
ExecuteInGameThread(function()
    apply(values)
    if _CCStart() then print("[CombatCamera] Initialized; saved settings applied.\n") end
end)
local ok,err=pcall(function()
    require("dmm_api").subscribe("CombatCamera",function(committed)
        apply(committed)
        if values.debugLogging==1 then print("[CombatCamera] Mod Setting Menu Apply received.\n") end
    end)
end)
if not ok then print("[CombatCamera] Settings Apply subscription failed: "..tostring(err).."\n") end
