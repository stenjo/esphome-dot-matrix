import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import spi
from esphome.const import CONF_ID, CONF_BRIGHTNESS

CODEOWNERS = ["@stenjo"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True

dot_matrix_ns = cg.esphome_ns.namespace("dot_matrix")
DotMatrix = dot_matrix_ns.class_("DotMatrix", cg.Component, spi.SPIDevice)

WriteAction = dot_matrix_ns.class_("WriteAction", automation.Action)
MarqueeAction = dot_matrix_ns.class_("MarqueeAction", automation.Action)
ClearAction = dot_matrix_ns.class_("ClearAction", automation.Action)

CONF_NUM_MODULES = "num_modules"
CONF_SCROLL_DELAY = "scroll_delay"
CONF_TEXT = "text"
CONF_CENTERED = "centered"
CONF_WRAP = "wrap"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DotMatrix),
            cv.Optional(CONF_NUM_MODULES, default=8): cv.int_range(min=1, max=16),
            cv.Optional(CONF_BRIGHTNESS, default=0): cv.int_range(min=0, max=15),
            cv.Optional(
                CONF_SCROLL_DELAY, default="30ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_num_modules(config[CONF_NUM_MODULES]))
    cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))
    cg.add(var.set_scroll_delay(config[CONF_SCROLL_DELAY].total_milliseconds))


# ------------------------------- Actions -----------------------------------

WRITE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(DotMatrix),
        cv.Required(CONF_TEXT): cv.templatable(cv.string),
        cv.Optional(CONF_CENTERED, default=False): cv.templatable(cv.boolean),
    }
)


@automation.register_action("dot_matrix.write", WriteAction, WRITE_SCHEMA, synchronous=True)
async def write_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    text = await cg.templatable(config[CONF_TEXT], args, cg.std_string)
    cg.add(var.set_text(text))
    centered = await cg.templatable(config[CONF_CENTERED], args, bool)
    cg.add(var.set_centered(centered))
    return var


MARQUEE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(DotMatrix),
        cv.Required(CONF_TEXT): cv.templatable(cv.string),
        cv.Optional(CONF_WRAP, default=True): cv.templatable(cv.boolean),
    }
)


@automation.register_action("dot_matrix.marquee", MarqueeAction, MARQUEE_SCHEMA, synchronous=True)
async def marquee_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    text = await cg.templatable(config[CONF_TEXT], args, cg.std_string)
    cg.add(var.set_text(text))
    wrap = await cg.templatable(config[CONF_WRAP], args, bool)
    cg.add(var.set_wrap(wrap))
    return var


CLEAR_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(DotMatrix)}
)


@automation.register_action("dot_matrix.clear", ClearAction, CLEAR_SCHEMA, synchronous=True)
async def clear_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
