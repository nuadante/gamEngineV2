-- test.lua
function update_entity(id, dt)
    local x,y,z = get_entity_pos(id)
    set_entity_pos(id, x + dt, y, z)
  end