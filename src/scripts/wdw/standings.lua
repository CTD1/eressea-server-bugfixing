conditions = { pyramid="Pyramide", gems="Handel", phoenix="Ph�nix" }

function log(file, line)
  print(line)
  file:write(line .. "\n")
end

function write_standings()
  print(reportpath .. "/victory.txt")
  local file = io.open(reportpath .. "/victory.txt", "w")
  
  log(file, "** Allianzen ** " .. reportpath .. "/victory.txt")
  local alliance
  for alliance in alliances() do
    local faction
    log(file, alliance.id .. ": " .. alliance.name)
    for faction in alliance.factions do
      log(file, "- " .. faction.name .." (" .. itoa36(faction.id) .. ")")
    end
    log (file, "")
  end

  log(file, "** Erf�llte Siegbedingungen **")
  local condition
  local index
  for index, condition in pairs(conditions) do
    local none = true
    log(file, condition)
    for alliance in alliances() do
      if victorycondition(alliance, index)==1 then
        log(file, "  - " .. alliance.name .. " (" .. alliance.id .. ")")
        none = false
      end
    end
    if none then
      log(file, " - Niemand")
    end
  end

  file:close()
end
