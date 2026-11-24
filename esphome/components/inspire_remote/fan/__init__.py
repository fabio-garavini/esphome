import esphome.codegen as cg
from esphome.components import fan
from esphome.components.const import CONF_REMOTE_ID
import esphome.config_validation as cv
from esphome.const import CONF_SPEED_COUNT
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import DOMAIN, InspireRemote, inspire_remote_ns

DEPENDENCIES = [DOMAIN]

InspireFan = inspire_remote_ns.class_("InspireFan", cg.Component, fan.Fan)

CONFIG_SCHEMA = (
    fan.fan_schema(InspireFan)
    .extend(
        {
            cv.GenerateID(CONF_REMOTE_ID): cv.use_id(InspireRemote),
            cv.Optional(CONF_SPEED_COUNT, default=3): cv.int_range(min=1, max=3),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain="fan",
    platform=DOMAIN,
    keys=(CONF_REMOTE_ID,),
    message="Only one inspire_remote fan entry is allowed per remote",
)


async def to_code(config: ConfigType) -> None:
    var = await fan.new_fan(config, config[CONF_SPEED_COUNT])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_REMOTE_ID])

    remote = await cg.get_variable(config[CONF_REMOTE_ID])
    cg.add(remote.set_fan(var))
