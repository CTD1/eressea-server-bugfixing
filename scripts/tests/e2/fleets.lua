require "lunit"

module("tests.e2.fleets", package.seeall, lunit.testcase)

function setup()
    eressea.free_game()
    eressea.settings.set("nmr.timeout", "0")
    eressea.settings.set("NewbieImmunity", "0")
end

function test_fleetbasics()
    local ocean = region.create(1, 0, "ocean")
    local r = region.create(0, 0, "plain")
    local f = faction.create("noreply@eressea.de", "human", "de")
    local f2 = faction.create("noreply@eressea.de", "human", "de")
    local f3 = faction.create("noreply@eressea.de", "human", "de")
    local s1 = ship.create(r, "longboat")
    local s2 = ship.create(r, "longboat")
    local s3 = ship.create(r, "longboat")
    local s4 = ship.create(r, "longboat")
    local s5 = ship.create(r, "longboat")
    local s6 = ship.create(r, "longboat")
    local u1 = unit.create(f, r, 4)
    local u2 = unit.create(f, r, 26)
    local u3 = unit.create(f2, r, 10)
    local u4 = unit.create(f2, r, 1)
    local u5 = unit.create(f3, r, 120)
    
  assert_not_nil(u2)
    u1:add_item("money", 1000)
    u2:add_item("money", 1000)
    u3:add_item("money", 1000)
    u4:add_item("money", 1000)
    u5:add_item("money", 10000)
    u1.ship = s1
    u2.ship = s2
    u3.ship = s3
    u4.ship = s4
    u5.ship = s3
    u1:set_skill("sailing", 10)
    u2:set_skill("sailing", 5)
    u1:clear_orders()
    u2:clear_orders()
    u3:clear_orders()
    u4:clear_orders()
    u1:add_order("NACH O")
    u3:add_order("Kontaktiere " .. itoa36(u1.id))
	u1:add_order("flotte erstellen")
  	u1:add_order("flotte beitreten " .. itoa36(s2.id))
   	u1:add_order("flotte beitreten " .. itoa36(s3.id))
    u1:add_order("flotte beitreten " .. itoa36(s4.id))
  	u1:add_order("flotte beitreten " .. itoa36(s5.id))
   	u1:add_order("flotte beitreten " .. itoa36(s6.id))
    process_orders()
    assert_equal(ocean.id, u1.region.id) -- the plain case: okay
    assert_equal(ocean.id, u2.region.id) -- u2 should be on the fleet
    assert_equal(ocean.id, u3.region.id) -- u3 too
    assert_equal(r.id, u4.region.id) -- u4 not, no contact
    assert_equal(ocean.id, u5.region.id) -- u5 is on the same ship like u3
    assert_equal(ocean.id, s1.region.id) -- the plain case: okay
    assert_equal(ocean.id, s2.region.id) -- s2 should be part of the fleet
    assert_equal(ocean.id, s3.region.id) -- s3 too
    assert_equal(r.id, s4.region.id) -- s4 not, no contact
    assert_equal(ocean.id, s5.region.id) -- s5 should be part of the fleet (no owner)
    assert_equal(r.id, s6.region.id) -- s6 not (only 4 persons in the capt'n unit -> max 4 ships)
   
end

function test_landing_terrain()
    local ocean = region.create(1, 0, "ocean")
    local r = region.create(0, 0, "glacier")
    local f = faction.create("noreply@eressea.de", "human", "de")
    local f2 = faction.create("noreply@eressea.de", "human", "de")
    local s = ship.create(ocean, "longboat")
    local u1 = unit.create(f, ocean, 1)
    local u2 = unit.create(f2, r, 1)
    assert_not_nil(u2)
    u1:add_item("money", 1000)
    u2:add_item("money", 1000)

    u1.ship = s
    u1:set_skill("sailing", 10)    u1:clear_orders()
    u1:add_order("NACH w")
    process_orders()

    assert_equal(ocean.id, u1.region.id) -- cannot land in glacier without harbour
end

function test_landing_insects()
  local ocean = region.create(1, 0, "ocean")
  local r = region.create(0, 0, "glacier")
  local harbour = building.create(r, "harbour")
  harbour.size = 25
  local f = faction.create("noreply@eressea.de", "insect", "de")
  local f2 = faction.create("noreply@eressea.de", "human", "de")
  local s = ship.create(ocean, "longboat")
  local u1 = unit.create(f, ocean, 1)
  local u2 = unit.create(f2, r, 1)
  assert_not_nil(u2)
  u1:add_item("money", 1000)
  u2:add_item("money", 1000)
  u2.building = harbour
  
  u1.ship = s
  u1:set_skill("sailing", 10)
  u1:clear_orders()
  u1:add_order("NACH w")
  process_orders()
  
  assert_equal(ocean.id, u1.region.id) -- insects cannot land in glaciers
end
