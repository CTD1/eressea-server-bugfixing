dofile("config.lua")
p = require("populate")

local function read_players()
--    return {{ email = "noreply@mailinator.com", race = "dwarf",  lang = "de" }}
    local players =  {}
    local input = io.open("newfactions", "r")
    while input do
        local str = input:read("*line")
        if str==nil then break end
        local email, race, lang = str:match("([^ ]*) ([^ ]*) ([^ ]*)")
        table.insert(players, { race = race, lang = lang, email = email })
    end
    return players
end

local function seed(r, email, race, lang)
    local f = faction.create(email, race, lang)
    local u = unit.create(f, r)
    u:set_skill("perception", 30)
    u:add_item("money", 20000)
    items = {
        log = 50,
        stone = 50,
        iron = 50,
        laen = 10,
        mallorn = 10,
        skillpotion = 5
    }
    for it, num in pairs(items) do
        u:add_item(it, num)
    end
    u = nil
    skills ={
    "crossbow",
    "bow",
    "building",
    "trade",
    "forestry",
    "catapult",
    "herbalism",
    "training",
    "riding",
    "armorer",
    "shipcraft",
    "melee",
    "sailing",
    "polearm",
    "espionage",
    "roadwork",
    "tactics",
    "stealth",
    "weaponsmithing",
    "cartmaking",
    "taxation",
    "stamina"
    }
    unit.create(f, r, 50):set_skill("entertainment", 15)
    unit.create(f, r, 5):set_skill("mining", 30)
    unit.create(f, r, 5):set_skill("quarrying", 30)
    for _, sk in ipairs(skills) do
        u = u or unit.create(f, r, 5)
        if u:set_skill(sk, 15)>0 then u=nil end
    end
    return f
end

local function dump_selection(sel)
    local best = { score = 0, r = nil }
    local r, score
    for _, r in ipairs(sel) do
        score = p.score(r)
        if score > best.score then
            best.r = r
            best.score = score
        end
        print(score, r, r.terrain)
    end
    return best
end

players = read_players()
local limit = 30000
local turn = get_turn()
local sel
if #players > 0 then
    eressea.read_game(("%d.dat"):format(turn))
    sel = p.select(regions(), limit)
    if #sel > 0 then
        local best = dump_selection(sel)
        print("finest region, " .. best.score .. " points: " .. tostring(best.r))
    end
end
math.randomseed(os.time())

local newbs = {}
for _, p in ipairs(players) do
    local index = math.random(#sel)
    local start = nil
    while not start or start.units() do
        start = sel[index]
    end
    local dupe = false
    for f in factions() do
        if f.email==p.email then
            print("seed: duplicate email " .. p.email .. " already used by faction " .. tostring(f))
            dupe = true
            break
        end
    end
    if not dupe then
        f = seed(start, p.email, p.race or "human", p.lang or "de")
        print("new faction ".. tostring(f) .. " starts in ".. tostring(start))
        table.insert(newbs, f)
    end
end

if #newbs > 0 then
    init_reports()
    for _, f in ipairs(newbs) do
        write_report(f)
    end
    eressea.write_game(("%d.dat.new"):format(turn))
end

