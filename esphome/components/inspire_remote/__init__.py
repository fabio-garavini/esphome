import esphome.codegen as cg
from esphome.components import remote_base
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@fabio-garavini"]

DEPENDENCIES = ["remote_transmitter"]
AUTO_LOAD = ["remote_base"]

MULTI_CONF = True

inspire_remote_ns = cg.esphome_ns.namespace("inspire_remote")
InspireRemote = inspire_remote_ns.class_(
    "InspireRemote",
    cg.Component,
    remote_base.RemoteReceiverListener,
    remote_base.RemoteTransmittable,
)

CONF_REMOTE_ID = "remote_id"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(InspireRemote),
            cv.Optional(remote_base.CONF_RECEIVER_ID): cv.use_id(
                remote_base.RemoteReceiverBase
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(remote_base.REMOTE_TRANSMITTABLE_SCHEMA)
)

InspireRemoteBaseSchema = cv.Schema(
    {
        cv.GenerateID(CONF_REMOTE_ID): cv.use_id(InspireRemote),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await remote_base.register_transmittable(var, config)
    if remote_base.CONF_RECEIVER_ID in config:
        await remote_base.register_listener(var, config)
