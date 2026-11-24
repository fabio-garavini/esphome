import esphome.codegen as cg
from esphome.components import light
from esphome.components.const import CONF_REMOTE_ID
import esphome.config_validation as cv
from esphome.const import CONF_ICON, CONF_OUTPUT_ID
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import DOMAIN, InspireRemote, inspire_remote_ns

DEPENDENCIES = [DOMAIN]

InspireLight = inspire_remote_ns.class_(
    "InspireLight", light.LightOutput, cg.Parented.template(InspireRemote)
)

CONFIG_SCHEMA = light.BINARY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(InspireLight),
        cv.GenerateID(CONF_REMOTE_ID): cv.use_id(InspireRemote),
        cv.Optional(CONF_ICON, default="mdi:ceiling-fan-light"): cv.icon,
    }
)

FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain="light",
    platform=DOMAIN,
    keys=(CONF_REMOTE_ID,),
    message="Only one inspire_remote light entry is allowed per remote",
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_parented(var, config[CONF_REMOTE_ID])
