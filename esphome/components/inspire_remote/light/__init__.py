import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_ICON, CONF_OUTPUT_ID

from .. import CONF_REMOTE_ID, InspireRemote, inspire_remote_ns

InspireLight = inspire_remote_ns.class_("InspireLight", light.LightOutput, cg.Component)

CONFIG_SCHEMA = light.BINARY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(InspireLight),
        cv.GenerateID(CONF_REMOTE_ID): cv.use_id(InspireRemote),
        cv.Optional(CONF_ICON, default="mdi:ceiling-fan-light"): cv.icon,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    await cg.register_parented(var, config[CONF_REMOTE_ID])
