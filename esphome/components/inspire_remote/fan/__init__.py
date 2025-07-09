import esphome.codegen as cg
from esphome.components import fan
import esphome.config_validation as cv
from esphome.const import CONF_SPEED_COUNT

from .. import CONF_REMOTE_ID, InspireRemote, inspire_remote_ns

DEPENDENCIES = ["inspire_remote"]

InspireFan = inspire_remote_ns.class_("InspireFan", cg.Component, fan.Fan)

CONFIG_SCHEMA = (
    fan.fan_schema(InspireFan)
    .extend(
        {
            cv.GenerateID(CONF_REMOTE_ID): cv.use_id(InspireRemote),
            cv.Optional(CONF_SPEED_COUNT, default=3): cv.int_range(min=1),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    remote = await cg.get_variable(config[CONF_REMOTE_ID])
    var = await fan.new_fan(config)
    await cg.register_component(var, config)

    await cg.register_parented(var, config[CONF_REMOTE_ID])

    cg.add(var.set_speed_count(config[CONF_SPEED_COUNT]))

    cg.add(remote.set_fan(var))
