import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, spi
from esphome.const import CONF_ID, CONF_INTENSITY, CONF_LAMBDA

CODEOWNERS = ["@stenjo"]
DEPENDENCIES = ["spi"]

CONF_NUM_MODULES = "num_modules"

dot_matrix_ns = cg.esphome_ns.namespace("dot_matrix")
DotMatrixDisplay = dot_matrix_ns.class_(
    "DotMatrixDisplay", spi.SPIDevice, display.DisplayBuffer, cg.PollingComponent
)
DotMatrixDisplayRef = DotMatrixDisplay.operator("ref")

CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(DotMatrixDisplay),
            cv.Optional(CONF_NUM_MODULES, default=8): cv.int_range(min=1, max=16),
            cv.Optional(CONF_INTENSITY, default=3): cv.int_range(min=0, max=15),
        }
    )
    .extend(cv.polling_component_schema("500ms"))
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await spi.register_spi_device(var, config, write_only=True)
    await display.register_display(var, config)

    cg.add(var.set_num_modules(config[CONF_NUM_MODULES]))
    cg.add(var.set_intensity(config[CONF_INTENSITY]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(DotMatrixDisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
