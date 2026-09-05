import esphome.codegen as cg
from esphome.components import binary_sensor, climate_ir
import esphome.config_validation as cv
from esphome.types import ConfigType

CONF_STATE_SENSOR_ID = "state_sensor_id"

AUTO_LOAD = ["climate_ir"]

daikin_wrc_ns = cg.esphome_ns.namespace("daikin_wrc")
DaikinWrcClimate = daikin_wrc_ns.class_("DaikinWrcClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(DaikinWrcClimate).extend(
    {
        cv.Optional(CONF_STATE_SENSOR_ID): cv.use_id(binary_sensor.BinarySensor),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)

    if (state_sensor_id := config.get(CONF_STATE_SENSOR_ID)) is not None:
        bs = await cg.get_variable(state_sensor_id)
        cg.add(var.set_state_sensor(bs))
