dofile(scriptpath .. "/hse/grails.lua")
dofile(scriptpath .. "/hse/spoils.lua")
dofile(scriptpath .. "/hse/buildings.lua")

function write_stats(filename)
  local file = io.open(reportpath .. "/" .. filename, "w")
  print("grails")
  write_grails(file)
  file:write("\n")
  print("spoils")
  write_spoils(file)
  print("buildings")
  write_buildings(file)
  file:close()
end
